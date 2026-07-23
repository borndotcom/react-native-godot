project(rtngodot)

cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE on)

set (CMAKE_CXX_STANDARD 20)

include("${REACT_NATIVE_DIR}/ReactAndroid/cmake-utils/folly-flags.cmake")
add_compile_options(${folly_FLAGS})

include("${REACT_NATIVE_DIR}/ReactCommon/cmake-utils/react-native-flags.cmake")

string(APPEND CMAKE_CXX_FLAGS " -fexceptions -fno-omit-frame-pointer -frtti -fstack-protector-all -std=c++${CMAKE_CXX_STANDARD} -Wall")

if(${IS_NEW_ARCHITECTURE_ENABLED})
    string(APPEND CMAKE_CXX_FLAGS " -DRCT_NEW_ARCH_ENABLED")
endif()

if(NOT ${CMAKE_BUILD_TYPE} MATCHES "Debug")
    string(APPEND CMAKE_CXX_FLAGS " -DNDEBUG")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefab packages from React Native
find_package(fbjni REQUIRED CONFIG)
add_library(fbjni ALIAS fbjni::fbjni)
find_package(ReactAndroid REQUIRED CONFIG)
add_library(jsi ALIAS ReactAndroid::jsi)
add_library(reactnative ALIAS ReactAndroid::reactnative)
find_package(react-native-worklets REQUIRED CONFIG)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../build/generated/source/codegen/jni" generated)
set(LIBGODOT_CPP_VERSION ${GODOT_CPP_VERSION})
set(LIBGODOT_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/libgodot-cpp-android/${LIBGODOT_CPP_VERSION}/godot-cpp-android)

add_library(
        godot-cpp
        STATIC
        IMPORTED
)
set_target_properties(godot-cpp PROPERTIES IMPORTED_LOCATION ${LIBGODOT_CPP_DIR}/${ANDROID_ABI}/libgodot-cpp.a)
target_include_directories(godot-cpp INTERFACE ${LIBGODOT_CPP_DIR}/include)

file(GLOB rtngodot_SRC CONFIGURE_DEPENDS *.cpp)
file(GLOB rtngodot_common_SRC CONFIGURE_DEPENDS ../../../../common/*.cpp)
add_library(rtngodot SHARED ${rtngodot_SRC} ${rtngodot_common_SRC})

# Android 15+ devices may use 16 KiB memory pages. Keep this module loadable
# there instead of relying on the OS page-size compatibility mode.
target_link_options(rtngodot PRIVATE "-Wl,-z,max-page-size=16384")

target_include_directories(rtngodot PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "../../../../common"
)

target_link_libraries(
        rtngodot
        fbjni::fbjni
        ReactAndroid::jsi
        ReactAndroid::reactnative
        react-native-worklets::worklets
        react_codegen_RTNGodotSpec
)

target_link_libraries(rtngodot
        log
        godot-cpp
        OpenSLES
        android
        z
        dl
)
