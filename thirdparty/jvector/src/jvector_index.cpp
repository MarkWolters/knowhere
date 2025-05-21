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
    if (jvm_ != nullptr) {
        DetachThreadLocalJNIEnv(jvm_);
    }
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
    // Check if index is initialized
    if (!index_object_) {
        LOG_ERROR_ << "Index not initialized";
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Index not initialized");
    }
    if (!dataset || dataset->GetRows() == 0) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Empty dataset");
    }

    if (dataset->GetDim() != dim_) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Dimension mismatch");
    }

    // Validate JSON parameters
    if (!json.contains("radius")) {
        LOG_ERROR_ << "Missing radius parameter";
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Missing radius parameter");
    }
    if (!json.contains("ef_search")) {
        LOG_ERROR_ << "Missing ef_search parameter";
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Missing ef_search parameter");
    }

    float radius;
    int ef_search;
    try {
        radius = json["radius"].get<float>();
        ef_search = json["ef_search"].get<int>();
    } catch (const std::exception& e) {
        LOG_ERROR_ << "Invalid parameter type: " << e.what();
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Invalid parameter type");
    }

    // Check for NaN or infinite radius
    if (std::isnan(radius) || std::isinf(radius)) {
        LOG_ERROR_ << "Invalid radius value: " << radius;
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Invalid radius value");
    }

    if (radius <= 0) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Invalid radius");
    }

    if (ef_search <= 0) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Invalid ef_search");
    }

    int64_t num_queries = dataset->GetRows();
    std::vector<std::vector<float>> distances;
    std::vector<std::vector<int64_t>> labels;

    auto status = jvector::RangeSearchVectors(jvm_env_, index_obj_, dataset->GetTensor(), num_queries,
                                           radius, distances, labels, ef_search, bitset);
    if (!status.ok()) {
        return expected<DataSetPtr>::Err(status);
    }

    // Count total number of results
    int64_t total_results = 0;
    for (const auto& row : labels) {
        total_results += row.size();
    }

    if (total_results == 0) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "No results found within radius");
    }

    // Create result dataset
    auto results = std::make_shared<DataSet>();
    results->SetRows(total_results);
    results->SetDim(1);  // Each result is a single distance-label pair

    auto result_distances = new float[total_results];
    auto result_labels = new int64_t[total_results];

    // Flatten results
    int64_t offset = 0;
    for (size_t i = 0; i < distances.size(); i++) {
        const auto& dist_row = distances[i];
        const auto& label_row = labels[i];
        for (size_t j = 0; j < dist_row.size(); j++) {
            result_distances[offset] = dist_row[j];
            result_labels[offset] = label_row[j];
            offset++;
        }
    }

    results->SetDistance(result_distances);
    results->SetLabels(result_labels);

    return expected<DataSetPtr>::Ok(results);
}

expected<DataSetPtr>
JVectorIndex::GetVectorByIds(const DataSetPtr& dataset) const {
    // Check if index is initialized
    if (!index_object_) {
        LOG_ERROR_ << "Index not initialized";
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Index not initialized");
    }
    if (!dataset || dataset->GetRows() == 0) {
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Empty dataset");
    }

    auto labels = dataset->GetLabels();
    if (labels == nullptr) {
        LOG_ERROR_ << "No labels provided";
        return expected<DataSetPtr>::Err(Status::invalid_argument, "No labels provided");
    }

    // Check if any label is out of bounds
    for (int64_t i = 0; i < dataset->GetRows(); i++) {
        if (labels[i] < 0 || labels[i] >= size_) {
            LOG_ERROR_ << "Label out of bounds: " << labels[i] << ", size: " << size_;
            return expected<DataSetPtr>::Err(Status::invalid_argument, "Label out of bounds");
        }
    }

    int64_t num_vectors = dataset->GetRows();
    auto result_vectors = new float[num_vectors * dim_];
    bool has_error = false;

    auto status = EnsureThreadLocalJNIEnv(jvm_, &env);
    if (!status.ok()) {
        return expected<DataSetPtr>::Err(status);
    }

    // Get the getVector method from GraphIndex class
    jmethodID get_vector_method = env->GetMethodID(g_jni_cache.graph_index_class, "getVector", "(J)[F");
    if (get_vector_method == nullptr) {
        delete[] result_vectors;
        LOG_ERROR_ << "Failed to get getVector method";
        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) return expected<DataSetPtr>::Err(status);
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Failed to get getVector method");
    }

    // Get vectors one by one
    for (int64_t i = 0; i < num_vectors && !has_error; i++) {
        jfloatArray vector_array = (jfloatArray)env->CallObjectMethod(index_object_, get_vector_method, labels[i]);
        if (vector_array == nullptr) {
            LOG_ERROR_ << "Failed to get vector for id " << labels[i];
            has_error = true;
            continue;
        }

        // Copy vector data
        env->GetFloatArrayRegion(vector_array, 0, dim_, result_vectors + i * dim_);
        env->DeleteLocalRef(vector_array);

        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) {
            has_error = true;
        }
    }

    if (has_error) {
        delete[] result_vectors;
        return expected<DataSetPtr>::Err(Status::invalid_argument, "Failed to get one or more vectors");
    }

    // Create result dataset
    auto results = std::make_shared<DataSet>();
    results->SetRows(num_vectors);
    results->SetDim(dim_);
    results->SetTensor(result_vectors);

    return expected<DataSetPtr>::Ok(results);
}

