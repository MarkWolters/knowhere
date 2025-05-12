cmake_minimum_required(VERSION 3.20)
project(knowhere_jvector)

# Find JNI
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

# Set source files
set(JVECTOR_SRC
    src/jvector_index.cpp
    src/jvector_jni.cpp
)

# Create library
add_library(knowhere_jvector STATIC ${JVECTOR_SRC})

# Define JVector path
set(KNOWHERE_JVECTOR_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# Include directories
target_include_directories(knowhere_jvector
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add shared library
add_library(jvector SHARED
    src/jvector_index.cpp
    src/jvector_jni.cpp
)

target_include_directories(jvector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${JNI_INCLUDE_DIRS}
)

target_link_libraries(jvector PUBLIC
    ${JNI_LIBRARIES}
    knowhere
)

# Add test executable
if(KNOWHERE_BUILD_TESTS)
    add_executable(test_jvector
        test/test_jvector_index.cpp
    )

    target_link_libraries(test_jvector PRIVATE
        jvector
        catch2
    )

    add_test(NAME test_jvector COMMAND test_jvector)
endif()

# Add compile definitions
target_compile_definitions(knowhere_jvector
    PRIVATE
        KNOWHERE_JVECTOR_PATH="${KNOWHERE_JVECTOR_PATH}"
)

# Link with JNI
target_link_libraries(knowhere_jvector
    PUBLIC
        ${JNI_LIBRARIES}
)

# Set C++ standard
set_target_properties(knowhere_jvector PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
