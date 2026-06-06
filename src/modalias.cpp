#include "hwdb_sensor.h"
#include <fstream>
#include <string>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>

char* hwdb_sensor_build_modalias(const char* sensor_modalias) {
    if (!sensor_modalias) return nullptr;

    std::string dmi_modalias;
    
    // Attempt to read DMI modalias
    std::ifstream dmi_file("/sys/class/dmi/id/modalias");
    if (dmi_file.is_open()) {
        std::getline(dmi_file, dmi_modalias);
        // Trim whitespace/newlines
        dmi_modalias.erase(std::find_if(dmi_modalias.rbegin(), dmi_modalias.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), dmi_modalias.end());
    } else {
        // Fallback or leave empty. 
        // If the user wants smbios-parser, they can construct the DMI string themselves
        // or we can add it later. For now, we will just proceed with empty DMI if not found.
    }
    std::string full_modalias = "sensor:modalias:";
    full_modalias += sensor_modalias;
    
    if (!dmi_modalias.empty()) {
        if (full_modalias.back() != ':') {
            full_modalias += ":";
        }
        
        // Sometimes /sys/class/dmi/id/modalias might or might not start with 'dmi:'
        // Usually it does: `dmi:bvn...`
        if (dmi_modalias.find("dmi:") != 0) {
            full_modalias += "dmi:";
        }
        full_modalias += dmi_modalias;
    } else {
        // If no DMI available, we might still want to append a wildcard or just leave it
        // systemd rules usually expect :dmi: at the end, even if empty, or they use *
        // Example match: sensor:modalias:acpi:BOSC0200:*:dmi:*:svnAcer:*
        if (full_modalias.back() != ':') {
            full_modalias += ":";
        }
        // Let's add dmi: anyway to allow basic prefix matching if needed
        full_modalias += "dmi:";
    }

    return strdup(full_modalias.c_str());
}
