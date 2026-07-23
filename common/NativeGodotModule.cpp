/**************************************************************************/
/*  NativeGodotModule.cpp                                                 */
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

#define LOG_TAG "NativeGodotModule"
#include "NativeGodotModule.h"
#include "godot-log.h"

#include "GodotModule.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <condition_variable>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#ifdef ON_ANDROID
#include <fbjni/fbjni.h>
#endif

#include <worklets/RunLoop/AsyncQueue.h>
#include <worklets/WorkletRuntime/WorkletRuntime.h>

#define NATIVE_GODOT_MODULE_PROPERTY "RTNGodot"

class GodotAsyncQueue : public worklets::AsyncQueue {
public:
	void push(std::function<void()> &&job) override {
		GodotModule::get_singleton()->runOnGodotThread(std::move(job));
	}
};

class GodotWorkletContext : public std::enable_shared_from_this<GodotWorkletContext> {
	jsi::Runtime *_jsRuntime;
	std::shared_ptr<facebook::react::CallInvoker> _jsCallInvoker;

public:
	GodotWorkletContext(jsi::Runtime *jsRuntime, std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker) :
			_jsRuntime(jsRuntime), _jsCallInvoker(std::move(jsCallInvoker)) {}

	jsi::Runtime *getJsRuntime() const {
		return _jsRuntime;
	}

	void invokeOnJsThread(std::function<void(jsi::Runtime &)> &&f) const {
		_jsCallInvoker->invokeAsync([weakSelf = weak_from_this(), f = std::move(f)]() mutable {
			auto self = weakSelf.lock();
			if (self) {
				f(*self->_jsRuntime);
			}
		});
	}
};

static std::string create_method_call_error_string(std::string methodName, GDExtensionCallError error) {
	std::string ret = "Method call error name: " + methodName + " ";
	switch (error.error) {
		case GDEXTENSION_CALL_ERROR_INVALID_METHOD:
			ret += "Invalid method";
			break;
		case GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT:
			ret += "Invalid argument";
			break;
		case GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL:
			ret += "Instance is null";
			break;
		case GDEXTENSION_CALL_ERROR_METHOD_NOT_CONST:
			ret += "Method not const";
			break;
		case GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS:
			ret += "Too few arguments (Expected: " + std::to_string(error.expected) + ", Actual: " + std::to_string(error.argument) + ")";
			break;
		case GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS:
			ret += "Too many arguments (Expected: " + std::to_string(error.expected) + ", Actual: " + std::to_string(error.argument) + ")";
			break;
		case GDEXTENSION_CALL_OK:
			ret += "Call OK (Should never happen)";
			break;
		default:
			ret += "Unknown Error";
	}
	return ret;
}

static std::vector<const godot::Variant *> createVariantArgArray(const std::vector<godot::Variant> &args) {
	std::vector<const godot::Variant *> ret;
	ret.reserve(args.size());
	for (size_t i = 0; i < args.size(); ++i) {
		ret.push_back(&args[i]);
	}
	return ret;
}

static const char *JAVASCRIPT_CALLABLE_NAME = "JavascriptCallable";
class JavascriptCallable : public godot::CallableCustom {
	std::weak_ptr<GodotWorkletContext> _workletContext;
	std::weak_ptr<worklets::WorkletRuntime> _workletRuntime;
	bool _isWorklet;
	jsi::Value _funcValue;

