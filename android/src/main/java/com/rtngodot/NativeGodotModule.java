/**************************************************************************/
/*  NativeGodotModule.java                                                */
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

package com.rtngodot;

import androidx.annotation.OptIn;

import java.util.Objects;

import com.facebook.jni.HybridData;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.common.annotations.FrameworkAPI;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;
import com.facebook.react.turbomodule.core.interfaces.CallInvokerHolder;
import com.migeran.NativeGodotModuleSpec;

@OptIn(markerClass = FrameworkAPI.class)
@ReactModule(name = NativeGodotModule.NAME)
public class NativeGodotModule extends NativeGodotModuleSpec {
	public static final String NAME = "NativeGodotModule";

	@DoNotStrip
	@SuppressWarnings("unused")
	private final HybridData mHybridData;

	public NativeGodotModule(ReactApplicationContext context) {
		super(context);
		// libgodot_create_godot_instance_android expects the Activity, Godot
		// engine wrapper and Android services to have been registered first.
		// Without this, starting an instance aborts in JNI GetLongField(null).
		RTNLibGodot.getInstance().init(context.getCurrentActivity());

		CallInvokerHolder callInvokerHolder = Objects.requireNonNull(
				context.getJSCallInvokerHolder(),
				"The JavaScript call invoker is not available");
		if (!(callInvokerHolder instanceof CallInvokerHolderImpl)) {
			throw new IllegalStateException(
					"Unsupported JavaScript call invoker implementation: "
							+ callInvokerHolder.getClass().getName());
		}
		mHybridData = initHybrid(
				Objects.requireNonNull(context.getJavaScriptContextHolder()).get(),
				(CallInvokerHolderImpl)callInvokerHolder);
	}

	private native HybridData initHybrid(long jsContext, CallInvokerHolderImpl jsCallInvokerHolder);

	@ReactMethod(isBlockingSynchronousMethod = true)
	@Override
	public native boolean installTurboModule();
}
