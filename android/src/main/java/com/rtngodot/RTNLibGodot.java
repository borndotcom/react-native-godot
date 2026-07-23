/**************************************************************************/
/*  RTNLibGodot.java                                                      */
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

import org.godotengine.godot.Godot;
import org.godotengine.godot.GodotHost;
import org.godotengine.godot.GodotIO;
import org.godotengine.godot.GodotLib;
import org.godotengine.godot.GodotLibImpl;
import org.godotengine.godot.GodotRenderView;
import org.godotengine.godot.IGodotLib;
import org.godotengine.godot.input.GodotInputHandler;
import org.godotengine.godot.io.directory.DirectoryAccessHandler;
import org.godotengine.godot.io.file.FileAccessHandler;
import org.godotengine.godot.plugin.AndroidRuntimePlugin;
import org.godotengine.godot.plugin.GodotPlugin;
import org.godotengine.godot.plugin.GodotPluginRegistry;
import org.godotengine.godot.tts.GodotTTS;
import org.godotengine.godot.utils.GodotNetUtils;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.PixelFormat;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

public class RTNLibGodot implements IGodotLib, GodotHost, GodotRenderView {
	private static final String TAG = "LibGodot";

	private static boolean inited = false;

	private static Activity mActivity;

	private static final RTNLibGodot INSTANCE = new RTNLibGodot();

	private Godot godot;

	private FrameLayout godotContainerLayout;

	private GodotInputHandler mInputHandler;

	private RTNLibGodot() {}

	public static RTNLibGodot getInstance() {
		return INSTANCE;
	}

	@Override
	public boolean initialize(Godot godot, AssetManager assetManager, GodotIO godotIO, GodotNetUtils godotNetUtils, DirectoryAccessHandler directoryAccessHandler, FileAccessHandler fileAccessHandler, boolean b) {
		ClassLoader loader = RTNLibGodot.class.getClassLoader();
		WindowSurfaceData mainWindow = windowData.get("");
		if (mainWindow == null || mainWindow.surface == null) {
			Log.e(TAG, "Main window surface is unavailable; call init() before initialize()");
			return false;
		}

		initialize(
				assetManager,
				godotNetUtils,
				directoryAccessHandler,
				fileAccessHandler,
				godotIO,
				mainWindow.surface,
				mainWindow.width,
				mainWindow.height,
				godot,
				mActivity,
				loader);
		return true;
	}

	@Override
	public void ondestroy() {
	}

	@Override
	public boolean setup(String[] strings, GodotTTS godotTTS) {
		return true;
	}

	@Override
	public void resize(Surface surface, int i, int i1) {
		Log.i(TAG, "Resize");
	}

	@Override
	public void newcontext(Surface surface) {
	}

	@Override
	public void back() {
	}

	@Override
	public boolean step() {
		return false;
	}

	@Override
	public void ttsCallback(int i, int i1, int i2) {
	}

	@Override
	public void dispatchTouchEvent(int i, int i1, int i2, float[] floats, boolean b) {
		dispatchTouchEvent("", i, i1, i2, floats, b);
	}

	@Override
	public void dispatchMouseEvent(int i, int i1, float v, float v1, float v2, float v3, boolean b, boolean b1, float v4, float v5, float v6) {
	}

	@Override
	public void magnify(float v, float v1, float v2) {
	}

	@Override
	public void pan(float v, float v1, float v2, float v3) {
	}

	@Override
	public void accelerometer(float v, float v1, float v2) {
	}

	@Override
	public void gravity(float v, float v1, float v2) {
	}

	@Override
	public void magnetometer(float v, float v1, float v2) {
	}

	@Override
	public void gyroscope(float v, float v1, float v2) {
	}

	@Override
	public void key(int i, int i1, int i2, boolean b, boolean b1) {
	}

	@Override
	public void joybutton(int i, int i1, boolean b) {
	}

	@Override
	public void joyaxis(int i, int i1, float v) {
	}

	@Override
	public void joyhat(int i, int i1, int i2) {
	}

	@Override
	public void joyconnectionchanged(int i, boolean b, String s) {
	}

	@Override
	public void focusin() {
	}

	@Override
	public void focusout() {
	}

