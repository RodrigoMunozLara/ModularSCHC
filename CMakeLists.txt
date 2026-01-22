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

# ------------------------------------------------------------------------------
# Project structure
# ------------------------------------------------------------------------------

option(BUILD_TESTS "Build tests" ON)
if (BUILD_TESTS AND BUILD_SCHC_GATEWAY)
    add_subdirectory(FragmentationSCHC)
    add_subdirectory(testing)
elseif(BUILD_TESTS AND BUILD_SCHC_NODE)
    add_subdirectory(FragmentationSCHC)
    add_subdirectory(testing)
endif()


