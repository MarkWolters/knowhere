#include "jvector_index.h"
#include "knowhere/log.h"
#include "jvector_jni.h"
#include "knowhere/index/index_factory.h"
#include "knowhere/comp/index_param.h"

namespace knowhere {

// Register JVector index types with the factory
void RegisterJVectorIndex() {
    // Register for float32
    IndexFactory::Instance().Register<fp32>(
        IndexEnum::INDEX_JVECTOR,
        [](const int32_t& version, const Object& object) {
            return std::make_shared<JVectorIndex>(version);
        },
        0  // No special features for now
    );

    // Register for float16
    IndexFactory::Instance().Register<fp16>(
        IndexEnum::INDEX_JVECTOR,
        [](const int32_t& version, const Object& object) {
            return std::make_shared<JVectorIndex>(version);
        },
        0
    );

    // Register for bfloat16
    IndexFactory::Instance().Register<bf16>(
        IndexEnum::INDEX_JVECTOR,
        [](const int32_t& version, const Object& object) {
            return std::make_shared<JVectorIndex>(version);
        },
        0
    );
}

// Register index types when this file is loaded
static bool jvector_index_registered = []() {
    RegisterJVectorIndex();
    return true;
}();

JVectorIndex::JVectorIndex(const int32_t& version) : IndexNode(version), jvm_(nullptr), index_object_(nullptr), index_class_(nullptr), dim_(0), size_(0) {
}

JVectorIndex::~JVectorIndex() {
    DestroyJVectorIndex();
}

Status
JVectorIndex::Build(const DataSetPtr& dataset, const Json& json, bool use_knowhere_build_pool) {
    // Initialize JVM if not already done
    auto status = InitJVM();
    if (!status.ok()) {
        return status;
    }

    JNIEnv* env;
    if (jvm_->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return Status(Status::Code::Invalid, "Failed to get JNI environment");
    }

    // Validate dataset
    if (!dataset || dataset->GetTensor() == nullptr) {
        return Status(Status::Code::Invalid, "Empty dataset");
    }

    auto rows = dataset->GetRows();
    if (rows == 0) {
        return Status(Status::Code::Invalid, "Empty dataset");
    }

    auto tensor = dataset->GetTensor();
    if (tensor->GetDim() != dim_) {
        return Status(Status::Code::Invalid, "Dimension mismatch");
    }

    // Create JVector index with configuration
    status = CreateJVectorIndex(json);
    if (!status.ok()) {
        return status;
    }

    // Create a builder for adding vectors
    jobject sim_func = nullptr;
    jfieldID field_id = nullptr;

    // Get similarity function based on metric type
    if (metric_type_ == "L2") {
        field_id = env->GetStaticFieldID(g_jni_cache.vector_sim_func_class, "EUCLIDEAN", 
            "Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;");
    } else if (metric_type_ == "IP") {
        field_id = env->GetStaticFieldID(g_jni_cache.vector_sim_func_class, "DOT_PRODUCT", 
            "Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;");
    } else if (metric_type_ == "COSINE") {
        field_id = env->GetStaticFieldID(g_jni_cache.vector_sim_func_class, "COSINE", 
            "Lio/github/jbellis/jvector/vector/VectorSimilarityFunction;");
    }

    if (field_id == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get similarity function field");
    }

    sim_func = env->GetStaticObjectField(g_jni_cache.vector_sim_func_class, field_id);
    if (sim_func == nullptr) {
        return Status(Status::Code::Invalid, "Failed to get similarity function instance");
    }

    // Create builder
    jobject builder = env->NewObject(g_jni_cache.graph_index_builder_class, g_jni_cache.builder_constructor, 
        sim_func, dim_);
    if (builder == nullptr) {
        env->DeleteLocalRef(sim_func);
        return Status(Status::Code::Invalid, "Failed to create GraphIndexBuilder");
    }

    // Configure builder parameters
    jclass builder_class = env->GetObjectClass(builder);
    
    // Set M (max connections per node)
    if (json.contains("M")) {
        jmethodID set_m = env->GetMethodID(builder_class, "setM", "(I)Lio/github/jbellis/jvector/graph/GraphIndexBuilder;");
        if (set_m != nullptr) {
            env->CallObjectMethod(builder, set_m, json["M"].get<int>());
        }
    }

    // Set ef_construction
    if (json.contains("efConstruction")) {
        jmethodID set_ef = env->GetMethodID(builder_class, "setEfConstruction", "(I)Lio/github/jbellis/jvector/graph/GraphIndexBuilder;");
        if (set_ef != nullptr) {
            env->CallObjectMethod(builder, set_ef, json["efConstruction"].get<int>());
        }
    }

    // Add vectors to builder
    status = jvector::AddVectors(env, builder, reinterpret_cast<const float*>(tensor->GetData()), rows, dim_);
    if (!status.ok()) {
        env->DeleteLocalRef(builder);
        env->DeleteLocalRef(sim_func);
        env->DeleteLocalRef(builder_class);
        return status;
    }

    // Build index
    jobject index = env->CallObjectMethod(builder, g_jni_cache.builder_build);
    if (index == nullptr) {
        env->DeleteLocalRef(builder);
        env->DeleteLocalRef(sim_func);
        env->DeleteLocalRef(builder_class);
        return Status(Status::Code::Invalid, "Failed to build index");
    }

    // Update index object
    if (index_object_ != nullptr) {
        env->DeleteGlobalRef(index_object_);
    }
    index_object_ = env->NewGlobalRef(index);
    size_ = rows;

    // Clean up local references
    env->DeleteLocalRef(index);
    env->DeleteLocalRef(builder);
    env->DeleteLocalRef(sim_func);
    env->DeleteLocalRef(builder_class);

    return Status::OK();
}

Status
JVectorIndex::Train(const DataSetPtr& dataset, const Json& json, bool use_knowhere_build_pool) {
    // JVector doesn't require separate training step
    return Status::OK();
}

Status
JVectorIndex::Add(const DataSetPtr& dataset, const Json& json, bool use_knowhere_build_pool) {
    // TODO: Implement add logic using JNI
    return Status::OK();
}

Status
JVectorIndex::Search(const DataSetPtr& dataset, const Json& json, SearchResult& results,
                   bool use_knowhere_search_pool) {
    if (jvm_ == nullptr || index_object_ == nullptr) {
        return Status(Status::Code::Invalid, "JVM or index not initialized");
    }

    JNIEnv* env;
    if (jvm_->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return Status(Status::Code::Invalid, "Failed to get JNI environment");
    }

    // Validate dataset
    if (!dataset || dataset->GetTensor() == nullptr) {
        return Status(Status::Code::Invalid, "Empty dataset");
    }

    auto rows = dataset->GetRows();
    if (rows == 0) {
        return Status(Status::Code::Invalid, "Empty dataset");
    }

    auto tensor = dataset->GetTensor();
    if (tensor->GetDim() != dim_) {
        return Status(Status::Code::Invalid, "Dimension mismatch");
    }

    // Get search parameters
    int64_t k = json["k"].get<int64_t>();
    int ef_search = json.value("ef_search", k * 2);  // Default ef_search to 2*k

    // Allocate memory for results
    results.distances_.resize(rows * k);
    results.labels_.resize(rows * k);

    // Perform search
    return jvector::SearchVectors(env, index_object_,
        reinterpret_cast<const float*>(tensor->GetData()),
        rows, k, results.distances_.data(), results.labels_.data(),
        ef_search);
}

expected<DataSetPtr>
JVectorIndex::Search(const DataSetPtr& dataset, const Json& json, const BitsetView& bitset) {
    if (!dataset || dataset->GetRows() == 0) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Empty dataset");
    }

