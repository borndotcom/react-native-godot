/**************************************************************************/
/*  native_godot_module_jni.h                                             */
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

#include <ReactCommon/CallInvokerHolder.h>
#include <fbjni/fbjni.h>
#include <jsi/jsi.h>

using namespace facebook;

class NativeGodotModuleJNI : public jni::HybridClass<NativeGodotModuleJNI> {
public:
	static auto constexpr kJavaDescriptor =
			"Lcom/rtngodot/NativeGodotModule;";
	static jni::local_ref<jhybriddata> initHybrid(
			jni::alias_ref<jhybridobject> jThis,
			jlong jsContext,
			jni::alias_ref<facebook::react::CallInvokerHolder::javaobject>
					jsCallInvokerHolder);

	static void registerNatives();

	~NativeGodotModuleJNI() {}

private:
	friend HybridBase;
	jni::global_ref<NativeGodotModuleJNI::javaobject> javaPart_;
	jsi::Runtime *rnRuntime_;
	std::shared_ptr<facebook::react::CallInvoker> callInvoker_;
	bool installTurboModule();

	explicit NativeGodotModuleJNI(
			jni::alias_ref<NativeGodotModuleJNI::jhybridobject> jThis,
			jsi::Runtime *rnRuntime,
			const std::shared_ptr<facebook::react::CallInvoker> &jsCallInvoker);
};
