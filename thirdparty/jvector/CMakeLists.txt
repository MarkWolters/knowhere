cmake_minimum_required(VERSION 3.20)
project(knowhere_jvector VERSION 1.0.0 LANGUAGES CXX)

# Find Boost (without components)
find_package(Boost 1.74.0 REQUIRED)
if(Boost_FOUND)
    message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")
    include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
endif()

# Create interface target for nlohmann_json (header-only)
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json INTERFACE IMPORTED)
    target_include_directories(nlohmann_json INTERFACE "/usr/include")
    target_link_libraries(nlohmann_json INTERFACE "-lnlohmann_json")
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

# Find Java with Panama support
find_package(Java 21 REQUIRED)
include(UseJava)

# Verify Java version
if(Java_VERSION_MAJOR LESS 21)
    message(FATAL_ERROR "JDK 21 or higher is required. Found version ${Java_VERSION}")
endif()

# Set JAVA_HOME if not set
if(NOT DEFINED ENV{JAVA_HOME})
    execute_process(
        COMMAND which java
        OUTPUT_VARIABLE JAVA_EXECUTABLE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND readlink -f ${JAVA_EXECUTABLE}
        OUTPUT_VARIABLE JAVA_EXECUTABLE_REAL
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    get_filename_component(JAVA_BIN_DIR ${JAVA_EXECUTABLE_REAL} DIRECTORY)
    get_filename_component(JAVA_HOME ${JAVA_BIN_DIR}/.. ABSOLUTE)
    set(ENV{JAVA_HOME} ${JAVA_HOME})
    message(STATUS "Setting JAVA_HOME to ${JAVA_HOME}")
endif()

# Get Java home
set(JAVA_HOME $ENV{JAVA_HOME})
message(STATUS "Found Java home: ${JAVA_HOME}")

# Find JNI
find_package(JNI REQUIRED)

# Set JNI include directories
set(JNI_INCLUDE_DIRS
    ${JAVA_HOME}/include
    ${JAVA_HOME}/include/linux
    ${JAVA_HOME}/include/jdk.incubator.foreign
)
message(STATUS "JNI include dirs: ${JNI_INCLUDE_DIRS}")

# Find JVM library
find_library(JAVA_JVM_LIBRARY
    NAMES jvm
    PATHS ${JAVA_HOME}/lib/server
          ${JAVA_HOME}/lib/amd64/server
          ${JAVA_HOME}/jre/lib/amd64/server
    NO_DEFAULT_PATH
)

# Set JNI libraries
if(JAVA_JVM_LIBRARY)
    message(STATUS "JVM library: ${JAVA_JVM_LIBRARY}")
    set(JNI_LIBRARIES ${JAVA_JVM_LIBRARY})
else()
    message(WARNING "JVM library not found, JNI might not work properly")
endif()

# Set source files
set(JVECTOR_SRC
    src/jvector_index.cpp
    src/jvector_jni.cpp
)

# Add JNI and Panama includes
include_directories(SYSTEM ${JNI_INCLUDE_DIRS})

# Add compiler definitions for Panama
add_definitions(-DPANAMA)

# Add link options for the JNI library
add_link_options(-Wl,--enable-new-dtags,-rpath,'$ORIGIN/../lib')

# Create the library with C++17 standard
add_library(knowhere_jvector STATIC ${JVECTOR_SRC})
add_library(knowhere::jvector ALIAS knowhere_jvector)

target_compile_options(knowhere_jvector PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
)

target_compile_definitions(knowhere_jvector PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

# Set C++ standard
set_target_properties(knowhere_jvector PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Download JVector JAR if not exists
set(JVECTOR_VERSION "1.0.0")
set(JVECTOR_JAR_NAME "jvector-${JVECTOR_VERSION}.jar")
set(JVECTOR_JAR_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/${JVECTOR_JAR_NAME}")

if(NOT EXISTS ${JVECTOR_JAR_PATH})
    message(STATUS "Downloading JVector JAR...")
    file(DOWNLOAD
        "https://github.com/jbellis/jvector/releases/download/v${JVECTOR_VERSION}/${JVECTOR_JAR_NAME}"
        ${JVECTOR_JAR_PATH}
        SHOW_PROGRESS
    )
endif()

# Get Knowhere root directory
get_filename_component(KNOWHERE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../ ABSOLUTE)

# Include directories
target_include_directories(knowhere_jvector
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${KNOWHERE_ROOT}/include
        ${KNOWHERE_ROOT}/src
        ${JNI_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(knowhere_jvector
    PRIVATE
        ${JNI_LIBRARIES}
        ${Boost_LIBRARIES}
        nlohmann_json::nlohmann_json
)

# Install targets
install(TARGETS knowhere_jvector
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install JVector JAR
install(FILES ${JVECTOR_JAR_PATH}
    DESTINATION lib
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/knowhere/jvector
    FILES_MATCHING PATTERN "*.h"
)
