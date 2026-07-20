# Pr0mt_X Text

Pr0mt_X Text is a lightweight native Windows app for sending text to ChatGPT with reusable prompt presets. It began as a grammar helper and now works with any text-based prompt workflow.

The app does not require an API key. It prepares the selected prompt and text, then opens a temporary ChatGPT conversation in the user's default browser.

## Features

- Create, rename, edit, delete, and reorder prompt presets
- Remember the most recently selected preset
- Paste text or use Paste&Go to paste and send in one action
- Preserve multiline text and formatting
- Keep an optional local history with the preset used for each entry
- Restore or delete individual history entries and clear all history
- Store presets, settings, and history beside the executable
- Native dark Windows interface with no runtime dependencies

## Requirements

- Windows 10 or Windows 11
- A web browser and a ChatGPT account
- For building: CMake 3.16 or newer and Visual Studio Build Tools with the Desktop development with C++ workload

## Download

Download the current Windows executable from the [latest GitHub release](https://github.com/StriK3FoRC3/Pr0mt_X-Text/releases/latest). No installation is required.

## Build

Run `build.bat` from the project directory. The release executable will be created at:

```text
build\Release\Pr0mt_X Text.exe
```

You can also build directly with CMake:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

## Local Data

Pr0mt_X Text creates these files beside the executable:

- `presets.dat`: prompt presets
- `settings.dat`: selected preset and history preference
- `history.dat`: saved input history

These files are plain local application data and are excluded from Git. History saving is enabled by default and can be disabled in Settings.

When Send or Paste&Go is used, the selected prompt and entered text are placed in the ChatGPT URL opened by the browser. The app itself does not connect to an API or collect telemetry.

## Contributing

Bug reports, feature ideas, and noncommercial contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a pull request.

## License

Pr0mt_X Text is available under the [PolyForm Noncommercial License 1.0.0](LICENSE). You may use, modify, and distribute it for permitted noncommercial purposes. Commercial use is not granted by this license.

This project is not affiliated with or endorsed by OpenAI.
