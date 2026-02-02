cmake_minimum_required(VERSION 3.16)
project(ModularSCHC LANGUAGES C CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/binaries)

# ------------------------------------------------------------------------------
# Build selection
# Only one target can be built at a time
# ------------------------------------------------------------------------------

option(BUILD_SCHC_NODE "Build SCHC Node application" OFF)
option(BUILD_SCHC_GATEWAY "Build SCHC Gateway application" OFF)

# Enforce mutual exclusivity
if(BUILD_SCHC_NODE AND BUILD_SCHC_GATEWAY)
    message(FATAL_ERROR
        "SCHC_Node and SCHC_Gateway cannot be built at the same time. "
        "Enable only one option."
    )
endif()

# Require at least one target
if(NOT BUILD_SCHC_NODE AND NOT BUILD_SCHC_GATEWAY)
    message(FATAL_ERROR
        "No target selected. Enable either BUILD_SCHC_NODE or BUILD_SCHC_GATEWAY."
    )
endif()

# ------------------------------------------------------------------------------
# Global configuration
# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(BUILD_SCHC_GATEWAY)
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
    add_library(module_schc STATIC ${CPP_FILES})

    # Include headers so other targets can use them
    target_include_directories(module_schc PUBLIC include)

    # Link dependencies (fmt, spdlog, mosquitto)
    target_link_libraries(module_schc PRIVATE fmt::fmt spdlog::spdlog ${MOSQUITTO_LIBRARIES})

    # Debug: list source files
    message(STATUS "ModuleSCHC sources: ${CPP_FILES}")

elseif(BUILD_SCHC_NODE)
    # Find required packages
    find_package(PkgConfig REQUIRED)
    find_package(fmt 9.1.0 REQUIRED)
    find_package(spdlog 1.12.0 REQUIRED)

    # Collect source files
    file(GLOB CPP_FILES "src/*.cpp")

    # Ahora (STATIC library)
    add_library(module_schc STATIC ${CPP_FILES})

    # Include headers so other targets can use them
    target_include_directories(module_schc PUBLIC include)

    # Link dependencies (fmt, spdlog, mosquitto)
    target_link_libraries(module_schc PRIVATE fmt::fmt spdlog::spdlog)

    # Debug: list source files
    message(STATUS "ModuleSCHC sources: ${CPP_FILES}")

endif()

add_subdirectory(testing)




