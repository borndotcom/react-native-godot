const {
  IOSConfig,
  withGradleProperties,
  withXcodeProject,
} = require('expo/config-plugins');

const ANDROID_PROPERTIES = {
  'android.minSdkVersion': '29',
  reactNativeArchitectures: 'armeabi-v7a,arm64-v8a',
};

function withGodotAndroid(config) {
  return withGradleProperties(config, projectConfig => {
    for (const [key, value] of Object.entries(ANDROID_PROPERTIES)) {
      const property = projectConfig.modResults.find(
        entry => entry.type === 'property' && entry.key === key,
      );

      if (property) {
        property.value = value;
      } else {
        projectConfig.modResults.push({type: 'property', key, value});
      }
    }

    return projectConfig;
  });
}

function withGodotPacks(config, iosPacks) {
  if (!iosPacks.length) {
    return config;
  }

  return withXcodeProject(config, projectConfig => {
    const project = projectConfig.modResults;
    IOSConfig.XcodeUtils.ensureGroupRecursively(project, 'Resources');

    for (const pack of iosPacks) {
      IOSConfig.XcodeUtils.addResourceFileToGroup({
        filepath: pack,
        groupName: 'Resources',
        isBuildFile: true,
        project,
        verbose: true,
      });
    }

    return projectConfig;
  });
}

module.exports = (config, {iosPacks = []} = {}) =>
  withGodotPacks(withGodotAndroid(config), iosPacks);
