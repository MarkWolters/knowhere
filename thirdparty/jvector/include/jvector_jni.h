#pragma once

#include <jni.h>
#include "knowhere/status.h"

namespace knowhere {
namespace jvector {

// JNI utility functions
Status InitializeJVM(JavaVM** jvm);
Status LoadJVectorClasses(JNIEnv* env);

// JNI wrapper functions for JVector operations
Status CreateGraphIndex(JNIEnv* env, jobject* index_obj, const std::string& metric_type, int dim);
Status AddVectors(JNIEnv* env, jobject index_obj, const float* vectors, int64_t num_vectors);
Status SearchVectors(JNIEnv* env, jobject index_obj, const float* query_vectors, int64_t num_queries,
                    int64_t k, float* distances, int64_t* labels);

// JNI exception handling
Status CheckJavaException(JNIEnv* env);

} // namespace jvector
} // namespace knowhere