	@Override
	public native String getGlobal(String s);

	@Override
	public native String[] getRendererInfo();

	@Override
	public String getEditorSetting(String s) {
		return "";
	}

	@Override
	public void setEditorSetting(String s, Object o) {
	}

	@Override
	public Object getEditorProjectMetadata(String s, String s1, Object o) {
		return null;
	}

	@Override
	public void setEditorProjectMetadata(String s, String s1, Object o) {
	}

	@Override
	public void requestPermissionResult(String s, boolean b) {
	}

	@Override
	public void onNightModeChanged() {
	}

	@Override
	public void hardwareKeyboardConnected(boolean b) {
	}

	@Override
	public void filePickerCallback(boolean b, String[] strings) {
	}

	@Override
	public void setVirtualKeyboardHeight(int i) {
	}

	@Override
	public void onRendererResumed() {
	}

	@Override
	public void onRendererPaused() {
	}

	@Override
	public boolean shouldDispatchInputToRenderThread() {
		return false;
	}

	@Override
	public String getProjectResourceDir() {
		return "";
	}

	@Override
	public boolean isEditorHint() {
		return false;
	}

	@Override
	public boolean isProjectManagerHint() {
		return false;
	}

	@Override
	public boolean providesRenderView() {
		return true;
	}

	@Override
	public GodotRenderView getRenderView() {
		return this;
	}

	@Nullable
	@Override
	public Activity getActivity() {
		return mActivity;
	}

	public Godot getGodot() {
		return godot;
	}

	@Override
	public SurfaceView getView() {
		return null;
	}

	@Override
	public void startRenderer() {
	}

	@Override
	public void queueOnRenderThread(Runnable runnable) {
	}

	@Override
	public void onActivityPaused() {
	}

	@Override
	public void onActivityStopped() {
	}

	@Override
	public void onActivityResumed() {
	}

	@Override
	public void onActivityStarted() {
	}

	@Override
	public void onActivityDestroyed() {
	}

	@Override
	public GodotInputHandler getInputHandler() {
		return mInputHandler;
	}

	@Override
	public void configurePointerIcon(int i, String s, float v, float v1) {
	}

	@Override
	public void setPointerIcon(int i) {
	}

	private static class WindowSurfaceData {
		public SurfaceControl attachedControl;
		public Surface attachedSurface;
		public final SurfaceControl control;
		public final Surface surface;
		public final boolean persistent;
		public int width;
		public int height;

		public WindowSurfaceData(SurfaceControl ctrl, int width, int height, boolean persistent) {
			this.control = ctrl;
			this.surface = new Surface(ctrl);
			this.width = width;
			this.height = height;
			this.persistent = persistent;
		}
	}

	private static final Map<String, WindowSurfaceData> windowData = new HashMap<>();

	private static void createWindowSurface(String name, int width, int height) {
		SurfaceControl.Builder b = new SurfaceControl.Builder();
		SurfaceControl control = b.setBufferSize(width, height)
										 .setFormat(PixelFormat.RGBA_8888)
										 .setName(name)
										 .build();

		WindowSurfaceData wsData = new WindowSurfaceData(control, width, height, name.isEmpty());

		windowData.put(name, wsData);
	}

	@NonNull
	private static WindowSurfaceData getOrCreateWindowSurface(String name, int width, int height) {
		WindowSurfaceData wsData = windowData.get(name);
		if (wsData == null) {
			createWindowSurface(name, width, height);
			wsData = Objects.requireNonNull(windowData.get(name));
		}
		return wsData;
	}

