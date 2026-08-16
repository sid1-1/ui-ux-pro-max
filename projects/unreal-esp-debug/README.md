# ESP Debug Overlay — Unreal Engine plugin

A development-only on-screen tracking overlay for **your own** actors. Drop a component on
a character, set the HUD class, and you get bounding boxes, health bars, labels, distance,
occlusion tinting, skeleton lines, off-screen arrows, a radar, and CSV session recording.

The entire draw path is wrapped in `#if !UE_BUILD_SHIPPING`, so it compiles out of Shipping
builds.

Tested against the UE5 API surface (5.0–5.5). UE4 is handled with a version guard in
`EspProjection.h`.

---

## 1. Get it onto your Mac

The plugin lives in this repo at `projects/unreal-esp-debug/EspDebug`. On your Mac:

```bash
git clone --branch claude/esp-tracking-game-character-wjjakv \
  https://github.com/sid1-1/ui-ux-pro-max.git ~/Downloads/esp-src
```

Already have the repo cloned? Just `git fetch origin claude/esp-tracking-game-character-wjjakv`
and `git checkout` that branch.

Then copy the plugin into your Unreal project:

```bash
mkdir -p "/path/to/YourGame/Plugins"
cp -R ~/Downloads/esp-src/projects/unreal-esp-debug/EspDebug "/path/to/YourGame/Plugins/"
```

You should end up with `YourGame/Plugins/EspDebug/EspDebug.uplugin`.

No git? Open the branch on github.com, **Code → Download ZIP**, unzip, and drag the
`EspDebug` folder into `YourGame/Plugins/`.

---

## 2. Prerequisites

| Requirement | Why | Check |
|---|---|---|
| **Xcode** (full app, not just CLI tools) | This is a C++ plugin; UE needs a compiler | `xcodebuild -version` |
| Xcode command line tools | Same | `xcode-select -p` |
| Unreal Engine 5.x | Launcher install is fine | Epic Games Launcher → Library |

**If your project is Blueprint-only**, adding a C++ plugin makes it a C++ project. That's
fine and reversible — Unreal will just ask to compile on next open. If you'd rather do it
explicitly first: in the editor, **Tools → New C++ Class → None → Create Class**. That
generates the `Source/` folder and the Xcode project.

---

## 3. Build it

Easiest path — **just open your `.uproject`**. Unreal detects the new module and shows:

> The following modules are missing or built with a different engine version… Would you
> like to rebuild them now?

Click **Yes**. First compile takes a few minutes.

If that dialog fails, build from the terminal (swap `UE_5.4` for your version and fix the
paths):

```bash
"/Users/Shared/Epic Games/UE_5.4/Engine/Build/BatchFiles/Mac/Build.sh" \
  YourGameEditor Mac Development \
  -project="/path/to/YourGame/YourGame.uproject" -waitmutex
```

Real errors show up in that output, which is much easier to read than the editor dialog.

To regenerate the Xcode workspace:

```bash
dotnet "/Users/Shared/Epic Games/UE_5.4/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" \
  -projectfiles -project="/path/to/YourGame/YourGame.uproject" -game -rocket -progress
```