    if (dataset->GetDim() != dim_) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Dimension mismatch");
    }

    int64_t k = json["k"].get<int64_t>();
    int ef_search = json["ef_search"].get<int>();

    if (k <= 0) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Invalid k");
    }

    if (ef_search <= 0) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Invalid ef_search");
    }

    int64_t num_queries = dataset->GetRows();
    auto results = std::make_shared<DataSet>();
    results->SetRows(num_queries);
    results->SetDim(k);

    auto distances = new float[num_queries * k];
    auto labels = new int64_t[num_queries * k];

    auto status = jvector::SearchVectors(jvm_env_, index_obj_, dataset->GetTensor(), num_queries,
                                        k, distances, labels, ef_search, bitset);
    if (!status.ok()) {
        delete[] distances;
        delete[] labels;
        return expected<DataSetPtr>::Err(status);
    }

    results->SetDistance(distances);
    results->SetLabels(labels);

    return expected<DataSetPtr>::Ok(results);
}

expected<DataSetPtr>
JVectorIndex::RangeSearch(const DataSetPtr& dataset, const Json& json, const BitsetView& bitset) const {
    // TODO: Implement range search logic using JNI
    return expected<DataSetPtr>();
}

expected<DataSetPtr>
JVectorIndex::GetVectorByIds(const DataSetPtr& dataset) const {
    // TODO: Implement get vectors logic using JNI
    return expected<DataSetPtr>();
}