Status
JVectorIndex::Serialize(BinarySet& binset) const {
    // Check if index is initialized
    if (!index_object_) {
        LOG_ERROR_ << "Index not initialized";
        return Status(Status::Code::Invalid, "Index not initialized");
    }

    // Check if index is empty
    if (size_ == 0) {
        LOG_ERROR_ << "Cannot serialize empty index";
        return Status(Status::Code::Invalid, "Cannot serialize empty index");
    }
    JNIEnv* env;
    if (jvm_->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return Status(Status::Code::Invalid, "Failed to get JNI environment");
    }

    // Get the serialize method from GraphIndex class
    jmethodID serialize_method = env->GetMethodID(g_jni_cache.graph_index_class, "serialize", "()[B");
    if (serialize_method == nullptr) {
        LOG_ERROR_ << "Failed to get serialize method";
        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) return status;
        return Status(Status::Code::Invalid, "Failed to get serialize method");
    }

    // Call serialize method
    jbyteArray bytes = (jbyteArray)env->CallObjectMethod(index_object_, serialize_method);
    if (bytes == nullptr) {
        LOG_ERROR_ << "Failed to serialize index";
        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) return status;
        return Status(Status::Code::Invalid, "Failed to serialize index");
    }

    // Copy serialized data
    jsize length = env->GetArrayLength(bytes);
    std::vector<uint8_t> data(length);
    env->GetByteArrayRegion(bytes, 0, length, reinterpret_cast<jbyte*>(data.data()));
    env->DeleteLocalRef(bytes);

    auto status = jvector::CheckJavaException(env);
    if (!status.ok()) return status;

    // Add to binary set
    binset.Append("JVectorIndex", data);

    return Status::OK();
}