	public void updateWindow(String name, SurfaceControl control, SurfaceHolder holder, int format, int width, int height) {
		if (!"".equals(name)) {
			// Render in the window surface directly
			updateWindowNative(name, holder.getSurface(), width, height);
			return;
		}

		WindowSurfaceData wsData = getOrCreateWindowSurface(name, width, height);

		if (wsData.attachedControl == null || wsData.attachedSurface == null || !wsData.attachedControl.equals(control) || !wsData.attachedSurface.equals(holder.getSurface())) {
			if (wsData.attachedControl != null) {
				wsData.attachedControl = null;
				wsData.attachedSurface = null;
			}
			Log.i(TAG, String.format("Attaching and resizing surface to: %d %d", width, height));
			try (SurfaceControl.Transaction t = new SurfaceControl.Transaction()) {
				// Set new parent
				t.reparent(wsData.control, control);
				t.setLayer(wsData.control, 1);
				t.setVisibility(wsData.control, true);
				if (wsData.width != width || wsData.height != height) {
					t.setBufferSize(wsData.control, width, height);
					wsData.width = width;
					wsData.height = height;
				}
				t.apply();
			}

			wsData.attachedControl = control;
			wsData.attachedSurface = holder.getSurface();
		} else if (wsData.width != width || wsData.height != height) {
			Log.i(TAG, String.format("Resizing surface to: %d %d", width, height));
			try (SurfaceControl.Transaction t = new SurfaceControl.Transaction()) {
				t.setBufferSize(wsData.control, width, height);
				wsData.width = width;
				wsData.height = height;
				t.apply();
			}
		}

		updateWindowNative(name, wsData.surface, width, height);
	}

	public void removeWindow(String name) {
		WindowSurfaceData wsData = windowData.get(name);
		if (wsData != null) {
			try (SurfaceControl.Transaction t = new SurfaceControl.Transaction()) {
				t.reparent(wsData.control, null);
				t.setVisibility(wsData.control, false);
				t.apply();
			}
			wsData.attachedControl = null;
			wsData.attachedSurface = null;

			removeWindowNative(name);

			if (!wsData.persistent) {
				windowData.remove(name);
			}
		} else {
			removeWindowNative(name);
		}
	}

	public synchronized void init(Activity activity) {
		if (inited) {
			return;
		}

		mActivity = activity;

		if (mActivity == null) {
			Log.e(TAG, "Activity not set, abort init");
			return;
		}
		DisplayMetrics metrics = new DisplayMetrics();
		mActivity.getWindowManager().getDefaultDisplay().getMetrics(metrics);
		getOrCreateWindowSurface("", metrics.widthPixels, metrics.heightPixels);

		GodotLib.setGodotLibImpl(RTNLibGodot.getInstance());
		godot = Godot.getInstance(mActivity);
		godot.setActivity(mActivity);

		Set<GodotPlugin> runtimePlugins = new HashSet<>();
		runtimePlugins.add(new AndroidRuntimePlugin(godot));
		runtimePlugins.addAll(getHostPlugins());

		List<String> commands = new ArrayList<>();

		if (!godot.initEngine(this, commands, runtimePlugins)) {
			Log.e(TAG, "Unable to initialize Godot engine layer");
			return;
		}

		mInputHandler = new GodotInputHandler(mActivity, godot);

		inited = true;
	}

	public void shutdown() {
		if (inited) {
			cleanup();
			inited = false;
		}
	}

	private static native void initialize(AssetManager asset_manager,
			GodotNetUtils net_utils,
			DirectoryAccessHandler directoryAccessHandler,
			FileAccessHandler fileAccessHandler,
			GodotIO io, Surface mainSurface, int width, int height,
			Godot godot, Activity host_activity, ClassLoader appClassLoader);

	private native void cleanup();

	/**
	 * Forward touch events.
	 */
	public native void dispatchTouchEvent(String windowName, int event, int pointer, int pointerCount, float[] positions, boolean doubleTap);

	/**
	 * Dispatch mouse events
	 */
	public native void dispatchMouseEvent(String windowName, int event, int buttonMask, float x, float y, float deltaX, float deltaY, boolean doubleClick, boolean sourceMouseRelative, float pressure, float tiltX, float tiltY);

	private native void updateWindowNative(String windowName, Surface surface, int width, int height);

	private native void removeWindowNative(String windowName);

	private final Set<GodotPlugin> hostPlugins = new HashSet<>();

	public void addHostPlugin(GodotPlugin plugin) {
		hostPlugins.add(plugin);
	}

	public Set<GodotPlugin> getHostPlugins() {
		return hostPlugins;
	}

	private static native void registerWindowUpdateCallbackNative(String name, Object handle, Runnable r);

	private static native void unregisterWindowUpdateCallbackNative(Object handle);
}
