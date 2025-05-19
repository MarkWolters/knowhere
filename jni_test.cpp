#include <jni.h>
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "JVector JNI Test - Starting" << std::endl;
    
    // Set up JVM initialization arguments
    JavaVMInitArgs vm_args;
    JavaVMOption options[4];
    
    // Set class path to include JVector JAR and dependencies
    std::string classpath = "-Djava.class.path=java_test:thirdparty/jvector/lib/jvector-4.0.0-beta.4.jar";
    
    // Add all dependency JARs to the classpath
    std::vector<std::string> deps = {
        "thirdparty/jvector/deps/slf4j-api-2.0.9.jar",
        "thirdparty/jvector/deps/slf4j-simple-2.0.9.jar",
        "thirdparty/jvector/deps/agrona-1.19.0.jar",
        "thirdparty/jvector/deps/commons-math3-3.6.1.jar"
    };
    
    for (const auto& dep : deps) {
        classpath += ":" + dep;
    }
    
    options[0].optionString = const_cast<char*>(classpath.c_str());
    
    // Disable JVM verification - useful for debugging
    options[1].optionString = const_cast<char*>("-Xverify:none");
    
    // Set initial heap size
    options[2].optionString = const_cast<char*>("-Xms128m");
    
    // Enable assertions
    options[3].optionString = const_cast<char*>("-ea");
    
    vm_args.version = JNI_VERSION_10;
    vm_args.nOptions = 4;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    
    // Variables for JNI
    JavaVM* jvm;
    JNIEnv* env;
    
    std::cout << "Creating JVM..." << std::endl;
    
    // Create the JVM
    jint res = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
    if (res != JNI_OK) {
        std::cerr << "Failed to create JVM: " << res << std::endl;
        return 1;
    }
    
    std::cout << "JVM created successfully!" << std::endl;
    
    // Test basic Java functionality
    jclass systemClass = env->FindClass("java/lang/System");
    if (systemClass == nullptr) {
        std::cerr << "Failed to find System class" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    // Get the getProperty method
    jmethodID getPropertyMethod = env->GetStaticMethodID(systemClass, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
    if (getPropertyMethod == nullptr) {
        std::cerr << "Failed to find getProperty method" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    // Create a Java string for the property name
    jstring propertyName = env->NewStringUTF("java.version");
    
    // Call the getProperty method
    jobject result = env->CallStaticObjectMethod(systemClass, getPropertyMethod, propertyName);
    
    // Convert the result to a C++ string
    const char* str = env->GetStringUTFChars((jstring)result, nullptr);
    std::string javaVersion(str);
    env->ReleaseStringUTFChars((jstring)result, str);
    
    std::cout << "Java version: " << javaVersion << std::endl;
    
    // Clean up basic test resources
    env->DeleteLocalRef(propertyName);
    env->DeleteLocalRef(result);
    
    // Now test JVector-specific functionality
    std::cout << "\nTesting JVector functionality..." << std::endl;
    
    // Find the VectorizationProvider class
    jclass vecProviderClass = env->FindClass("io/github/jbellis/jvector/vector/VectorizationProvider");
    if (vecProviderClass == nullptr) {
        std::cerr << "Failed to find VectorizationProvider class" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    std::cout << "Found VectorizationProvider class!" << std::endl;
    
    // Get the getInstance method
    jmethodID getInstanceMethod = env->GetStaticMethodID(vecProviderClass, "getInstance", "()Lio/github/jbellis/jvector/vector/VectorizationProvider;");
    if (getInstanceMethod == nullptr) {
        std::cerr << "Failed to find getInstance method" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    // Call getInstance to get the VectorizationProvider instance
    jobject vecProviderInstance = env->CallStaticObjectMethod(vecProviderClass, getInstanceMethod);
    if (vecProviderInstance == nullptr) {
        std::cerr << "Failed to get VectorizationProvider instance" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    std::cout << "Got VectorizationProvider instance!" << std::endl;
    
    // Get the getVectorTypeSupport method
    jmethodID getVectorTypeSupportMethod = env->GetMethodID(vecProviderClass, "getVectorTypeSupport", "()Lio/github/jbellis/jvector/vector/types/VectorTypeSupport;");
    if (getVectorTypeSupportMethod == nullptr) {
        std::cerr << "Failed to find getVectorTypeSupport method" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    // Call getVectorTypeSupport to get the VectorTypeSupport instance
    jobject vtsInstance = env->CallObjectMethod(vecProviderInstance, getVectorTypeSupportMethod);
    if (vtsInstance == nullptr) {
        std::cerr << "Failed to get VectorTypeSupport instance" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    std::cout << "Got VectorTypeSupport instance!" << std::endl;
    
    // Find the VectorSimilarityFunction class
    jclass vecSimClass = env->FindClass("io/github/jbellis/jvector/vector/VectorSimilarityFunction");
    if (vecSimClass == nullptr) {
        std::cerr << "Failed to find VectorSimilarityFunction class" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    std::cout << "Found VectorSimilarityFunction class!" << std::endl;
    
    // Get the COSINE field
    jfieldID cosineField = env->GetStaticFieldID(vecSimClass, "COSINE", "Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;");
    if (cosineField == nullptr) {
        std::cerr << "Failed to find COSINE field" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    // Get the COSINE instance
    jobject cosineInstance = env->GetStaticObjectField(vecSimClass, cosineField);
    if (cosineInstance == nullptr) {
        std::cerr << "Failed to get COSINE instance" << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        return 1;
    }
    
    std::cout << "Got COSINE similarity function instance!" << std::endl;
    
    // Clean up JVector test resources
    env->DeleteLocalRef(vecProviderClass);
    env->DeleteLocalRef(vecProviderInstance);
    env->DeleteLocalRef(vtsInstance);
    env->DeleteLocalRef(vecSimClass);
    env->DeleteLocalRef(cosineInstance);
    
    // Destroy the JVM
    jvm->DestroyJavaVM();
    
    std::cout << "JVector JNI Test - Completed Successfully" << std::endl;
    return 0;
}
