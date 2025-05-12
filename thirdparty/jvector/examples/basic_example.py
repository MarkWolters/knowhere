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

def basic_jvector_example():
    """
    Basic example demonstrating JVector index creation, building, and searching.
    """
    # Generate sample data
    dim = 128
    nb = 10000  # database size
    nq = 10     # number of queries
    k = 5       # top-k

    # Generate random vectors for database and queries
    xb = np.random.rand(nb, dim).astype('float32')  # database vectors
    xq = np.random.rand(nq, dim).astype('float32')  # query vectors

    # Create JVector index
    index = knowhere.CreateIndex('JVECTOR')

    # Set index parameters
    index_params = {
        'dim': dim,
        'metric_type': 'L2',
        'M': 16,              # max connections per node
        'ef_construction': 128 # size of dynamic candidate list during construction
    }

    # Build the index
    index.Build(
        data=xb,
        params=index_params
    )

    # Set search parameters
    search_params = {
        'k': k,
        'ef_search': 64  # size of dynamic candidate list during search
    }

    # Perform search
    results = index.Search(
        data=xq,
        params=search_params
    )

    # Print results
    print(f"\nSearch Results (showing first query):")
    print(f"Top {k} distances:", results.distances[0])
    print(f"Top {k} indices:", results.ids[0])

def main():
    print("Running basic JVector example...")
    basic_jvector_example()
    print("\nExample completed successfully!")

if __name__ == "__main__":
    main()
