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

import { TurboModule, TurboModuleRegistry } from "react-native";
import { IWorkletContext, Worklets } from "react-native-worklets-core";

export interface Spec extends TurboModule {
  installTurboModule(): boolean;
}

const GodotInstaller =
  TurboModuleRegistry.getEnforcing<Spec>("NativeGodotModule");

export interface GodotModuleInterface {
  createInstance(args: Array<string>): any;
  getInstance(): any;
  API(): any;
  updateWindow(windowName: string): any;
  pause(): void;
  resume(): void;
  is_paused(): boolean;
  runOnGodotThread<T>(f: () => T): Promise<T>;
  destroyInstance(): void;
  crash(): void;
}

console.log("Loading NativeGodotModule...");

declare global {
  var RTNGodot: GodotModuleInterface; // Godot
  var __godotWorkletContext: IWorkletContext;
}

if (globalThis.RTNGodot == null) {
  if (
    GodotInstaller == null ||
    typeof GodotInstaller.installTurboModule !== "function"
  ) {
    console.error(
      "Native Godot Module cannot be found! Make sure you correctly " +
        "installed native dependencies and rebuilt your app."
    );
  } else {
    console.log("Calling NativeGodotModule.installTurboModule()");
    globalThis.__godotWorkletContext =
      Worklets.createContext("ReactNativeGodot");
    let result = GodotInstaller.installTurboModule();
    if (!result) {
      console.log("Failed NativeGodotModule.installTurboModule()");
    }
  }

  if (globalThis.RTNGodot == null) {
    console.log("Unable to load NativeGodotModule: " + globalThis.RTNGodot);
  }
} else {
  console.log("NativeGodotModule loaded.");
}

export const RTNGodot = globalThis.RTNGodot as GodotModuleInterface;

export function runOnGodotThread<T>(f: () => T): Promise<T> {
  console.log("Calling: runOnGodotThread");
