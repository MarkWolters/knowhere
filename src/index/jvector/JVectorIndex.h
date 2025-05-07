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

#include "knowhere/index/vector_index.h"
#include "knowhere/index/jvector/impl/JVectorIndexWrapper.h"

namespace knowhere {

class JVectorIndex : public VectorIndex {
public:
    JVectorIndex(const size_t dimension, const IndexType index_type);
    ~JVectorIndex() override;

    Status Build(const DataSet& dataset, const Config& cfg) override;
    Status Train(const DataSet& dataset, const Config& cfg) override;
    Status Add(const DataSet& dataset, const Config& cfg) override;
    Status Search(const DataSet& dataset, const Config& cfg, DataSet& results) override;
    Status RangeSearch(const DataSet& dataset, const Config& cfg, DataSet& results) override;
    Status GetVectorByIds(const DataSet& dataset, DataSet& results) override;
    
    // Index info and stats
    IndexType GetType() const override;
    std::string GetType() const override;
    std::string GetTypeName() const override;
    bool IsBuilt() const override;
    Status Serialize(BinarySet& binset) const override;
    Status Deserialize(const BinarySet& binset, const Config& config) override;
    
private:
    std::unique_ptr<JVectorIndexWrapper> index_;
    size_t dimension_;
    bool is_built_;
};

} // namespace knowhere
