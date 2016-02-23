cmake_minimum_required(VERSION 2.8.8)
project(llvmtest)

include(CheckCXXCompilerFlag)

message("CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti")

# if (NOT MSVC)
# 	if(LIBCXX_HAS_STDCXX11_FLAG)
# 		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
# 	else()
# 		CHECK_CXX_COMPILER_FLAG(-std=c++0x LIBCXX_HAS_STDCXX0X_FLAG)
# 		if(LIBCXX_HAS_STDCXX0X_FLAG)
# 			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fno-rtti")
# 		else()
# 			if(NOT CMAKE_CXX_COMPILER MATCHES "icc")
# 				message(FATAL "need at least gcc 4.4.7 or clang 3.2")
# 			endif(NOT CMAKE_CXX_COMPILER MATCHES "icc")
# 		endif()
# 	endif(LIBCXX_HAS_STDCXX11_FLAG)
# 	# endif() # if(NOT CMAKE_CXX_COMPILER MATCHES "icc")
# endif(NOT MSVC)

set(_LLVM_CMAKE_DIR "/usr/local/share/llvm/cmake")
set(_LLVM_LIBRARY_DIR "/usr/local/lib")
#set()

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++11 support
# for your compiler.

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

#message(STATUS "LLVM_CMAKE_DIR = ${LLVM_CMAKE_DIR}")
#message(STATUS "LLVM_INCLUDE_DIRS = ${LLVM_INCLUDE_DIRS}")
#message(STATUS "LLVM_DEFINITIONS = ${LLVM_DEFINITIONS}")

#SET(CMAKE_EXE_LINKER_FLAGS "-L/usr/local/lib")

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

BISON_TARGET(QBParse parser.ypp ${CMAKE_CURRENT_SOURCE_DIR}/parser.cpp)
FLEX_TARGET(QBLex qblex.ll ${CMAKE_CURRENT_SOURCE_DIR}/qblex.cpp)
ADD_FLEX_BISON_DEPENDENCY(QBLex QBParse)

# Now build our tools
add_executable(llvmtest  ${BISON_QBParse_OUTPUTS} ${FLEX_QBLex_OUTPUTS} llvmwrapper.cpp ast.cpp type.cpp codegen.cpp operator.cpp main.cpp)

#add_executable(llvmtest  main.cpp)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs core executionengine interpreter mc mcjit support nativecodegen X86AsmParser)

# Link against LLVM libraries
message(STATUS "Using LLVM libs: ${llvm_libs}")

SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} ${LLVM_CPPFLAGS} ")
#message(STATUS "LLVM_CPPFLAGS = ${LLVM_CPPFLAGS}")

# Link against LLVM libraries
target_link_libraries(llvmtest ${llvm_libs})

#message(STATUS "CMAKE_OSX_ARCHITECTURES = ${CMAKE_OSX_ARCHITECTURES}")

