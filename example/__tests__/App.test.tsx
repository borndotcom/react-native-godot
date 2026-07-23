/**
 * @format
 */

import "react-native";
import ReactTestRenderer from "react-test-renderer";
import App from "../App";

jest.mock("@borndotcom/react-native-godot", () => {
  const React = require("react");
  const { View } = require("react-native");

  return {
    RTNGodot: {
      getInstance: jest.fn(() => null),
      pause: jest.fn(),
      resume: jest.fn(),
    },
    RTNGodotView: (props: object) => React.createElement(View, props),
    runOnGodotThread: jest.fn(() => Promise.resolve()),
  };
});

jest.mock("expo-file-system/legacy", () => ({
  bundleDirectory: "file:///",
}));

jest.mock("expo-device", () => ({
  isDevice: false,
}));

test("renders correctly", async () => {
  await ReactTestRenderer.act(() => {
    ReactTestRenderer.create(<App />);
  });
});
