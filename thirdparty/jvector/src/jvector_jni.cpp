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
constexpr char kSearchResultClass[] = "io/github/jbellis/jvector/graph/SearchResult";

// Cache for commonly used class references and method IDs
struct {
    // Class references
    jclass graph_index_class;
    jclass graph_index_builder_class;
    jclass vector_sim_func_class;
    jclass array_vector_float_class;
    jclass search_result_class;
    
    // GraphIndexBuilder methods
    jmethodID builder_constructor;
    jmethodID builder_add_vector;
    jmethodID builder_build;
    
    // GraphIndex methods
    jmethodID search_method;
    jmethodID get_vector_method;
    jmethodID size_method;
    
    // ArrayVectorFloat methods
    jmethodID vector_constructor;
    jmethodID vector_get_values;
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

    // Load SearchResult class
    jclass local_search_result_class = env->FindClass(kSearchResultClass);
    if (local_search_result_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find SearchResult class");
    }
    g_jni_cache.search_result_class = (jclass)env->NewGlobalRef(local_search_result_class);
    env->DeleteLocalRef(local_search_result_class);

    // Get GraphIndexBuilder method IDs
    g_jni_cache.builder_constructor = env->GetMethodID(g_jni_cache.graph_index_builder_class, "<init>", 
        "(Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;I)V");
    if (g_jni_cache.builder_constructor == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get builder constructor");
    }

    g_jni_cache.builder_add_vector = env->GetMethodID(g_jni_cache.graph_index_builder_class, "add", 
        "([F)V");
    if (g_jni_cache.builder_add_vector == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get add vector method");
    }

    g_jni_cache.builder_build = env->GetMethodID(g_jni_cache.graph_index_builder_class, "build", 
        "()Lio/github/jbellis/jvector/graph/GraphIndex;");
    if (g_jni_cache.builder_build == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get build method");
    }

    // Get GraphIndex method IDs
    g_jni_cache.search_method = env->GetMethodID(g_jni_cache.graph_index_class, "search", 
        "([FI)[I");
    if (g_jni_cache.search_method == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get search method");
    }

    g_jni_cache.get_vector_method = env->GetMethodID(g_jni_cache.graph_index_class, "getVector", 
        "(I)[F");
    if (g_jni_cache.get_vector_method == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get getVector method");
    }

    g_jni_cache.size_method = env->GetMethodID(g_jni_cache.graph_index_class, "size", 
        "()I");
    if (g_jni_cache.size_method == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get size method");
    }

    // Get ArrayVectorFloat method IDs
    g_jni_cache.vector_constructor = env->GetMethodID(g_jni_cache.array_vector_float_class, "<init>", 
        "([F)V");
    if (g_jni_cache.vector_constructor == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get vector constructor");
    }

    g_jni_cache.vector_get_values = env->GetMethodID(g_jni_cache.array_vector_float_class, "getValues", 
        "()[F");
    if (g_jni_cache.vector_get_values == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get vector getValues method");
    }

    return Status::OK();
}

Status CreateGraphIndex(JNIEnv* env, jobject* index_obj, const std::string& metric_type, int dim, const Json& config) {
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
        env->DeleteLocalRef(sim_func);
        return Status(Status::Code::Invalid, "Failed to create GraphIndexBuilder");
    }

    // Configure builder parameters
    jclass builder_class = env->GetObjectClass(builder);
    
    // Set M (max connections per node)
    if (config.contains("M")) {
        jmethodID set_m = env->GetMethodID(builder_class, "setM", "(I)Lio/github/jbellis/jvector/graph/GraphIndexBuilder;");
        if (set_m != nullptr) {
            env->CallObjectMethod(builder, set_m, config["M"].get<int>());
        }
    }

    // Set ef_construction
    if (config.contains("efConstruction")) {
        jmethodID set_ef = env->GetMethodID(builder_class, "setEfConstruction", "(I)Lio/github/jbellis/jvector/graph/GraphIndexBuilder;");
        if (set_ef != nullptr) {
            env->CallObjectMethod(builder, set_ef, config["efConstruction"].get<int>());
        }
    }

    // Build empty index
    jobject index = env->CallObjectMethod(builder, g_jni_cache.builder_build);
    if (index == nullptr) {
        env->DeleteLocalRef(builder);
        env->DeleteLocalRef(sim_func);
        env->DeleteLocalRef(builder_class);
        return Status(Status::Code::Invalid, "Failed to build index");
    }

    // Create global reference for the index
    *index_obj = env->NewGlobalRef(index);

    // Clean up local references
    env->DeleteLocalRef(index);
    env->DeleteLocalRef(builder);
    env->DeleteLocalRef(sim_func);
    env->DeleteLocalRef(builder_class);

    return Status::OK();
}

