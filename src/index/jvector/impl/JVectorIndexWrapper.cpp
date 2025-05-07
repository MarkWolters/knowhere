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
#include <io.github.jbellis.jvector.graph.GraphSearcher.h>
#include <io.github.jbellis.jvector.graph.SearchResult.h>

namespace knowhere {

JVectorIndexWrapper::JVectorIndexWrapper(OnDiskGraphIndex* underlying_index) 
    : index_(underlying_index) {}

JVectorIndexWrapper::~JVectorIndexWrapper() = default;

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

    // For each query vector
    #pragma omp parallel for
    for (faiss::idx_t i = 0; i < n; i++) {
        const float* query = x + i * index_->getDimension();
        
        // Create vector view for the query
        auto query_vector = VectorFloat::from(query, index_->getDimension());
        
        // Setup search parameters
        auto searcher = std::make_unique<GraphSearcher>(index_.get());
        auto score_provider = SearchScoreProvider::exact(
            query_vector,
            index_->getSimilarityFunction(),
            index_->getView()
        );

        // Perform search
        auto search_result = searcher->search(
            score_provider,
            k,                              // number of results wanted
            k * jvector_params->ef_search,  // number of nodes to visit
            0.0f,                          // epsilon
            jvector_params->alpha,         // alpha for filtering
            nullptr                        // no bits filter
        );

        // Copy results
        const auto& nodes = search_result.getNodes();
        for (size_t j = 0; j < nodes.size(); j++) {
            labels[i * k + j] = nodes[j].node;
            distances[i * k + j] = nodes[j].score;
        }
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

    // For each query vector
    for (faiss::idx_t i = 0; i < n; i++) {
        const float* query = x + i * index_->getDimension();
        
        // Create vector view
        auto query_vector = VectorFloat::from(query, index_->getDimension());
        
        // Setup search
        auto searcher = std::make_unique<GraphSearcher>(index_.get());
        auto score_provider = SearchScoreProvider::exact(
            query_vector,
            index_->getSimilarityFunction(),
            index_->getView()
        );

        // Perform range search
        auto search_result = searcher->search(
            score_provider,
            std::numeric_limits<size_t>::max(),  // unlimited k
            jvector_params->ef_search,
            radius,                              // epsilon = radius
            jvector_params->alpha,
            nullptr
        );

        // Copy results
        const auto& nodes = search_result.getNodes();
        size_t nres = nodes.size();
        
        result->add_results(nres, nodes[0].node, nodes[0].score);
        result->lims[i + 1] = result->lims[i] + nres;
    }
}

} // namespace knowhere
