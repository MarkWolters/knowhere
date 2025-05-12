# JVector Integration for Knowhere

JVector is a high-performance graph-based approximate nearest neighbor search library implemented in Java. This integration provides a C++ interface to JVector through JNI, making it available as a Knowhere index type.

## Features

- Graph-based ANN index (similar to HNSW)
- Multiple distance metrics support (L2, Inner Product, Cosine)
- Thread-safe operations with per-thread JNI views
- Support for both in-memory and on-disk indexes
- Native vector operations via JNI

## Configuration Parameters

### Required Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| dim | int | Vector dimension. Must be > 0 |
| metric_type | string | Distance metric. One of: "L2", "IP" (Inner Product), "COSINE" |

### Optional Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| M | int | 16 | Maximum number of connections per node in the graph |
| ef_construction | int | 128 | Size of dynamic candidate list during index construction |
| ef_search | int | 64 | Size of dynamic candidate list during search |
| beam_width | int | 10 | Beam width for search algorithm |
| queue_size | int | 100 | Queue size for search results |

## Usage Example

```cpp
#include "knowhere/index_node.h"
#include "knowhere/factory.h"

// Create JVector index
auto index = std::make_shared<JVectorIndex>();

// Configure index
Json config;
config["dim"] = 128;              // Vector dimension
config["metric_type"] = "L2";     // Distance metric
config["M"] = 16;                 // Max connections per node
config["ef_construction"] = 128;  // Construction quality parameter
config["ef_search"] = 64;         // Search quality parameter

// Build index
auto status = index->Build(dataset, config);
```

## Search Parameters

For search operations, the following parameters are supported:

| Parameter | Type | Description |
|-----------|------|-------------|
| k | int | Number of nearest neighbors to return |
| ef_search | int | Dynamic candidate list size (optional, overrides index setting) |
| radius | float | Distance threshold for range search |

## Thread Safety

The JVector integration is thread-safe and handles JNI thread attachment/detachment automatically. Each thread gets its own JNI environment for concurrent operations.

## Error Handling

All operations include comprehensive error checking:
- Parameter validation
- JNI exception handling
- Memory management
- Thread safety

## Dependencies

- JDK 8 or higher
- JVector JAR (automatically included in build)
- Knowhere core library

## Build Instructions

The JVector integration is built automatically as part of Knowhere. The build system will:
1. Download the required JVector JAR
2. Set up JNI includes
3. Build the integration library

## Performance Considerations

- Use appropriate `ef_construction` values during index building for better accuracy
- Adjust `ef_search` based on your speed/accuracy requirements
- Consider using thread-local JNI caching for high-throughput scenarios
