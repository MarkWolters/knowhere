# Add JVector support
knowhere_option(WITH_JVECTOR "Build with JVector index" ON)

if(WITH_JVECTOR)
    message(STATUS "Building with JVector support")
    add_definitions(-DKNOWHERE_WITH_JVECTOR)

    # Find Java
    find_package(Java REQUIRED)
    
    # Set JNI paths for macOS
    set(JAVA_HOME "/Library/Java/JavaVirtualMachines/jdk-23.jdk/Contents/Home")
    set(JAVA_INCLUDE_PATH "${JAVA_HOME}/include")
    set(JAVA_INCLUDE_PATH2 "${JAVA_HOME}/include/darwin")
    set(JAVA_AWT_INCLUDE_PATH "${JAVA_HOME}/include")
    
    find_package(JNI REQUIRED)
    include(UseJava)

    # Add JVector source files
    set(KNOWHERE_JVECTOR_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/index/jvector/JVectorIndex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/index/jvector/impl/JVectorIndexWrapper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/index/jvector/register.cpp
    )

    # Add the source files to the main KNOWHERE_SRCS variable
    list(APPEND KNOWHERE_SRCS ${KNOWHERE_JVECTOR_SRCS})

    # Set up JVector build directory
    set(JVECTOR_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/thirdparty/jvector)
    set(JVECTOR_BUILD_DIR ${JVECTOR_SOURCE_DIR}/target)
    set(JVECTOR_JAR ${JVECTOR_BUILD_DIR}/jvector-1.0-SNAPSHOT.jar)

    # Create thirdparty directory
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/thirdparty)

    # Clone JVector if not exists
    if(NOT EXISTS ${JVECTOR_SOURCE_DIR})
        execute_process(
            COMMAND git clone https://github.com/jbellis/jvector.git ${JVECTOR_SOURCE_DIR}
            RESULT_VARIABLE GIT_RESULT
        )
        if(NOT GIT_RESULT EQUAL "0")
            message(FATAL_ERROR "Failed to clone JVector repository")
        endif()
    endif()

    # Build JVector using Maven
    execute_process(
        COMMAND mvn clean package -DskipTests
        WORKING_DIRECTORY ${JVECTOR_SOURCE_DIR}
        RESULT_VARIABLE MVN_RESULT
    )
    if(NOT MVN_RESULT EQUAL "0")
        message(FATAL_ERROR "Failed to build JVector with Maven")
    endif()

    # Create lib directory if it doesn't exist
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

    # Copy JVector JAR to lib directory
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lib/jvector-1.0-SNAPSHOT.jar
        COMMAND ${CMAKE_COMMAND} -E copy
                ${JVECTOR_JAR}
                ${CMAKE_CURRENT_BINARY_DIR}/lib/jvector-1.0-SNAPSHOT.jar
        DEPENDS ${JVECTOR_JAR}
        COMMENT "Copying JVector JAR to lib directory"
    )

    # Generate JNI headers
    set(JVECTOR_CLASSES
        io.github.jbellis.jvector-base.graph.GraphIndex
        io.github.jbellis.jvector-base.graph.OnDiskGraphIndex
    )

    # Create include directory
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

    # Get Maven dependencies
    execute_process(
        COMMAND mvn dependency:build-classpath -Dmdep.outputFile=${CMAKE_BINARY_DIR}/classpath.txt
        WORKING_DIRECTORY ${JVECTOR_SOURCE_DIR}/jvector-base
        RESULT_VARIABLE MVN_RESULT
        ERROR_VARIABLE MVN_ERROR
    )
    if(NOT MVN_RESULT EQUAL "0")
        message(FATAL_ERROR "Failed to get Maven dependencies: ${MVN_ERROR}")
    endif()

    # Read the classpath file
    file(READ ${CMAKE_BINARY_DIR}/classpath.txt MAVEN_CLASSPATH)
    string(STRIP "${MAVEN_CLASSPATH}" MAVEN_CLASSPATH)

    # Generate JNI headers using javac with Maven dependencies
    execute_process(
        COMMAND ${Java_JAVAC_EXECUTABLE} 
                -h ${CMAKE_BINARY_DIR}/include
                -cp "${JVECTOR_SOURCE_DIR}/jvector-base/target/classes:${MAVEN_CLASSPATH}"
                ${JVECTOR_SOURCE_DIR}/jvector-base/src/main/java/io/github/jbellis/jvector/graph/GraphIndex.java
                ${JVECTOR_SOURCE_DIR}/jvector-base/src/main/java/io/github/jbellis/jvector/graph/disk/OnDiskGraphIndex.java
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        RESULT_VARIABLE JAVAC_RESULT
        ERROR_VARIABLE JAVAC_ERROR
        OUTPUT_VARIABLE JAVAC_OUTPUT
    )
    message(STATUS "Javac output: ${JAVAC_OUTPUT}")
    if(NOT JAVAC_RESULT EQUAL "0")
        message(FATAL_ERROR "Failed to generate JNI headers: ${JAVAC_ERROR}")
    endif()

    # Add include directories
    include_directories(${JNI_INCLUDE_DIRS})
    include_directories(${CMAKE_BINARY_DIR}/include)
    include_directories(${JVECTOR_SOURCE_DIR}/src/main/java)

    # Link against JNI
    list(APPEND KNOWHERE_LINKER_LIBS ${JNI_LIBRARIES})
endif()