	static bool runInContext(const JavascriptCallable *c,
			std::function<bool(const JavascriptCallable *, jsi::Runtime &)> func) {
		std::shared_ptr<GodotWorkletContext> wc = c->_workletContext.lock();
		if (!wc) {
			LOGE("WorkletContext is invalid");
			return false;
		}
		{
			std::mutex mtx;
			std::condition_variable cv;
			bool done = false;
			bool success = false;
			auto execute = [&mtx, &cv, &done, &success, &func, c](jsi::Runtime &rt) {
				try {
					success = func(c, rt);
				} catch (const std::exception &exc) {
					LOGE("JavaScript callback failed: %s", exc.what());
				} catch (...) {
					LOGE("JavaScript callback failed with an unknown error");
				}
				{
					std::lock_guard<std::mutex> lock(mtx);
					done = true;
				}
				cv.notify_one();
			};
			if (c->_isWorklet) {
				std::shared_ptr<worklets::WorkletRuntime> runtime = c->_workletRuntime.lock();
				if (!runtime) {
					LOGE("WorkletRuntime is invalid");
					return false;
				}
				runtime->schedule(std::move(execute));
			} else {
				wc->invokeOnJsThread(std::move(execute));
			}
			{
				std::unique_lock<std::mutex> lock(mtx);
				cv.wait(lock, [&]() { return done; });
			}
			return success;
		}
	}

public:
	JavascriptCallable(std::shared_ptr<GodotWorkletContext> workletContext, jsi::Runtime &rt, const jsi::Function &func) :
			_workletContext(workletContext), _funcValue(jsi::Value(rt, func)) {
		_isWorklet = workletContext->getJsRuntime() != &rt;
		if (_isWorklet) {
			try {
				_workletRuntime = worklets::WorkletRuntime::getWeakRuntimeFromJSIRuntime(rt);
			} catch (const std::exception &exc) {
				LOGE("Unable to resolve WorkletRuntime: %s", exc.what());
			}
		}
	}

	uint32_t hash() const override {
		return 0; // Use default hash function
	}
	godot::String get_as_text() const override {
		return godot::String(JAVASCRIPT_CALLABLE_NAME);
	}
	CompareEqualFunc get_compare_equal_func() const override {
		return [](const CallableCustom *p_a, const CallableCustom *p_b) {
			if (p_a == nullptr) {
				LOGE("First parameter is NULL");
				return false;
			}
			if (p_b == nullptr) {
				LOGE("Second parameter is NULL");
				return false;
			}
			if (p_a->get_as_text() != JAVASCRIPT_CALLABLE_NAME) {
				LOGE("First parameter is not a JavascriptCallable");
				return false;
			}
			if (p_b->get_as_text() != JAVASCRIPT_CALLABLE_NAME) {
				LOGE("Second parameter is not a JavascriptCallable");
				return false;
			}
			if (p_a == p_b) {
				return true;
			}
			const JavascriptCallable *j_a = (const JavascriptCallable *)p_a;
			const JavascriptCallable *j_b = (const JavascriptCallable *)p_b;
			if (j_a->_isWorklet != j_b->_isWorklet) {
				LOGE("Different WorkletContext: %d, %d", j_a->_isWorklet, j_b->_isWorklet);
				return false;
			}
			std::shared_ptr<GodotWorkletContext> j_a_wc = j_a->_workletContext.lock();
			if (!j_a_wc) {
				LOGE("First WorkletContext is invalid");
				return false;
			}

			std::shared_ptr<GodotWorkletContext> j_b_wc = j_b->_workletContext.lock();
			if (!j_b_wc) {
				LOGE("Second WorkletContext is invalid");
				return false;
			}

			if (j_a_wc != j_b_wc) {
				LOGE("Different WorkletContext");
				return false;
			}

			{
				bool result = false;
				runInContext(j_a, [&j_a, &j_b, &result](const JavascriptCallable *c, jsi::Runtime &rt) {
					const jsi::Value &a_funcRef_value = j_a->_funcValue;
					const jsi::Value &b_funcRef_value = j_b->_funcValue;

					if (!a_funcRef_value.isObject()) {
						result = false;
						return false;
					}

					auto a_funcRef_obj = a_funcRef_value.asObject(rt);

					if (!a_funcRef_obj.isFunction(rt)) {
						result = false;
						return false;
					}

					auto a_funcRef = a_funcRef_obj.asFunction(rt);

					if (!b_funcRef_value.isObject()) {
						result = false;
						return false;
					}

					auto b_funcRef_obj = b_funcRef_value.asObject(rt);

					if (!b_funcRef_obj.isFunction(rt)) {
						result = false;
						return false;
					}

					auto b_funcRef = b_funcRef_obj.asFunction(rt);

					result = jsi::Object::strictEquals(rt, a_funcRef, b_funcRef);
					return true;
				});
				return result;
			}
		};
	}

