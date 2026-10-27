## Neo — a fast, cross-platform desktop calculator with:
- **Developer shortcuts**
- **Custom formulas**
- **In-app session history**

*Made on C++ with Qt Framework. Designed in 70s matrix style.*

#### Download:

Prebuilt packages are attached to the latest [GitHub Release](https://github.com/wayne-14e/neo-calc/releases/latest):

- **Linux x86_64** — `Neo-linux-x86_64.AppImage` (`chmod +x` it, then run)
- **Windows x64** — `Neo-windows-x64.zip` (extract and run `Neo.exe`)
- **macOS Apple Silicon** — `Neo-macos-arm64.dmg`

The builds are not code-signed. On macOS, use right-click → Open the first time. On Windows, SmartScreen may ask for confirmation.

#### History:

Every completed calculation is kept in memory for the current session. Press **LOG** in the status bar (or **Ctrl + H**) to open the history. It is not saved to disk and is cleared when the app closes.

**Example contents:**
```
[12:07:35]  123 + 45 = 168
[12:07:36]  100  TAX +10% = 110
[12:07:36]  2 + 3 = 5
[12:07:36]  5 × 4 = 20
```

#### Shortcuts:
- **Ctrl + F** -> open FX formulas
- **Ctrl + D** -> open DEV view
- **Ctrl + H** -> open LOG history

> Build & Run (Linux):
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
./build/Neo
```