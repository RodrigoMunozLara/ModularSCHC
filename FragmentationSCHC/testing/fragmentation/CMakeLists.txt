# Definimos el ejecutable
add_executable(modular_schc main.cpp) 

# Enlazamos con tu librería y dependencias
target_link_libraries(modular_schc PRIVATE module_schc correct)

# Comando de Copia
# Usamos PROJECT_SOURCE_DIR para subir un nivel desde 'testing' a la raíz
set(NODE_DIR "${PROJECT_SOURCE_DIR}/binaries/schc_node")
set(GATEWAY_DIR "${PROJECT_SOURCE_DIR}/binaries/schc_gateway")

add_custom_command(TARGET modular_schc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${NODE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GATEWAY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:modular_schc> "${NODE_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:modular_schc> "${GATEWAY_DIR}/"
    COMMENT "Deploying EXECUTABLE BINARY only to /binaries/schc_node and /binaries/schc_gateway"
)