Status AddVectors(JNIEnv* env, jobject builder_obj, const float* vectors, int64_t num_vectors, int dim) {
    // Create a float array for each vector and add it to the builder
    jfloatArray vector_array = env->NewFloatArray(dim);
    if (vector_array == nullptr) {
        return Status(Status::Code::Invalid, "Failed to create float array");
    }

    for (int64_t i = 0; i < num_vectors; i++) {
        // Copy vector data to Java array
        env->SetFloatArrayRegion(vector_array, 0, dim, vectors + i * dim);

        // Add vector to builder
        env->CallVoidMethod(builder_obj, g_jni_cache.builder_add_vector, vector_array);
        
        // Check for exceptions
        if (env->ExceptionCheck()) {
            env->DeleteLocalRef(vector_array);
            return CheckJavaException(env);
        }
    }


Status SearchVectors(JNIEnv* env, jobject index_obj, const float* query_vectors, int64_t num_queries,
                    int64_t k, float* distances, int64_t* labels, int ef_search, const BitsetView& bitset) {
    if (env == nullptr || index_obj == nullptr || query_vectors == nullptr ||
        distances == nullptr || labels == nullptr || k <= 0 || ef_search <= 0) {
        return Status(Status::Code::Invalid, "Invalid input parameters");
    }

    // Get the GraphIndex class
    jclass graph_index_class = g_jni_cache.graph_index_class;
    if (graph_index_class == nullptr) {
        return Status(Status::Code::Invalid, "GraphIndex class not found");
    }

    // Create float array for query vectors
    int64_t total_floats = num_queries * g_jni_cache.dim;
    jfloatArray query_array = env->NewFloatArray(total_floats);
    if (query_array == nullptr) {
        return Status(Status::Code::Invalid, "Failed to create query array");
    }
    env->SetFloatArrayRegion(query_array, 0, total_floats, query_vectors);

    // Create boolean array for bitset if it's not empty
    jbooleanArray bitset_array = nullptr;
    if (!bitset.empty()) {
        bitset_array = env->NewBooleanArray(bitset.size());
        if (bitset_array == nullptr) {
            env->DeleteLocalRef(query_array);
            return Status(Status::Code::Invalid, "Failed to create bitset array");
        }

        // Convert bitset to boolean array
        std::vector<jboolean> bool_values(bitset.size());
        for (size_t i = 0; i < bitset.size(); i++) {
            bool_values[i] = !bitset.test(i);  // Invert because in JVector true means valid
        }
        env->SetBooleanArrayRegion(bitset_array, 0, bitset.size(), bool_values.data());
    }

    // Call search method with bitset
    jobject results = env->CallObjectMethod(index_obj, g_jni_cache.search_method,
                                          query_array, k, ef_search, bitset_array);
    if (results == nullptr) {
        env->DeleteLocalRef(query_array);
        if (bitset_array != nullptr) env->DeleteLocalRef(bitset_array);
        return Status(Status::Code::Invalid, "Search failed");
    }

    // Get the results arrays
    jfieldID distances_field = env->GetFieldID(g_jni_cache.search_result_class, "distances", "[F");
    jfieldID labels_field = env->GetFieldID(g_jni_cache.search_result_class, "labels", "[J");

    jfloatArray distances_array = (jfloatArray)env->GetObjectField(results, distances_field);
    jlongArray labels_array = (jlongArray)env->GetObjectField(results, labels_field);

    // Copy results back to C++ arrays
    env->GetFloatArrayRegion(distances_array, 0, num_queries * k, distances);
    env->GetLongArrayRegion(labels_array, 0, num_queries * k, (jlong*)labels);

    // Clean up local references
    env->DeleteLocalRef(query_array);
    if (bitset_array != nullptr) env->DeleteLocalRef(bitset_array);
    env->DeleteLocalRef(distances_array);
    env->DeleteLocalRef(labels_array);
    env->DeleteLocalRef(results);

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
