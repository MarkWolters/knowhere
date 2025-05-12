#!/usr/bin/env python3
# Copyright (C) 2019-2023 Zilliz. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under the License
# is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
# or implied. See the License for the specific language governing permissions and limitations under the License.

import knowhere
import numpy as np
import time
from typing import Dict, List, Tuple

def run_benchmark(
    dim: int,
    nb: int,
    nq: int,
    k: int,
    ef_construction_values: List[int],
    ef_search_values: List[int],
    metric_type: str = 'L2'
) -> Dict[Tuple[int, int], Dict[str, float]]:
    """
    Run benchmark with different ef_construction and ef_search values.
    
    Args:
        dim: Vector dimension
        nb: Number of base vectors
        nq: Number of query vectors
        k: Number of nearest neighbors to find
        ef_construction_values: List of ef_construction values to test
        ef_search_values: List of ef_search values to test
        metric_type: Distance metric to use
    
    Returns:
        Dictionary mapping (ef_construction, ef_search) to performance metrics
    """
    # Generate data
    xb = np.random.rand(nb, dim).astype('float32')
    xq = np.random.rand(nq, dim).astype('float32')

    # Calculate ground truth using brute force
    print("Computing ground truth...")
    index_flat = knowhere.CreateIndex('FLAT')
    index_flat.Build(
        data=xb,
        params={'dim': dim, 'metric_type': metric_type}
    )
    ground_truth = index_flat.Search(
        data=xq,
        params={'k': k}
    )

    results = {}
    
    # Test different parameter combinations
    for ef_construction in ef_construction_values:
        for ef_search in ef_search_values:
            print(f"\nTesting ef_construction={ef_construction}, ef_search={ef_search}")
            
            # Create and build index
            index = knowhere.CreateIndex('JVECTOR')
            index_params = {
                'dim': dim,
                'metric_type': metric_type,
                'M': 16,
                'ef_construction': ef_construction
            }
            
            build_start = time.time()
            index.Build(data=xb, params=index_params)
            build_time = time.time() - build_start
            
            # Perform search
            search_params = {'k': k, 'ef_search': ef_search}
            
            search_times = []
            for _ in range(5):  # Run multiple times to get average
                search_start = time.time()
                results_jv = index.Search(data=xq, params=search_params)
                search_times.append(time.time() - search_start)
            
            avg_search_time = sum(search_times) / len(search_times)
            
            # Calculate recall
            recall = 0
            for i in range(nq):
                # Convert to sets for intersection
                truth_set = set(ground_truth.ids[i])
                result_set = set(results_jv.ids[i])
                recall += len(truth_set.intersection(result_set)) / k
            recall /= nq
            
            # Store results
            results[(ef_construction, ef_search)] = {
                'build_time': build_time,
                'search_time': avg_search_time,
                'recall': recall,
                'memory_usage': index.Size() * 4  # Size in bytes (float32)
            }
            
            print(f"Build time: {build_time:.3f}s")
            print(f"Average search time: {avg_search_time:.3f}s")
            print(f"Recall@{k}: {recall:.3f}")
            print(f"Memory usage: {results[(ef_construction, ef_search)]['memory_usage'] / 1024 / 1024:.2f} MB")
    
    return results

def benchmark_example():
    """
    Example benchmarking JVector performance with different parameters.
    """
    # Benchmark parameters
    dim = 128
    nb = 100000   # database size
    nq = 1000     # number of queries
    k = 10        # top-k

    # Parameter values to test
    ef_construction_values = [100, 200, 400]
    ef_search_values = [50, 100, 200]

    print("Starting JVector benchmark...")
    print(f"Database size: {nb}")
    print(f"Query size: {nq}")
    print(f"Dimension: {dim}")
    print(f"k: {k}")

    # Run benchmarks for different metrics
    for metric in ['L2', 'IP', 'COSINE']:
        print(f"\nBenchmarking {metric} metric...")
        results = run_benchmark(
            dim=dim,
            nb=nb,
            nq=nq,
            k=k,
            ef_construction_values=ef_construction_values,
            ef_search_values=ef_search_values,
            metric_type=metric
        )

        # Print summary
        print(f"\n{metric} Metric Summary:")
        print("=" * 80)
        print("ef_construction | ef_search | Build Time | Search Time | Recall | Memory (MB)")
        print("-" * 80)
        
        for (ef_c, ef_s), metrics in results.items():
            print(f"{ef_c:13d} | {ef_s:9d} | {metrics['build_time']:10.3f} | "
                  f"{metrics['search_time']:11.3f} | {metrics['recall']:6.3f} | "
                  f"{metrics['memory_usage']/1024/1024:10.2f}")

def main():
    print("Running JVector benchmark example...")
    benchmark_example()
    print("\nBenchmark completed successfully!")

if __name__ == "__main__":
    main()
