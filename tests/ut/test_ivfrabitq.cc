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

#include <filesystem>
#include <future>

#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/generators/catch_generators.hpp"
#include "faiss/invlists/InvertedLists.h"
#include "knowhere/comp/index_param.h"
#include "knowhere/comp/knowhere_check.h"
#include "knowhere/index/index_factory.h"
#include "knowhere/utils.h"
#include "utils.h"
#include "knowhere/comp/index_param.h"
#include "knowhere/index/index_table.h"

TEST_CASE("Test IVFRaBitQ Basic", "[IVFRaBitQ]") {
    using Catch::Approx;

    auto metric = GENERATE(as<std::string>{}, knowhere::metric::L2, knowhere::metric::COSINE);
    auto version = GenTestVersionList();

    int64_t nb = 10000, nq = 1000;
    int64_t dim = 128;
    int64_t seed = 42;
    int64_t top_k = 100;

    auto base_gen = [=]() {
        knowhere::Json json;
        json[knowhere::meta::DIM] = dim;
        json[knowhere::meta::METRIC_TYPE] = metric;
        json[knowhere::meta::TOPK] = top_k;
        json[knowhere::meta::RADIUS] = knowhere::IsMetricType(metric, knowhere::metric::L2) ? 10.0 : 0.99;
        json[knowhere::meta::RANGE_FILTER] = knowhere::IsMetricType(metric, knowhere::metric::L2) ? 0.0 : 1.01;
        return json;
    };

    auto ivf_rabitq_gen = [base_gen]() {
        knowhere::Json json = base_gen();
        json[knowhere::indexparam::NLIST] = 128;
        json[knowhere::indexparam::NPROBE] = 16;
        json["rbq_bits_query"] = 8;  // 8-bit quantization for query
        json["refine"] = true;       // Enable refinement
        json["refine_type"] = "fp16"; // Use FP16 for refinement
        return json;
    };

    SECTION("Test Basic CRUD Operations") {
        auto train_ds = GenDataSet(nb, dim, seed);
        auto query_ds = GenDataSet(nq, dim, seed + 1);
        auto json = ivf_rabitq_gen();

        auto idx = knowhere::IndexFactory::Instance()
                       .Create<knowhere::fp32>(knowhere::IndexEnum::INDEX_FAISS_IVFRABITQ, version)
                       .value();
        REQUIRE(idx.Type() == knowhere::IndexEnum::INDEX_FAISS_IVFRABITQ);

        // Build index (includes training)
        auto status = idx.Build(train_ds, json);
        REQUIRE(status == knowhere::Status::success);

        // Add vectors
        status = idx.Add(train_ds, json);
        REQUIRE(status == knowhere::Status::success);

        // Search
        {
            auto results = idx.Search(query_ds, json, nullptr);
            REQUIRE(results.has_value());
            REQUIRE(results.value()->GetRows() == nq);
            REQUIRE(results.value()->GetDim() == top_k);
            auto distances = results.value()->GetDistance();
            REQUIRE(distances != nullptr);
            auto ids = results.value()->GetIds();
            REQUIRE(ids != nullptr);
        }

        // Range Search
        {
            auto results = idx.RangeSearch(query_ds, json, nullptr);
            REQUIRE(results.has_value());
            REQUIRE(results.value()->GetRows() == nq);
            auto distances = results.value()->GetDistance();
            REQUIRE(distances != nullptr);
            auto ids = results.value()->GetIds();
            REQUIRE(ids != nullptr);
            auto lims = results.value()->GetLims();
            REQUIRE(lims != nullptr);
        }
    }
}
