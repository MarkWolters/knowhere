#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "knowhere/comp/index_param.h"
#include "knowhere/comp/knowhere_config.h"
#include "knowhere/dataset.h"
#include "../include/jvector_index.h"

using namespace knowhere;
using Catch::Approx;

namespace {
// Helper function to generate random vectors
std::vector<float> GenerateRandomVectors(int64_t num_vectors, int64_t dim, int64_t seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    std::vector<float> vectors(num_vectors * dim);
    for (size_t i = 0; i < vectors.size(); i++) {
        vectors[i] = dist(rng);
    }
    return vectors;
}

// Helper function to compute L2 distance
float ComputeL2Distance(const float* a, const float* b, int64_t dim) {
    float sum = 0.0f;
    for (int64_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

// Helper function to compute Inner Product distance
float ComputeIPDistance(const float* a, const float* b, int64_t dim) {
    float sum = 0.0f;
    for (int64_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return -sum;  // Negative because we want to maximize IP
}

// Helper function to compute Cosine distance
float ComputeCosineDistance(const float* a, const float* b, int64_t dim) {
    float dot = 0.0f, norma = 0.0f, normb = 0.0f;
    for (int64_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norma += a[i] * a[i];
        normb += b[i] * b[i];
    }
    norma = std::sqrt(norma);
    normb = std::sqrt(normb);
    return 1.0f - dot / (norma * normb);
}
}  // namespace

// Helper functions for distance calculations
float ComputeL2Distance(const float* a, const float* b, int64_t dim) {
    float sum = 0.0f;
    for (int64_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

float ComputeIPDistance(const float* a, const float* b, int64_t dim) {
    float sum = 0.0f;
    for (int64_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return -sum;  // Negative because we want to maximize IP
}

float ComputeCosineDistance(const float* a, const float* b, int64_t dim) {
    float dot = 0.0f, norma = 0.0f, normb = 0.0f;
    for (int64_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norma += a[i] * a[i];
        normb += b[i] * b[i];
    }
    return 1.0f - dot / (sqrt(norma) * sqrt(normb));
}

// Helper function to generate random vectors
std::vector<float> GenerateRandomVectors(int64_t num_vectors, int64_t dim, int seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    std::vector<float> vectors(num_vectors * dim);
    for (int64_t i = 0; i < num_vectors * dim; i++) {
        vectors[i] = dist(rng);
    }
    return vectors;
}

// Helper function to create a DataSet
DataSetPtr GenDataSet(int64_t rows, int64_t dim, const float* data) {
    auto dataset = std::make_shared<DataSet>();
    dataset->SetRows(rows);
    dataset->SetDim(dim);
    dataset->SetTensor(const_cast<float*>(data));
    return dataset;
}

// Helper function to create a BitsetView
std::pair<std::vector<uint8_t>, BitsetView> CreateBitsetView(size_t num_bits, const std::vector<int64_t>& filtered_indices) {
    std::vector<uint8_t> bitset_data((num_bits + 7) / 8, 0);  // Initialize all bits to 0
    
    // Set bits for filtered indices to 1
    for (auto idx : filtered_indices) {
        if (idx >= 0 && idx < num_bits) {
            bitset_data[idx / 8] |= (1 << (idx % 8));
        }
    }
    
    return {bitset_data, BitsetView(bitset_data.data(), num_bits, filtered_indices.size())};
}

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

TEST_CASE("JVectorIndex Regular Search", "[jvector]") {
    // Create test dataset
    const int64_t nb = 1000;     // number of base vectors
    const int64_t nq = 10;       // number of query vectors
    const int64_t dim = 128;     // dimension
    const int64_t k = 10;        // number of nearest neighbors

    // Generate base vectors and query vectors
    auto base_vectors = GenerateRandomVectors(nb, dim, 42);
    auto query_vectors = GenerateRandomVectors(nq, dim, 43);

    // Create datasets
    auto base_dataset = GenDataSet(nb, dim, base_vectors.data());
    auto query_dataset = GenDataSet(nq, dim, query_vectors.data());
    REQUIRE(base_dataset != nullptr);
    REQUIRE(query_dataset != nullptr);

    SECTION("Search with L2 distance") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        // Build index
        Json build_json;
        build_json["dim"] = dim;
        build_json["metric_type"] = "L2";
        build_json["M"] = 16;
        build_json["efConstruction"] = 64;

        auto status = index->Build(base_dataset, build_json);
        REQUIRE(status.ok());

        // Search with BitsetView
        Json search_json;
        search_json["k"] = k;
        search_json["ef_search"] = k * 2;

        auto result = index->Search(query_dataset, search_json, bitset);
        REQUIRE(result.has_value());
        auto results = result.value();

        // Verify results
        REQUIRE(results->GetRows() == nq);
        REQUIRE(results->GetDim() == k);

        // Verify no results come from filtered vectors
        auto labels = results->GetLabels();
        for (int64_t i = 0; i < nq; i++) {
            for (int64_t j = 0; j < k; j++) {
                REQUIRE(labels[i * k + j] >= nb/2);  // All results should be from second half
            }
        }

        // Verify results
        REQUIRE(results->GetRows() == nq);
        REQUIRE(results->GetDim() == k);

        // Verify distances are non-decreasing
        auto distances = results->GetDistance();
        auto labels = results->GetLabels();
        for (int64_t i = 0; i < nq; i++) {
            for (int64_t j = 1; j < k; j++) {
                REQUIRE(distances[i * k + j] >= distances[i * k + j - 1]);
            }
        }

        // Verify distances match ground truth for top result
        const float* query = query_vectors.data();
        const float* base = base_vectors.data();
        for (int64_t i = 0; i < nq; i++) {
            int64_t nearest = labels[i * k];
            float computed_dist = ComputeL2Distance(query + i * dim, base + nearest * dim, dim);
            REQUIRE(distances[i * k] == Approx(computed_dist).margin(1e-5));
        }
    }

    SECTION("Search with Inner Product distance") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        Json build_json;
        build_json["dim"] = dim;
        build_json["metric_type"] = "IP";
        build_json["M"] = 16;
        build_json["efConstruction"] = 64;

        auto status = index->Build(base_dataset, build_json);
        REQUIRE(status.ok());

        Json search_json;
        search_json["k"] = k;
        search_json["ef_search"] = k * 2;

        auto result = index->Search(query_dataset, search_json);
        REQUIRE(result.has_value());
        auto results = result.value();

        REQUIRE(results->GetRows() == nq);
        REQUIRE(results->GetDim() == k);

        // Verify distances are non-decreasing
        auto distances = results->GetDistance();
        auto labels = results->GetLabels();
        for (int64_t i = 0; i < nq; i++) {
            for (int64_t j = 1; j < k; j++) {
                REQUIRE(distances[i * k + j] >= distances[i * k + j - 1]);
            }
        }

        // Verify distances match ground truth for top result
        const float* query = query_vectors.data();
        const float* base = base_vectors.data();
        for (int64_t i = 0; i < nq; i++) {
            int64_t nearest = labels[i * k];
            float computed_dist = ComputeIPDistance(query + i * dim, base + nearest * dim, dim);
            REQUIRE(distances[i * k] == Approx(computed_dist).margin(1e-5));
        }
    }

    SECTION("Search with empty BitsetView") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        // Build index
        Json build_json;
        build_json["dim"] = dim;
        build_json["metric_type"] = "L2";
        build_json["M"] = 16;
        build_json["efConstruction"] = 64;

        auto status = index->Build(base_dataset, build_json);
        REQUIRE(status.ok());

        // Create empty BitsetView
        BitsetView empty_bitset;

        // Search with empty BitsetView (should behave like regular search)
        Json search_json;
        search_json["k"] = k;
        search_json["ef_search"] = k * 2;

        auto result = index->Search(query_dataset, search_json, empty_bitset);
        REQUIRE(result.has_value());
        auto results = result.value();

        // Results should include vectors from entire index
        auto labels = results->GetLabels();
        bool found_early = false, found_late = false;
        for (int64_t i = 0; i < nq * k; i++) {
            if (labels[i] < nb/2) found_early = true;
            if (labels[i] >= nb/2) found_late = true;
        }
        REQUIRE(found_early);
        REQUIRE(found_late);
    }

    SECTION("Search with all-filtered BitsetView") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        // Build index
        Json build_json;
        build_json["dim"] = dim;
        build_json["metric_type"] = "L2";
        build_json["M"] = 16;
        build_json["efConstruction"] = 64;

        auto status = index->Build(base_dataset, build_json);
        REQUIRE(status.ok());

        // Create BitsetView that filters out all vectors
        std::vector<int64_t> all_indices;
        for (int64_t i = 0; i < nb; i++) {
            all_indices.push_back(i);
        }
        auto [bitset_data, bitset] = CreateBitsetView(nb, all_indices);

        // Search with all-filtered BitsetView
        Json search_json;
        search_json["k"] = k;
        search_json["ef_search"] = k * 2;

        auto result = index->Search(query_dataset, search_json, bitset);
        REQUIRE_FALSE(result.has_value());  // Should fail when all vectors are filtered
    }

    SECTION("Search with invalid BitsetView parameters") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        // Build index
        Json build_json;
        build_json["dim"] = dim;
        build_json["metric_type"] = "L2";
        build_json["M"] = 16;
        build_json["efConstruction"] = 64;

        auto status = index->Build(base_dataset, build_json);
        REQUIRE(status.ok());

        Json search_json;
        search_json["k"] = k;
        search_json["ef_search"] = k * 2;

        // Test BitsetView with wrong size
        std::vector<int64_t> indices = {0};
        auto [wrong_size_data, wrong_size_bitset] = CreateBitsetView(nb + 1, indices);
        auto result = index->Search(query_dataset, search_json, wrong_size_bitset);
        REQUIRE_FALSE(result.has_value());

        // Test BitsetView with invalid indices
        std::vector<int64_t> invalid_indices = {-1, nb + 1};
        auto [invalid_data, invalid_bitset] = CreateBitsetView(nb, invalid_indices);
        result = index->Search(query_dataset, search_json, invalid_bitset);
        REQUIRE(result.has_value());  // Should still work, invalid indices are ignored

        // Test with other invalid parameters
        // Test missing k
        search_json.erase("k");
        result = index->Search(query_dataset, search_json, invalid_bitset);
        REQUIRE_FALSE(result.has_value());

        // Test k = 0
        search_json["k"] = 0;
        result = index->Search(query_dataset, search_json, invalid_bitset);
        REQUIRE_FALSE(result.has_value());

        // Test empty query dataset
        search_json["k"] = k;
        auto empty_dataset = GenDataSet(0, dim, nullptr);
        result = index->Search(empty_dataset, search_json, invalid_bitset);
        REQUIRE_FALSE(result.has_value());

        // Test dimension mismatch
        auto wrong_dim_vectors = GenerateRandomVectors(nq, dim + 1, 44);
        auto wrong_dim_dataset = GenDataSet(nq, dim + 1, wrong_dim_vectors.data());
        result = index->Search(wrong_dim_dataset, search_json, invalid_bitset);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("JVectorIndex BitsetView Search", "[jvector]") {
    // Create test dataset
    const int64_t nb = 1000;     // number of base vectors
    const int64_t nq = 10;       // number of query vectors
    const int64_t dim = 128;     // dimension
    const int64_t k = 10;        // number of nearest neighbors

    // Generate base vectors and query vectors
    auto base_vectors = GenerateRandomVectors(nb, dim, 42);
    auto query_vectors = GenerateRandomVectors(nq, dim, 43);

    // Create datasets
    auto base_dataset = GenDataSet(nb, dim, base_vectors.data());
    auto query_dataset = GenDataSet(nq, dim, query_vectors.data());
    REQUIRE(base_dataset != nullptr);
    REQUIRE(query_dataset != nullptr);

    SECTION("Search with L2 distance") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        // Build index
        Json build_json;
        build_json["dim"] = dim;
        build_json["metric_type"] = "L2";
        build_json["M"] = 16;
        build_json["efConstruction"] = 64;

        auto status = index->Build(base_dataset, build_json);
        REQUIRE(status.ok());

        // Search
        Json search_json;
        search_json["k"] = k;
        search_json["ef_search"] = k * 2;

        SearchResult results;
        status = index->Search(query_dataset, search_json, results);
        REQUIRE(status.ok());

        // Verify results
        REQUIRE(results.distances_.size() == nq * k);
        REQUIRE(results.labels_.size() == nq * k);

        // Verify distances are non-decreasing
        for (int64_t i = 0; i < nq; i++) {
            for (int64_t j = 1; j < k; j++) {
                REQUIRE(results.distances_[i * k + j] >= results.distances_[i * k + j - 1]);
            }
        }

        // Verify distances match ground truth for top result
        const float* query = query_vectors.data();
        const float* base = base_vectors.data();
        for (int64_t i = 0; i < nq; i++) {
            int64_t nearest = results.labels_[i * k];
            float computed_dist = ComputeL2Distance(query + i * dim, base + nearest * dim, dim);
            REQUIRE(results.distances_[i * k] == Approx(computed_dist).margin(1e-5));
        }
    }

    SECTION("Search with Inner Product distance") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        Json build_json;
        build_json["dim"] = dim;
        build_json["metric_type"] = "IP";
        build_json["M"] = 16;
        build_json["efConstruction"] = 64;

        auto status = index->Build(base_dataset, build_json);
        REQUIRE(status.ok());

        Json search_json;
        search_json["k"] = k;
        search_json["ef_search"] = k * 2;

        SearchResult results;
        status = index->Search(query_dataset, search_json, results);
        REQUIRE(status.ok());

        REQUIRE(results.distances_.size() == nq * k);
        REQUIRE(results.labels_.size() == nq * k);

        // Verify distances are non-decreasing
        for (int64_t i = 0; i < nq; i++) {
            for (int64_t j = 1; j < k; j++) {
                REQUIRE(results.distances_[i * k + j] >= results.distances_[i * k + j - 1]);
            }
        }

        // Verify distances match ground truth for top result
        const float* query = query_vectors.data();
        const float* base = base_vectors.data();
        for (int64_t i = 0; i < nq; i++) {
            int64_t nearest = results.labels_[i * k];
            float computed_dist = ComputeIPDistance(query + i * dim, base + nearest * dim, dim);
            REQUIRE(results.distances_[i * k] == Approx(computed_dist).margin(1e-5));
        }
    }

    SECTION("Search with Cosine distance") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        Json build_json;
        build_json["dim"] = dim;
        build_json["metric_type"] = "COSINE";
        build_json["M"] = 16;
        build_json["efConstruction"] = 64;

        auto status = index->Build(base_dataset, build_json);
        REQUIRE(status.ok());

        Json search_json;
        search_json["k"] = k;
        search_json["ef_search"] = k * 2;

        SearchResult results;
        status = index->Search(query_dataset, search_json, results);
        REQUIRE(status.ok());

        REQUIRE(results.distances_.size() == nq * k);
        REQUIRE(results.labels_.size() == nq * k);

        // Verify distances are non-decreasing
        for (int64_t i = 0; i < nq; i++) {
            for (int64_t j = 1; j < k; j++) {
                REQUIRE(results.distances_[i * k + j] >= results.distances_[i * k + j - 1]);
            }
        }

        // Verify distances match ground truth for top result
        const float* query = query_vectors.data();
        const float* base = base_vectors.data();
        for (int64_t i = 0; i < nq; i++) {
            int64_t nearest = results.labels_[i * k];
            float computed_dist = ComputeCosineDistance(query + i * dim, base + nearest * dim, dim);
            REQUIRE(results.distances_[i * k] == Approx(computed_dist).margin(1e-5));
        }
    }

