# Optional video controls (feature/video-options)

Based on MattPonton/doa3-recomp at `7f5a76314bf0091352fa98978c56ce833cbbc60f`.
These changes need a Windows build and in-game visual testing before release.
They are not a tested binary release.

## Escape > Video

- **Custom render resolution** exposes **Render width**, **Render height** and
  **Apply render resolution**. This changes the internal colour/depth targets,
  not the window size. Dimensions may be non-integer multiples of 480, including
  odd sizes. Each side must be 320-8192 pixels, with at most 16,777,216 total
  pixels. Actual GPU feature-level limits are also checked. Allocation may still
  fail on a low-memory GPU. The old targets and accepted settings are retained
  on failure and a message appears in Video. Existing 1x-4x presets remain.
  Aspect ratio controls the game camera separately from pixel dimensions.
- **4:3 stretched (fill window)** selects the original 4:3 Full Screen camera and
  stretches its completed image across the client area, without black bars.
  The guest HUD retains its original 4:3 layout, so it stretches with the image.
  The host settings menu is still drawn last at window resolution. Selecting
  another aspect in the UI turns stretch off. The saved stretch flag is only
  effective in native 4:3 Full Screen, not Anamorphic or widescreen modes.
- **Disable fog** is off by default. It suppresses fixed-function distance fog
  and forces the no-fog factor in the direct NV2A final combiner. It does not
  use the developer untextured mode or remove textures. Guest fog state is
  retained, so switching it off restores the game's current fog settings.

Use **Save** after applying changes. **Reset to defaults** restores the original
16:9/3x settings and turns stretch and fog disable off. The ordinary controls
retain their previous behavior with the new options off.

## INI keys

All keys are in `[Video]` in `doa3_settings.ini` next to the executable:

```ini
CustomResolution=0
RenderWidth=2560
RenderHeight=1440
Stretch4_3=0
DisableFog=0
```

`CustomResolution=1` uses the explicit dimensions. `InternalResolution` remains
1-4 for older builds; custom mode writes a compatible 3x fallback there. Missing
keys keep old behavior. Invalid custom sizes fall back to the saved valid preset,
or 3x if that preset is invalid. The new options do not modify game assets.

## Tests

From a Visual Studio developer command prompt:

```bat
cmake -S doa3/tests/video_settings -B build-video-tests
cmake --build build-video-tests --config Release
ctest --test-dir build-video-tests -C Release --output-on-failure
build-video-tests\Release\fog_shader_test.exe --compile
```

The normal test suite covers existing aspect/projection/HUD behavior, INI
round-trips, custom bounds and invalid settings, stretch and fog persistence,
all five replacement-resource allocation failure points, successful target
replacement, frame-history invalidation, and the generated final-combiner fog
override/CPU constant-buffer layout. `--compile` additionally compiles generated
HLSL with the local Direct3D compiler.

During implementation the Windows tests were cross-built with MinGW and the
normal tests passed under Wine. All modified renderer and UI translation units
passed Windows cross-compiler syntax checks. Wine 6's shader compiler rejected
the generated shader, so successful native HLSL compilation is NOT claimed.
The full game was not built or run, and no in-game pixels or UI layout were
visually verified.

## Windows visual acceptance checklist

1. With new options off, compare the same scenes against the original build at
   all five aspects and 1x-4x. Check menus, character select, stage select and
   fights. No new camera, HUD, colour or fog changes should appear.
2. Try 1280x720, 1920x1080, 1921x1081 and 3840x2160. Confirm screenshot pixel
   dimensions, viewport/scissor edges, menu/model alignment, movie transitions,
   photo mode and framebuffer/after-image effects. Resize repeatedly with the
   asynchronous GPU path both enabled and disabled.
3. Reject too-small, too-large and over-budget inputs. Simulate allocation
   failure on the target machine if possible. The old image/settings must stay
   usable; the overlay must show the failure rather than leave a black target.
4. On a widescreen window, toggle 4:3 stretch. The original 4:3 camera/HUD must
   fill the window when enabled. The host menu must not stretch. Verify both
   windowed and borderless modes and transitions between aspect choices.
5. On foggy stages, toggle Disable fog in both directions, including during a
   fight and after a movie. Check characters, lighting, water/reflections,
   transparency, particles, menus and transitions. Test the normal NV2A path
   and the developer `DOA3_NO_COMBINERS=1` fallback separately.
6. Save/restart each combination, then reset and save/restart. Verify old INI
   files still load. Keep master unchanged until this checklist passes.

No pull request to upstream or release publication is part of this branch.
