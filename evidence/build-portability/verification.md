# Build entry validation — 2026-10-27

- CMake4.4.4 obtained from the cmake registry package into /tmp/explicit-ui-cmake. No system install/settings changes.
- Linux Release configure/build succeeded for eui-scripted, eui-live, eui-window. Graphics dependencies resolved to installed Linux runtimes through explicit fallback.
- Default window layout is examples/responsive.layout.json (v2); final configuration explicitly selected this path to avoid previous CMake cache state.
- CTest: scripted_smoke and window_synthetic both passed, 0.64seconds total. Synthetic uses SDL dummy video and is not visible desktop proof.
- Separate EUI_BUILD_WINDOW=OFF configuration/build succeeded and its one scripted CTest passed.
- EUI_LINUX_RUNTIME_FALLBACK=OFF failed configure as expected because this environment has no SDL/Cairo development metadata. Error provides missing dependencies and core-only alternative; log retained.
- No macOS/Windows compilation, execution, installation, signing or packaging was attempted or claimed. No corresponding SDK/compiler/runtime was available.
- Parent runs broader regression/integration separately; this test is not a replacement for Python unit suites.
