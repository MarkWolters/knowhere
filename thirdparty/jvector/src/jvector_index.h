#pragma once

#include "knowhere/index/index_node.h"
#include "knowhere/config.h"
#include "knowhere/dataset.h"
#include "knowhere/expected.h"
#include "knowhere/binaryset.h"

#include <jni.h>
#include <memory>

namespace knowhere {
namespace jvector {

/**
 * @brief JVector index implementation for Knowhere
 *
 * This class provides a C++ interface to the JVector library through JNI.
 * It implements graph-based approximate nearest neighbor search with support
 * for multiple distance metrics (L2, Inner Product, Cosine).
 *
 * Configuration parameters:
 * - dim: Vector dimension (required)
 * - metric_type: Distance metric, one of "L2", "IP", "COSINE" (required)
 * - M: Maximum connections per node (optional)
 * - ef_construction: Size of dynamic candidate list during construction (optional)
 * - ef_search: Size of dynamic candidate list during search (optional)
 * - beam_width: Beam width for search algorithm (optional)
 * - queue_size: Queue size for search results (optional)
 *
 * Thread safety:
 * - All operations are thread-safe
 * - Each thread gets its own JNI environment
 * - Automatic thread attachment/detachment
 */
class JVectorIndex : public IndexNode {
public:
    /**
     * @brief Construct a new JVector Index object
     * 
     * @param version Index version number
     */
    JVectorIndex(const int32_t& version = 0);
    ~JVectorIndex() override;

    // Required IndexNode interface methods
    /**
     * @brief Build the JVector index from the given dataset
     * 
     * @param dataset Input vectors to build index from
     * @param json Configuration parameters
     * @param use_knowhere_build_pool Whether to use Knowhere's thread pool
     * @return Status Operation status
     */
    Status Build(const DataSetPtr& dataset, const Json& json, bool use_knowhere_build_pool = false) override;
    /**
     * @brief Train the JVector index from the given dataset
     * 
     * @param dataset Input vectors to train index from
     * @param json Configuration parameters
     * @param use_knowhere_build_pool Whether to use Knowhere's thread pool
     * @return Status Operation status
     */
    Status Train(const DataSetPtr& dataset, const Json& json, bool use_knowhere_build_pool = true) override;
    /**
     * @brief Add vectors to the JVector index
     * 
     * @param dataset Input vectors to add
     * @param json Configuration parameters
     * @param use_knowhere_build_pool Whether to use Knowhere's thread pool
     * @return Status Operation status
     */
    Status Add(const DataSetPtr& dataset, const Json& json, bool use_knowhere_build_pool = true) override;
    /**
     * @brief Perform k-NN search on the index
     * 
     * @param dataset Query vectors
     * @param json Search parameters (k, ef_search)
     * @param bitset Optional filter for search results
     * @return expected<DataSetPtr> Search results or error
     */
    expected<DataSetPtr> Search(const DataSetPtr& dataset, const Json& json, const BitsetView& bitset) const override;
    /**
     * @brief Perform range search on the index
     * 
     * @param dataset Query vectors
     * @param json Search parameters (radius, ef_search)
     * @param bitset Optional filter for search results
     * @return expected<DataSetPtr> Search results or error
     */
    expected<DataSetPtr> RangeSearch(const DataSetPtr& dataset, const Json& json, const BitsetView& bitset) const override;
    /**
     * @brief Retrieve vectors by their IDs
     * 
     * @param dataset Dataset containing vector IDs to retrieve
     * @return expected<DataSetPtr> Retrieved vectors or error
     */
    expected<DataSetPtr> GetVectorByIds(const DataSetPtr& dataset) const override;
    /**
     * @brief Serialize the index to binary format
     * 
     * @param binset Output binary set
     * @return Status Operation status
     */
    Status Serialize(BinarySet& binset) const override;
    /**
     * @brief Deserialize the index from binary format
     * 
     * @param binset Input binary set
     * @param json Configuration parameters
     * @return Status Operation status
     */
    Status Deserialize(const BinarySet& binset, const Json& json) override;
    /**
     * @brief Deserialize the index from a file
     * 
     * @param filename Input file name
     * @param json Configuration parameters
     * @return Status Operation status
     */
    Status DeserializeFromFile(const std::string& filename, const Json& json = {}) override;
    /**
     * @brief Get the dimension of the index
     * 
     * @return int64_t Dimension of the index
     */
    int64_t Dim() const override;
    /**
     * @brief Get the size of the index
     * 
     * @return int64_t Size of the index
     */
    int64_t Size() const override;
    /**
     * @brief Get the count of vectors in the index
     * 
     * @return int64_t Count of vectors in the index
     */
    int64_t Count() const override;
    /**
     * @brief Get the type of the index
     * 
     * @return std::string Type of the index
     */
    std::string Type() const override;

private:
    // JNI helper methods
    /**
     * @brief Validate configuration parameters
     * 
     * Checks:
     * - Required parameters presence
     * - Parameter types and ranges
     * - Metric type validity
     * 
     * @param config Configuration to validate
     * @return Status Validation status
     */
    Status ValidateConfig(const Json& config) const;
    /**
     * @brief Initialize the JVM
     * 
     * @return Status Initialization status
     */
    Status InitJVM();
    /**
     * @brief Create a new JVector index
     * 
     * @param config Configuration parameters
     * @return Status Creation status
     */
    Status CreateJVectorIndex(const Json& config);
    /**
     * @brief Destroy the JVector index
     * 
     * @return Status Destruction status
     */
    Status DestroyJVectorIndex();

    // JNI references
    JavaVM* jvm_;
    jobject index_object_;
    jclass index_class_;

    // Index properties
    int64_t dim_;
    int64_t size_;
    std::string metric_type_;
};

} // namespace jvector
} // namespace knowhere