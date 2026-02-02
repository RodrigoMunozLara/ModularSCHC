add_executable(test_orchestrator main.cpp)

# Enlaza con la librería correcta
target_link_libraries(test_orchestrator PRIVATE module_schc)

# Copiar config.json después de build
add_custom_command(TARGET test_orchestrator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/config/config_frag_node.json
        $<TARGET_FILE_DIR:test_orchestrator>/config_frag_node.json
    COMMENT "Copying config_frag_node.json to binaries folder"
)