	CompareLessFunc get_compare_less_func() const override {
		return nullptr;
	}
	bool is_valid() const override {
		return true;
	}
	godot::ObjectID get_object() const override {
		return godot::ObjectID();
	}
	void call(const godot::Variant **p_arguments, int p_argcount, godot::Variant &r_return_value, GDExtensionCallError &r_call_error) const override;

};

static godot::Callable createJSCallable(std::shared_ptr<GodotWorkletContext> workletContext, jsi::Runtime &rt, jsi::Function func) {
	return godot::Callable(memnew(JavascriptCallable(workletContext, rt, func)));
}

class GodotHostObject : public jsi::HostObject {
	std::shared_ptr<GodotWorkletContext> _workletContext;
	godot::Variant _value;

public:
	static godot::Variant jsiValueToGodotVariant(std::shared_ptr<GodotWorkletContext> workletContext, jsi::Runtime &rt, const jsi::Value &value) {
		if (value.isNull() || value.isUndefined()) {
			return godot::Variant(nullptr);
		}
		if (value.isBool()) {
			return godot::Variant(value.asBool());
		}
		if (value.isNumber()) {
			return godot::Variant(value.asNumber());
		}
		if (value.isString()) {
			std::string s = value.asString(rt).utf8(rt);
			return godot::Variant(godot::String::utf8(s.c_str()));
		}
		if (value.isBigInt()) {
			jsi::BigInt b = value.asBigInt(rt);
			return godot::Variant(b.getInt64(rt));
		}
		if (value.isObject()) {
			jsi::Object o = value.asObject(rt);
			if (o.isHostObject(rt)) {
				std::shared_ptr<GodotHostObject> ho = o.getHostObject<GodotHostObject>(rt);
				return ho->_value;
			}
			if (o.isFunction(rt)) {
				godot::Variant v(createJSCallable(workletContext, rt, o.asFunction(rt)));
				return v;
			}
			if (o.isArray(rt)) {
				throw jsi::JSINativeException("JavaScript Array binding not yet supported");
			}
			if (o.isArrayBuffer(rt)) {
				throw jsi::JSINativeException("JavaScript ArrayBuffer binding not yet supported");
			}
			throw jsi::JSINativeException("JavaScript Object binding not yet supported");
		}
		throw jsi::JSINativeException("Unhandled Object Type");
	}

