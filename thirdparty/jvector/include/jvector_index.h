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

class JVectorIndex : public IndexNode {
public:
    JVectorIndex(const int32_t& version = 0);
    ~JVectorIndex() override;

    // Required IndexNode interface methods
    Status Build(const DataSetPtr& dataset, const Json& json, bool use_knowhere_build_pool = true) override;
    Status Train(const DataSetPtr& dataset, const Json& json, bool use_knowhere_build_pool = true) override;
    Status Add(const DataSetPtr& dataset, const Json& json, bool use_knowhere_build_pool = true) override;
    expected<DataSetPtr> Search(const DataSetPtr& dataset, const Json& json, const BitsetView& bitset) const override;
    expected<DataSetPtr> RangeSearch(const DataSetPtr& dataset, const Json& json, const BitsetView& bitset) const override;
    expected<DataSetPtr> GetVectorByIds(const DataSetPtr& dataset) const override;
    Status Serialize(BinarySet& binset) const override;
    Status Deserialize(const BinarySet& binset, const Json& json = {}) override;
    Status DeserializeFromFile(const std::string& filename, const Json& json = {}) override;
    int64_t Dim() const override;
    int64_t Size() const override;
    int64_t Count() const override;
    std::string Type() const override;

private:
    // JNI helper methods
    Status InitJVM();
    Status CreateJVectorIndex(const Json& config);
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
