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
import tempfile
import os

def advanced_jvector_example():
    """
    Advanced example demonstrating JVector features including:
    - Different distance metrics
    - Serialization/Deserialization
    - BitsetView filtering
    - GetVectorByIds
    """
    # Generate sample data
    dim = 128
    nb = 10000  # database size
    nq = 10     # number of queries
    k = 5       # top-k

    # Generate random vectors
    xb = np.random.rand(nb, dim).astype('float32')
    xq = np.random.rand(nq, dim).astype('float32')

    # Example 1: Different Distance Metrics
    print("\nTesting different distance metrics...")
    metrics = ['L2', 'IP', 'COSINE']
    for metric in metrics:
        index = knowhere.CreateIndex('JVECTOR')
        index_params = {
            'dim': dim,
            'metric_type': metric,
            'M': 16,
            'ef_construction': 128
        }
        index.Build(data=xb, params=index_params)
        
        search_params = {'k': k, 'ef_search': 64}
        results = index.Search(data=xq, params=search_params)
        
        print(f"\n{metric} metric results (first query):")
        print(f"Distances: {results.distances[0]}")
        print(f"Indices: {results.ids[0]}")

    # Example 2: Serialization/Deserialization
    print("\nTesting serialization/deserialization...")
    index = knowhere.CreateIndex('JVECTOR')
    index_params = {
        'dim': dim,
        'metric_type': 'L2',
        'M': 16,
        'ef_construction': 128
    }
    index.Build(data=xb, params=index_params)

    # Save index to temporary file
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        index.Serialize(tmp.name)
        print(f"Index serialized to {tmp.name}")

        # Load index from file
        new_index = knowhere.CreateIndex('JVECTOR')
        new_index.Deserialize(tmp.name, index_params)
        print("Index deserialized successfully")

        # Verify search results are the same
        search_params = {'k': k, 'ef_search': 64}
        orig_results = index.Search(data=xq, params=search_params)
        new_results = new_index.Search(data=xq, params=search_params)
        
        np.testing.assert_array_almost_equal(
            orig_results.distances, new_results.distances, decimal=5)
        print("Search results verified after serialization")

    os.unlink(tmp.name)

    # Example 3: BitsetView Filtering
    print("\nTesting BitsetView filtering...")
    # Create a bitset that excludes even-numbered vectors
    bitset = np.zeros(nb, dtype=bool)
    bitset[1::2] = True  # Select odd-numbered vectors

    filtered_results = index.Search(
        data=xq,
        params=search_params,
        bitset=bitset
    )
    print("\nFiltered search results (first query):")
    print(f"Distances: {filtered_results.distances[0]}")
    print(f"Indices: {filtered_results.ids[0]}")
    # Verify all returned indices are odd numbers
    assert all(i % 2 == 1 for i in filtered_results.ids[0])
    print("Verified all returned indices are odd numbers")

    # Example 4: GetVectorByIds
    print("\nTesting GetVectorByIds...")
    # Get vectors for the first search result
    ids_to_get = filtered_results.ids[0]
    vectors = index.GetVectorByIds(ids_to_get)
    
    print(f"Retrieved {len(vectors)} vectors")
    print(f"Vector shape: {vectors.shape}")
    
    # Verify the retrieved vectors match the original data
    for i, idx in enumerate(ids_to_get):
        np.testing.assert_array_almost_equal(
            vectors[i], xb[idx], decimal=5)
    print("Retrieved vectors verified against original data")

def main():
    print("Running advanced JVector example...")
    advanced_jvector_example()
    print("\nExample completed successfully!")

if __name__ == "__main__":
    main()
