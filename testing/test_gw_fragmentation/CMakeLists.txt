add_executable(test_gw_fragmentation main.cpp)
target_link_libraries(test_gw_fragmentation PRIVATE schc_gateway)

# Copy config.json after build
add_custom_command(TARGET test_gw_fragmentation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/config/config.ini
        $<TARGET_FILE_DIR:test_gw_fragmentation>/config.ini
    COMMENT "Copying config.ini to binaries folder"
)
