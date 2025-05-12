#include "jvector_jni.h"
#include "knowhere/log.h"

#include <string>
#include <vector>

namespace knowhere {
namespace jvector {

namespace {
// JNI class and method signatures
constexpr char kGraphIndexClass[] = "io/github/jbellis/jvector/graph/GraphIndex";
constexpr char kGraphIndexBuilderClass[] = "io/github/jbellis/jvector/graph/GraphIndexBuilder";
constexpr char kVectorSimFuncClass[] = "io/github/jbellis/jvector/vector/VectorSimilarityFunction";
constexpr char kArrayVectorFloatClass[] = "io/github/jbellis/jvector/vector/ArrayVectorFloat";

// Cache for commonly used class references and method IDs
struct {
    jclass graph_index_class;
    jclass graph_index_builder_class;
    jclass vector_sim_func_class;
    jclass array_vector_float_class;
    
    jmethodID builder_constructor;
    jmethodID builder_add_vector;
    jmethodID builder_build;
    jmethodID search_method;
} g_jni_cache;

} // namespace

Status InitializeJVM(JavaVM** jvm) {
    if (*jvm != nullptr) {
        return Status::OK();
    }

    JavaVMInitArgs vm_args;
    JavaVMOption options[1];
    
    // TODO: Set proper classpath to include JVector JAR
    options[0].optionString = const_cast<char*>("-Djava.class.path=/path/to/jvector.jar");
    
    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = 1;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_FALSE;

    jint res = JNI_CreateJavaVM(jvm, (void**)&g_jni_env, &vm_args);
    if (res != JNI_OK) {
        return Status(Status::Code::Invalid, "Failed to create JVM");
    }

    return LoadJVectorClasses(g_jni_env);
}

Status LoadJVectorClasses(JNIEnv* env) {
    // Load GraphIndex class
    jclass local_graph_index_class = env->FindClass(kGraphIndexClass);
    if (local_graph_index_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find GraphIndex class");
    }
    g_jni_cache.graph_index_class = (jclass)env->NewGlobalRef(local_graph_index_class);
    env->DeleteLocalRef(local_graph_index_class);

    // Load GraphIndexBuilder class
    jclass local_builder_class = env->FindClass(kGraphIndexBuilderClass);
    if (local_builder_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find GraphIndexBuilder class");
    }
    g_jni_cache.graph_index_builder_class = (jclass)env->NewGlobalRef(local_builder_class);
    env->DeleteLocalRef(local_builder_class);

    // Load VectorSimilarityFunction class
    jclass local_sim_func_class = env->FindClass(kVectorSimFuncClass);
    if (local_sim_func_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find VectorSimilarityFunction class");
    }
    g_jni_cache.vector_sim_func_class = (jclass)env->NewGlobalRef(local_sim_func_class);
    env->DeleteLocalRef(local_sim_func_class);

    // Load ArrayVectorFloat class
    jclass local_vector_class = env->FindClass(kArrayVectorFloatClass);
    if (local_vector_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find ArrayVectorFloat class");
    }
    g_jni_cache.array_vector_float_class = (jclass)env->NewGlobalRef(local_vector_class);
    env->DeleteLocalRef(local_vector_class);

    // Get method IDs
    g_jni_cache.builder_constructor = env->GetMethodID(g_jni_cache.graph_index_builder_class, "<init>", "(Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;I)V");
    if (g_jni_cache.builder_constructor == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get builder constructor");
    }

    // TODO: Get other required method IDs

    return Status::OK();
}

Status CreateGraphIndex(JNIEnv* env, jobject* index_obj, const std::string& metric_type, int dim) {
    // Create VectorSimilarityFunction enum value based on metric type
    jobject sim_func = nullptr;
    jfieldID field_id = nullptr;

    if (metric_type == "L2") {
        field_id = env->GetStaticFieldID(g_jni_cache.vector_sim_func_class, "EUCLIDEAN", "Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;");
    } else if (metric_type == "IP") {
        field_id = env->GetStaticFieldID(g_jni_cache.vector_sim_func_class, "DOT_PRODUCT", "Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;");
    } else if (metric_type == "COSINE") {
        field_id = env->GetStaticFieldID(g_jni_cache.vector_sim_func_class, "COSINE", "Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;");
    } else {
        return Status(Status::Code::Invalid, "Unsupported metric type: " + metric_type);
    }

    if (field_id == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get similarity function field");
    }

    sim_func = env->GetStaticObjectField(g_jni_cache.vector_sim_func_class, field_id);
    if (sim_func == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get similarity function instance");
    }

    // Create GraphIndexBuilder
    jobject builder = env->NewObject(g_jni_cache.graph_index_builder_class, g_jni_cache.builder_constructor, sim_func, dim);
    if (builder == nullptr) {
        return Status(Status::Code::Invalid, "Failed to create GraphIndexBuilder");
    }

    // TODO: Configure builder parameters (M, ef_construction, etc.)

    // Create empty index
    // TODO: Call build() on builder to get initial empty index
    *index_obj = nullptr; // Replace with actual index object

    return Status::OK();
}

Status AddVectors(JNIEnv* env, jobject index_obj, const float* vectors, int64_t num_vectors) {
    // TODO: Implement vector addition
    return Status::OK();
}

Status SearchVectors(JNIEnv* env, jobject index_obj, const float* query_vectors, int64_t num_queries,
                    int64_t k, float* distances, int64_t* labels) {
    // TODO: Implement vector search
    return Status::OK();
}

Status CheckJavaException(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        jthrowable exception = env->ExceptionOccurred();
        env->ExceptionClear();

        jclass exception_class = env->GetObjectClass(exception);
        jmethodID getMessage = env->GetMethodID(exception_class, "getMessage", "()Ljava/lang/String;");
        
        jstring message = (jstring)env->CallObjectMethod(exception, getMessage);
        const char* error_message = env->GetStringUTFChars(message, nullptr);
        
        Status status(Status::Code::Invalid, error_message);
        
        env->ReleaseStringUTFChars(message, error_message);
        env->DeleteLocalRef(message);
        env->DeleteLocalRef(exception_class);
        env->DeleteLocalRef(exception);
        
        return status;
    }
    
    return Status::OK();
}

} // namespace jvector
} // namespace knowhere
