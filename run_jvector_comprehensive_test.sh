#!/bin/bash
set -e

# Set environment variables
export JAVA_HOME=/usr/lib/jvm/jdk-23.0.2-oracle-x64
export PATH=$PATH:$HOME/.local/bin:$JAVA_HOME/bin
export LD_LIBRARY_PATH=$JAVA_HOME/lib:$JAVA_HOME/lib/server:$LD_LIBRARY_PATH

# Check if JVector JAR exists
EXPECTED_JAR="thirdparty/jvector/lib/jvector-4.0.0-beta.4.jar"
if [ ! -f "$EXPECTED_JAR" ]; then
    echo "Error: JVector JAR file not found at $EXPECTED_JAR"
    echo "Please run build_jvector_from_source.sh first."
    exit 1
fi

# Find all dependency JARs
DEPS_CLASSPATH=""
for jar in thirdparty/jvector/deps/*.jar; do
    DEPS_CLASSPATH="$DEPS_CLASSPATH:$jar"
done

# Compile the comprehensive test
echo "=== Compiling JVector comprehensive test ==="
javac -cp "$EXPECTED_JAR$DEPS_CLASSPATH" java_test/JVectorComprehensiveTest.java

# Run the comprehensive test
echo "=== Running JVector comprehensive test ==="
java -cp "$EXPECTED_JAR$DEPS_CLASSPATH:java_test" JVectorComprehensiveTest

echo "=== JVector comprehensive test completed! ==="
