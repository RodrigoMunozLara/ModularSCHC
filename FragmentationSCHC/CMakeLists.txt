# ------------------------------------------------------------------------------
# Fragmentation SCHC layer
# ------------------------------------------------------------------------------

if(BUILD_SCHC_NODE)
    add_subdirectory(SCHC_Node)
endif()

if(BUILD_SCHC_GATEWAY)
    add_subdirectory(SCHC_Gateway)
endif()
