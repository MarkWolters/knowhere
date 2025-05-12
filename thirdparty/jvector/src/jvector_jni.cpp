#include "jvector_jni.h"
#include "knowhere/log.h"

#include <string>
#include <vector>

namespace knowhere {
namespace jvector {

namespace {
// JNI class and method signatures
constexpr const char* kArrayVectorFloatClass = "io/github/jbellis/jvector/vector/ArrayVectorFloat";
constexpr const char* kGraphIndexBuilderClass = "io/github/jbellis/jvector/graph/GraphIndexBuilder";
constexpr const char* kGraphIndexClass = "io/github/jbellis/jvector/graph/GraphIndex";
constexpr const char* kSearchResultClass = "io/github/jbellis/jvector/graph/SearchResult";
constexpr const char* kRangeSearchResultClass = "io/github/jbellis/jvector/graph/RangeSearchResult";

// Cache for commonly used class references and method IDs
struct JNICache {
    jclass array_vector_float_class;
    jclass graph_index_builder_class;
    jclass graph_index_class;
    jclass search_result_class;
    jclass range_search_result_class;

    jmethodID builder_constructor;
    jmethodID builder_add_method;
    jmethodID builder_build_method;
    jmethodID index_search_method;
    jmethodID index_range_search_method;
    jmethodID search_result_constructor;
    jmethodID range_search_result_constructor;

    JNICache() : array_vector_float_class(nullptr),
                 graph_index_builder_class(nullptr),
                 graph_index_class(nullptr),
                 search_result_class(nullptr),
                 range_search_result_class(nullptr),
                 builder_constructor(nullptr),
                 builder_add_method(nullptr),
                 builder_build_method(nullptr),
                 index_search_method(nullptr),
                 index_range_search_method(nullptr),
                 search_result_constructor(nullptr),
                 range_search_result_constructor(nullptr) {}
};

JNICache g_jni_cache;

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
    // Load ArrayVectorFloat class
    jclass local_vector_class = env->FindClass(kArrayVectorFloatClass);
    if (local_vector_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find ArrayVectorFloat class");
    }
    g_jni_cache.array_vector_float_class = (jclass)env->NewGlobalRef(local_vector_class);
    env->DeleteLocalRef(local_vector_class);

    // Load GraphIndexBuilder class
    jclass local_builder_class = env->FindClass(kGraphIndexBuilderClass);
    if (local_builder_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find GraphIndexBuilder class");
    }
    g_jni_cache.graph_index_builder_class = (jclass)env->NewGlobalRef(local_builder_class);
    env->DeleteLocalRef(local_builder_class);

    // Load GraphIndex class
    jclass local_graph_index_class = env->FindClass(kGraphIndexClass);
    if (local_graph_index_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find GraphIndex class");
    }
    g_jni_cache.graph_index_class = (jclass)env->NewGlobalRef(local_graph_index_class);
    env->DeleteLocalRef(local_graph_index_class);

    // Load SearchResult class
    jclass local_search_result_class = env->FindClass(kSearchResultClass);
    if (local_search_result_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find SearchResult class");
    }
    g_jni_cache.search_result_class = (jclass)env->NewGlobalRef(local_search_result_class);
    env->DeleteLocalRef(local_search_result_class);

    // Load RangeSearchResult class
    jclass local_range_result_class = env->FindClass(kRangeSearchResultClass);
    if (local_range_result_class == nullptr) {
        return Status(Status::Code::Invalid, "Failed to find RangeSearchResult class");
    }
    g_jni_cache.range_search_result_class = (jclass)env->NewGlobalRef(local_range_result_class);
    env->DeleteLocalRef(local_range_result_class);

    // Get GraphIndexBuilder method IDs
    g_jni_cache.builder_constructor = env->GetMethodID(g_jni_cache.graph_index_builder_class, "<init>", 
        "(Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;I)V");
    if (g_jni_cache.builder_constructor == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get builder constructor");
    }

    g_jni_cache.builder_add_method = env->GetMethodID(g_jni_cache.graph_index_builder_class, "add", 
        "([F)V");
    if (g_jni_cache.builder_add_method == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get add vector method");
    }

    g_jni_cache.builder_build_method = env->GetMethodID(g_jni_cache.graph_index_builder_class, "build", 
        "()Lio/github/jbellis/jvector/graph/GraphIndex;");
    if (g_jni_cache.builder_build_method == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get build method");
    }

    // Get GraphIndex method IDs
    g_jni_cache.index_search_method = env->GetMethodID(g_jni_cache.graph_index_class, "search",
        "(Lio/github/jbellis/jvector/vector/VectorFloat;IF)Lio/github/jbellis/jvector/graph/SearchResult;");
    if (g_jni_cache.index_search_method == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get search method");
    }

