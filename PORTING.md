# Custom Keybinds v2.3.0 (Geode 6 / Geometry Dash 2.2091)

This source port updates the mod's compatibility metadata for Geode 6.0.0 and
Geometry Dash 2.2091. It preserves the original 82 keybind settings and their
IDs, defaults, priorities, categories, and `migrate-from` identifiers.

## iOS build

The existing CMake project already selects the `arm64` architecture for iOS.
The existing GitHub Actions workflow already includes an iOS build job on
`macos-latest` using `target: iOS`.

Push this source tree to a GitHub repository and run the **Build Geode Mod**
workflow. Download the **Build Output** artifact after the workflow succeeds.
The resulting iOS `.geode` package is the installable artifact.

## Important

This is a source/metadata port, not a precompiled iOS release. The C++ sources
were reviewed against the current Geode keybind-setting/event API documentation,
which still documents `KeybindSettingPressedEventV3`, `Keybind`, and keybind
settings. However, this environment does not include Apple's iOS SDK or the
Geode 6 SDK, so an iOS compile and on-device runtime test have not been run.
Do not treat this package as a verified installable `.geode` until the iOS
workflow completes successfully and the result is tested on-device.
