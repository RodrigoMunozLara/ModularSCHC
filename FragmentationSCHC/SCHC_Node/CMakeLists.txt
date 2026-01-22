# Find required packages
find_package(PkgConfig REQUIRED)
find_package(fmt 9.1.0 REQUIRED)
find_package(spdlog 1.12.0 REQUIRED)


# Collect source files
file(GLOB CPP_FILES "src/*.cpp")

# Ahora (STATIC library)
add_library(schc_node STATIC ${CPP_FILES})

# Include headers so other targets can use them
target_include_directories(schc_node PUBLIC include)

# Link dependencies (fmt, spdlog)
target_link_libraries(schc_node PRIVATE fmt::fmt spdlog::spdlog)

# Debug: list source files
message(STATUS "SCHC Node sources: ${CPP_FILES}")

