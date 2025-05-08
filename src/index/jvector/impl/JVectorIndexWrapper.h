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

#pragma once

#include <faiss/cppcontrib/knowhere/IndexWrapper.h>
#include <jni.h>
#include "io_github_jbellis_jvector_graph_GraphIndex.h"
#include "io_github_jbellis_jvector_graph_OnDiskGraphIndex.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace knowhere {

// Custom parameters for JVector search
struct SearchParametersJVector : public faiss::SearchParameters {
    // Default search parameters
    int ef_search = 100;
    int max_connections = 16;
    float alpha = 1.0f;  // Filtering parameter similar to HNSW

    inline ~SearchParametersJVector() {}
};

// Wrapper around JVector's OnDiskGraphIndex
struct JVectorIndexWrapper : public faiss::cppcontrib::knowhere::IndexWrapper {
    explicit JVectorIndexWrapper(OnDiskGraphIndex* underlying_index);
    ~JVectorIndexWrapper() override;

    /// Search entry point
    void search(faiss::idx_t n,           // number of query vectors
                const float* x,            // query vectors
                faiss::idx_t k,           // number of nearest neighbors
                float* distances,          // output distances
                faiss::idx_t* labels,      // output indices
                const faiss::SearchParameters* params) const override;

    /// Range search entry point
    void range_search(faiss::idx_t n,     // number of query vectors
                     const float* x,       // query vectors
                     float radius,         // search radius
                     faiss::RangeSearchResult* result,
                     const faiss::SearchParameters* params) const override;

private:
    jobject index_;
    JNIEnv* env_;
    jclass index_class_;
};

}  // namespace knowhere
