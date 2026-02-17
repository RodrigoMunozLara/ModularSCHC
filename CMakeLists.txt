cmake_minimum_required(VERSION 3.16)
project(ModularSCHC LANGUAGES C CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/binaries)

# ------------------------------------------------------------------------------
# Global configuration
# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Find required packages
find_package(PkgConfig REQUIRED)
find_package(fmt 9.1.0 REQUIRED)
find_package(spdlog 1.12.0 REQUIRED)

# Find Mosquitto using pkg-config
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# Debug information
message(STATUS "Mosquitto include dirs: ${MOSQUITTO_INCLUDE_DIRS}")
message(STATUS "Mosquitto libraries: ${MOSQUITTO_LIBRARIES}")

set(CPP_FILES
    "src/BackhaulCore.cpp"
    "src/ConfigManager.cpp"
    "src/Orchestrator.cpp"
    "src/SCHCCore.cpp"
    "src/SCHCNodeMessage.cpp"
    "src/SCHCGWMessage.cpp"
    "src/SCHCSession.cpp"
    "src/SCHCTimer.cpp"
    "src/SCHCAckOnErrorSender.cpp"
    "src/SCHCAckOnErrorSender_END.cpp"
    "src/SCHCAckOnErrorSender_ERROR.cpp"
    "src/SCHCAckOnErrorSender_INIT.cpp"
    "src/SCHCAckOnErrorSender_RESEND_MISSING_FRAG.cpp"
    "src/SCHCAckOnErrorSender_SEND.cpp"
    "src/SCHCAckOnErrorSender_WAIT_X_ACK.cpp"
    "src/SCHCLoRaWANStack.cpp"
    "src/SCHCLoRaWAN_NS_MQTT_Stack.cpp"
    "src/SCHCAckOnErrorReceiver.cpp"
    "src/SCHCAckOnErrorReceiver_END.cpp"
    "src/SCHCAckOnErrorReceiver_ERROR.cpp"
    "src/SCHCAckOnErrorReceiver_INIT.cpp"
    "src/SCHCAckOnErrorReceiver_RCV_WINDOW.cpp"
    "src/SCHCAckOnErrorReceiver_WAIT_END.cpp"
    "src/SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG.cpp"
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

# Ahora (STATIC library)
add_library(module_schc STATIC ${CPP_FILES})

# Include headers so other targets can use them
target_include_directories(module_schc PUBLIC include)

# Link dependencies (fmt, spdlog, mosquitto)
target_link_libraries(module_schc PRIVATE fmt::fmt spdlog::spdlog ${MOSQUITTO_LIBRARIES})

# Debug: list source files
message(STATUS "ModuleSCHC sources: ${CPP_FILES}")



#######################################################################
##### Comment this lines if you don't need AddressSanitizer o ASan Tool
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
#######################################################################

add_subdirectory(testing)




