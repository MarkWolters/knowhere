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

    // Create JVector index with configuration
    status = CreateJVectorIndex(json);
    if (!status.ok()) {
        return status;
    }

    // TODO: Implement build logic using JNI
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

expected<DataSetPtr>
JVectorIndex::Search(const DataSetPtr& dataset, const Json& json, const BitsetView& bitset) const {
    // TODO: Implement search logic using JNI
    return expected<DataSetPtr>();
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
