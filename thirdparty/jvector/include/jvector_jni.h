#pragma once

#include <jni.h>
#include "knowhere/status.h"

namespace knowhere {
namespace jvector {

// Thread-local JNI environment
struct ThreadLocalJNIEnv {
    JNIEnv* env;
    bool attached;

    ThreadLocalJNIEnv() : env(nullptr), attached(false) {}
};

extern thread_local ThreadLocalJNIEnv g_thread_local_env;

// JNI utility functions
Status InitializeJVM(JavaVM** jvm);
Status GetThreadLocalJNIEnv(JavaVM* jvm, JNIEnv** env);
Status DetachThreadLocalJNIEnv(JavaVM* jvm);
Status EnsureThreadLocalJNIEnv(JavaVM* jvm, JNIEnv** env);
Status LoadJVectorClasses(JNIEnv* env);

// JNI wrapper functions for JVector operations
Status CreateGraphIndex(JNIEnv* env, jobject* index_obj, const std::string& metric_type, int dim, const Json& config);
Status AddVectors(JNIEnv* env, jobject builder_obj, const float* vectors, int64_t num_vectors, int dim);
Status SearchVectors(JNIEnv* env, jobject index_obj, const float* query_vectors, int64_t num_queries,
                    int64_t k, float* distances, int64_t* labels, int ef_search, const BitsetView& bitset);

Status RangeSearchVectors(JNIEnv* env, jobject index_obj, const float* query_vectors, int64_t num_queries,
                       float radius, std::vector<std::vector<float>>& distances,
                       std::vector<std::vector<int64_t>>& labels, int ef_search, const BitsetView& bitset);

// JNI exception handling
Status CheckJavaException(JNIEnv* env);

} // namespace jvector
} // namespace knowhere
