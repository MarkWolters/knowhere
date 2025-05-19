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

echo "=== Compiling JNI Test ==="
g++ -std=c++17 -o jni_test jni_test.cpp \
    -I${JAVA_HOME}/include \
    -I${JAVA_HOME}/include/linux \
    -L${JAVA_HOME}/lib/server \
    -ljvm \
    -Wl,-rpath,${JAVA_HOME}/lib/server

echo "=== Running JNI Test ==="
./jni_test

echo "=== JNI Test Completed ==="
