cmake_minimum_required(VERSION 3.16)

set(PROJECT webrtc_ns)
set(CMAKE_CXX_STANDARD 20)

project(${PROJECT} VERSION 0.1.0)

if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -fPIC -Wno-deprecated -m64 -fexceptions)
endif()

if(APPLE)
    add_definitions(-DWEBRTC_MAC -DWEBRTC_POSIX)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DWEBRTC_LINUX -DWEBRTC_POSIX)
elseif(WIN32)
    add_definitions(-DWEBRTC_WIN)
endif()

# Collect all source files
file(GLOB_RECURSE NS_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.h
)

# Exclude SIMD files on non-x86 platforms
if(NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64"))
    list(FILTER NS_SRC EXCLUDE REGEX ".*_avx2\\.cc$")
    list(FILTER NS_SRC EXCLUDE REGEX ".*_sse2\\.cc$")
    list(FILTER NS_SRC EXCLUDE REGEX ".*_sse\\.cc$")
endif()

# Exclude NEON files on non-ARM platforms
if(NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64"))
    list(FILTER NS_SRC EXCLUDE REGEX ".*_neon\\.cc$")
endif()

add_library(${PROJECT} SHARED ${NS_SRC})

target_include_directories(${PROJECT} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
    ${CMAKE_CURRENT_SOURCE_DIR}/src/base
    ${CMAKE_CURRENT_SOURCE_DIR}/src/base/rtc_base
    ${CMAKE_CURRENT_SOURCE_DIR}/src/base/system_wrappers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_processing
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_processing/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_processing/ns
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_processing/resampler
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common_audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common_audio/third_party/ooura/fft_size_256
    ${CMAKE_CURRENT_SOURCE_DIR}/src/absl
)

set_target_properties(${PROJECT} PROPERTIES LINKER_LANGUAGE CXX)

if(WIN32)
    # Export the public API from the DLL and import it in consumers.
    target_compile_definitions(${PROJECT}
        PRIVATE WEBRTC_ENABLE_SYMBOL_EXPORT WEBRTC_LIBRARY_IMPL)
    target_compile_definitions(${PROJECT} INTERFACE WEBRTC_ENABLE_SYMBOL_EXPORT)
endif()

option(BUILD_TESTS "Build the NS test executable" OFF)

if(BUILD_TESTS)
    add_executable(ns_test
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/ns_test.cc
    )

    target_link_libraries(ns_test PRIVATE ${PROJECT})

    target_include_directories(ns_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_processing
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_processing/logging
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_processing/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/base/rtc_base
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common_audio
    )
endif()
