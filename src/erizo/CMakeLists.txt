cmake_minimum_required(VERSION 2.8)
#functions
function(test_lib LIB_NAME)
 if (${LIB_NAME} MATCHES "^.*-NOTFOUND")
  message(FATAL_ERROR "lib not found: " ${LIB_NAME} " check README")
  return()
 endif(${LIB_NAME} MATCHES "^.*-NOTFOUND")
endfunction(test_lib)
project (ERIZO)

set(ERIZO_VERSION_MAJOR 0)
set(ERIZO_VERSION_MINOR 1)

file(GLOB_RECURSE ERIZO_SOURCES ${ERIZO_SOURCE_DIR}/*.cpp ${ERIZO_SOURCE_DIR}/*.h)
add_library(erizo SHARED ${ERIZO_SOURCES})
find_package(GTK2 REQUIRED gtk)
# GTK2
if (GTK2_FOUND)
 include_directories(${GTK2_INCLUDE_DIRS})
 target_link_libraries(erizo ${GTK2_LIBRARIES})
endif(GTK2_FOUND)
# BOOST
set (BOOST_LIBS thread regex)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(erizo ${Boost_LIBRARIES})

#THE REST
find_library(SRTP srtp)
test_lib(${SRTP})
set (LIBS ${LIBS} ${SRTP})
find_library(NICE nice)
test_lib(${NICE})
set (LIBS ${LIBS} ${NICE})
find_library(GTHREAD gthread-2.0)
test_lib(${GTHREAD})
set (LIBS ${LIBS} ${GTHREAD})
find_library(AVCODEC avcodec)
set (LIBS ${LIBS} ${AVCODEC})
find_library(AVFORMAT avformat)
set (LIBS ${LIBS} ${AVFORMAT})
target_link_libraries(erizo ${LIBS})