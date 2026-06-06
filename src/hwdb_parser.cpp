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
#include <vector>
#include <map>
#include <fnmatch.h>
#include <iostream>
#include <algorithm>

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

struct HwdbEntry {
    std::vector<std::string> matches;
    std::map<std::string, std::string> properties;
};

struct hwdb_sensor_context {
    std::vector<HwdbEntry> entries;
    std::map<std::string, std::string> last_query_result;
};

// Helper to trim whitespace from the end of a string
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

static bool parse_hwdb_file(hwdb_sensor_context* ctx, const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    HwdbEntry current_entry;
    bool collecting_properties = false;

    while (std::getline(file, line)) {
        // Remove trailing whitespace and comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        rtrim(line);

        if (line.empty()) {
            continue;
        }

        if (line[0] == ' ' || line[0] == '\t') {
            // Property line
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = line.substr(0, eq_pos);
                std::string value = line.substr(eq_pos + 1);
                
                // Trim leading whitespace from key
                size_t key_start = key.find_first_not_of(" \t");
                if (key_start != std::string::npos) {
                    key = key.substr(key_start);
                }
                
                // Trim leading/trailing whitespace from value if needed
                size_t val_start = value.find_first_not_of(" \t");
                if (val_start != std::string::npos) {
                    value = value.substr(val_start);
                } else {
                    value = ""; // Empty value
                }

                current_entry.properties[key] = value;
                collecting_properties = true;
            }
        } else {
            // Match pattern line
            if (collecting_properties) {
                // We were collecting properties and now hit a new match line.
                // Save the current entry and start a new one.
                if (!current_entry.matches.empty() && !current_entry.properties.empty()) {
                    ctx->entries.push_back(current_entry);
                }
                current_entry.matches.clear();
                current_entry.properties.clear();
                collecting_properties = false;
            }
            
            // Trim leading whitespace just in case, though it shouldn't have any
            size_t start = line.find_first_not_of(" \t");
            if (start != std::string::npos) {
                current_entry.matches.push_back(line.substr(start));
            }
        }
    }

    // Push the last entry
    if (!current_entry.matches.empty() && !current_entry.properties.empty()) {
        ctx->entries.push_back(current_entry);
    }

    return true;
}

hwdb_sensor_context* hwdb_sensor_init(const char* custom_path) {
    hwdb_sensor_context* ctx = new hwdb_sensor_context();

    if (custom_path != nullptr) {
        if (parse_hwdb_file(ctx, custom_path)) {
            return ctx;
        }
    } else {
        // Try Android property first if on Android
#ifdef __ANDROID__
        char prop_value[PROP_VALUE_MAX];
        if (__system_property_get("persist.vendor.sensor.hwdb_path", prop_value) > 0) {
            if (parse_hwdb_file(ctx, prop_value)) {
                return ctx;
            }
        }
        
        // Fallback Android path if property isn't set
        if (parse_hwdb_file(ctx, "/vendor/etc/60-sensor.hwdb")) {
            return ctx;
        }
#endif
        // Try default Linux locations
        const char* default_paths[] = {
            "/etc/udev/hwdb.d/60-sensor.hwdb",
            "/usr/lib/udev/hwdb.d/60-sensor.hwdb"
        };
        for (const char* path : default_paths) {
            if (parse_hwdb_file(ctx, path)) {
                return ctx;
            }
        }
    }

    // Failed to find or parse any hwdb file
    delete ctx;
    return nullptr;
}

void hwdb_sensor_free(hwdb_sensor_context* ctx) {
    if (ctx) {
        delete ctx;
    }
}

const char* hwdb_sensor_get_property(hwdb_sensor_context* ctx, const char* modalias, const char* property_key) {
    if (!ctx || !modalias || !property_key) return nullptr;

    ctx->last_query_result.clear();

    // Iterate through all entries and accumulate properties where modalias matches
    for (const auto& entry : ctx->entries) {
        bool matched = false;
        for (const auto& match_pattern : entry.matches) {
            if (fnmatch(match_pattern.c_str(), modalias, 0) == 0) {
                matched = true;
                break;
            }
        }

        if (matched) {
            for (const auto& prop : entry.properties) {
                ctx->last_query_result[prop.first] = prop.second;
            }
        }
    }

    auto it = ctx->last_query_result.find(property_key);
    if (it != ctx->last_query_result.end()) {
        return it->second.c_str();
    }

    return nullptr;
}
