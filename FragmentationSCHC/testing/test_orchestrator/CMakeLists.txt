# 1. Definimos el ejecutable (supongo que tienes algo así)
add_executable(modular_schc main.cpp) 

# 2. Enlazamos con tu librería y dependencias
target_link_libraries(modular_schc PRIVATE module_schc correct)

# 3. COMANDO DE COPIA (Atado al ejecutable)
# Usamos PROJECT_SOURCE_DIR para subir un nivel desde 'testing' a la raíz
set(NODE_DIR "${PROJECT_SOURCE_DIR}/binaries/schc_node")
set(GATEWAY_DIR "${PROJECT_SOURCE_DIR}/binaries/schc_gateway")

add_custom_command(TARGET modular_schc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${NODE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GATEWAY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:modular_schc> "${NODE_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:modular_schc> "${GATEWAY_DIR}/"
    COMMENT "Deploying EXECUTABLE BINARY to simulation folders... /binaries; /binaries/schc_node; /binaries/schc_gateway"
)

#add_custom_command(TARGET modular_schc POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        ${PROJECT_SOURCE_DIR}/config/config_frag.json
#        $<TARGET_FILE_DIR:modular_schc>/config_frag.json


#    COMMENT "Copying config_frag.json to binaries folder")




