/**************************************************************************/
/*  GodotPackage.java                                                     */
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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.facebook.react.TurboReactPackage;
import com.facebook.react.bridge.NativeModule;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.module.model.ReactModuleInfo;
import com.facebook.react.module.model.ReactModuleInfoProvider;
import com.facebook.react.turbomodule.core.interfaces.TurboModule;
import com.facebook.react.uimanager.ViewManager;

public class GodotPackage extends TurboReactPackage {
	static {
		System.loadLibrary("rtngodot");
	}

	@Override
	public List<ViewManager> createViewManagers(@NonNull ReactApplicationContext reactContext) {
		return Collections.singletonList(new RTNGodotViewManager(reactContext));
	}

	@Nullable
	@Override
	public NativeModule getModule(String name, ReactApplicationContext reactContext) {
		if (NativeGodotModule.NAME.equals(name)) {
			return new NativeGodotModule(reactContext);
		}
		return null;
	}

	@Override
	public ReactModuleInfoProvider getReactModuleInfoProvider() {
		return () -> {
			final Map<String, ReactModuleInfo> moduleInfos = new HashMap<>();
			Class<? extends NativeModule> moduleClass = NativeGodotModule.class;
			ReactModule reactModule = moduleClass.getAnnotation(ReactModule.class);
			moduleInfos.put(
					reactModule.name(),
					new ReactModuleInfo(
							reactModule.name(),
							moduleClass.getName(),
							true,
							reactModule.needsEagerInit(),
							reactModule.hasConstants(),
							reactModule.isCxxModule(),
							TurboModule.class.isAssignableFrom(moduleClass)));
			return moduleInfos;
		};
	}
}