	static jsi::Value godotVariantToJsiValue(std::shared_ptr<GodotWorkletContext> workletContext, jsi::Runtime &rt, const godot::Variant &variant) {
		switch (variant.get_type()) {
			case godot::Variant::Type::NIL: {
				return jsi::Value::null();
			}
			// atomic types
			case godot::Variant::Type::BOOL: {
				return jsi::Value((bool)variant);
			}
			case godot::Variant::Type::INT: {
				return jsi::Value((double)variant);
			}
			case godot::Variant::Type::FLOAT: {
				return jsi::Value((double)variant);
			}
			case godot::Variant::Type::STRING: {
				godot::String s = variant;
				jsi::String ret = jsi::String::createFromUtf8(rt, (uint8_t *)s.utf8().get_data(), s.length());
				return ret;
			}
			// math types
			case godot::Variant::Type::VECTOR2:
			case godot::Variant::Type::VECTOR2I:
			case godot::Variant::Type::RECT2:
			case godot::Variant::Type::RECT2I:
			case godot::Variant::Type::VECTOR3:
			case godot::Variant::Type::VECTOR3I:
			case godot::Variant::Type::TRANSFORM2D:
			case godot::Variant::Type::VECTOR4:
			case godot::Variant::Type::VECTOR4I:
			case godot::Variant::Type::PLANE:
			case godot::Variant::Type::QUATERNION:
			case godot::Variant::Type::AABB:
			case godot::Variant::Type::BASIS:
			case godot::Variant::Type::TRANSFORM3D:
			case godot::Variant::Type::PROJECTION: {
				return jsi::Object::createFromHostObject(rt, std::shared_ptr<HostObject>(new GodotHostObject(workletContext, variant)));
			}
			// misc types
			case godot::Variant::Type::COLOR: {
				return jsi::Object::createFromHostObject(rt, std::shared_ptr<HostObject>(new GodotHostObject(workletContext, variant)));
			}
			case godot::Variant::Type::STRING_NAME: {
				godot::StringName sn = variant;
				return jsi::String::createFromUtf8(rt, sn.to_utf8_buffer().ptr(), sn.length());
			}
			case godot::Variant::Type::NODE_PATH:
			case godot::Variant::Type::RID: {
				return jsi::Object::createFromHostObject(rt, std::shared_ptr<HostObject>(new GodotHostObject(workletContext, variant)));
			}
			case godot::Variant::Type::OBJECT: {
				godot::Object *vo = (godot::Object *)variant;
				if (vo == nullptr) {
					return jsi::Value::null();
				}
				return jsi::Object::createFromHostObject(rt, std::shared_ptr<HostObject>(new GodotHostObject(workletContext, variant)));
			}
			case godot::Variant::Type::CALLABLE:
			case godot::Variant::Type::SIGNAL:
			case godot::Variant::Type::DICTIONARY: {
				return jsi::Object::createFromHostObject(rt, std::shared_ptr<HostObject>(new GodotHostObject(workletContext, variant)));
			}
			case godot::Variant::Type::ARRAY: {
				throw jsi::JSINativeException("Arrays not supported yet.");
			}
			// typed arrays
			case godot::Variant::Type::PACKED_BYTE_ARRAY:
			case godot::Variant::Type::PACKED_INT32_ARRAY:
			case godot::Variant::Type::PACKED_INT64_ARRAY:
			case godot::Variant::Type::PACKED_FLOAT32_ARRAY:
			case godot::Variant::Type::PACKED_FLOAT64_ARRAY:
			case godot::Variant::Type::PACKED_STRING_ARRAY:
			case godot::Variant::Type::PACKED_VECTOR2_ARRAY:
			case godot::Variant::Type::PACKED_VECTOR3_ARRAY:
			case godot::Variant::Type::PACKED_COLOR_ARRAY:
			case godot::Variant::Type::PACKED_VECTOR4_ARRAY: {
				throw jsi::JSINativeException("Packed Arrays not supported yet.");
			}
			default: {
				throw jsi::JSINativeException("Unhandled Object Type");
			}
		}
	}

	GodotHostObject(std::shared_ptr<GodotWorkletContext> workletContext, const godot::Variant v) :
			jsi::HostObject(), _workletContext(workletContext), _value(v) {}

	jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &name) override {
		godot::StringName propName(name.utf8(rt).c_str());
		if (_value.get_type() == godot::Variant::Type::NIL) {
			return jsi::Value(nullptr);
		}
		if (_value.has_method(propName)) {
			std::shared_ptr<GodotWorkletContext> wc = _workletContext;
			return jsi::Function::createFromHostFunction(rt, name, 0, [propName, wc](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
				// LOGI("Calling: %s", propName.to_utf8_buffer().ptr());
				if (!thisVal.isObject()) {
					throw jsi::JSINativeException("Calling Godot Method on a value that is not an object");
				}
				jsi::Object obj = thisVal.asObject(rt);

				if (!obj.isHostObject(rt)) {
					throw jsi::JSINativeException("Calling Godot Method on a value that is not a HostObject");
				}

				std::shared_ptr<GodotHostObject> ho = obj.getHostObject<GodotHostObject>(rt);
				std::vector<godot::Variant> godotArgs;
				godotArgs.reserve(count);
				for (size_t i = 0; i < count; ++i) {
					godotArgs.push_back(jsiValueToGodotVariant(wc, rt, args[i]));
				}

				std::vector<const godot::Variant *> variantArgs = createVariantArgArray(godotArgs);
				godot::Variant r_ret;
				GDExtensionCallError r_error;
				ho->_value.callp(propName, variantArgs.data(), count, r_ret, r_error);
				if (r_error.error != GDEXTENSION_CALL_OK) {
					std::string mName = (const char *)propName.to_utf8_buffer().ptr();
					throw jsi::JSINativeException(create_method_call_error_string(mName, r_error));
				}
				return godotVariantToJsiValue(wc, rt, r_ret);
			});
		}
		{
			bool r_valid = false;
			godot::Variant v = _value.get_named(propName, r_valid);
			if (r_valid) {
				return godotVariantToJsiValue(_workletContext, rt, v);
			}
		}
		throw jsi::JSINativeException(std::string("Unable to resolve name as property or method: ") + name.utf8(rt));
	}

	void set(jsi::Runtime &rt, const jsi::PropNameID &name, const jsi::Value &value) override {
		godot::StringName propName(name.utf8(rt).c_str());
		bool r_valid = false;
		_value.set_named(propName, jsiValueToGodotVariant(_workletContext, rt, value), r_valid);
		if (!r_valid) {
			throw jsi::JSINativeException(std::string("Unable to set property: ") + name.utf8(rt));
		}
	}
};

