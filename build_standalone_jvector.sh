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

# Create jvector_jni.h header file
cat > jvector_standalone/include/jvector_jni.h << 'EOF'
#pragma once

#include <jni.h>
#include <vector>

namespace jvector {

bool InitJVM();
void DestroyJVM();
JNIEnv* GetJNIEnv();
bool ReleaseJNIEnv(JNIEnv* env);

class JVectorJNI {
public:
    JVectorJNI();
    ~JVectorJNI();

    bool Initialize();
    bool BuildIndex(const std::vector<float>& vectors, int dimension, int num_vectors);
    std::vector<int> Search(const std::vector<float>& query, int k);

private:
    JavaVM* jvm_;
    jclass builder_class_;
    jclass graph_index_class_;
    jclass vector_similarity_class_;
    jobject builder_object_;
    jobject graph_index_object_;
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
#include <stdexcept>

namespace jvector {

static JavaVM* g_jvm = nullptr;

bool InitJVM() {
    if (g_jvm != nullptr) {
        return true;
    }

    JavaVMInitArgs vm_args;
    JavaVMOption options[3];
    
    // Set class path to include the JVector JAR
    options[0].optionString = const_cast<char*>("-Djava.class.path=../../thirdparty/jvector/lib/jvector-1.0.0.jar");
    
    // Disable JVM verification - useful for debugging
    options[1].optionString = const_cast<char*>("-Xverify:none");
    
    // Set initial heap size
    options[2].optionString = const_cast<char*>("-Xms64m");
    
    vm_args.version = JNI_VERSION_10;
    vm_args.nOptions = 3;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    
    JNIEnv* env;
    jint res = JNI_CreateJavaVM(&g_jvm, (void**)&env, &vm_args);
    if (res != JNI_OK) {
        std::cerr << "Failed to create JVM: " << res << std::endl;
        return false;
    }
    
    std::cout << "JVM created successfully!" << std::endl;
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

JVectorJNI::JVectorJNI() 
    : jvm_(nullptr), 
      builder_class_(nullptr), 
      graph_index_class_(nullptr),
      vector_similarity_class_(nullptr),
      builder_object_(nullptr), 
      graph_index_object_(nullptr) {
}

JVectorJNI::~JVectorJNI() {
    JNIEnv* env = GetJNIEnv();
    if (env != nullptr) {
        if (builder_object_ != nullptr) {
            env->DeleteGlobalRef(builder_object_);
        }
        if (graph_index_object_ != nullptr) {
            env->DeleteGlobalRef(graph_index_object_);
        }
        if (builder_class_ != nullptr) {
            env->DeleteGlobalRef(builder_class_);
        }
        if (graph_index_class_ != nullptr) {
            env->DeleteGlobalRef(graph_index_class_);
        }
        if (vector_similarity_class_ != nullptr) {
            env->DeleteGlobalRef(vector_similarity_class_);
        }
    }
}

bool JVectorJNI::Initialize() {
    JNIEnv* env = GetJNIEnv();
    if (env == nullptr) {
        std::cerr << "Failed to get JNI environment" << std::endl;
        return false;
    }
    
    jvm_ = g_jvm;
    
    // Find the GraphIndexBuilder class
    jclass localBuilderClass = env->FindClass("com/datastax/jvector/graph/GraphIndexBuilder");
    if (localBuilderClass == nullptr) {
        std::cerr << "Failed to find GraphIndexBuilder class" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return false;
    }
    
    // Create a global reference to the class
    builder_class_ = (jclass)env->NewGlobalRef(localBuilderClass);
    env->DeleteLocalRef(localBuilderClass);
    
    // Find the GraphIndex class
    jclass localGraphIndexClass = env->FindClass("com/datastax/jvector/graph/GraphIndex");
    if (localGraphIndexClass == nullptr) {
        std::cerr << "Failed to find GraphIndex class" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return false;
    }
    
    // Create a global reference to the class
    graph_index_class_ = (jclass)env->NewGlobalRef(localGraphIndexClass);
    env->DeleteLocalRef(localGraphIndexClass);
    
    // Find the VectorSimilarity class
    jclass localVectorSimilarityClass = env->FindClass("com/datastax/jvector/vector/VectorSimilarity");
    if (localVectorSimilarityClass == nullptr) {
        std::cerr << "Failed to find VectorSimilarity class" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return false;
    }
    
    // Create a global reference to the class
    vector_similarity_class_ = (jclass)env->NewGlobalRef(localVectorSimilarityClass);
    env->DeleteLocalRef(localVectorSimilarityClass);
    
    // Find the DOT_PRODUCT field in VectorSimilarity
    jfieldID dotProductField = env->GetStaticFieldID(vector_similarity_class_, "DOT_PRODUCT", "Lcom/datastax/jvector/vector/VectorSimilarity;");
    if (dotProductField == nullptr) {
        std::cerr << "Failed to find DOT_PRODUCT field" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return false;
    }
    
    // Get the DOT_PRODUCT value
    jobject dotProductObject = env->GetStaticObjectField(vector_similarity_class_, dotProductField);
    if (dotProductObject == nullptr) {
        std::cerr << "Failed to get DOT_PRODUCT value" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return false;
    }
    
    // Find the create method
    jmethodID createMethod = env->GetStaticMethodID(builder_class_, "create", "()Lcom/datastax/jvector/graph/GraphIndexBuilder;");
    if (createMethod == nullptr) {
        std::cerr << "Failed to find create method" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        env->DeleteLocalRef(dotProductObject);
        return false;
    }
    
    // Call the create method
    jobject localBuilderObject = env->CallStaticObjectMethod(builder_class_, createMethod);
    if (localBuilderObject == nullptr) {
        std::cerr << "Failed to create GraphIndexBuilder" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        env->DeleteLocalRef(dotProductObject);
        return false;
    }
    
    // Find the withSimilarity method
    jmethodID withSimilarityMethod = env->GetMethodID(builder_class_, "withSimilarity", "(Lcom/datastax/jvector/vector/VectorSimilarity;)Lcom/datastax/jvector/graph/GraphIndexBuilder;");
    if (withSimilarityMethod == nullptr) {
        std::cerr << "Failed to find withSimilarity method" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        env->DeleteLocalRef(localBuilderObject);
        env->DeleteLocalRef(dotProductObject);
        return false;
    }
    
    // Call the withSimilarity method
    jobject builderWithSimilarity = env->CallObjectMethod(localBuilderObject, withSimilarityMethod, dotProductObject);
    if (builderWithSimilarity == nullptr) {
        std::cerr << "Failed to call withSimilarity method" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        env->DeleteLocalRef(localBuilderObject);
        env->DeleteLocalRef(dotProductObject);
        return false;
    }
    
    // Clean up the local reference we don't need anymore
    env->DeleteLocalRef(localBuilderObject);
    env->DeleteLocalRef(dotProductObject);
    
    // Store the builder object as a global reference
    builder_object_ = env->NewGlobalRef(builderWithSimilarity);
    env->DeleteLocalRef(builderWithSimilarity);
    
    std::cout << "JVectorJNI initialized successfully!" << std::endl;
    return true;
}

bool JVectorJNI::BuildIndex(const std::vector<float>& vectors, int dimension, int num_vectors) {
    JNIEnv* env = GetJNIEnv();
    if (env == nullptr || builder_object_ == nullptr) {
        std::cerr << "JNI environment or builder object is null" << std::endl;
        return false;
    }
    
    try {
        // Find the withDimension method
        jmethodID withDimensionMethod = env->GetMethodID(builder_class_, "withDimension", "(I)Lcom/datastax/jvector/graph/GraphIndexBuilder;");
        if (withDimensionMethod == nullptr) {
            std::cerr << "Failed to find withDimension method" << std::endl;
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            return false;
        }
        
        // Call the withDimension method
        jobject builderWithDimension = env->CallObjectMethod(builder_object_, withDimensionMethod, dimension);
        if (builderWithDimension == nullptr) {
            std::cerr << "Failed to call withDimension method" << std::endl;
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            return false;
        }
        
        // Find the build method
        jmethodID buildMethod = env->GetMethodID(builder_class_, "build", "()Lcom/datastax/jvector/graph/GraphIndex;");
        if (buildMethod == nullptr) {
            std::cerr << "Failed to find build method" << std::endl;
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            env->DeleteLocalRef(builderWithDimension);
            return false;
        }
        
        // Call the build method
        jobject localGraphIndexObject = env->CallObjectMethod(builderWithDimension, buildMethod);
        if (localGraphIndexObject == nullptr) {
            std::cerr << "Failed to call build method" << std::endl;
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            env->DeleteLocalRef(builderWithDimension);
            return false;
        }
        
        // Clean up the local reference we don't need anymore
        env->DeleteLocalRef(builderWithDimension);
        
        // Store the graph index object as a global reference
        graph_index_object_ = env->NewGlobalRef(localGraphIndexObject);
        env->DeleteLocalRef(localGraphIndexObject);
        
        // Find the addVector method
        jmethodID addVectorMethod = env->GetMethodID(graph_index_class_, "addVector", "([F)I");
        if (addVectorMethod == nullptr) {
            std::cerr << "Failed to find addVector method" << std::endl;
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            return false;
        }
        
        // Add vectors to the index
        for (int i = 0; i < num_vectors; i++) {
            // Create a float array for the vector
            jfloatArray vectorArray = env->NewFloatArray(dimension);
            if (vectorArray == nullptr) {
                std::cerr << "Failed to create float array" << std::endl;
                return false;
            }
            
            // Copy the vector data to the array
            env->SetFloatArrayRegion(vectorArray, 0, dimension, &vectors[i * dimension]);
            
            // Call the addVector method
            jint id = env->CallIntMethod(graph_index_object_, addVectorMethod, vectorArray);
            
            // Check for exceptions
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
                env->DeleteLocalRef(vectorArray);
                return false;
            }
            
            // Clean up the local reference
            env->DeleteLocalRef(vectorArray);
        }
        
        std::cout << "Added " << num_vectors << " vectors to the index" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in BuildIndex: " << e.what() << std::endl;
        return false;
    }
}

std::vector<int> JVectorJNI::Search(const std::vector<float>& query, int k) {
    JNIEnv* env = GetJNIEnv();
    if (env == nullptr || graph_index_object_ == nullptr) {
        std::cerr << "JNI environment or graph index object is null" << std::endl;
        return std::vector<int>();
    }
    
    try {
        // Create a float array for the query vector
        jfloatArray queryArray = env->NewFloatArray(query.size());
        if (queryArray == nullptr) {
            std::cerr << "Failed to create float array for query" << std::endl;
            return std::vector<int>();
        }
        
        // Copy the query data to the array
        env->SetFloatArrayRegion(queryArray, 0, query.size(), query.data());
        
        // Find the search method
        jmethodID searchMethod = env->GetMethodID(graph_index_class_, "search", "([FI)[I");
        if (searchMethod == nullptr) {
            std::cerr << "Failed to find search method" << std::endl;
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            env->DeleteLocalRef(queryArray);
            return std::vector<int>();
        }
        
        // Call the search method
        jintArray resultArray = (jintArray)env->CallObjectMethod(graph_index_object_, searchMethod, queryArray, k);
        
        // Check for exceptions
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            env->DeleteLocalRef(queryArray);
            return std::vector<int>();
        }
        
        // Clean up the query array
        env->DeleteLocalRef(queryArray);
        
        if (resultArray == nullptr) {
            std::cerr << "Failed to get search results" << std::endl;
            return std::vector<int>();
        }
        
        // Get the result data
        jsize resultSize = env->GetArrayLength(resultArray);
        std::vector<int> results(resultSize);
        env->GetIntArrayRegion(resultArray, 0, resultSize, results.data());
        
        // Clean up the result array
        env->DeleteLocalRef(resultArray);
        
        return results;
    } catch (const std::exception& e) {
        std::cerr << "Exception in Search: " << e.what() << std::endl;
        return std::vector<int>();
    }
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
