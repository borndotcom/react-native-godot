/**
 * Sample React Native App
 * https://github.com/facebook/react-native
 *
 * @format
 */

import 'setimmediate'; // Required by New Architecture
import React, {useEffect} from 'react';
import {
  RTNGodot,
  RTNGodotView,
  runOnGodotThread,
} from '@borndotcom/react-native-godot';
import * as FileSystem from 'expo-file-system/legacy';
import {Button, StyleSheet, View, Platform} from 'react-native';

import {NavigationContainer} from '@react-navigation/native';
import {createNativeStackNavigator} from '@react-navigation/native-stack';
import type {NativeStackScreenProps} from '@react-navigation/native-stack';

import * as Device from 'expo-device';

type RootStackParamList = {
  MainWindow: undefined;
  SubWindow: undefined;
};

const Stack = createNativeStackNavigator<RootStackParamList>();

function initGodot(name: string) {
  if (RTNGodot.getInstance() != null) {
    return;
  }

  runOnGodotThread(() => {
    'worklet';

    if (Platform.OS === 'android') {
      RTNGodot.createInstance([
        // Uncomment and fill in the correct IP address and port for debugging in the Godot Editor.
        // Check the documentation for the complete procedure.
        // "--remote-debug",
        // "tcp://IP_ADDRESS:6007",
        '--verbose',
        '--path',
        '/' + name,
        '--rendering-driver',
        'opengl3',
        '--rendering-method',
        'gl_compatibility',
        '--display-driver',
        'embedded',
      ]);
    } else {
      const args = [
        // Uncomment and fill in the correct IP address and port for debugging in the Godot Editor.
        // Check the documentation for the complete procedure.
        // "--remote-debug",
        // "tcp://IP_ADDRESS:6007",
        '--verbose',
        '--main-pack',
        FileSystem.bundleDirectory + name + '.pck',
        '--display-driver',
        'embedded',
      ];

      if (Device.isDevice) {
        args.push(
          '--rendering-driver',
          'opengl3',
          '--rendering-method',
          'gl_compatibility',
        );
      } else {
        args.push(
          '--rendering-driver',
          'metal',
          '--rendering-method',
          'mobile',
        );
      }

      RTNGodot.createInstance(args);
    }
  });
}

function pauseGodot() {
  RTNGodot.pause();
}

function resumeGodot() {
  RTNGodot.resume();
}

function destroyGodot() {
  runOnGodotThread(() => {
    'worklet';
    RTNGodot.destroyInstance();
  });
}

export interface AppController {
  open_window(windowName: string): void;
  close_window(windowName: string): void;
}

const instance = () => {
  'worklet';

  return RTNGodot.getInstance();
};

const appController = () => {
  'worklet';
  if (!instance()) return null;

  const Godot = RTNGodot.API();
  const engine = Godot.Engine;
  const sceneTree = engine.get_main_loop();
  const root = sceneTree.get_root();
  const controller = root.find_child(
    'AppController',
    true,
    false,
  ) as AppController;

  if (!controller) return null;

  return controller;
};

function openSubwindow() {
  runOnGodotThread(() => {
    'worklet';
    const controller = appController();
    if (!controller) return;
    controller.open_window('subwindow');
  });
}

function closeSubwindow() {
  runOnGodotThread(() => {
    'worklet';
    const controller = appController();
    if (!controller) return;
    controller.close_window('subwindow');
  });
}

function MainWindow({
  navigation,
}: NativeStackScreenProps<RootStackParamList, 'MainWindow'>) {
  return (
    <View style={styles.container}>
      <View style={styles.buttonContainer}>
        <Button
          title="Start 1"
          onPress={() => initGodot('GodotTest')}
        />
        <Button
          title="Start 2"
          onPress={() => initGodot('GodotTest2')}
        />
        <Button title="Stop" onPress={destroyGodot} />
        <Button title="Pause" onPress={pauseGodot} />
        <Button title="Resume" onPress={resumeGodot} />
        <Button
          title="Open Window"
          onPress={() => {
            navigation.navigate('SubWindow');
          }}
        />
      </View>
      <View style={styles.godotContainer}>
        <RTNGodotView style={styles.godot} />
      </View>
    </View>
  );
}

function SubWindow({
  navigation,
}: NativeStackScreenProps<RootStackParamList, 'SubWindow'>) {
  useEffect(() => {
    openSubwindow();
    return closeSubwindow;
  }, []);

  return (
    <View style={styles.container}>
      <View style={styles.buttonContainer}>
        <Button title="Close" onPress={navigation.goBack} />
      </View>
      <View style={styles.godotContainer}>
        <RTNGodotView style={styles.godot} windowName="subwindow" />
      </View>
    </View>
  );
}

const App = () => {
  return (
    <NavigationContainer>
      <Stack.Navigator initialRouteName="MainWindow">
        <Stack.Screen name="MainWindow" component={MainWindow} />
        <Stack.Screen
          name="SubWindow"
          component={SubWindow}
          options={{
            headerBackVisible: false,
          }}
        />
      </Stack.Navigator>
    </NavigationContainer>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 20,
    flexDirection: 'column',
  },
  buttonContainer: {
    flex: 1,
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'center',
    alignItems: 'center',
    height: 20,
  },
  godotContainer: {
    flex: 8,
    padding: 20,
  },
  godot: {
    flex: 1,
    padding: 0,
    margin: 0,
  },
});

export default App;
