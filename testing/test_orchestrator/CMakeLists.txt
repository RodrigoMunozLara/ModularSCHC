
message(STATUS "Compiling schc_test")
add_executable(schc_test main.cpp)

# Enlaza con la librería correcta
target_link_libraries(schc_test PRIVATE module_schc)

#add_custom_command(TARGET schc_test POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        ${PROJECT_SOURCE_DIR}/config/config_frag.json
#        $<TARGET_FILE_DIR:schc_test>/config_frag.json

#    COMMENT "Copying config_frag.json to binaries folder")




