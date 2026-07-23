module.exports = {
  root: true,
  extends: "@react-native",
  overrides: [
    {
      files: ['app.plugin.js'],
      env: {node: true},
      parserOptions: {requireConfigFile: false},
    },
  ],
};
