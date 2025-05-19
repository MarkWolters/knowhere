#!/bin/bash
set -e

# Set environment variables
export JAVA_HOME=/usr/lib/jvm/jdk-23.0.2-oracle-x64
export PATH=$PATH:$HOME/.local/bin:$JAVA_HOME/bin
export LD_LIBRARY_PATH=$JAVA_HOME/lib:$JAVA_HOME/lib/server:$LD_LIBRARY_PATH

echo "=== Building Standalone JVector Library ==="
# First build the standalone JVector library
./build_standalone_jvector.sh

# Create integration build directory
rm -rf build_integrated
mkdir -p build_integrated
cd build_integrated

echo "=== Creating Integration Build ==="

# Create a minimal CMakeLists.txt for the Knowhere project with JVector integration
cat > ../CMakeLists.txt.integrated << 'EOF'
cmake_minimum_required(VERSION 3.20)
project(knowhere VERSION 0.1.0 LANGUAGES CXX C)

# Include standard CMake modules
include(GNUInstallDirs)
include(ExternalProject)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set compiler flags without -Werror
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fPIC -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required packages
find_package(JNI REQUIRED)
find_package(Boost 1.74.0 REQUIRED)
find_package(OpenMP REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${JNI_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Add nlohmann_json as a header-only library
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Define the JVector library as an imported library
add_library(jvector SHARED IMPORTED)
set_target_properties(jvector PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/build_standalone/jvector_standalone/libjvector.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/build_standalone/jvector_standalone/include"
)

# Create a simple test executable
add_executable(jvector_test tests/jvector_test.cpp)
target_link_libraries(jvector_test
    jvector
    ${JNI_LIBRARIES}
    ${Boost_LIBRARIES}
    nlohmann_json::nlohmann_json
    OpenMP::OpenMP_CXX
    Threads::Threads
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Install targets
install(TARGETS jvector_test
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/build_standalone/jvector_standalone/libjvector.so"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build_standalone/jvector_standalone/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/knowhere/jvector
    FILES_MATCHING PATTERN "*.h"
)
EOF

# Create a simple test file
mkdir -p ../tests
cat > ../tests/jvector_test.cpp << 'EOF'
#include <iostream>
#include <vector>
#include <jni.h>
#include "jvector_index.h"

// JNI initialization helper functions
static JavaVM* g_jvm = nullptr;

bool initJVM() {
    if (g_jvm != nullptr) {
        return true;
    }

    JavaVMInitArgs vm_args;
    JavaVMOption options[3];
    
    // Set class path to include the JVector JAR
    options[0].optionString = const_cast<char*>("-Djava.class.path=../thirdparty/jvector/lib/jvector-1.0.0.jar");
    
    // Disable JVM verification - useful for debugging
    options[1].optionString = const_cast<char*>("-Xverify:none");
    
    // Set initial heap size
    options[2].optionString = const_cast<char*>("-Xms64m");
    
    vm_args.version = JNI_VERSION_10;
    vm_args.nOptions = 3;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    
    // Create the JVM
    JNIEnv* env;
    jint res = JNI_CreateJavaVM(&g_jvm, (void**)&env, &vm_args);
    if (res != JNI_OK) {
        std::cerr << "Failed to create JVM: " << res << std::endl;
        return false;
    }
    
    std::cout << "JVM initialized successfully!" << std::endl;
    
    // Verify JVector classes are available
    jclass graphIndexBuilderClass = env->FindClass("com/datastax/jvector/graph/GraphIndexBuilder");
    if (graphIndexBuilderClass == nullptr) {
        std::cerr << "Failed to find GraphIndexBuilder class" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return false;
    }
    
    std::cout << "Found GraphIndexBuilder class!" << std::endl;
    env->DeleteLocalRef(graphIndexBuilderClass);
    
    return true;
}

void destroyJVM() {
    if (g_jvm != nullptr) {
        g_jvm->DestroyJavaVM();
        g_jvm = nullptr;
    }
}

int main() {
    std::cout << "Initializing JVector..." << std::endl;
    
    try {
        // Initialize JVM first
        if (!initJVM()) {
            std::cerr << "Failed to initialize JVM" << std::endl;
            return 1;
        }
        
        std::cout << "JVM initialized successfully!" << std::endl;
        
        // Create a JVector index
        jvector::JVectorIndex index;
        
        // Create some test vectors
        const int dimension = 128;
        const int num_vectors = 1000;
        std::vector<float> vectors(dimension * num_vectors, 0.0f);
        
        // Initialize vectors with some random data
        for (int i = 0; i < num_vectors; i++) {
            for (int j = 0; j < dimension; j++) {
                vectors[i * dimension + j] = static_cast<float>(rand()) / RAND_MAX;
            }
        }
        
        // Build the index
        std::cout << "Building index..." << std::endl;
        bool success = index.Build(vectors, dimension, num_vectors);
        if (!success) {
            std::cerr << "Failed to build index" << std::endl;
            destroyJVM();
            return 1;
        }
        
        // Create a query vector
        std::vector<float> query(dimension, 0.0f);
        for (int j = 0; j < dimension; j++) {
            query[j] = static_cast<float>(rand()) / RAND_MAX;
        }
        
        // Search the index
        std::cout << "Searching index..." << std::endl;
        std::vector<int> results = index.Search(query, 10);
        
        // Print results
        std::cout << "Search results:" << std::endl;
        for (size_t i = 0; i < results.size(); i++) {
            std::cout << "  " << i << ": " << results[i] << std::endl;
        }
        
        // Clean up JVM
        destroyJVM();
        
        std::cout << "JVector test completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        destroyJVM();
        return 1;
    }
    
    return 0;
}
EOF

# Create a backup of the original CMakeLists.txt if not already backed up
if [ ! -f /home/mwolters138/Dev/repos/knowhere/CMakeLists.txt.orig ]; then
    cp /home/mwolters138/Dev/repos/knowhere/CMakeLists.txt /home/mwolters138/Dev/repos/knowhere/CMakeLists.txt.orig
fi

# Replace the original CMakeLists.txt with our integrated version
cp ../CMakeLists.txt.integrated /home/mwolters138/Dev/repos/knowhere/CMakeLists.txt

# Configure with CMake
echo "=== Configuring with CMake ==="
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE="Release" \
    -DJAVA_HOME="${JAVA_HOME}" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    "/home/mwolters138/Dev/repos/knowhere/."

# Build the project
echo "=== Building the integrated project ==="
cmake --build . -j$(nproc)

echo "=== Build completed! ==="
echo "You can now run the JVector test with:"
echo "./jvector_test"

# Restore the original CMakeLists.txt
cp /home/mwolters138/Dev/repos/knowhere/CMakeLists.txt.orig /home/mwolters138/Dev/repos/knowhere/CMakeLists.txt