class GodotAPIObject : public jsi::HostObject {
	std::shared_ptr<GodotWorkletContext> _workletContext;
	std::map<std::string, jsi::Value> builtin_types;

public:
	static jsi::Value createBuiltinTypeConstructor(std::shared_ptr<GodotWorkletContext> workletContext, jsi::Runtime &rt, std::string name, std::function<godot::Variant()> constructor) {
		jsi::Function ctorFunc = jsi::Function::createFromHostFunction(rt,
				jsi::PropNameID::forUtf8(rt, name),
				0,
				[constructor, workletContext](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
					return GodotHostObject::godotVariantToJsiValue(workletContext, rt, constructor());
				});
		return jsi::Value(rt, ctorFunc);
	}

	static jsi::Value createStaticFunction(std::shared_ptr<GodotWorkletContext> workletContext, jsi::Runtime &rt, std::string name, GDExtensionMethodBindPtr mb) {
		jsi::Function f = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forUtf8(rt, name), 0, [name, mb, workletContext](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
			std::vector<godot::Variant> godotArgs;
			godotArgs.reserve(count);
			for (size_t i = 0; i < count; ++i) {
				godotArgs.push_back(GodotHostObject::jsiValueToGodotVariant(workletContext, rt, args[i]));
			}
			std::vector<const godot::Variant *> variantArgs = createVariantArgArray(godotArgs);
			godot::Variant r_ret;
			GDExtensionCallError r_error;

			godot::internal::gdextension_interface_object_method_bind_call(mb, nullptr, (GDExtensionConstVariantPtr *)variantArgs.data(), count, &r_ret, &r_error);
			if (r_error.error != GDEXTENSION_CALL_OK) {
				throw jsi::JSINativeException(create_method_call_error_string(name, r_error));
			}
			return GodotHostObject::godotVariantToJsiValue(workletContext, rt, r_ret);
		});
		return jsi::Value(rt, f);
	}

	static jsi::Value createClassConstructor(std::shared_ptr<GodotWorkletContext> workletContext, jsi::Runtime &rt, std::string name) {
		godot::StringName godotClassName(name.c_str());
		jsi::Function ctorFunc = jsi::Function::createFromHostFunction(rt,
				jsi::PropNameID::forUtf8(rt, name),
				0,
				[name, godotClassName, workletContext](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
					if (!godot::ClassDB::can_instantiate(godotClassName)) {
						throw jsi::JSINativeException("Unable to instantiate class: " + name);
					}
					godot::Variant v = godot::ClassDB::instantiate(godotClassName);
					return GodotHostObject::godotVariantToJsiValue(workletContext, rt, v);
				});
		godot::TypedArray<godot::Dictionary> methodList = godot::ClassDB::class_get_method_list(godotClassName);
		for (int i = 0; i < methodList.size(); ++i) {
			godot::Dictionary d = methodList[i];
			godot::StringName methodName = (godot::StringName)d["name"];
			// LOGI("Method name: %s", methodName.to_utf8_buffer().ptr());
			bool isStatic = (bool)d["is_static"];
			if (isStatic) {
				int64_t methodHash = (int64_t)d["hash"];
				GDExtensionMethodBindPtr mb = godot::internal::gdextension_interface_classdb_get_method_bind(godotClassName._native_ptr(), methodName._native_ptr(), methodHash);
				std::string methodNameStd = (const char *)methodName.to_utf8_buffer().ptr();
				ctorFunc.setProperty(rt, methodNameStd.c_str(), createStaticFunction(workletContext, rt, methodNameStd, mb));
			}
		}
		return jsi::Value(rt, ctorFunc);
	}