Status
JVectorIndex::Serialize(BinarySet& binset) const {
    // TODO: Implement serialization logic
    return Status::OK();
}

Status
JVectorIndex::Deserialize(const BinarySet& binset, const Json& json) {
    // TODO: Implement deserialization logic
    return Status::OK();
}

Status
JVectorIndex::DeserializeFromFile(const std::string& filename, const Json& json) {
    // TODO: Implement file deserialization logic
    return Status::OK();
}

int64_t
JVectorIndex::Dim() const {
    return dim_;
}

int64_t
JVectorIndex::Size() const {
    return size_;
}

int64_t
JVectorIndex::Count() const {
    return size_;
}

std::string
JVectorIndex::Type() const {
    return "JVECTOR";
}

Status
JVectorIndex::InitJVM() {
    if (jvm_ != nullptr) {
        return Status::OK();
    }

    // Set up JVM options
    JavaVMInitArgs vm_args;
    JavaVMOption options[1];
    
    // Set classpath to include JVector JAR
    std::string classpath = "-Djava.class.path=" + 
        std::string(KNOWHERE_JVECTOR_PATH) + "/lib/jvector-4.0.0-beta.5-SNAPSHOT.jar";
    options[0].optionString = const_cast<char*>(classpath.c_str());
    
    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = 1;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_FALSE;

    // Create JVM
    JNIEnv* env;
    jint res = JNI_CreateJavaVM(&jvm_, (void**)&env, &vm_args);
    if (res != JNI_OK) {
        return Status(Status::Code::Invalid, "Failed to create JVM");
    }

    // Load JVector classes
    return jvector::LoadJVectorClasses(env);
}

Status
JVectorIndex::CreateJVectorIndex(const Json& config) {
    if (jvm_ == nullptr) {
        return Status(Status::Code::Invalid, "JVM not initialized");
    }

    JNIEnv* env;
    if (jvm_->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return Status(Status::Code::Invalid, "Failed to get JNI environment");
    }

    // Get dimension from config
    if (!config.contains("dim")) {
        return Status(Status::Code::Invalid, "Missing dimension in config");
    }
    dim_ = config["dim"].get<int>();

    // Get metric type from config
    std::string metric_type = "L2";  // Default to L2
    if (config.contains("metric_type")) {
        metric_type = config["metric_type"].get<std::string>();
    }
    metric_type_ = metric_type;

    // Create the JVector index
    return jvector::CreateGraphIndex(env, &index_object_, metric_type, dim_, config);
}

Status
JVectorIndex::DestroyJVectorIndex() {
    if (jvm_ == nullptr) {
        return Status::OK();
    }

    JNIEnv* env;
    if (jvm_->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return Status(Status::Code::Invalid, "Failed to get JNI environment");
    }

    if (index_object_ != nullptr) {
        env->DeleteGlobalRef(index_object_);
        index_object_ = nullptr;
    }

    if (index_class_ != nullptr) {
        env->DeleteGlobalRef(index_class_);
        index_class_ = nullptr;
    }

    return Status::OK();
}

}  // namespace knowhere
