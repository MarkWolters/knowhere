include(FetchContent)

FetchContent_Declare(
    jvector
    GIT_REPOSITORY https://github.com/jbellis/jvector.git
    GIT_TAG main  # You might want to pin this to a specific version
)

FetchContent_MakeAvailable(jvector)

# Add jvector to the include directories
include_directories(${jvector_SOURCE_DIR}/include)

# Add jvector source files to knowhere
target_sources(knowhere PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/index/jvector/JVectorIndex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/index/jvector/impl/JVectorIndexWrapper.cpp
)

# Link against jvector library
target_link_libraries(knowhere PRIVATE jvector)
