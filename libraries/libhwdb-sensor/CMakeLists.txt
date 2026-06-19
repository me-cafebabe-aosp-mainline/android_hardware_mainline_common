cmake_minimum_required(VERSION 3.10)
project(libhwdb-sensor)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add smbios-parser dependency
add_subdirectory(../smbios-parser smbios-parser)

# Create the static library
add_library(hwdb_sensor STATIC src/hwdb_parser.cpp src/modalias.cpp)
target_include_directories(hwdb_sensor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(hwdb_sensor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../smbios-parser)
target_link_libraries(hwdb_sensor PUBLIC smbios_static)

# Create the test executable
add_executable(test_hwdb test/test_hwdb.cpp)
target_link_libraries(test_hwdb hwdb_sensor)

# Android specific property fetching might need log/cutils in actual Android build,
# but for Linux build we will mock or bypass it if not available.
