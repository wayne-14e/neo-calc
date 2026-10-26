## Neo — a fast, cross-platform desktop calculator with:
- **Developer shortcuts**
- **Custom formulas**
- **Persistent history**

*Made on C++ with Qt Framework. Designed in 70s matrix style.*

#### History:

| OS | Log file |
| --- | --- |
| Linux | `~/.local/share/Neo/history.txt` |
| macOS | `~/Library/Application Support/Neo/history.txt` |
| Windows | `%LOCALAPPDATA%\Neo\history.txt` |
**Example contents:**
[2026-10-26 12:07:35] 123 + 45 = 168
[2026-10-26 12:07:36] 100  TAX +10% = 110
[2026-10-26 12:07:36] 2 + 3 = 5
[2026-10-26 12:07:36] 5 × 4 = 20

- Hover over **NEO//CALC** in the status bar to see the resolved log path.

#### Shortcuts:
- **Ctrl + F** -> open FX folumas
- **Ctrl + D** -> open DEV view

> Build & Run:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
./build/Neo
```