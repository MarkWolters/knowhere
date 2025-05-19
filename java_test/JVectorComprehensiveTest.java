import io.github.jbellis.jvector.graph.GraphIndexBuilder;
import io.github.jbellis.jvector.graph.GraphSearcher;
import io.github.jbellis.jvector.graph.ListRandomAccessVectorValues;
import io.github.jbellis.jvector.graph.RandomAccessVectorValues;
import io.github.jbellis.jvector.graph.SearchResult;
import io.github.jbellis.jvector.graph.similarity.ScoreFunction;
import io.github.jbellis.jvector.graph.similarity.SearchScoreProvider;
import io.github.jbellis.jvector.util.Bits;
import io.github.jbellis.jvector.vector.VectorSimilarityFunction;
import io.github.jbellis.jvector.vector.VectorizationProvider;
import io.github.jbellis.jvector.vector.types.VectorFloat;
import io.github.jbellis.jvector.vector.types.VectorTypeSupport;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 * A comprehensive test for JVector that demonstrates:
 * 1. Building a graph index
 * 2. Adding vectors to the graph
 * 3. Searching for nearest neighbors
 */
public class JVectorComprehensiveTest {
    public static void main(String[] args) {
        System.out.println("Starting comprehensive JVector test...");
        
        try {
            // Get vector type support
            VectorTypeSupport vts = VectorizationProvider.getInstance().getVectorTypeSupport();
            
            // Create test vectors
            int dimension = 128;
            int numVectors = 1000;
            List<VectorFloat<?>> vectors = new ArrayList<>();
            
            Random random = new Random(42); // Fixed seed for reproducibility
            
            System.out.println("Creating " + numVectors + " random vectors with dimension " + dimension);
            
            // Create random vectors
            for (int i = 0; i < numVectors; i++) {
                float[] values = new float[dimension];
                for (int j = 0; j < dimension; j++) {
                    values[j] = random.nextFloat();
                }
                vectors.add(vts.createFloatVector(values));
            }
            
            // Create a RandomAccessVectorValues from the vectors
            RandomAccessVectorValues ravv = new ListRandomAccessVectorValues(vectors, dimension);
            
            // Create a GraphIndexBuilder
            System.out.println("Creating GraphIndexBuilder...");
            GraphIndexBuilder builder = new GraphIndexBuilder(
                ravv,                           // Vector values
                VectorSimilarityFunction.COSINE, // Similarity function
                16,                             // Max connections per node
                100,                            // Beam width
                1.2f,                           // Alpha for diversity
                1.2f,                           // Beta for diversity
                true                            // Use hierarchical graph
            );
            
            // Build the graph
            System.out.println("Building graph...");
            for (int i = 0; i < numVectors; i++) {
                builder.addGraphNode(i, vectors.get(i));
                if (i % 100 == 0 && i > 0) {
                    System.out.println("Added " + i + " vectors to the graph");
                }
            }
            
            // Clean up the graph
            builder.cleanup();
            
            System.out.println("Graph built successfully with " + builder.getGraph().size() + " nodes");
            
            // Create a query vector
            System.out.println("Creating query vector and searching for nearest neighbors...");
            float[] queryValues = new float[dimension];
            for (int i = 0; i < dimension; i++) {
                queryValues[i] = random.nextFloat();
            }
            VectorFloat<?> queryVector = vts.createFloatVector(queryValues);
            
            // Create a score function for the query vector
            final VectorFloat<?> finalQueryVector = queryVector;
            ScoreFunction.ExactScoreFunction scoreFunction = node -> {
                VectorFloat<?> v = vectors.get(node);
                return VectorSimilarityFunction.COSINE.compare(finalQueryVector, v);
            };
            
            // Create a search score provider
            SearchScoreProvider searchScoreProvider = new SearchScoreProvider(scoreFunction, null);
            
            // Search for nearest neighbors
            try (GraphSearcher searcher = new GraphSearcher(builder.getGraph())) {
                int k = 10; // Number of nearest neighbors to find
                SearchResult result = searcher.search(searchScoreProvider, k, Bits.ALL);
                
                System.out.println("\nTop " + k + " nearest neighbors:");
                SearchResult.NodeScore[] nodeScores = result.getNodes();
                for (int i = 0; i < Math.min(k, nodeScores.length); i++) {
                    SearchResult.NodeScore nodeScore = nodeScores[i];
                    System.out.printf("Node %d: Score = %.6f%n", nodeScore.node, nodeScore.score);
                }
                
                System.out.println("\nSearch statistics:");
                System.out.println("Visited nodes: " + result.getVisitedCount());
                System.out.println("Expanded nodes: " + result.getExpandedCount());
                System.out.println("Expanded nodes in base layer: " + result.getExpandedCountBaseLayer());
            }
            
            System.out.println("\nJVector comprehensive test completed successfully!");
            
        } catch (IOException e) {
            System.err.println("Error during JVector test: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
