if (BUILD_SCHC_GATEWAY)
    add_subdirectory(test_gw_fragmentation)
endif()

if (BUILD_SCHC_NODE)
    add_subdirectory(test_node_fragmentation)
endif()
