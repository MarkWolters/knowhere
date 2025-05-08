// Copyright (C) 2019-2024 Zilliz. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License.

#include "knowhere/index/jvector/impl/JVectorIndexWrapper.h"
#include <faiss/impl/AuxIndexStructures.h>
#include <jni.h>

namespace knowhere {

JVectorIndexWrapper::JVectorIndexWrapper(OnDiskGraphIndex* underlying_index) {
    // Get JNI environment
    JavaVM* jvm;
    JavaVMInitArgs vm_args;
    JavaVMOption options;
    options.optionString = (char*)"-Djava.class.path=lib/jvector-1.0-SNAPSHOT.jar";
    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = 1;
    vm_args.options = &options;
    vm_args.ignoreUnrecognized = false;
    
    JNI_CreateJavaVM(&jvm, (void**)&env_, NULL);
    
    // Find the OnDiskGraphIndex class
    index_class_ = env_->FindClass("io/github/jbellis/jvector/graph/OnDiskGraphIndex");
    if (index_class_ == NULL) {
        // Handle error
        return;
    }
    
    // Get the constructor
    jmethodID constructor = env_->GetMethodID(index_class_, "<init>", "()V");
    if (constructor == NULL) {
        // Handle error
        return;
    }
    
    // Create a new instance
    index_ = env_->NewObject(index_class_, constructor);
}

JVectorIndexWrapper::~JVectorIndexWrapper() {
    if (env_ != NULL && index_ != NULL) {
        env_->DeleteGlobalRef(index_);
    }
}

void JVectorIndexWrapper::search(
    faiss::idx_t n,
    const float* x,
    faiss::idx_t k,
    float* distances,
    faiss::idx_t* labels,
    const faiss::SearchParameters* params) const {
    
    // Cast parameters to JVector-specific parameters
    const auto* jvector_params = dynamic_cast<const SearchParametersJVector*>(params);
    if (jvector_params == nullptr) {
        // Use default parameters if none provided
        jvector_params = new SearchParametersJVector();
    }

    // Find the search method
    jmethodID search_method = env_->GetMethodID(index_class_, "search",
        "([FII)[[I");
    if (search_method == NULL) {
        // Handle error
        return;
    }

    // For each query vector
    for (faiss::idx_t i = 0; i < n; i++) {
        // Convert query vector to Java float array
        jfloatArray query = env_->NewFloatArray(index_->getDimension());
        env_->SetFloatArrayRegion(query, 0, index_->getDimension(), x + i * index_->getDimension());

        // Call search method
        jobjectArray results = (jobjectArray)env_->CallObjectMethod(index_,
            search_method, query, k,
            jvector_params->ef_search);

        // Process results
        jintArray result = (jintArray)env_->GetObjectArrayElement(results, 0);
        jint* elements = env_->GetIntArrayElements(result, 0);

        // Copy distances and labels
        for (int j = 0; j < k; j++) {
            labels[i * k + j] = elements[j];
            distances[i * k + j] = elements[j + k];
        }

        // Cleanup
        env_->ReleaseIntArrayElements(result, elements, 0);
        env_->DeleteLocalRef(result);
        env_->DeleteLocalRef(results);
        env_->DeleteLocalRef(query);
    }
}

void JVectorIndexWrapper::range_search(
    faiss::idx_t n,
    const float* x,
    float radius,
    faiss::RangeSearchResult* result,
    const faiss::SearchParameters* params) const {
    
    // Initialize range search result
    result->clear();
    result->lims[0] = 0;
    
    const auto* jvector_params = dynamic_cast<const SearchParametersJVector*>(params);
    if (jvector_params == nullptr) {
        jvector_params = new SearchParametersJVector();
    }

    // Find the rangeSearch method
    jmethodID range_search_method = env_->GetMethodID(index_class_, "rangeSearch",
        "([FF)[[I");
    if (range_search_method == NULL) {
        // Handle error
        return;
    }

    // For each query vector
    for (faiss::idx_t i = 0; i < n; i++) {
        // Convert query vector to Java float array
        jfloatArray query = env_->NewFloatArray(index_->getDimension());
        env_->SetFloatArrayRegion(query, 0, index_->getDimension(), x + i * index_->getDimension());

        // Call range search method
        jobjectArray results = (jobjectArray)env_->CallObjectMethod(index_,
            range_search_method, query, radius);

        // Process results
        jintArray result_array = (jintArray)env_->GetObjectArrayElement(results, 0);
        jint* elements = env_->GetIntArrayElements(result_array, 0);
        jsize nres = env_->GetArrayLength(result_array) / 2; // Divide by 2 because array contains both ids and distances

        // Add results
        result->add_results(nres, elements, elements + nres);
        result->lims[i + 1] = result->lims[i] + nres;

        // Cleanup
        env_->ReleaseIntArrayElements(result_array, elements, 0);
        env_->DeleteLocalRef(result_array);
        env_->DeleteLocalRef(results);
        env_->DeleteLocalRef(query);
    }
}

} // namespace knowhere
