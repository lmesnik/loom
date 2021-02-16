# CMakeLists files to generate lib JDWP project
cmake_minimum_required (VERSION 3.4.1)
set(CMAKE_CXX_STANDARD 11)

project (jdwp LANGUAGES C)

add_compile_definitions(JDWP_LOGGING)

find_library(iconv_lib iconv)

set(JDWP_SRC_DIR .)
set(BUILD_DIR ${JDWP_SRC_DIR}/../../build/macosx-x86_64-server-fastdebug)

file(GLOB_RECURSE SOURCES ${JDWP_SRC_DIR}/share/native/libjdwp/*.c ${JDWP_SRC_DIR}/unix/native/libjdwp/*.c)
add_library(jdwp SHARED ${SOURCES})

target_link_libraries(jdwp libiconv.a)

target_include_directories(jdwp PUBLIC ${JDWP_SRC_DIR}/share/native/libjdwp)
target_include_directories(jdwp PUBLIC ${JDWP_SRC_DIR}/unix/native/libjdwp)
target_include_directories(jdwp PUBLIC ${JDWP_SRC_DIR}/share/native/libjdwp/export)

target_include_directories(jdwp PUBLIC ${JDWP_SRC_DIR}/../hotspot/os/posix/include)
target_include_directories(jdwp PUBLIC ${JDWP_SRC_DIR}/../hotspot/share/include)

target_include_directories(jdwp PUBLIC ${JDWP_SRC_DIR}/../java.base/share/native/libjava)
target_include_directories(jdwp PUBLIC ${JDWP_SRC_DIR}/../java.base/unix/native/libjava)

target_include_directories(jdwp PUBLIC ${BUILD_DIR}/images/jdk/include)
target_include_directories(jdwp PUBLIC ${BUILD_DIR}/images/jdk/include/darwin)
target_include_directories(jdwp PUBLIC ${BUILD_DIR}/support/headers/jdk.jdwp.agent)

