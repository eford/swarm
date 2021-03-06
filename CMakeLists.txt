# Project Settings
PROJECT(Swarm_NG)
SET(Swarm_NG_VERSION_MAJOR 0)
SET(Swarm_NG_VERSION_MINOR 2)

# CMake Settings
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
SET(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)

# Directory Structure 
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
SET(swarm_INCLUDE_DIRS 
	${CMAKE_CURRENT_SOURCE_DIR}/src 
	${CMAKE_CURRENT_BINARY_DIR}/src
	)
SET(SAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/samples) 
SET(TESTDIR ${CMAKE_CURRENT_SOURCE_DIR}/test) 



# All the cached variables and parameters that control
# the compilation go in this file
INCLUDE(variables.cmake)



# Find the libraries and packages
FIND_PACKAGE(CUDA REQUIRED)
FIND_PACKAGE(Boost REQUIRED COMPONENTS program_options regex)


# Macro to build other executables based on swarm library
MACRO(SWARM_ADD_EXECUTABLE target)
	ADD_EXECUTABLE(${target} ${ARGN})
	INCLUDE_DIRECTORIES(${swarm_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})
	TARGET_LINK_LIBRARIES(${target} swarmng ${Boost_LIBRARIES})
ENDMACRO(SWARM_ADD_EXECUTABLE target)

# Macro to build other libraries based on swarm library
MACRO(SWARM_ADD_LIBRARY target)
	ADD_LIBRARY(${target} ${ARGN})
	INCLUDE_DIRECTORIES(${swarm_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})
	TARGET_LINK_LIBRARIES(${target} swarmng ${Boost_LIBRARIES})
ENDMACRO(SWARM_ADD_LIBRARY target)

INCLUDE(doc.cmake)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)


# Include src
ADD_SUBDIRECTORY(src)



INCLUDE(test.cmake)
INCLUDE(custom.cmake)