#define DECLARE_BUILTIN_TYPE(name) builtin_types[#name] = createBuiltinTypeConstructor(workletContext, rt, #name, []() { return godot::Variant(godot::name()); })

	GodotAPIObject(std::shared_ptr<GodotWorkletContext> workletContext, jsi::Runtime &rt) :
			jsi::HostObject(), _workletContext(workletContext) {
		DECLARE_BUILTIN_TYPE(Vector2);
		DECLARE_BUILTIN_TYPE(Vector2i);
		DECLARE_BUILTIN_TYPE(Rect2);
		DECLARE_BUILTIN_TYPE(Rect2i);
		DECLARE_BUILTIN_TYPE(Vector3);
		DECLARE_BUILTIN_TYPE(Vector3i);
		DECLARE_BUILTIN_TYPE(Transform2D);
		DECLARE_BUILTIN_TYPE(Vector4);
		DECLARE_BUILTIN_TYPE(Vector4i);
		DECLARE_BUILTIN_TYPE(Plane);
		DECLARE_BUILTIN_TYPE(Quaternion);
		DECLARE_BUILTIN_TYPE(AABB);
		DECLARE_BUILTIN_TYPE(Basis);
		DECLARE_BUILTIN_TYPE(Transform3D);
		DECLARE_BUILTIN_TYPE(Projection);
		DECLARE_BUILTIN_TYPE(Color);
		DECLARE_BUILTIN_TYPE(StringName);
		DECLARE_BUILTIN_TYPE(NodePath);
		DECLARE_BUILTIN_TYPE(RID);
		// DECLARE_BUILTIN_TYPE(Callable);
		// DECLARE_BUILTIN_TYPE(Object);
		DECLARE_BUILTIN_TYPE(Signal);
		DECLARE_BUILTIN_TYPE(Dictionary);
		DECLARE_BUILTIN_TYPE(Array);
		DECLARE_BUILTIN_TYPE(PackedByteArray);
		DECLARE_BUILTIN_TYPE(PackedInt32Array);
		DECLARE_BUILTIN_TYPE(PackedInt64Array);
		DECLARE_BUILTIN_TYPE(PackedFloat32Array);
		DECLARE_BUILTIN_TYPE(PackedFloat64Array);
		DECLARE_BUILTIN_TYPE(PackedStringArray);
		DECLARE_BUILTIN_TYPE(PackedVector2Array);
		DECLARE_BUILTIN_TYPE(PackedVector3Array);
		DECLARE_BUILTIN_TYPE(PackedColorArray);
		DECLARE_BUILTIN_TYPE(PackedVector4Array);
	}

	jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &name) override {
		if (GodotModule::get_singleton()->get_instance() == nullptr) {
			throw jsi::JSINativeException("Godot Engine not initialized yet.");
		}

		std::string typeName = name.utf8(rt);

		if (builtin_types.count(typeName)) {
			return jsi::Value(rt, builtin_types[typeName]);
		}

		godot::StringName godotTypeName(name.utf8(rt).c_str());
		if (godot::ClassDB::class_exists(godotTypeName)) {
			if (godot::Engine::get_singleton()->has_singleton(godotTypeName)) {
				std::shared_ptr<HostObject> ho(new GodotHostObject(_workletContext, godot::Variant(godot::Engine::get_singleton()->get_singleton(godotTypeName))));
				return jsi::Object::createFromHostObject(rt, ho);
			} else {
				return createClassConstructor(_workletContext, rt, name.utf8(rt));
			}
		}

		throw jsi::JSINativeException(std::string("Unable to resolve name as a type: ") + name.utf8(rt));
	}

	void set(jsi::Runtime &rt, const jsi::PropNameID &name, const jsi::Value &value) override {
		throw jsi::JSINativeException("Setting property values is not supported on API Object");
	}
};

