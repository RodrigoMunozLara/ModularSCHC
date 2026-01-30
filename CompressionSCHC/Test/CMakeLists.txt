cmake_minimum_required(VERSION 3.10)

project(CompressionSCHC_Test VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/SCHC_RulesManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/SCHC_PacketParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/SCHC_RuleID.cpp
)

add_executable(CompressionSCHC_Test ${SOURCES})

target_include_directories(CompressionSCHC_Test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../includes
)

# Copia archivos de config/ al directorio del ejecutable
add_custom_command(TARGET CompressionSCHC_Test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/yang
        $<TARGET_FILE_DIR:CompressionSCHC_Test>/yang
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/../config/rules.json
        $<TARGET_FILE_DIR:CompressionSCHC_Test>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../packets
        $<TARGET_FILE_DIR:CompressionSCHC_Test>/packets

    
)

include(CTest)
enable_testing()


