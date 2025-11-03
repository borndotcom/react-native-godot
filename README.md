![Cover-21](https://github.com/user-attachments/assets/770e4972-84f7-433e-87db-6391601256ba)
Born React Native Godot
-----------------------

React Native Godot allows embedding the Godot Engine into React Native applications.

Born React Native Godot was created by [Born](https://born.com) and developed by [Migeran](https://migeran.com), in close collaboration between the two teams.

# Main Features

* Supports Android and iOS, built on [LibGodot](https://github.com/migeran/libgodot).
* Stable implementation serving millions of users in [Born](https://born.com)'s applications.
* Supports starting, stopping and restarting the Godot Engine. [(docs)](#initialize-the-godot-instance)
* When restarting, the engine can be reconfigured, so a different Godot app may be loaded each time. [(docs)](#stop-the-godot-instance)
* It is also possible to pause and resume the running Godot instance. [(docs)](#pause-the-godot-instance)
* Godot is running on a separate thread, so it does not affect the main thread of the application nor the React Native JavaScript thread. [(docs)](#threading-and-javascript-in-react-native)
* The Godot main window and any subwindows created by the Godot app may be embedded into the React Native application either on the same screen, or on separate screens (see [example app](example/)).
* The whole Godot API is accessible from TypeScript / JavaScript. It is possible to instantiate objects, call methods, get and set properties, attach JS functions to signals, provide JS functions as callables to Godot methods ... etc. [(docs)](#godot-api-usage)

<p align="center">
  <video src="https://github.com/user-attachments/assets/33266f05-d733-4c1d-ab49-edaaf426e3e1" width="600" controls></video>
</p>

# Getting Started with the Example App

The [example app](example/) shows the main features of React Native Godot in action.

## Install Prerequisites

During development we use [ASDF](https://asdf-vm.com/) to manage most external dependencies required for React Native development, like node, java, gradle or ruby. If you also use ASDF, just run:

```sh
asdf install
```
    
This will make sure that all the dependencies are the same like in our environment. Otherwise you may also install React Native prerequisites using any other method.

## Export the Godot app

Run the following scripts for either platform you plan to test (or both):

```sh
cd example
./export_godot_GodotTest.sh android
./export_godot_GodotTest.sh ios
./export_godot_GodotTest2.sh android
./export_godot_GodotTest2.sh ios
```

The script is configured to look for Godot in the standard system wide installation folder on MacOS. If your Godot is installed elsewhere, or you are on Linux, just point the GODOT_EDITOR
environment variable to your GODOT EDITOR prior to running the above scripts:

```sh
export GODOT_EDITOR=/path/to/godot_editor
```

## Configure and download LibGodot

```sh
cd example
yarn
yarn download-prebuilt
```

These commands will resolve all the React Native and other dependencies from NPM. The second one will download the prebuilt LibGodot release from GitHub.

## Run on the iOS Simulator

```sh
cd example/ios
bundle install
bundle exec pod install
cd ..
yarn ios
```

## Run on the Android Emulator

```sh
cd example
yarn android
```

## Use your native IDEs

You may use Xcode and Android Studio the same way as with any other project. Just open:

* ``ios/GodotTest.xcworkspace`` from Xcode
* ``android`` from Android Studio

Note: If you are using ASDF to manage your java and Node dependencies, you should start Android Studio from under the react-native-godot (or example) folder, so it can find these tools. For example on MacOS:

```sh
cd example
open -a "Android Studio"
```

## Convenience script for dependency management

There is an update_deps.sh script included in the example app's folder. It will execute all the setup commands for both iOS and Android in one step, so you may start your work immediately.

```sh
cd example
./update_deps.sh
yarn ios # or yarn android
```

# Your first React Native Godot App

Born React Native Godot is distributed on NPM.

Just follow these steps to add it to your React Native application:

## Update package.json

```sh
yarn add @borndotcom/react-native-godot
```

## Download the prebuilt LibGodot packages

The LibGodot packages used by React Native Godot are not distributed on NPM. Instead, they are downloaded separately by issuing the following command:

```sh
yarn download-prebuilt
```

This way React Native Godot can be updated independently from LibGodot, and also local, customized builds of LibGodot are supported.

## Import React Native Godot in your App code

```typescript
import { RTNGodot, RTNGodotView, runOnGodotThread } from "@borndotcom/react-native-godot";
```

## Add the Godot View to your view, e.g.

```typescript
const App = () => {
	return (
        <View>
            <RTNGodotView style={...}/>
        </View>
	)
};
```

If no windowName property is specified, that view is for the main window of Godot.

## Initialize the Godot instance

We will also add Expo Filesystem module for handling file system paths from React Native
easily.

```sh
yarn add expo-file-system
```

```typescript
import * as FileSystem from 'expo-file-system/legacy';

function initGodot() {
  runOnGodotThread(() => {
    'worklet';
    console.log("Initializing Godot");

    if (Platform.OS === 'android') {
        RTNGodot.createInstance(
            ["--verbose", "--path",
            "/main",
            "--rendering-driver", "opengl3",
            "--rendering-method", "gl_compatibility",
            "--display-driver", "embedded"]
        );  
    } else {
        RTNGodot.createInstance(
            ["--verbose", "--main-pack",
            FileSystem.bundleDirectory + "main.pck",
            "--rendering-driver", "opengl3",
            "--rendering-method", "gl_compatibility",
            "--display-driver", "embedded"]
        );  
    }
}
```
A couple of things to note here:

* The usual Godot command line parameters can be passed to the initialization function.
* It is key to use the "embedded" display driver, which is required to embed Godot into the React Native application.
* It is possible to specify both a directory or a pack file.
  * On Android, inside the main package the access of the pack file's contents is much slower than accessing pack files stored in the private area of the application. If the Godot app is stored inside the main package, then it should be stored as a folder of files in the asset folder. 
  * On iOS, there is no such limitation, so we use a pack file there. 
* In many cases the best way is to download the Godot apps at runtime, which has many advantages, including:
  * Smaller initial application size
  * Deliver specialized app builds for different devices
  * Update the Godot app without going through the whole app review process

## Add the Godot initialization to the App component

```typescript
const App = () => {
    useEffect(() => {
      initGodot()
      return () => {
      }
    }, [])

    return (
        <View>
            <RTNGodotView style={...}/>
        </View>
	)
};
```

In this Hello World app, we just initialize Godot when the App view is displayed.
Check out the [example app](example/) in this repository for a more elaborate example.

## Stop the Godot instance

To stop a running Godot instance, call RTNGodot.destroyInstance() on the Godot thread:

```typescript
function destroyGodot() {
  runOnGodotThread(() => {
    "worklet";
    RTNGodot.destroyInstance();
  });
}
```

NOTE: After stopping an instance, you can start a new one by calling RTNGodot.createInstance again. The new instance can be started with different parameters including other Godot projects.

## Pause the Godot instance

To pause a running Godot instance, call RTNGodot.pause() on the Javascript main thread:

```typescript
RTNGodot.pause();
```

This won't shut down the running instance, just halt it's execution until further notice.

## Resume the Godot instance

To resume a paused Godot instance, call RTNGodot.resume() on the Javascript main thread:

```typescript
RTNGodot.resume();
```

## Export your Godot project

You may use the usual export functionality of Godot Engine, just make sure to export to PCK or ZIP and not the whole application.

In the React Native Godot example application an ``export_godot.sh`` and an associated ``export_godot_GodotTest.sh`` script is provided, which can make the export process easier.

To export your project to iOS or Android, use our included export_godot.sh script with the following parameters:

```
--target: Base directory where the project will be exported. Depending on the platform a pck file or a folder will be created here.
--project: The directory of the project that will be exported.
--name: The name of the exported project.
--preset: The export preset to be used.
--platform: The platform to export the project to (iOS or Android).
```

Notes:
* Android projects will be exported into project folders while iOS project will be exported as PCK files.
* By default, export_godot.sh will look for an official Godot installation under the Applications folder. If your installation is somewhere else or you would like to use a custom Godot build, specify it's location in the GODOT_EDITOR environment variable.

# Godot API Usage

After importing React Native Godot, you can access the Godot API through the RTNGodot class.

## Get the API entry point

To access the Godot API, first call RTNGodot.API():

```typescript
let Godot = RTNGodot.API();
```

## Access Singletons

From the entry point, you can get the engine's singletons:

```typescript
// Get Godot's Engine singleton.
let Godot = RTNGodot.API();
var engine = Godot.Engine;
```

## Instantiate Objects

You can instantiate Godot API objects using their defined constructors:

```typescript
var vector = Godot.Vector2();
```

## Access properties

Object properties can be accessed directly:

```typescript
var vector = Godot.Vector2();
vector.x = 1.0;
vector.y = 2.0;
```

## Call methods

Godot API methods can also be called:

```typescript
let Godot = RTNGodot.API();
var engine = Godot.Engine;

// Call Godot API methods:
var sceneTree = engine.get_main_loop();
var root = sceneTree.get_root();
// NOTE: From here you can access nodes of the scene tree and manipulate them from TypeScript.
```

## Attach to signals

JS functions can be connected to Godot signals:

```typescript
let Godot = RTNGodot.API();

var button = Godot.Button();
button.set_text("Button");

button.pressed.connect(function() {
  console.log("Button pressed.")
})
```

## Pass JS functions to Godot

JS functions can be passed to Godot methods to be used as Callables:

Let's create a custom RNInterface Node, and add it to the Godot project's scene tree. It's GDScript should read:

```gdscript
extends Node
class_name RNInterface

func test_callable(c: Callable) -> void:
	c.call("Hello from Godot")

```

From TypeScript we may access it with the following code snippet:

```typescript
let Godot = RTNGodot.API();
var engine = Godot.Engine;
var sceneTree = engine.get_main_loop();
var root = sceneTree.get_root();

var iface = root.find_child("RNInterface", true, false)

iface.test_callable(function(s: string) {
  console.log("Received text from Godot: " + s)
}); 
```

## Threading and JavaScript in React Native

In a React Native app, the main JavaScript thread, where the bulk of the JavaScript code of the application runs is separate from the Android or iOS apps's main thread.

This way the JS Thread's processing does not affect the main application UI. Following the same pattern, the Godot Engine is also running on its own thread that is separate from both the application's and React Native's main JavaScript thread. As JavaScript is single-threaded by design, to be able to communicate with the Godot thread from JavaScript, we use the well-known [react-native-worklets-core](https://github.com/margelo/react-native-worklets-core) library, which allows us running JS code in the Godot thread using worklets.

Worklets are JavaScript functions designated with a 'worklet' keyword. 

```typescript
function worklet() {
    'worklet'
}
```

These functions and all their external dependencies are transpiled into self contained JS bundles so they can be executed in separate JS contexts associated with separate threads. For more information on how this works, please refer to the [React Native Worklets Core documentation](https://github.com/margelo/react-native-worklets-core/blob/main/docs/USAGE.md).

React Native Godot provides a helper function called ``runOnGodotThread()`` which will allow you to execute such _workletized_ JS functions on the Godot thread.

In general, this is the recommended way of interacting with the Godot Engine, as shown in our example app.

While it is possible to interact with the Godot Engine directly from the main React Native thread, there are some caveats and it is generally not recommended:

* From the Godot Engine's view, such invocations will be executed on a "background" thread, which means, that for example the Scene Tree cannot be fully accessed from this thread directly. For more information on how background threads in Godot work, please refer to its [documentation](https://docs.godotengine.org/en/stable/tutorials/performance/thread_safe_apis.html).

* Godot object references that were obtained in the Main JS thread and in worklets are not interchangeable, because they are associated with separate JS contexts. It is possible to create object references that can work in multiple JS contexts, but these would add runtime overhead and are currently not supported in React Native Godot. If you would require this feature in your application, then please contact [Migeran](https://migeran.com/contact) to discuss your requirements in detail.



# Advanced Topics

## Using a custom LibGodot build

1. To use a custom LibGodot build, you first need to clone the [LibGodot project](https://github.com/migeran/libgodot) on the branch ``libgodot_migeran_45``, including all its submodules.

    ```sh
    git clone --recursive https://github.com/migeran/libgodot -b libgodot_migeran_45
    ```

1. Then build LibGodot from source:

    ```sh
    cd libgodot
    ./build_prebuilt_release.sh   # for release libraries
    # OR
    ./build_prebuilt_dev.sh   # for development libraries
    ```

    NOTE: You may comment out the unnecessary lines in the build script if you're only building for one platform.

    At the end of the process, the necessary files will be produced in the ``build/prebuilt/release`` or the ``build/prebuilt/dev`` folder.

1. Once the build finished, set these environment variables:

    ```sh
    export LIBGODOT_XCFRAMEWORK_PATH=path/to/libgodot/build/prebuilt/release or dev/libgodot.xcframework.zip 
    export LIBGODOT_CPP_XCFRAMEWORK_PATH=path/to/libgodot/build/prebuilt/release or dev/libgodot-cpp.xcframework.zip
    export LIBGODOT_ANDROID_PATH=path/to/libgodot/build/prebuilt/release or dev/libgodot-android.zip
    export LIBGODOT_CPP_ANDROID_PATH=path/to/libgodot/build/prebuilt/release or dev/godot-cpp-android.zip
    export SHASUM_CHECK=false
    export REPLACE_EXISTING=true
    ```

    These environment variables override the download logic of the download-prebuilt script of react-native-godot:

    * Use locally present files instead of the URLs specified in the package.json of react-native-godot.
    * Do not check the provided SHA-256 hash.
    * Force replacing the installed library, even if the same version was already installed.


1. Then run the download-prebuilts script to install the custom built libraries:

    ```sh
    yarn download-prebuilt
    ```

    NOTE: An example usage of this script can be found in our example/update_deps.sh script.

1. On Android, if using the development builds, in the react-native-godot/android/build.gradle the
following line needs to be updated to include ``godot-dev`` as the artifactId:

    ```groovy
    api "com.migeran.libgodot:godot:${libGodotVersion}-SNAPSHOT"
    ```

## Debug Native Godot code

To debug native engine code (C++), you first need to build and install a development version of LibGodot as described above.

On iOS this allows you to set breakpoints into the Godot Engine itself in Xcode and debug it as easily as your own app.

Depending on the Xcode version, you might have to open these files separately in Xcode, and then move the editor from the new window into the window your application's Xcode project.

On Android a couple more steps will be required:

* In the Android Studio Run/Debug configurations you also need to add a Symbol directory, where libgodot_android.so with the included debug symbols is located. One good place is: ``path/to/libgodot/godot/platform/android/java/lib/libs/dev/arm64-v8a``.

* Depending on the Android Studio version, you may have to include the Godot source tree under the Android Studio project to be able to set breakpoints. A way to do this is to create a symbolic link under the React Native Godot library as follows:

```sh
cd /path/to/your/app/node_modules/@borndotcom/react-native-godot/android/src/main/cpp
ln -s /path/to/libgodot/godot godot
```

* Then reimport the project / resync the Gradle project files into Android Studio.

## Debugging Godot Projects in the Godot Editor

### Android

1. For remote debugging, you first need to build and install a development version of LibGodot as described above.

1. Then add these 2 extra parameters to your RTNGodot.createInstance call:

    ```typescript
    '--remote-debug',
    'tcp://127.0.0.1:6007'
    ```

1. To expose TCP port 6007 on your Android device via TCP port 6007 on your computer, open a terminal and call:

    ```sh
    adb reverse tcp:6007 tcp:6007
    ```

    NOTE: This needs to be called again every time you disconnect and reconnect your device.

1. Now you can open your project in the Godot Editor. First enable 'Debug' -> 'Keep Debug Server Open' from the top menu then you can set your breakpoints. You can also set breakpoints while the app is running.

1. Once your breakpoints are set, you can start debugging by launching your app from Android Studio.

### iOS

1. For remote debugging, you first need to build and install a development version of LibGodot as described above.

1. Open the Godot Editor, then go to 'godot' -> 'Editor Settings' -> 'Network' -> 'Debug' -> 'Remote Host' and set your Mac's IP address.

1. Also enable 'Debug' -> 'Keep Debug Server Open'.

1. Then add these 2 extra parameters to your RTNGodot.createInstance call:

    ```typescript
    '--remote-debug',
    'tcp://<your_macs_ip_address>:6007'
    ```

1. Now you can start debugging by launching your app from Xcode. You can set your breakpoints both before running the app or at runtime.

NOTE: Your Mac and your iOS device must be connected to the same network/router in order to establish the connection. 

# Born
We’ve opened new positions in Berlin and New York! We’re looking for the best React Native Engineers who want to reach millions of people every day and work at the frontier of technology.

Check out our openings at: [born.com](https://born.com)

# Commercial Support & Development

[Migeran](https://migeran.com) offers commercial support and development services around React Native Godot, [LibGodot](https://github.com/migeran/libgodot) and the Godot Engine. [Start Here](https://migeran.com/contact) with describing your requirements.

# License

Born React Native Godot is released under the MIT license.
