# AdhanBar

AdhanBar is a lightweight Windows taskbar widget that shows the next prayer time and a countdown. It is built with native C++ and the Win32 API; it does not require the .NET runtime.

## Features

- Shows only the next prayer and the time remaining.
- Blinks for one minute when a prayer time arrives.
- Runs from the system tray and places the widget in the Windows taskbar.
- Supports Bahasa Indonesia and English; the widget, tray menu, and settings follow the selected language.
- Uses `app.ico` for the application window and system-tray icon.
- Lets you change the background color and transparency. `100%` makes the background fully transparent while keeping the text visible.
- Lets you set the widget font size from 8 to 16 pt.
- Lets you adjust the widget's horizontal position in pixels.
- Retrieves prayer times by city from the Aladhan API.
- Lets you configure the country and optional Aladhan calculation method for that city.
- Retries failed schedule downloads every 30 seconds and refreshes the schedule when the local date changes.
- Applies a selected city and saves it only after its schedule is downloaded successfully.

## Requirements

- Windows 10 or later.
- Internet access to retrieve prayer times.
- Zig C++ compiler on `PATH` to use `build.bat`.

## Build

Run `build.bat` from the project directory. The executable is written to:

```text
native-build\AdhanBar.exe
```

Alternatively, build with CMake and a Windows C++ toolchain:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Use

Run `AdhanBar.exe`. The widget appears in the taskbar; the app itself is controlled from its system-tray icon. Right-click the tray icon to open settings, refresh prayer times, show or hide the widget, or exit.

The settings include the city, taskbar position offset, background color and transparency, display language, and font size.

## Settings file

Settings are stored at:

```text
%LOCALAPPDATA%\AdhanBar\settings.json
```

Settings are stored locally on the device.