void JavascriptCallable::call(const godot::Variant **p_arguments, int p_argcount, godot::Variant &r_return_value, GDExtensionCallError &r_call_error) const {
	r_call_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
	std::shared_ptr<GodotWorkletContext> wc = _workletContext.lock();
	if (!wc) {
		// Func ref no longer valid
		r_call_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
		return;
	}

	auto caller = [p_argcount, p_arguments, &r_return_value, &r_call_error](const JavascriptCallable *c, jsi::Runtime &rt) {
		auto wc = c->_workletContext.lock();
		if (!wc) {
			// WorkletContext no longer valid
			LOGE("JavascriptCallable::call caller workletContext no longer valid");
			r_call_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
			return false;
		}
		const jsi::Value &val = c->_funcValue;
		if (!val.isObject()) {
			// Func ref no longer valid
			r_call_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
			return false;
		}
		jsi::Object obj = val.asObject(rt);
		if (!obj.isFunction(rt)) {
			// Func ref is not a function
			r_call_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
			return false;
		}
		jsi::Function func = obj.asFunction(rt);

		std::vector<jsi::Value> args;
		args.reserve(p_argcount);
		for (int i = 0; i < p_argcount; ++i) {
			args.push_back(GodotHostObject::godotVariantToJsiValue(wc, rt, *p_arguments[i]));
		}
		const jsi::Value *argptr = args.data();
		jsi::Value ret = func.call(rt, argptr, (size_t)p_argcount);
		r_return_value = GodotHostObject::jsiValueToGodotVariant(wc, rt, ret);
		r_call_error.error = GDEXTENSION_CALL_OK;
		return true;
	};
	runInContext(this, caller);
}

