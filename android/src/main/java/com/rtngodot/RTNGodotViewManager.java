/**************************************************************************/
/*  RTNGodotViewManager.java                                              */
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

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.RTNGodotViewManagerDelegate;
import com.facebook.react.viewmanagers.RTNGodotViewManagerInterface;

@ReactModule(name = RTNGodotViewManager.NAME)
public class RTNGodotViewManager extends SimpleViewManager<RTNGodotView> implements RTNGodotViewManagerInterface<RTNGodotView> {
	private final RTNGodotViewManagerDelegate<RTNGodotView, RTNGodotViewManager> mDelegate;

	static final String NAME = "RTNGodotView";

	public RTNGodotViewManager(ReactApplicationContext context) {
		mDelegate = new RTNGodotViewManagerDelegate<>(this);
	}

	@Nullable
	@Override
	protected ViewManagerDelegate<RTNGodotView> getDelegate() {
		return mDelegate;
	}

	@NonNull
	@Override
	public String getName() {
		return RTNGodotViewManager.NAME;
	}

	@NonNull
	@Override
	protected RTNGodotView createViewInstance(@NonNull ThemedReactContext context) {
		// TurboModules can be created before React Native has attached the
		// current Activity. The view is created on the UI thread, so retrying
		// here makes initialization deterministic for bridgeless/Expo apps.
		RTNLibGodot.getInstance().init(context.getCurrentActivity());
		return new RTNGodotView(context);
	}

	@ReactProp(name = "windowName")
	public void setWindowName(RTNGodotView view, @Nullable String text) {
		view.setWindowName(text);
	}
}
