/**************************************************************************/
/*  OnLoad.cpp                                                            */
/**************************************************************************/
/* Copyright (c) 2024-2025 Slay GmbH                                      */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// This C++ file is part of the default configuration used by apps and is placed
// inside react-native to encapsulate it from user space (so you won't need to
// touch C++/Cmake code at all on Android).
//
// If you wish to customize it (because you want to manually link a C++ library
// or pass a custom compilation flag) you can:
//
// 1. Copy this CMake file inside the `android/app/src/main/jni` folder of your
// project
// 2. Copy the OnLoad.cpp (in this same folder) file inside the same folder as
// above.
// 3. Extend your `android/app/build.gradle` as follows
//
// android {
//    // Other config here...
//    externalNativeBuild {
//        cmake {
//            path "src/main/jni/CMakeLists.txt"
//        }
//    }
// }

#include "native_godot_module_jni.h"
#include "libgodot_jni.h"
#include <fbjni/fbjni.h>

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
	// LibGodot threads can request a JNIEnv before the engine-level initialize
	// callback runs, so retain the VM as soon as this shared library is loaded.
	LibGodot::set_java_vm(vm);
	return facebook::jni::initialize(vm, [] {
		NativeGodotModuleJNI::registerNatives();
	});
}
