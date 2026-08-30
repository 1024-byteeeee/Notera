# Notera

Notera is a local-first sheet music reader for macOS and Windows, built with Qt 6, QML and C++20.

## Development prerequisites

- Qt 6.6+ with `Quick`, `QuickControls2`, `Pdf` and `Sql`
- CMake 3.24+
- Ninja
- A C++20 compiler (Apple Clang on macOS; MSVC on Windows)

Configure and build using a matching preset:

```bash
cmake --preset macos-arm64
cmake --build --preset macos-arm64
```

## Packaging

macOS produces a `.dmg`:

```bash
cpack --config build/macos-arm64/CPackConfig.cmake -G DragNDrop
```

Windows produces an `.msi`; configure with the `windows-x64` preset on Windows and install WiX Toolset first:

```powershell
cpack --config build/windows-x64/CPackConfig.cmake -G WIX
```

The Qt deployment script is run at install/package time, so the packaged app includes
its required Qt frameworks on macOS and DLLs on Windows.

Phase 1 establishes the shell and architecture only. Importing, persistence and rendering are introduced in later phases.

## Continuous packaging

GitHub Actions runs the packaging workflow manually or when a `v*` tag is pushed.
It builds a universal macOS DMG (Apple Silicon and Intel) and a Windows x64 MSI,
then uploads both as workflow artifacts.
