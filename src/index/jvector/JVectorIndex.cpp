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

#include "knowhere/index/jvector/JVectorIndex.h"
#include "knowhere/common/config.h"
#include "knowhere/common/log.h"
#include "knowhere/utils/distances.h"

namespace knowhere {

JVectorIndex::JVectorIndex(const size_t dimension, const IndexType index_type)
    : dimension_(dimension), is_built_(false) {}

JVectorIndex::~JVectorIndex() = default;

Status JVectorIndex::Build(const DataSet& dataset, const Config& cfg) {
    try {
        const auto rows = dataset.GetRows();
        const auto* vectors = dataset.GetTensor();
        
        // Get build parameters from config
        const auto ef_construction = cfg.GetWithDefault(knowhere::indexparam::EFCONSTRUCTION, 100);
        const auto max_connections = cfg.GetWithDefault(knowhere::indexparam::MAX_CONNECTIONS, 16);
        
        // Create JVector index
        auto builder = GraphIndexBuilder::create()
            .withDimension(dimension_)
            .withMaxConnections(max_connections)
            .withEfConstruction(ef_construction);
            
        // Add vectors to index
        for (size_t i = 0; i < rows; i++) {
            builder.addVector(vectors + i * dimension_, i);
        }
        
        // Build the index
        auto graph_index = builder.build();
        index_ = std::make_unique<JVectorIndexWrapper>(graph_index);
        is_built_ = true;
        
        return Status::success;
    } catch (const std::exception& e) {
        LOG_KNOWHERE_ERROR_ << "Error building JVector index: " << e.what();
        return Status::invalid_args;
    }
}

Status JVectorIndex::Search(const DataSet& dataset, const Config& cfg, DataSet& results) {
    if (!is_built_) {
        return Status::not_implemented;
    }

    try {
        const auto rows = dataset.GetRows();
        const auto* queries = dataset.GetTensor();
        const auto k = cfg.GetWithDefault(knowhere::meta::TOPK, 10);
        
        // Prepare result buffers
        std::vector<float> distances(rows * k);
        std::vector<idx_t> labels(rows * k);
        
        // Setup search parameters
        SearchParametersJVector params;
        params.ef_search = cfg.GetWithDefault(knowhere::indexparam::EF, 100);
        params.alpha = cfg.GetWithDefault(knowhere::indexparam::ALPHA, 1.0f);
        
        // Perform search
        index_->search(rows, queries, k, distances.data(), labels.data(), &params);
        
        // Set results
        results.SetTensor(distances.data());
        results.SetIds(labels.data());
        results.SetRows(rows);
        results.SetDim(k);
        
        return Status::success;
    } catch (const std::exception& e) {
        LOG_KNOWHERE_ERROR_ << "Error searching JVector index: " << e.what();
        return Status::invalid_args;
    }
}

Status JVectorIndex::RangeSearch(const DataSet& dataset, const Config& cfg, DataSet& results) {
    if (!is_built_) {
        return Status::not_implemented;
    }

    try {
        const auto rows = dataset.GetRows();
        const auto* queries = dataset.GetTensor();
        const auto radius = cfg.GetWithDefault(knowhere::meta::RADIUS, 1.0f);
        
        // Initialize range search result
        faiss::RangeSearchResult res(rows);
        
        // Setup search parameters
        SearchParametersJVector params;
        params.ef_search = cfg.GetWithDefault(knowhere::indexparam::EF, 100);
        params.alpha = cfg.GetWithDefault(knowhere::indexparam::ALPHA, 1.0f);
        
        // Perform range search
        index_->range_search(rows, queries, radius, &res, &params);
        
        // Convert results
        std::vector<float> distances(res.distances, res.distances + res.nres);
        std::vector<idx_t> labels(res.labels, res.labels + res.nres);
        
        // Set results
        results.SetTensor(distances.data());
        results.SetIds(labels.data());
        results.SetRows(rows);
        results.SetDim(res.nres / rows);
        
        return Status::success;
    } catch (const std::exception& e) {
        LOG_KNOWHERE_ERROR_ << "Error in range search: " << e.what();
        return Status::invalid_args;
    }
}

IndexType JVectorIndex::GetType() const {
    return IndexType::kJVector;
}

std::string JVectorIndex::GetTypeName() const {
    return "JVector";
}

bool JVectorIndex::IsBuilt() const {
    return is_built_;
}

Status JVectorIndex::Serialize(BinarySet& binset) const {
    if (!is_built_) {
        return Status::not_implemented;
    }
    
    try {
        // TODO: Implement serialization using JVector's serialization capabilities
        return Status::success;
    } catch (const std::exception& e) {
        LOG_KNOWHERE_ERROR_ << "Error serializing JVector index: " << e.what();
        return Status::invalid_args;
    }
}

Status JVectorIndex::Deserialize(const BinarySet& binset, const Config& config) {
    try {
        // TODO: Implement deserialization using JVector's deserialization capabilities
        is_built_ = true;
        return Status::success;
    } catch (const std::exception& e) {
        LOG_KNOWHERE_ERROR_ << "Error deserializing JVector index: " << e.what();
        return Status::invalid_args;
    }
}

} // namespace knowhere