    SECTION("Search with invalid parameters") {
        auto index = std::make_shared<JVectorIndex>();
        REQUIRE(index != nullptr);

        // Build index
        Json build_json;
        build_json["dim"] = dim;
        build_json["metric_type"] = "L2";
        build_json["M"] = 16;
        build_json["efConstruction"] = 64;

        auto status = index->Build(base_dataset, build_json);
        REQUIRE(status.ok());

        SearchResult results;
        Json search_json;

        // Test missing k
        status = index->Search(query_dataset, search_json, results);
        REQUIRE_FALSE(status.ok());

        // Test k = 0
        search_json["k"] = 0;
        status = index->Search(query_dataset, search_json, results);
        REQUIRE_FALSE(status.ok());

        // Test k > index size
        search_json["k"] = nb + 1;
        status = index->Search(query_dataset, search_json, results);
        REQUIRE_FALSE(status.ok());

        // Test empty query dataset
        search_json["k"] = k;
        auto empty_dataset = GenDataSet(0, dim, nullptr);
        status = index->Search(empty_dataset, search_json, results);
        REQUIRE_FALSE(status.ok());

        // Test dimension mismatch
        auto wrong_dim_vectors = GenerateRandomVectors(nq, dim + 1, 44);
        auto wrong_dim_dataset = GenDataSet(nq, dim + 1, wrong_dim_vectors.data());
        status = index->Search(wrong_dim_dataset, search_json, results);
        REQUIRE_FALSE(status.ok());
    }
}
