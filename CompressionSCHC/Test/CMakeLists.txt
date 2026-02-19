cmake_minimum_required(VERSION 3.10)

project(CompressionSCHC_Test VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/SCHC_RulesManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PacketParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/SCHC_Rule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/SCHC_Yang.cpp
)

add_executable(CompressionSCHC_Test ${SOURCES})

target_include_directories(CompressionSCHC_Test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBYANG REQUIRED libyang)

target_include_directories(CompressionSCHC_Test PRIVATE ${LIBYANG_INCLUDE_DIRS})
target_link_directories(CompressionSCHC_Test PRIVATE ${LIBYANG_LIBRARY_DIRS})
target_link_libraries(CompressionSCHC_Test PRIVATE ${LIBYANG_LIBRARIES})
target_compile_options(CompressionSCHC_Test PRIVATE ${LIBYANG_CFLAGS_OTHER})



# Copia archivos de config/ al directorio del ejecutable
add_custom_command(TARGET CompressionSCHC_Test POST_BUILD
    # Crear carpetas destino
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:CompressionSCHC_Test>/config
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:CompressionSCHC_Test>/yang

    # Copiar config/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../config/rules.json
        $<TARGET_FILE_DIR:CompressionSCHC_Test>/config/rules.json

    # Copiar yang/ completo (incluye dependencias si las tienes ahí)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../yang
        $<TARGET_FILE_DIR:CompressionSCHC_Test>/yang

    # Copiar packets/
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../packets
        $<TARGET_FILE_DIR:CompressionSCHC_Test>/packets
)


include(CTest)
enable_testing()


