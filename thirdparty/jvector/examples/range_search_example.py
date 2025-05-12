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

def range_search_example():
    """
    Example demonstrating JVector range search functionality.
    """
    # Generate sample data
    dim = 128
    nb = 10000  # database size
    nq = 10     # number of queries

    # Generate random vectors for database and queries
    xb = np.random.rand(nb, dim).astype('float32')  # database vectors
    xq = np.random.rand(nq, dim).astype('float32')  # query vectors

    # Create JVector index
    index = knowhere.CreateIndex('JVECTOR')

    # Set index parameters
    index_params = {
        'dim': dim,
        'metric_type': 'L2',
        'M': 16,
        'ef_construction': 128
    }

    # Build the index
    index.Build(
        data=xb,
        params=index_params
    )

    # Set range search parameters
    search_params = {
        'radius': 0.5,      # distance threshold
        'ef_search': 64     # size of dynamic candidate list during search
    }

    # Perform range search
    results = index.RangeSearch(
        data=xq,
        params=search_params
    )

    # Print results for first query
    print(f"\nRange Search Results (showing first query):")
    print(f"Found {len(results.distances[0])} neighbors within radius {search_params['radius']}")
    print("Distances:", results.distances[0])
    print("Indices:", results.ids[0])

    # Try different radius values
    for radius in [0.3, 0.7, 1.0]:
        search_params['radius'] = radius
        results = index.RangeSearch(
            data=xq[:1],  # just use first query
            params=search_params
        )
        print(f"\nResults for radius {radius}:")
        print(f"Found {len(results.distances[0])} neighbors")

def main():
    print("Running JVector range search example...")
    range_search_example()
    print("\nExample completed successfully!")

if __name__ == "__main__":
    main()
