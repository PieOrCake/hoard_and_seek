# Prompt: Fix ThemeGuard balance in Nexus addon

Your addon uses a `PushGW2Theme` / `PopGW2Theme` pair to apply a custom ImGui theme during rendering — a pattern borrowed from Hoard & Seek. The problem: `PushGW2Theme` writes directly to `ImGui::GetStyle()`, the shared global style struct used by every addon in the process. If any code path returns from your render function after `PushGW2Theme` without calling `PopGW2Theme`, the style is permanently corrupted for all other addons until GW2 is restarted.

**The fix:** Replace the manual pair with a RAII guard. Add this struct immediately after `PopGW2Theme`:

```cpp
struct ThemeGuard {
    ThemeGuard()  { PushGW2Theme(); }
    ~ThemeGuard() { PopGW2Theme(); }
};
```

Then, in every function that currently calls `PushGW2Theme()` at the top and `PopGW2Theme()` at the bottom:

1. Replace the opening `PushGW2Theme()` call with `ThemeGuard themeGuard;`
2. Remove every `PopGW2Theme()` call from that function — including any before early returns
3. Remove any `PopGW2Theme()` call that precedes a `return` inside an `if (!ImGui::Begin(...))` block

The destructor guarantees `PopGW2Theme` fires on every exit path, including exceptions and early returns.

**Do not change:**
- `PushGW2Theme` or `PopGW2Theme` themselves
- `BuildGW2Theme` (it snapshots the style without touching the global style)
- Any logic, threading, API calls, or ImGui widget code

**Verify:** The DLL compiles with no errors or warnings. Search for any remaining bare `PushGW2Theme(` or `PopGW2Theme(` calls outside of the three definitions (`PushGW2Theme` body, `PopGW2Theme` body, `ThemeGuard` body) — there should be none.