Status
JVectorIndex::Deserialize(const BinarySet& binset, const Json& json) {
    // Validate JSON parameters
    if (!json.contains("dim")) {
        LOG_ERROR_ << "Missing dimension parameter";
        return Status(Status::Code::Invalid, "Missing dimension parameter");
    }

    if (!json.contains("metric_type")) {
        LOG_ERROR_ << "Missing metric_type parameter";
        return Status(Status::Code::Invalid, "Missing metric_type parameter");
    }

    try {
        int dim = json["dim"].get<int>();
        std::string metric_type = json["metric_type"].get<std::string>();
        
        if (dim <= 0) {
            LOG_ERROR_ << "Invalid dimension: " << dim;
            return Status(Status::Code::Invalid, "Invalid dimension");
        }

        if (metric_type != "L2" && metric_type != "IP" && metric_type != "COSINE") {
            LOG_ERROR_ << "Invalid metric type: " << metric_type;
            return Status(Status::Code::Invalid, "Invalid metric type");
        }
    } catch (const std::exception& e) {
        LOG_ERROR_ << "Invalid JSON parameter type: " << e.what();
        return Status(Status::Code::Invalid, "Invalid JSON parameter type");
    }
    auto binary = binset.GetByName("JVectorIndex");
    if (!binary) {
        LOG_ERROR_ << "Failed to find JVectorIndex binary data";
        return Status(Status::Code::Invalid, "Failed to find JVectorIndex binary data");
    }

    // Initialize JVM and create index
    auto status = InitJVM();
    if (!status.ok()) return status;

    status = CreateJVectorIndex(json);
    if (!status.ok()) return status;

    JNIEnv* env;
    if (jvm_->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return Status(Status::Code::Invalid, "Failed to get JNI environment");
    }

    // Get the deserialize method from GraphIndex class
    jmethodID deserialize_method = env->GetMethodID(g_jni_cache.graph_index_class, "deserialize", "([B)V");
    if (deserialize_method == nullptr) {
        LOG_ERROR_ << "Failed to get deserialize method";
        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) return status;
        return Status(Status::Code::Invalid, "Failed to get deserialize method");
    }

    // Create byte array with serialized data
    jbyteArray bytes = env->NewByteArray(binary->size);
    if (bytes == nullptr) {
        LOG_ERROR_ << "Failed to create byte array";
        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) return status;
        return Status(Status::Code::Invalid, "Failed to create byte array");
    }

    env->SetByteArrayRegion(bytes, 0, binary->size, reinterpret_cast<const jbyte*>(binary->data.get()));

    // Call deserialize method
    env->CallVoidMethod(index_object_, deserialize_method, bytes);
    env->DeleteLocalRef(bytes);

    auto status = jvector::CheckJavaException(env);
    if (!status.ok()) return status;

    // Update dimension and size from the deserialized index
    jmethodID get_dim_method = env->GetMethodID(g_jni_cache.graph_index_class, "getDimension", "()I");
    jmethodID get_size_method = env->GetMethodID(g_jni_cache.graph_index_class, "size", "()J");

    if (get_dim_method == nullptr || get_size_method == nullptr) {
        LOG_ERROR_ << "Failed to get dimension/size methods";
        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) return status;
        return Status(Status::Code::Invalid, "Failed to get dimension/size methods");
    }

    dim_ = env->CallIntMethod(index_object_, get_dim_method);
    size_ = env->CallLongMethod(index_object_, get_size_method);

    return Status::OK();
}

