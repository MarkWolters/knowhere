import io.github.jbellis.jvector.graph.GraphIndexBuilder;
import io.github.jbellis.jvector.graph.ListRandomAccessVectorValues;
import io.github.jbellis.jvector.graph.RandomAccessVectorValues;
import io.github.jbellis.jvector.vector.VectorSimilarityFunction;
import io.github.jbellis.jvector.vector.VectorizationProvider;
import io.github.jbellis.jvector.vector.types.VectorFloat;
import io.github.jbellis.jvector.vector.types.VectorTypeSupport;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class JVectorTest {
    public static void main(String[] args) {
        System.out.println("Testing JVector library...");
        
        try {
            // Get vector type support
            VectorTypeSupport vts = VectorizationProvider.getInstance().getVectorTypeSupport();
            
            // Create some test vectors
            int dimension = 128;
            int numVectors = 100;
            List<VectorFloat<?>> vectors = new ArrayList<>();
            
            Random random = new Random(42); // Fixed seed for reproducibility
            
            // Create random vectors
            for (int i = 0; i < numVectors; i++) {
                float[] values = new float[dimension];
                for (int j = 0; j < dimension; j++) {
                    values[j] = random.nextFloat();
                }
                vectors.add(vts.createFloatVector(values));
            }
            
            System.out.println("Created " + numVectors + " random vectors with dimension " + dimension);
            
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
            }
            
            // Clean up the graph
            builder.cleanup();
            
            System.out.println("Graph built successfully with " + builder.getGraph().size() + " nodes");
            System.out.println("JVector test completed successfully!");
            
        } catch (Exception e) {
            System.err.println("Error testing JVector: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
