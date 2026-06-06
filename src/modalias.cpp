/*
 * Copyright (C) 2026 BlissLabs
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hwdb_sensor.h"
#include <fstream>
#include <string>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
#include "smbios.h"

// Kernel ascii_filter equivalent
static void ascii_filter(std::string& d, const char* s) {
    if (!s) return;
    for (; *s; s++) {
        if (*s > ' ' && *s < 127 && *s != ':') {
            d += *s;
        }
    }
}

static bool get_dmi_data_sysfs(const char *path, uint8_t **buffer, size_t *size) {
    FILE *input;
    char fileName[256];
    snprintf(fileName, sizeof(fileName), "%s/DMI", path);
    struct stat info;
    if (stat(fileName, &info) != 0) return false;
    *size = (size_t) info.st_size + 32;
    *buffer = (uint8_t*) malloc(*size);
    if (*buffer == NULL) return false;

    input = fopen(fileName, "rb");
    if (input == NULL) { free(*buffer); return false; }
    fread((char*) *buffer + 32, (size_t) info.st_size, 1, input);
    fclose(input);

    snprintf(fileName, sizeof(fileName), "%s/smbios_entry_point", path);
    input = fopen(fileName, "rb");
    if (input == NULL) { free(*buffer); return false; }
    fread((char*) *buffer, 32, 1, input);
    fclose(input);
    return true;
}

static std::string build_dmi_from_smbios() {
    uint8_t *buffer = nullptr;
    size_t size = 0;
    
    // Attempt to read from sysfs (Android/Linux standard location for raw DMI tables)
    if (!get_dmi_data_sysfs("/sys/firmware/dmi/tables", &buffer, &size)) {
        return "";
    }

    struct ParserContext parser;
    if (smbios_initialize(&parser, buffer, size, SMBIOS_ANY) != SMBERR_OK) {
        free(buffer);
        return "";
    }

    std::string bvn, bvr, bd, svn, pn, pvr, rvn, rn, rvr, cvn, cvr;
    int ct = -1;

    const struct Entry *entry = nullptr;
    while (smbios_next(&parser, &entry) == SMBERR_OK) {
        if (entry->type == TYPE_BIOS_INFO) {
            ascii_filter(bvn, entry->data.bios_info.Vendor);
            ascii_filter(bvr, entry->data.bios_info.BIOSVersion);
            ascii_filter(bd, entry->data.bios_info.BIOSReleaseDate);
        } else if (entry->type == TYPE_SYSTEM_INFO) {
            ascii_filter(svn, entry->data.system_info.Manufacturer);
            ascii_filter(pn, entry->data.system_info.ProductName);
            ascii_filter(pvr, entry->data.system_info.Version);
        } else if (entry->type == TYPE_BASEBOARD_INFO) {
            ascii_filter(rvn, entry->data.baseboard_info.Manufacturer);
            ascii_filter(rn, entry->data.baseboard_info.Product);
            ascii_filter(rvr, entry->data.baseboard_info.Version);
        } else if (entry->type == TYPE_SYSTEM_ENCLOSURE) {
            ascii_filter(cvn, entry->data.system_enclosure.Manufacturer);
            ct = entry->data.system_enclosure.Type & 0x7f; // Mask lock bit
            ascii_filter(cvr, entry->data.system_enclosure.Version);
        }
    }

    free(buffer);

    std::string result = "dmi:";
    if (!bvn.empty()) result += "bvn" + bvn + ":";
    if (!bvr.empty()) result += "bvr" + bvr + ":";
    if (!bd.empty()) result += "bd" + bd + ":";
    if (!svn.empty()) result += "svn" + svn + ":";
    if (!pn.empty()) result += "pn" + pn + ":";
    if (!pvr.empty()) result += "pvr" + pvr + ":";
    if (!rvn.empty()) result += "rvn" + rvn + ":";
    if (!rn.empty()) result += "rn" + rn + ":";
    if (!rvr.empty()) result += "rvr" + rvr + ":";
    if (!cvn.empty()) result += "cvn" + cvn + ":";
    if (ct != -1) result += "ct" + std::to_string(ct) + ":";
    if (!cvr.empty()) result += "cvr" + cvr + ":";
    
    return result;
}

char* hwdb_sensor_build_modalias(const char* sensor_modalias) {
    if (!sensor_modalias) return nullptr;

    std::string dmi_modalias;
    
    // Attempt to read DMI modalias from sysfs first
    std::ifstream dmi_file("/sys/class/dmi/id/modalias");
    if (dmi_file.is_open()) {
        std::getline(dmi_file, dmi_modalias);
        // Trim whitespace/newlines
        dmi_modalias.erase(std::find_if(dmi_modalias.rbegin(), dmi_modalias.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), dmi_modalias.end());
    }

    // Fallback to smbios-parser if sysfs is not available or empty
    if (dmi_modalias.empty()) {
        dmi_modalias = build_dmi_from_smbios();
    }

    std::string full_modalias = "sensor:modalias:";
    full_modalias += sensor_modalias;
    
    if (!dmi_modalias.empty()) {
        if (full_modalias.back() != ':') {
            full_modalias += ":";
        }
        
        if (dmi_modalias.find("dmi:") != 0) {
            full_modalias += "dmi:";
        }
        full_modalias += dmi_modalias;
    } else {
        if (full_modalias.back() != ':') {
            full_modalias += ":";
        }
        full_modalias += "dmi:";
    }

    return strdup(full_modalias.c_str());
}
