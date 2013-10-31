project(charls)
cmake_minimum_required(VERSION 2.6)


# When user specify build type do not override settings:
IF(NOT CMAKE_BUILD_TYPE)
# The following compiler option are only meant for GCC:
IF(CMAKE_COMPILER_IS_GNUCC)
# the following is optimization micromanagement that made better code for x86
# SET(CMAKE_CXX_FLAGS "-D NDEBUG -O3 -Wall -Wextra -pedantic -fvisibility=hidden -fomit-frame-pointer -momit-leaf-frame-pointer -fweb  -ftracer" )
SET(CMAKE_CXX_FLAGS "-D NDEBUG -O3" )
ENDIF(CMAKE_COMPILER_IS_GNUCC)
ENDIF(NOT CMAKE_BUILD_TYPE)

OPTION(charls_BUILD_SHARED_LIBS "Build CharLS with shared libraries." OFF)
SET(BUILD_SHARED_LIBS ${charls_BUILD_SHARED_LIBS})

if(WIN32)
   if(BUILD_SHARED_LIBS)
     add_definitions(-D CHARLS_DLL)
   else(BUILD_SHARED_LIBS)
     add_definitions(-D CHARLS_STATIC)
   endif(BUILD_SHARED_LIBS)
endif(WIN32)

add_library(CharLS header.cpp  interface.cpp  jpegls.cpp  )

add_executable(charlstest test/main.cpp test/time.cpp test/util.cpp test/bitstreamdamage.cpp test/compliance.cpp test/performance.cpp test/dicomsamples.cpp)
target_link_libraries (charlstest CharLS)


# First thing define the common source:
set(CharLS_JNI_SRCS
  charls_jni.cpp
  )

# JNI binding:
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})


add_library(CharLS_JNI MODULE  ${CharLS_JNI_SRCS} )

# Java module should not have a SONAME:
set_property(TARGET CharLS_JNI PROPERTY NO_SONAME 1)

target_link_libraries(CharLS_JNI CharLS)

if(UNIX)
  target_link_libraries(CharLS_JNI m)
endif()
