# JVector Integration Guide for Knowhere

This guide explains how to build and use the JVector plugin with Knowhere for high-performance vector similarity search.

## Prerequisites

- Python 3.7 or higher
- JDK 21
- Conan 1.64.1 or higher
- CMake 3.20 or higher
- C++17 compatible compiler (GCC 9.4.0 or higher recommended)
- Linux environment (recommended for production)

## Installation Steps

### 1. Set Up Python Virtual Environment

```bash
# Create and activate a Python virtual environment
python -m venv knowhere_venv
source knowhere_venv/bin/activate

# Upgrade pip and install required Python packages
pip install --upgrade pip
pip install conan==1.64.1
```

### 2. Build Knowhere with JVector Support

```bash
# Clone Knowhere repository if you haven't already
git clone https://github.com/milvus-io/knowhere.git
cd knowhere

# Checkout the jvector integration branch
git checkout jvec2

# Create build directory and navigate to it
mkdir -p build && cd build

# Install dependencies using Conan
conan install .. --build=missing -o with_ut=True -s compiler.libcxx=libstdc++11 -s build_type=Debug

# Build the project
conan build ..
```

### 3. Set Up JVM Environment

Add the following to your environment variables (e.g., in your `~/.bashrc` or `~/.zshrc`):

```bash
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64  # Update this path to your JDK installation
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${JAVA_HOME}/lib/server
```

## Using JVector in Knowhere

### 1. Verify JVector Integration

After building, you can verify that JVector is properly integrated by checking the built library:

```bash
# Check for JVector symbols in the built library
nm -C Debug/libknowhere.so | grep -i jvec
```

You should see JVector-related symbols in the output, including:
- `knowhere::indexparam::JVECTOR_EF_SEARCH`
- `knowhere::indexparam::JVECTOR_BEAM_WIDTH`
- `knowhere::indexparam::JVECTOR_QUEUE_SIZE`
- `knowhere::indexparam::JVECTOR_EF_CONSTRUCTION`
- `knowhere::indexparam::JVECTOR_M`
- `knowhere::IndexEnum::INDEX_JVECTOR`

### 2. Running Tests

To run the test suite and verify JVector functionality:

```bash
# Navigate to the build directory if not already there
cd build

# Run the test executable
./tests/ut/knowhere_tests --gtest_filter=*JVector*
```

## Integration with Milvus

### 1. Configure Milvus

Update your Milvus configuration to enable JVector:

```yaml
# milvus.yaml
knowhere:
  enable_jvector: true
  jvm_options:
    - "-Xms1g"
    - "-Xmx4g"
    - "-XX:+UseG1GC"
```

### 2. Create Collection with JVector Index

```python
from pymilvus import Collection, FieldSchema, CollectionSchema, DataType, connections

# Connect to Milvus
connections.connect()

# Define collection schema
dim = 128
fields = [
    FieldSchema(name="id", dtype=DataType.INT64, is_primary=True),
    FieldSchema(name="vector", dtype=DataType.FLOAT_VECTOR, dim=dim)
]
schema = CollectionSchema(fields=fields, description="Collection using JVector index")

# Create collection
collection = Collection(name="jvector_example", schema=schema)

# Create JVector index
index_params = {
    "index_type": "JVECTOR",
    "metric_type": "L2",
    "params": {
        "M": 16,
        "ef_construction": 128
    }
}
collection.create_index(field_name="vector", index_params=index_params)
```

### 2. Search Configuration

When performing searches, you can tune these parameters:

```python
search_params = {
    "metric_type": "L2",
    "params": {
        "ef_search": 64
    }
}

results = collection.search(
    data=query_vectors,
    anns_field="vector",
    param=search_params,
    limit=10
)
```

## Performance Tuning

### 1. Index Building Parameters

- `M`: Controls graph connectivity (default: 16)
  - Higher values → better accuracy, slower build/search
  - Recommended range: 8-64

- `ef_construction`: Build-time accuracy parameter (default: 128)
  - Higher values → better accuracy, slower build
  - Recommended range: 64-512

### 2. Search Parameters

- `ef_search`: Runtime accuracy parameter (default: 64)
  - Higher values → better accuracy, slower search
  - Recommended range: 32-256

### 3. JVM Configuration

Optimize based on your data size:

```yaml
jvm_options:
  - "-Xms4g"        # Initial heap size
  - "-Xmx16g"       # Maximum heap size
  - "-XX:+UseG1GC"  # Use G1 Garbage Collector
```

## Memory Requirements

Estimate memory usage:
- Index size ≈ (vector_dimension * 4 + 32) * num_vectors bytes
- Additional JVM overhead: ~20%

Example for 1M vectors with dim=128:
- Index: ~650MB
- JVM overhead: ~130MB
- Total: ~780MB

## Monitoring

Monitor these metrics for optimal performance:

1. Build time metrics:
   - Index build time
   - Memory usage during build

2. Search metrics:
   - QPS (Queries Per Second)
   - Latency (p95, p99)
   - Recall@k

## Troubleshooting

### Common Issues

1. JVM Initialization Failed
```
Solution: Check JAVA_HOME and LD_LIBRARY_PATH settings
```

2. Out of Memory Errors
```
Solution: Adjust JVM heap size in milvus.yaml
```

3. Poor Search Performance
```
Solution: Tune ef_search parameter or increase JVM heap size
```

### Debug Logging

Enable detailed logging in Milvus configuration:

```yaml
logs:
  level: debug
  trace.enable: true
```

## Support

For issues and questions:
1. Milvus GitHub Issues: [github.com/milvus-io/milvus/issues](https://github.com/milvus-io/milvus/issues)
2. Milvus Discord: [discord.gg/milvus](https://discord.gg/milvus)
3. JVector GitHub: [github.com/jbellis/jvector](https://github.com/jbellis/jvector)

## References

- [Milvus Documentation](https://milvus.io/docs)
- [JVector Documentation](https://github.com/jbellis/jvector/blob/main/README.md)
- [Knowhere Repository](https://github.com/milvus-io/knowhere)
