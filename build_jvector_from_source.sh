#!/bin/bash
set -e

# Set environment variables
export JAVA_HOME=/usr/lib/jvm/jdk-23.0.2-oracle-x64
export PATH=$PATH:$HOME/.local/bin:$JAVA_HOME/bin
export LD_LIBRARY_PATH=$JAVA_HOME/lib:$JAVA_HOME/lib/server:$LD_LIBRARY_PATH

# Create directories
mkdir -p thirdparty/jvector_src
mkdir -p thirdparty/jvector/lib

# Clean up any existing JAR files to avoid confusion
echo "=== Cleaning up existing JAR files ==="
rm -f thirdparty/jvector/lib/*.jar

# Clone the JVector repository
echo "=== Cloning JVector repository ==="
if [ ! -d "thirdparty/jvector_src/.git" ]; then
    git clone https://github.com/datastax/jvector.git thirdparty/jvector_src
    cd thirdparty/jvector_src
else
    cd thirdparty/jvector_src
    git fetch
fi

# Checkout the latest release tag
echo "=== Checking out latest release ==="
git checkout 4.0.0-beta.4

# Build JVector with Maven
echo "=== Building JVector with Maven ==="
mvn clean package -DskipTests

# Copy the JAR file to the lib directory
echo "=== Copying JAR file to lib directory ==="
find ./jvector-multirelease/target -name "jvector-*.jar" -not -name "*-sources.jar" -not -name "*-javadoc.jar" -exec cp {} ../jvector/lib/ \;

# Download dependencies for the test
echo "=== Downloading dependencies ==="
mkdir -p ../jvector/deps
cd ../jvector/deps

# SLF4J dependency
if [ ! -f "slf4j-api-2.0.9.jar" ]; then
    curl -O https://repo1.maven.org/maven2/org/slf4j/slf4j-api/2.0.9/slf4j-api-2.0.9.jar
fi
if [ ! -f "slf4j-simple-2.0.9.jar" ]; then
    curl -O https://repo1.maven.org/maven2/org/slf4j/slf4j-simple/2.0.9/slf4j-simple-2.0.9.jar
fi

# Agrona dependency
if [ ! -f "agrona-1.19.0.jar" ]; then
    curl -O https://repo1.maven.org/maven2/org/agrona/agrona/1.19.0/agrona-1.19.0.jar
fi

# Commons Math dependency
if [ ! -f "commons-math3-3.6.1.jar" ]; then
    curl -O https://repo1.maven.org/maven2/org/apache/commons/commons-math3/3.6.1/commons-math3-3.6.1.jar
fi

# Verify the JAR file
cd ../lib
EXPECTED_JAR="jvector-4.0.0-beta.4.jar"
if [ ! -f "$EXPECTED_JAR" ]; then
    echo "Error: Expected JAR file $EXPECTED_JAR not found!"
    ls -la
    exit 1
fi

echo "Found JVector JAR: $EXPECTED_JAR"

# Create a simple Java test program to verify the JAR
cd ../../..
mkdir -p java_test
cat > java_test/JVectorTest.java << EOF
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
EOF

# Compile and run the Java test
DEPS_CLASSPATH=""
for jar in thirdparty/jvector/deps/*.jar; do
    DEPS_CLASSPATH="$DEPS_CLASSPATH:$jar"
done

echo "=== Compiling Java test ==="
javac -cp "thirdparty/jvector/lib/$EXPECTED_JAR$DEPS_CLASSPATH" java_test/JVectorTest.java

echo "=== Running Java test ==="
java -cp "thirdparty/jvector/lib/$EXPECTED_JAR$DEPS_CLASSPATH:java_test" JVectorTest

echo "=== JVector build and verification completed! ==="
echo "JVector version: 4.0.0-beta.4"
echo "JAR file: thirdparty/jvector/lib/$EXPECTED_JAR"
