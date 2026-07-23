/**************************************************************************/
/*  native_godot_module_jni.cpp                                           */
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

#include "native_godot_module_jni.h"
#include <NativeGodotModule.h>

#define LOG_TAG "NativeGodotModuleJNI"
#include "godot-log.h"

jni::local_ref<NativeGodotModuleJNI::jhybriddata> NativeGodotModuleJNI::initHybrid(
		jni::alias_ref<jhybridobject> jThis,
		jlong jsContext,
		jni::alias_ref<facebook::react::CallInvokerHolder::javaobject>
				jsCallInvokerHolder) {
	auto jsCallInvoker = jsCallInvokerHolder->cthis()->getCallInvoker();
	return makeCxxInstance(
			jThis,
			(jsi::Runtime *)jsContext,
			jsCallInvoker);
}

void NativeGodotModuleJNI::registerNatives() {
	registerHybrid({
			makeNativeMethod("initHybrid", NativeGodotModuleJNI::initHybrid),
			makeNativeMethod("installTurboModule", NativeGodotModuleJNI::installTurboModule),
	});
}

bool NativeGodotModuleJNI::installTurboModule() {
	// A zero JavaScript context must never be reported as a successful install:
	// doing so leaves global.RTNGodot undefined and merely moves the crash to
	// the first API call. Fail cleanly instead of dereferencing a null runtime.
	if (rnRuntime_ == nullptr) {
		LOGE("JavaScript runtime is unavailable; cannot install NativeGodotModule.");
		return false;
	}

	jsi::Runtime &rnRuntime = *rnRuntime_;
	jsi::Value godotModule = createNativeGodotModule(rnRuntime, callInvoker_);
	if (!godotModule.isObject()) {
		LOGE("Could not install NativeGodotModule.");
		return false;
	}
	return true;
}

NativeGodotModuleJNI::NativeGodotModuleJNI(
		jni::alias_ref<NativeGodotModuleJNI::jhybridobject> jThis,
		jsi::Runtime *rnRuntime,
		const std::shared_ptr<facebook::react::CallInvoker> &jsCallInvoker) :
		javaPart_(jni::make_global(jThis)),
		rnRuntime_(rnRuntime),
		callInvoker_(jsCallInvoker) {}
