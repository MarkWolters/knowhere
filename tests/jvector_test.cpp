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
    
    // Set class path to current directory
    options[0].optionString = const_cast<char*>("-Djava.class.path=.");
    
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