Then confirm the plugin is on: **Edit → Plugins → Debugging → ESP Debug Overlay**.
(Plugins in a project's `Plugins/` folder are enabled by default.)

---

## 4. Wire it up — two steps

**a. Point your GameMode at the HUD.** Open your GameMode Blueprint → Class Defaults →
**HUD Class → EspHUD**. If you already have a HUD Blueprint you care about, instead open it
→ **File → Reparent Blueprint → EspHUD**.

**b. Tag what you want tracked.** Open your character Blueprint → **Add Component → ESP
Target**. Set `Label` and `Color` in the Details panel. Repeat on the third character.

That's it. Press Play.

To make the health bar real, drive `Health01` from your own health logic — in Blueprint:
`Get ESP Target → Set Health01` with `CurrentHealth / MaxHealth`.

---

## 5. Controls

Open the console (**`~`** in editor, **four-finger tap** on a mobile development build):

| Command | Effect |
|---|---|
| `EspToggle` | All overlay drawing on/off |
| `EspRadar` | Radar on/off |
| `EspSkeleton` | Skeleton lines on/off |
| `EspSnaplines` | Snaplines on/off |
| `EspRecord 1` / `EspRecord 0` | Start / stop recording positions |
| `EspDump` | Write recording to `Saved/Esp/session.csv` |

All of these are also `BlueprintCallable`, so you can bind them to an on-screen debug button
for touch-only testing.

Pulling the CSV off a device:

```bash
# Android
adb pull /sdcard/Android/data/<your.package>/files/UnrealGame/YourGame/Saved/Esp/session.csv

# iOS — set bSupportsItunesFileSharing=true in Info.plist, then use Finder → device → Files
```

---

## 6. Running it on the phone, mirrored to your Mac

The overlay is drawn by the game itself, so **anything that mirrors the device screen shows
it automatically** — no extra setup.

- **iPhone:** cable to Mac → QuickTime Player → File → New Movie Recording → click the arrow
  next to the record button → pick your iPhone as the camera source.
- **Android:** `brew install scrcpy` then `scrcpy` with USB debugging enabled.

For fast iteration you often don't need the device at all: **Play → New Editor Window (PIE)**
with the Mobile preview renderer catches the vast majority of overlay issues. Go to device
when you're checking real touch input or real framerate.

To get it on the device: **Platforms → iOS/Android → Package Project**, with build
configuration set to **Development** (Shipping compiles the overlay out). iOS also needs a
signing certificate and provisioning profile set in **Project Settings → iOS**.

---

## 7. Tuning

All on the `EspHUD` class defaults:

| Setting | Default | Note |
|---|---|---|
| `MaxDistance` | 8000 cm | Targets past this are skipped entirely |
| `TraceInterval` | 3 | Visibility traces per target every N frames — the main perf dial |
| `SkeletonDistanceFraction` | 0.4 | Skeleton is 19 projections + 19 lines per character |
| `LabelDistanceFraction` | 0.5 | Canvas text is the priciest draw call on mobile |
| `OccludedAlpha` | 0.35 | Dim factor for targets behind geometry |
| `RadarWorldRange` | 5000 cm | World distance mapped to the radar edge |

---

## 8. Troubleshooting

| Symptom | Cause |
|---|---|
| Nothing draws at all | HUD class not set on the GameMode, or `ShowHUD` is off — type `ShowHUD` in the console |
| Boxes but no skeleton | Your rig's bone names differ. Check the log for the `[ESP] … no bone named 'pelvis'` warning and edit `GBoneLinks` in `EspHUD.cpp` |
| Boxes drift from the character | Actor pivot isn't at the capsule centre. Turn off `bCacheBounds` on the component |
| Everything reads as occluded | The visibility trace is hitting the target's own collision. Confirm the actor is passed to `AddIgnoredActor` (it is by default) — usually means a second collision actor is parented to it |
| Arrows point the wrong way | You modified `DrawOffscreenArrow` and dropped the `IsInFront()` un-mirror step |
| Editor won't open after copying | Missing Xcode. Install it, then rerun the `Build.sh` command in step 3 |

---

## 9. A note on shipping

This is a debug tool for a game you control. If your game ever gets a multiplayer mode, an
overlay that reveals other players through walls is only safe because you own the client —
the durable fix isn't hiding the overlay, it's making sure the **server never replicates**
positions a client shouldn't know (network relevancy / dormancy). Build that assumption in
now and this stays a harmless dev tool forever.

The `#if !UE_BUILD_SHIPPING` guard covers the draw path. For belt and braces, also strip the
HUD class in Shipping:

```cpp
#if UE_BUILD_SHIPPING
    HUDClass = AHUD::StaticClass();
#endif
```
