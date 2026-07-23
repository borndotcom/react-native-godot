/**************************************************************************/
/*  NativeGodotModule.ts                                                  */
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

import { TurboModuleRegistry, type TurboModule } from "react-native";
import {
  createWorkletRuntime,
  runOnRuntimeAsync,
  type WorkletRuntime,
} from "react-native-worklets";

export interface Spec extends TurboModule {
  installTurboModule(): boolean;
}

const GodotInstaller = TurboModuleRegistry.get<Spec>("NativeGodotModule");

export interface GodotModuleInterface {
  createInstance(args: Array<string>): any;
  getInstance(): any;
  API(): any;
  updateWindow(windowName: string, adding: boolean): void;
  pause(): void;
  resume(): void;
  is_paused(): boolean;
  createGodotQueue(): object;
  destroyInstance(): void;
}

declare global {
  var RTNGodot: GodotModuleInterface | undefined; // Godot
  var __godotWorkletRuntime: WorkletRuntime | undefined;
}

if (globalThis.RTNGodot == null) {
  if (GodotInstaller == null) {
    console.error(
      "Native Godot Module cannot be found! Make sure you correctly " +
        "installed native dependencies and rebuilt your app."
    );
  } else if (!GodotInstaller.installTurboModule()) {
    console.error("NativeGodotModule installation failed.");
  }
}

const installedGodotModule = globalThis.RTNGodot;
if (installedGodotModule != null && globalThis.__godotWorkletRuntime == null) {
  const godotQueue = installedGodotModule.createGodotQueue();
  globalThis.__godotWorkletRuntime = createWorkletRuntime({
    name: "ReactNativeGodot",
    queue: godotQueue,
    initializer: () => {
      "worklet";
      globalThis.RTNGodot = installedGodotModule;
    },
  });
}

export const RTNGodot = globalThis.RTNGodot as GodotModuleInterface;

export function runOnGodotThread<T>(f: () => T): Promise<T> {
  const runtime = globalThis.__godotWorkletRuntime;
  if (runtime == null) {
    return Promise.reject(
      new Error("NativeGodotModule worklet runtime is not installed")
    );
  }
  return runOnRuntimeAsync(runtime, f);
}
