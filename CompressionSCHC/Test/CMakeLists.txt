cmake_minimum_required(VERSION 3.10)

project(CompressionSCHC_Test VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    main.cpp
    ../src/SCHC_RulesTest.cpp
    ../src/SCHC_RuleID.cpp
)

add_executable(CompressionSCHC_Test ${SOURCES})

target_include_directories(CompressionSCHC_Test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../includes
)

include(CTest)
enable_testing()
