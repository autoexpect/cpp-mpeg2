cmake_minimum_required(VERSION 3.10)
project(mpeg2_cpp VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/bitstream.cpp
    src/pes_proto.cpp
    src/ps_proto.cpp
    src/ts_proto.cpp
    src/ps_demuxer.cpp
    src/ps_muxer.cpp
    src/ts_demuxer.cpp
    src/ts_muxer.cpp
)

# Create library
add_library(mpeg2 STATIC ${SOURCES})

# Examples
option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_executable(example_ps_demuxer examples/example_ps_demuxer.cpp)
    target_link_libraries(example_ps_demuxer mpeg2)

    add_executable(example_ts_demuxer examples/example_ts_demuxer.cpp)
    target_link_libraries(example_ts_demuxer mpeg2)
    
    add_executable(example_ps_muxer examples/example_ps_muxer.cpp)
    target_link_libraries(example_ps_muxer mpeg2)
    
    add_executable(example_ts_muxer examples/example_ts_muxer.cpp)
    target_link_libraries(example_ts_muxer mpeg2)
endif()

# Installation
install(TARGETS mpeg2
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include/mpeg2)