Status
JVectorIndex::DeserializeFromFile(const std::string& filename, const Json& json) {
    // Check if file exists and is readable
    std::ifstream file(filename);
    if (!file.good()) {
        LOG_ERROR_ << "File does not exist or is not readable: " << filename;
        return Status(Status::Code::Invalid, "File does not exist or is not readable");
    }
    file.close();

    // Validate JSON parameters
    if (!json.contains("dim")) {
        LOG_ERROR_ << "Missing dimension parameter";
        return Status(Status::Code::Invalid, "Missing dimension parameter");
    }

    if (!json.contains("metric_type")) {
        LOG_ERROR_ << "Missing metric_type parameter";
        return Status(Status::Code::Invalid, "Missing metric_type parameter");
    }

    try {
        int dim = json["dim"].get<int>();
        std::string metric_type = json["metric_type"].get<std::string>();
        
        if (dim <= 0) {
            LOG_ERROR_ << "Invalid dimension: " << dim;
            return Status(Status::Code::Invalid, "Invalid dimension");
        }

        if (metric_type != "L2" && metric_type != "IP" && metric_type != "COSINE") {
            LOG_ERROR_ << "Invalid metric type: " << metric_type;
            return Status(Status::Code::Invalid, "Invalid metric type");
        }
    } catch (const std::exception& e) {
        LOG_ERROR_ << "Invalid JSON parameter type: " << e.what();
        return Status(Status::Code::Invalid, "Invalid JSON parameter type");
    }
    // Initialize JVM and create index
    auto status = InitJVM();
    if (!status.ok()) return status;

    status = CreateJVectorIndex(json);
    if (!status.ok()) return status;

    JNIEnv* env;
    if (jvm_->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return Status(Status::Code::Invalid, "Failed to get JNI environment");
    }

    // Get the deserializeFromFile method
    jmethodID deserialize_file_method = env->GetMethodID(g_jni_cache.graph_index_class, "deserializeFromFile", "(Ljava/lang/String;)V");
    if (deserialize_file_method == nullptr) {
        LOG_ERROR_ << "Failed to get deserializeFromFile method";
        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) return status;
        return Status(Status::Code::Invalid, "Failed to get deserializeFromFile method");
    }

    // Convert filename to Java string
    jstring j_filename = env->NewStringUTF(filename.c_str());
    if (j_filename == nullptr) {
        LOG_ERROR_ << "Failed to create Java string from filename";
        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) return status;
        return Status(Status::Code::Invalid, "Failed to create Java string from filename");
    }

    // Call deserializeFromFile method
    env->CallVoidMethod(index_object_, deserialize_file_method, j_filename);
    env->DeleteLocalRef(j_filename);

    auto status = jvector::CheckJavaException(env);
    if (!status.ok()) return status;

    // Update dimension and size from the deserialized index
    jmethodID get_dim_method = env->GetMethodID(g_jni_cache.graph_index_class, "getDimension", "()I");
    jmethodID get_size_method = env->GetMethodID(g_jni_cache.graph_index_class, "size", "()J");

    if (get_dim_method == nullptr || get_size_method == nullptr) {
        LOG_ERROR_ << "Failed to get dimension/size methods";
        auto status = jvector::CheckJavaException(env);
        if (!status.ok()) return status;
        return Status(Status::Code::Invalid, "Failed to get dimension/size methods");
    }

    dim_ = env->CallIntMethod(index_object_, get_dim_method);
    size_ = env->CallLongMethod(index_object_, get_size_method);

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
JVectorIndex::ValidateConfig(const Json& config) const {
    // Required parameters
    if (!config.contains("dim")) {
        LOG_ERROR_ << "Missing dimension parameter";
        return Status(Status::Code::Invalid, "Missing dimension parameter");
    }

    // Validate dimension
    int dim = config["dim"].get<int>();
    if (dim <= 0) {
        LOG_ERROR_ << "Invalid dimension: " << dim;
        return Status(Status::Code::Invalid, "Invalid dimension");
    }

    // Validate metric type
    std::string metric_type = "L2";  // Default to L2
    if (config.contains("metric_type")) {
        metric_type = config["metric_type"].get<std::string>();
        if (metric_type != "L2" && metric_type != "IP" && metric_type != "COSINE") {
            LOG_ERROR_ << "Invalid metric type: " << metric_type;
            return Status(Status::Code::Invalid, "Invalid metric type");
        }
    }

    // Validate M (max connections)
    if (config.contains(indexparam::JVECTOR_M)) {
        int M = config[indexparam::JVECTOR_M].get<int>();
        if (M <= 0) {
            LOG_ERROR_ << "Invalid M value: " << M;
            return Status(Status::Code::Invalid, "Invalid M value");
        }
    }

    // Validate ef_construction
    if (config.contains(indexparam::JVECTOR_EF_CONSTRUCTION)) {
        int ef_construction = config[indexparam::JVECTOR_EF_CONSTRUCTION].get<int>();
        if (ef_construction <= 0) {
            LOG_ERROR_ << "Invalid ef_construction value: " << ef_construction;
            return Status(Status::Code::Invalid, "Invalid ef_construction value");
        }
    }

    // Validate ef_search
    if (config.contains(indexparam::JVECTOR_EF_SEARCH)) {
        int ef_search = config[indexparam::JVECTOR_EF_SEARCH].get<int>();
        if (ef_search <= 0) {
            LOG_ERROR_ << "Invalid ef_search value: " << ef_search;
            return Status(Status::Code::Invalid, "Invalid ef_search value");
        }
    }

    // Validate beam_width
    if (config.contains(indexparam::JVECTOR_BEAM_WIDTH)) {
        int beam_width = config[indexparam::JVECTOR_BEAM_WIDTH].get<int>();
        if (beam_width <= 0) {
            LOG_ERROR_ << "Invalid beam_width value: " << beam_width;
            return Status(Status::Code::Invalid, "Invalid beam_width value");
        }
    }

    // Validate queue_size
    if (config.contains(indexparam::JVECTOR_QUEUE_SIZE)) {
        int queue_size = config[indexparam::JVECTOR_QUEUE_SIZE].get<int>();
        if (queue_size <= 0) {
            LOG_ERROR_ << "Invalid queue_size value: " << queue_size;
            return Status(Status::Code::Invalid, "Invalid queue_size value");
        }
    }

    return Status::OK();
}

Status
JVectorIndex::CreateJVectorIndex(const Json& config) {
    if (jvm_ == nullptr) {
        return Status(Status::Code::Invalid, "JVM not initialized");
    }

    // Validate configuration
    auto status = ValidateConfig(config);
    if (!status.ok()) {
        return status;
    }

    JNIEnv* env;
    auto status = EnsureThreadLocalJNIEnv(jvm_, &env);
    if (!status.ok()) {
        return status;
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
    auto status = EnsureThreadLocalJNIEnv(jvm_, &env);
    if (!status.ok()) {
        return status;
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