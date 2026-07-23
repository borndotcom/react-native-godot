/**************************************************************************/
/*  libgodot_jni.h                                                        */
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

#pragma once

#include <jni.h>
#include <godot_cpp/variant/string.hpp>
#include <string>

struct ANativeWindow;

class LibGodot {
	static inline JavaVM *java_vm = nullptr;
	static inline jobject asset_manager = nullptr;
	static inline jobject net_utils = nullptr;
	static inline jobject dir_access_handler = nullptr;
	static inline jobject file_access_handler = nullptr;
	static inline jobject godot_io = nullptr;
	static inline jobject godot_engine = nullptr;
	static inline jobject class_loader = nullptr;
	static inline jobject host_activity = nullptr;

public:
	static jobject get_asset_manager() {
		return asset_manager;
	}
	static jobject get_net_utils() {
		return net_utils;
	}
	static jobject get_dir_access_handler() {
		return dir_access_handler;
	}
	static jobject get_file_access_handler() {
		return file_access_handler;
	}

	static jobject get_godot_io() {
		return godot_io;
	}

	static jobject get_godot_engine() {
		return godot_engine;
	}

	static jobject get_host_activity() {
		return host_activity;
	}

	static jobject get_class_loader() {
		return class_loader;
	}

	static ANativeWindow *get_main_surface();

	static int get_main_width();

	static int get_main_height();

	static JNIEnv *get_jni_env();

	static void set_java_vm(JavaVM *vm);

	static void initialize(JNIEnv *env, jobject p_asset_manager, jobject p_net_utils, jobject p_dir_access_handler, jobject p_file_access_handler, jobject p_godot_io, jobject p_main_surface, jint p_width, jint p_height, jobject p_godot_engine, jobject p_host_activity, jobject p_class_loader);

	static void cleanup(JNIEnv *env);

	static void updateWindowNative(JNIEnv *env, jstring p_name, jobject p_surface, jint p_width, jint p_height);

	static void removeWindowNative(JNIEnv *env, jstring p_name);

	static void updateWindow(std::string windowName);

	static void updateWindows();

	static void registerWindowUpdateCallbackNative(JNIEnv *env, jstring name, jlong handle, jobject r);

	static void unregisterWindowUpdateCallbackNative(jlong handle);
};

static inline godot::String jstring_to_string(jstring source, JNIEnv *env = nullptr) {
	godot::String result;
	if (source) {
		if (!env) {
			env = LibGodot::get_jni_env();
		}
		const char *const source_utf8 = env->GetStringUTFChars(source, nullptr);
		if (source_utf8) {
			result.parse_utf8(source_utf8);
			env->ReleaseStringUTFChars(source, source_utf8);
		}
	}
	return result;
}

static inline std::string jstring_to_std_string(jstring source, JNIEnv *env = nullptr) {
	std::string result;
	if (source) {
		if (!env) {
			env = LibGodot::get_jni_env();
		}
		const char *const source_utf8 = env->GetStringUTFChars(source, nullptr);
		if (source_utf8) {
			result = source_utf8;
			env->ReleaseStringUTFChars(source, source_utf8);
		}
	}
	return result;
}
