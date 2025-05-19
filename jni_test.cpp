#include <jni.h>
#include <iostream>
#include <string>

int main() {
    std::cout << "JNI Test - Starting" << std::endl;
    
    // Print JNI version
    std::cout << "JNI Version: " << JNI_VERSION_10 << std::endl;
    
    // Set up JVM initialization arguments
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
    
    // Find the System class
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
    
    std::cout << "Found System class!" << std::endl;
    
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
    
    std::cout << "Found getProperty method!" << std::endl;
    
    // Create a Java string for the property name
    jstring propertyName = env->NewStringUTF("java.version");
    
    // Call the getProperty method
    jobject result = env->CallStaticObjectMethod(systemClass, getPropertyMethod, propertyName);
    
    // Convert the result to a C++ string
    const char* str = env->GetStringUTFChars((jstring)result, nullptr);
    std::string javaVersion(str);
    env->ReleaseStringUTFChars((jstring)result, str);
    
    std::cout << "Java version: " << javaVersion << std::endl;
    
    // Clean up
    env->DeleteLocalRef(propertyName);
    env->DeleteLocalRef(result);
    
    // Destroy the JVM
    jvm->DestroyJavaVM();
    
    std::cout << "JNI Test - Completed Successfully" << std::endl;
    return 0;
}
