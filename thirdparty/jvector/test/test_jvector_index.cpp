#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "knowhere/comp/index_param.h"
#include "knowhere/comp/knowhere_config.h"
#include "knowhere/dataset.h"
#include "../include/jvector_index.h"

using namespace knowhere;
using Catch::Approx;

TEST_CASE("JVectorIndex Build", "[jvector]") {
    // Create test dataset
    const int64_t nb = 1000;     // number of vectors
    const int64_t dim = 128;     // dimension
    const int64_t seed = 42;     // random seed

    // Generate random vectors
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    std::vector<float> vectors(nb * dim);
    for (size_t i = 0; i < vectors.size(); i++) {
        vectors[i] = dist(rng);
    }

    // Create dataset
    auto dataset = GenDataSet(nb, dim, vectors.data());
    REQUIRE(dataset != nullptr);

    SECTION("Build with L2 distance") {
        // Create index
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        // Configure index
        Json json;
        json["dim"] = dim;
        json["metric_type"] = "L2";
        json["M"] = 16;              // max number of connections
        json["efConstruction"] = 64;  // size of dynamic candidate list

        // Build index
        auto status = index->Build(dataset, json);
        REQUIRE(status.ok());

        // Verify index properties
        REQUIRE(index->GetDim() == dim);
        REQUIRE(index->GetSize() == nb);
        REQUIRE(index->GetMetricType() == "L2");
    }

    SECTION("Build with IP (Inner Product) distance") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        Json json;
        json["dim"] = dim;
        json["metric_type"] = "IP";
        json["M"] = 16;
        json["efConstruction"] = 64;

        auto status = index->Build(dataset, json);
        REQUIRE(status.ok());

        REQUIRE(index->GetDim() == dim);
        REQUIRE(index->GetSize() == nb);
        REQUIRE(index->GetMetricType() == "IP");
    }

    SECTION("Build with Cosine distance") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        Json json;
        json["dim"] = dim;
        json["metric_type"] = "COSINE";
        json["M"] = 16;
        json["efConstruction"] = 64;

        auto status = index->Build(dataset, json);
        REQUIRE(status.ok());

        REQUIRE(index->GetDim() == dim);
        REQUIRE(index->GetSize() == nb);
        REQUIRE(index->GetMetricType() == "COSINE");
    }

    SECTION("Build with invalid dimension") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        Json json;
        json["dim"] = dim + 1;  // Wrong dimension
        json["metric_type"] = "L2";
        json["M"] = 16;
        json["efConstruction"] = 64;

        auto status = index->Build(dataset, json);
        REQUIRE_FALSE(status.ok());
    }

    SECTION("Build with invalid metric type") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        Json json;
        json["dim"] = dim;
        json["metric_type"] = "INVALID";  // Invalid metric type
        json["M"] = 16;
        json["efConstruction"] = 64;

        auto status = index->Build(dataset, json);
        REQUIRE_FALSE(status.ok());
    }
}
