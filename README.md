## Neo — a fast, cross-platform desktop calculator with:
- **Developer shortcuts**
- **Custom formulas**
- **In-app session history**

*Made on C++ with Qt Framework. Designed in 70s matrix style.*

Currently only Linux executable is available, since I don't have access to Windows and MacOS for now.

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