cmake_minimum_required(VERSION 3.16)
project(ModularSCHC LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Definimos las rutas de destino claramente
set(BIN_OUT_DIR "${PROJECT_SOURCE_DIR}/binaries")
set(NODE_DIR "${BIN_OUT_DIR}/schc_node")
set(GATEWAY_DIR "${BIN_OUT_DIR}/schc_gateway")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_OUT_DIR})

# 2. Configuración de paquetes y dependencias
find_package(PkgConfig REQUIRED)
find_package(fmt 9.1.0 REQUIRED)
find_package(spdlog 1.12.0 REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# 3. Lista de archivos fuente
set(CPP_FILES
    "src/BackhaulCore.cpp"
    "src/Orchestrator.cpp"
    "src/SCHCCore.cpp"
    "src/SCHCSession.cpp"
    "src/ConfigManager.cpp"
    "src/SCHCTimer.cpp"
    "src/stacks/SCHCLoRaWANStack.cpp"
    "src/stacks/SCHCLoRaWAN_NS_MQTT_Stack.cpp"
    "src/schcAckOnError/SCHCNodeMessage.cpp"
    "src/schcAckOnError/SCHCGWMessage.cpp"   
    "src/schcAckOnError/SCHCAckOnErrorSender.cpp"
    "src/schcAckOnError/SCHCAckOnErrorSender_END.cpp"
    "src/schcAckOnError/SCHCAckOnErrorSender_ERROR.cpp"
    "src/schcAckOnError/SCHCAckOnErrorSender_INIT.cpp"
    "src/schcAckOnError/SCHCAckOnErrorSender_RESEND_MISSING_FRAG.cpp"
    "src/schcAckOnError/SCHCAckOnErrorSender_SEND.cpp"
    "src/schcAckOnError/SCHCAckOnErrorSender_WAIT_X_ACK.cpp"
    "src/schcAckOnError/SCHCAckOnErrorReceiver.cpp"
    "src/schcAckOnError/SCHCAckOnErrorReceiver_END.cpp"
    "src/schcAckOnError/SCHCAckOnErrorReceiver_ERROR.cpp"
    "src/schcAckOnError/SCHCAckOnErrorReceiver_INIT.cpp"
    "src/schcAckOnError/SCHCAckOnErrorReceiver_RCV_WINDOW.cpp"
    "src/schcAckOnError/SCHCAckOnErrorReceiver_WAIT_END.cpp"
    "src/schcAckOnError/SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG.cpp"
)

add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

# 4. CREAR EL TARGET (Primero se crea, luego se configura)
add_library(module_schc STATIC ${CPP_FILES})

target_include_directories(module_schc PUBLIC include)
target_link_libraries(module_schc PRIVATE fmt::fmt spdlog::spdlog ${MOSQUITTO_LIBRARIES})

# 5. ASAN (AddressSanitizer) - Mejorado para ser más limpio
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()


add_subdirectory(testing)




