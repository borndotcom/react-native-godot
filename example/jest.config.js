module.exports = {
  preset: "@react-native/jest-preset",
  transformIgnorePatterns: [
    "node_modules/(?!((jest-)?react-native|@react-native(-community)?|@react-navigation/.*|expo|expo-.*|@expo/.*|react-native-worklets)/)",
  ],
};
