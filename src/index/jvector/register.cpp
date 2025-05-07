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
#include "knowhere/index/index_factory.h"

namespace {
// Register JVector for float32 vectors, supporting both CPU and GPU
KNOWHERE_SIMPLE_REGISTER_GLOBAL(
    "JVector",                   // Index name
    JVectorIndex,               // Index class
    fp32,                       // Data type (float32)
    knowhere::feature::FLOAT32  // Features
        | knowhere::feature::KNN
        | knowhere::feature::METRIC_TYPE
        | knowhere::feature::CPU
);

// Register index type mapping
std::set<std::pair<std::string, knowhere::VecType>> jvector_index_type{
    {"JVector", knowhere::VecType::FLOAT32}
};

KNOWHERE_SET_STATIC_GLOBAL_INDEX_TABLE(0, jvector_index_type_ref, jvector_index_type);
}