jsi::Value createNativeGodotModule(jsi::Runtime &rt, const std::shared_ptr<facebook::react::CallInvoker> &callInvoker) {
	// Perform initialization

	std::shared_ptr<GodotWorkletContext> workletContext =
			std::make_shared<GodotWorkletContext>(&rt, callInvoker);

	LOGI("NativeGodotModule::createNativeModule");

	auto createGodotQueueFunc = [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
		jsi::Object queue(runtime);
		queue.setNativeState(runtime, std::make_shared<GodotAsyncQueue>());
		return queue;
	};

	auto isPausedFunc = [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
		return jsi::Value(GodotModule::get_singleton()->is_paused());
	};

	auto pauseFunc = [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
		GodotModule::get_singleton()->pause();
		return jsi::Value::undefined();
	};

	auto resumeFunc = [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
		GodotModule::get_singleton()->resume();
		return jsi::Value::undefined();
	};

	auto createInstanceFunc = [workletContext](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
		std::vector<std::string> godotArgs;

		if (count < 1) {
			LOGE("createInstance requires at least 1 argument.");
			return jsi::Value::undefined();
		}

		if (count == 1) {
			const jsi::Value &arg = args[0];
			bool processed = false;
			if (arg.isObject()) {
				jsi::Object obj = arg.asObject(rt);
				if (obj.isArray(rt)) {
					jsi::Array arr = obj.asArray(rt);
					size_t length = arr.length(rt);
					for (size_t i = 0; i < length; ++i) {
						jsi::Value v = arr.getValueAtIndex(rt, i);
						jsi::String s = v.toString(rt);
						godotArgs.push_back(s.utf8(rt));
						processed = true;
					}
				}
			}
			if (!processed) {
				godotArgs.push_back(arg.toString(rt).utf8(rt));
			}
		} else {
			for (size_t i = 0; i < count; ++i) {
				const jsi::Value &arg = args[i];
				godotArgs.push_back(arg.toString(rt).utf8(rt));
			}
		}

		GodotModule *mod = GodotModule::get_singleton();
		godot::GodotInstance *instance = mod->get_or_create_instance(godotArgs);
		if (!instance) {
			return jsi::Value::undefined();
		}
		return GodotHostObject::godotVariantToJsiValue(workletContext, rt, godot::Variant(instance));
	};

	auto getInstanceFunc = [workletContext](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
		GodotModule *mod = GodotModule::get_singleton();
		godot::GodotInstance *instance = mod->get_instance();
		if (instance == nullptr) {
			return jsi::Value::null();
		}
		return GodotHostObject::godotVariantToJsiValue(workletContext, rt, godot::Variant(instance));
	};

	auto updateWindowFunc = [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
		if (count >= 2) {
			GodotModule *mod = GodotModule::get_singleton();
			mod->updateWindow(args[0].asString(rt).utf8(rt), args[1].asBool());
		}
		return jsi::Value::undefined();
	};

	auto APIFunc = [workletContext](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
		return jsi::Object::createFromHostObject(rt, std::shared_ptr<jsi::HostObject>(std::make_shared<GodotAPIObject>(workletContext, rt)));
	};

	auto destroyInstanceFunc = [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) {
		GodotModule *mod = GodotModule::get_singleton();
		mod->destroy_instance();
		return jsi::Value::undefined();
	};

	jsi::Function createGodotQueue = jsi::Function::createFromHostFunction(rt,
			jsi::PropNameID::forUtf8(rt, "createGodotQueue"),
			0,
			createGodotQueueFunc);

	jsi::Function createInstance = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forUtf8(rt, "createInstance"),
			1,
			createInstanceFunc);

	jsi::Function getInstance = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forUtf8(rt, "getInstance"),
			0,
			getInstanceFunc);

	jsi::Function API = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forUtf8(rt, "API"),
			0,
			APIFunc);

	jsi::Function updateWindow = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forUtf8(rt, "updateWindow"),
			2,
			updateWindowFunc);

	jsi::Function is_paused = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forUtf8(rt, "is_paused"),
			0,
			isPausedFunc);

	jsi::Function pause = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forUtf8(rt, "pause"),
			0,
			pauseFunc);

	jsi::Function resume = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forUtf8(rt, "resume"),
			0,
			resumeFunc);

	jsi::Function destroyInstance = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forUtf8(rt, "destroyInstance"),
			0,
			destroyInstanceFunc);

	jsi::Object o(rt);
	o.setProperty(rt, jsi::PropNameID::forUtf8(rt, "createGodotQueue"), createGodotQueue);
	o.setProperty(rt, jsi::PropNameID::forUtf8(rt, "createInstance"), createInstance);
	o.setProperty(rt, jsi::PropNameID::forUtf8(rt, "getInstance"), getInstance);
	o.setProperty(rt, jsi::PropNameID::forUtf8(rt, "API"), API);
	o.setProperty(rt, jsi::PropNameID::forUtf8(rt, "updateWindow"), updateWindow);
	o.setProperty(rt, jsi::PropNameID::forUtf8(rt, "resume"), resume);
	o.setProperty(rt, jsi::PropNameID::forUtf8(rt, "pause"), pause);
	o.setProperty(rt, jsi::PropNameID::forUtf8(rt, "is_paused"), is_paused);
	o.setProperty(rt, jsi::PropNameID::forUtf8(rt, "destroyInstance"), destroyInstance);

	auto result = jsi::Value(rt, o);
	rt.global().setProperty(rt, NATIVE_GODOT_MODULE_PROPERTY, result);
	LOGI("NativeGodotModule initialized in Runtime: %s", rt.description().c_str());

	return result;
}

namespace facebook::react {

NativeGodotModule::NativeGodotModule(std::shared_ptr<CallInvoker> jsInvoker) :
		NativeGodotModuleCxxSpec(std::move(jsInvoker)) {}

bool NativeGodotModule::installTurboModule(jsi::Runtime &rt) {
	jsi::Value godotModule = createNativeGodotModule(rt, jsInvoker_);

	rt.global().setProperty(rt, NATIVE_GODOT_MODULE_PROPERTY, godotModule);
	if (!rt.global().getProperty(rt, NATIVE_GODOT_MODULE_PROPERTY).isObject()) {
		LOGE("Could not set NativeGodotModule property.");
		return false;
	}
	return true;
}

}; //namespace facebook::react