    g_jni_cache.index_range_search_method = env->GetMethodID(g_jni_cache.graph_index_class, "rangeSearch",
        "(Lio/github/jbellis/jvector/vector/VectorFloat;FIF)Lio/github/jbellis/jvector/graph/RangeSearchResult;");
    if (g_jni_cache.index_range_search_method == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get range search method");
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

    // Get SearchResult method IDs
    g_jni_cache.search_result_constructor = env->GetMethodID(g_jni_cache.search_result_class, "<init>", "([F[J)V");
    if (g_jni_cache.search_result_constructor == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get SearchResult constructor");
    }

    // Get RangeSearchResult method IDs
    g_jni_cache.range_search_result_constructor = env->GetMethodID(g_jni_cache.range_search_result_class, "<init>", "([[F[[J)V");
    if (g_jni_cache.range_search_result_constructor == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get RangeSearchResult constructor");
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

Status RangeSearchVectors(JNIEnv* env, jobject index_obj, const float* query_vectors, int64_t num_queries,
                       float radius, std::vector<std::vector<float>>& distances,
                       std::vector<std::vector<int64_t>>& labels, int ef_search, const BitsetView& bitset) {
    if (query_vectors == nullptr || num_queries <= 0) {
        return Status(Status::Code::Invalid, "Invalid query vectors");
    }

    if (radius <= 0) {
        return Status(Status::Code::Invalid, "Invalid radius");
    }

    if (ef_search <= 0) {
        return Status(Status::Code::Invalid, "Invalid ef_search");
    }

    // Create query vector array
    jfloatArray query_array = env->NewFloatArray(num_queries);
    if (query_array == nullptr) {
        return Status(Status::Code::Invalid, "Failed to create query vector array");
    }
    env->SetFloatArrayRegion(query_array, 0, num_queries, query_vectors);

    // Create ArrayVectorFloat object for query
    jobject query_vector = env->NewObject(g_jni_cache.array_vector_float_class,
                                        g_jni_cache.vector_constructor,
                                        query_array);
    if (query_vector == nullptr) {
        env->DeleteLocalRef(query_array);
        return Status(Status::Code::Invalid, "Failed to create query vector object");
    }

    // Call rangeSearch method
    jobject range_result = env->CallObjectMethod(index_obj,
                                               g_jni_cache.index_range_search_method,
                                               query_vector,
                                               radius,
                                               ef_search,
                                               bitset.data());
    if (range_result == nullptr) {
        env->DeleteLocalRef(query_array);
        env->DeleteLocalRef(query_vector);
        return Status(Status::Code::Invalid, "Failed to execute range search");
    }

    // Get distances array from RangeSearchResult
    jclass range_result_class = env->GetObjectClass(range_result);
    jfieldID distances_field = env->GetFieldID(range_result_class, "distances", "[[F");
    jobjectArray distances_array = (jobjectArray)env->GetObjectField(range_result, distances_field);

    // Get labels array from RangeSearchResult
    jfieldID labels_field = env->GetFieldID(range_result_class, "labels", "[[J");
    jobjectArray labels_array = (jobjectArray)env->GetObjectField(range_result, labels_field);

    if (distances_array == nullptr || labels_array == nullptr) {
        env->DeleteLocalRef(query_array);
        env->DeleteLocalRef(query_vector);
        env->DeleteLocalRef(range_result);
        return Status(Status::Code::Invalid, "Failed to get results from range search");
    }

    // Convert results to C++ vectors
    jsize num_results = env->GetArrayLength(distances_array);
    distances.resize(num_results);
    labels.resize(num_results);

    for (jsize i = 0; i < num_results; i++) {
        jfloatArray dist_row = (jfloatArray)env->GetObjectArrayElement(distances_array, i);
        jlongArray label_row = (jlongArray)env->GetObjectArrayElement(labels_array, i);

        if (dist_row == nullptr || label_row == nullptr) {
            env->DeleteLocalRef(query_array);
            env->DeleteLocalRef(query_vector);
            env->DeleteLocalRef(range_result);
            env->DeleteLocalRef(distances_array);
            env->DeleteLocalRef(labels_array);
            return Status(Status::Code::Invalid, "Failed to get result row");
        }

        jsize row_size = env->GetArrayLength(dist_row);
        distances[i].resize(row_size);
        labels[i].resize(row_size);

        env->GetFloatArrayRegion(dist_row, 0, row_size, distances[i].data());
        env->GetLongArrayRegion(label_row, 0, row_size, reinterpret_cast<jlong*>(labels[i].data()));

        env->DeleteLocalRef(dist_row);
        env->DeleteLocalRef(label_row);
    }

    // Cleanup JNI references
    env->DeleteLocalRef(query_array);
    env->DeleteLocalRef(query_vector);
    env->DeleteLocalRef(range_result);
    env->DeleteLocalRef(distances_array);
    env->DeleteLocalRef(labels_array);

    return Status::OK();
}

} // namespace jvector
} // namespace knowhere
