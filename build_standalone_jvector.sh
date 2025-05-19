#!/bin/bash
set -e

# Set environment variables
export JAVA_HOME=/usr/lib/jvm/jdk-23.0.2-oracle-x64
export PATH=$PATH:$HOME/.local/bin:$JAVA_HOME/bin
export LD_LIBRARY_PATH=$JAVA_HOME/lib:$JAVA_HOME/lib/server:$LD_LIBRARY_PATH

# Create build directory
rm -rf build_standalone
mkdir -p build_standalone
cd build_standalone

# Create a standalone JVector project
mkdir -p jvector_standalone/include
mkdir -p jvector_standalone/src

# Create a minimal CMakeLists.txt for the standalone JVector project
cat > jvector_standalone/CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.20)
project(jvector_standalone VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find required packages
find_package(JNI REQUIRED)
find_package(Boost 1.74.0 REQUIRED)

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

# Define source files
set(SOURCES
    src/jvector_jni.cpp
    src/jvector_index.cpp
)

# Create library
add_library(jvector SHARED ${SOURCES})
target_link_libraries(jvector
    ${JNI_LIBRARIES}
    ${Boost_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Install targets
install(TARGETS jvector
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
EOF

# Create a minimal jvector_jni.h header
cat > jvector_standalone/include/jvector_jni.h << 'EOF'
#pragma once

#include <jni.h>
#include <string>
#include <vector>

namespace jvector {

// JNI utility functions
bool InitJVM();
void DestroyJVM();

// JNI environment management
JNIEnv* GetJNIEnv();
bool ReleaseJNIEnv(JNIEnv* env);

// JVector JNI interface
class JVectorJNI {
public:
    JVectorJNI();
    ~JVectorJNI();

    bool Initialize();
    bool BuildIndex(const std::vector<float>& vectors, int dimension, int num_vectors);
    std::vector<int> Search(const std::vector<float>& query, int k);

private:
    JavaVM* jvm_;
    jobject jvector_object_;
};

} // namespace jvector
EOF

# Create a minimal jvector_index.h header
cat > jvector_standalone/include/jvector_index.h << 'EOF'
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace jvector {

class JVectorIndex {
public:
    JVectorIndex();
    ~JVectorIndex();

    bool Build(const std::vector<float>& vectors, int dimension, int num_vectors);
    std::vector<int> Search(const std::vector<float>& query, int k);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace jvector
EOF

# Create a minimal jvector_jni.cpp implementation
cat > jvector_standalone/src/jvector_jni.cpp << 'EOF'
#include "jvector_jni.h"
#include <iostream>

namespace jvector {

static JavaVM* g_jvm = nullptr;

bool InitJVM() {
    if (g_jvm != nullptr) {
        return true;
    }

    JavaVMInitArgs vm_args;
    JavaVMOption options[3];
    
    // Set class path to current directory and include JVector JAR
    options[0].optionString = const_cast<char*>("-Djava.class.path=./lib/jvector-1.0.0.jar");
    
    // Disable JVM verification - useful for debugging
    options[1].optionString = const_cast<char*>("-Xverify:none");
    
    // Set initial heap size
    options[2].optionString = const_cast<char*>("-Xms64m");
    
    vm_args.version = JNI_VERSION_10;
    vm_args.nOptions = 3;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    
    jint res = JNI_CreateJavaVM(&g_jvm, (void**)nullptr, &vm_args);
    if (res != JNI_OK) {
        std::cerr << "Failed to create JVM: " << res << std::endl;
        return false;
    }
    
    return true;
}

void DestroyJVM() {
    if (g_jvm != nullptr) {
        g_jvm->DestroyJavaVM();
        g_jvm = nullptr;
    }
}

JNIEnv* GetJNIEnv() {
    if (g_jvm == nullptr) {
        if (!InitJVM()) {
            return nullptr;
        }
    }
    
    JNIEnv* env = nullptr;
    jint res = g_jvm->GetEnv((void**)&env, JNI_VERSION_10);
    if (res == JNI_EDETACHED) {
        res = g_jvm->AttachCurrentThread((void**)&env, nullptr);
        if (res != JNI_OK) {
            std::cerr << "Failed to attach thread to JVM: " << res << std::endl;
            return nullptr;
        }
    } else if (res != JNI_OK) {
        std::cerr << "Failed to get JNI environment: " << res << std::endl;
        return nullptr;
    }
    
    return env;
}

bool ReleaseJNIEnv(JNIEnv* /*env*/) {
    if (g_jvm == nullptr) {
        return false;
    }
    
    g_jvm->DetachCurrentThread();
    return true;
}

JVectorJNI::JVectorJNI() : jvm_(nullptr), jvector_object_(nullptr) {
}

JVectorJNI::~JVectorJNI() {
    JNIEnv* env = GetJNIEnv();
    if (env != nullptr && jvector_object_ != nullptr) {
        env->DeleteGlobalRef(jvector_object_);
    }
}

bool JVectorJNI::Initialize() {
    JNIEnv* env = GetJNIEnv();
    if (env == nullptr) {
        return false;
    }
    
    jvm_ = g_jvm;
    
    // This is a placeholder implementation
    // In a real implementation, we would create a JVector object here
    return true;
}

bool JVectorJNI::BuildIndex(const std::vector<float>& /*vectors*/, int /*dimension*/, int /*num_vectors*/) {
    // This is a placeholder implementation
    return true;
}

std::vector<int> JVectorJNI::Search(const std::vector<float>& /*query*/, int /*k*/) {
    // This is a placeholder implementation
    return std::vector<int>();
}

} // namespace jvector
EOF

# Create a minimal jvector_index.cpp implementation
cat > jvector_standalone/src/jvector_index.cpp << 'EOF'
#include "jvector_index.h"
#include "jvector_jni.h"

namespace jvector {

class JVectorIndex::Impl {
public:
    Impl() : jni_() {
        jni_.Initialize();
    }
    
    bool Build(const std::vector<float>& vectors, int dimension, int num_vectors) {
        return jni_.BuildIndex(vectors, dimension, num_vectors);
    }
    
    std::vector<int> Search(const std::vector<float>& query, int k) {
        return jni_.Search(query, k);
    }
    
private:
    JVectorJNI jni_;
};

JVectorIndex::JVectorIndex() : impl_(new Impl()) {
}

JVectorIndex::~JVectorIndex() = default;

bool JVectorIndex::Build(const std::vector<float>& vectors, int dimension, int num_vectors) {
    return impl_->Build(vectors, dimension, num_vectors);
}

std::vector<int> JVectorIndex::Search(const std::vector<float>& query, int k) {
    return impl_->Search(query, k);
}

} // namespace jvector
EOF

# Build the standalone JVector project
cd jvector_standalone
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE="Release" \
    -DJAVA_HOME="${JAVA_HOME}" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    .

# Build the project
echo "Building the standalone JVector project..."
cmake --build . -j$(nproc)

echo "Build completed!"
