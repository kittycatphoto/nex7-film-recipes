# NEX-7 Film Recipes

Turn the Sony NEX-7's Creative Style menu into a **12-look film recipe selector**, with automatic
Picture Profile and white-balance handling. Standard stays neutral. No firmware is flashed.

## What you get

- 12 named Creative Style recipes, applied automatically when selected.
- Live WB mode, Kelvin temperature, and adjustment values for every recipe.
- An interactive web configurator for enabling, disabling, renaming, and tuning looks.
- A self-contained installer bundle with source, prebuilt add-on, USB tooling, and uninstall support.
- Host-side tests for the generation guard, OSAL message layout, and UXC string-bank rewriting.

**Compatibility:** Sony NEX-7, firmware **1.03 only**. The installer currently supports **macOS** with
Homebrew. Other camera models and operating systems are not supported by this package.

## Quick install

1. Download `nex7-base-installer.zip` from the project page or create a customized bundle.
2. Unzip it and open a terminal in the extracted directory.
3. Set the camera's USB Connection to **Mass Storage**, connect it, and eject the card volume in Finder.
4. Run:
   ```sh
   ./install.sh
   ```
   The script asks for your password, installs libusb if needed, uploads the add-on to `/tmp`, and runs
   the isolated Stage A test. It installs only after that test passes, then uses a read-only updater
   session to force a cold boot. When the camera says **“OK to reset”**, press **OK** on the camera.
5. Open **Creative Style** and choose a look.

The installer does **not** update firmware or rewrite the settings store. It installs files under
`/setting`, sets `LD_PRELOAD`, and uses the camera's normal debug environment path. Recipes and WB changes
are applied in RAM through the camera's existing message APIs.

## Customize

Open the project page to:

- enable or disable individual looks;
- rename entries (up to 13 bytes);
- change base style, saturation, hue, PP shift, WB mode, temperature, and adjustments;
- download a customized self-contained installer with `nexhook-recipes.bin` and `string_english.uxc`
  regenerated locally in your browser.

## Return to stock

Connect the camera in Mass Storage mode, eject the card, and run:

```sh
./uninstall.sh
```

This removes `/setting/env.txt` and `/setting/mode/dmode`, then forces a cold boot. Files left on
`/setting` are inert without the preload environment. You can also delete them later from a service shell.

## Safety

- **Never flash firmware or a patched app image.** A crashing main app has no known USB recovery route.
- Every add-on build must pass its own Stage A test before boot installation.
- Do not overwrite the currently mapped add-on or delete its live crash guard.
- Do not use this on another firmware version without revalidation.
- The project is unofficial and has no warranty. You are responsible for your camera and data.

See `docs/INSTALL.md` for detailed installation and recovery instructions and `docs/RECIPES.md` for the
complete style/WB mapping.

## Build and test

Prebuilt artifacts are included, so building is optional. To rebuild the add-on, install `clang` and `lld`
and provide `firmware/libsonydl.so` from your own firmware extraction, then run:

```sh
./build.sh
./test.sh
python3 scripts/package.py
```

The page's configurator and base installer are generated locally; no camera is needed for tests.

## License and third-party code

Project code is MIT licensed. Sony-PMCA-RE and fflate retain their own MIT notices. See
`THIRD_PARTY_NOTICES.md`. Sony and NEX are trademarks of Sony Corporation; this is an independent project.
