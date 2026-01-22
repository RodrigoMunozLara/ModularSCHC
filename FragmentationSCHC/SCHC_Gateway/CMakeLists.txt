# Find required packages
find_package(PkgConfig REQUIRED)
find_package(fmt 9.1.0 REQUIRED)
find_package(spdlog 1.12.0 REQUIRED)

# Find Mosquitto using pkg-config
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# Debug information
message(STATUS "Mosquitto include dirs: ${MOSQUITTO_INCLUDE_DIRS}")
message(STATUS "Mosquitto libraries: ${MOSQUITTO_LIBRARIES}")

# Collect source files
file(GLOB CPP_FILES "src/*.cpp")

# Ahora (STATIC library)
add_library(schc_gateway STATIC ${CPP_FILES})

# Include headers so other targets can use them
target_include_directories(schc_gateway PUBLIC include)

# Link dependencies (fmt, spdlog, mosquitto)
target_link_libraries(schc_gateway PRIVATE fmt::fmt spdlog::spdlog ${MOSQUITTO_LIBRARIES})

# Debug: list source files
message(STATUS "SCHC Gateway sources: ${CPP_FILES}")

