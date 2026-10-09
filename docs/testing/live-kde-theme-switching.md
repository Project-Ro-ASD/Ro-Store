# Live KDE theme switching — Ro-Store QA

Ro-Store's existing `SystemPalette` color bindings and Kirigami `Platform.Units` geometry bindings are intended to follow the active desktop settings. This checklist **tests that behavior**; it does not assume all third-party Plasma SVG styles expose their geometry to Qt.

## Prerequisites

- Fedora 44 KDE Plasma session, with `kf6-kirigami` installed
- At least two color schemes (for example BreezeDark/BreezeLight, or RoDark/RoLight)
- Ro-Store built from `feature/system-theme-integration`
- Do not run `apply-dark-defaults` or overwrite `kdeglobals` to test the store

## Test without restarting Ro-Store

1. Note the current color scheme: `kreadconfig6 --file kdeglobals --group General --key ColorScheme`
2. Start one Ro-Store instance; keep it running throughout the test.
3. Open **System Settings → Colors & Themes → Colors** and apply a different color scheme.
   Alternatively, use `plasma-apply-colorscheme RoDark` / `plasma-apply-colorscheme RoLight` if installed.
4. Without restarting Ro-Store, check Home: window/card backgrounds, secondary labels, outlined badges, search controls, and the accent on actionable buttons.
5. Open an app detail page and check labels, footer actions, progress track, and removal confirmation (cancel the dialog; no package removal required).
6. Open Downloads; check queue/history text, track fill, and status contrast.
7. Switch back to the first color scheme, **still without restarting Ro-Store**. Verify all the same UI areas update.
8. If a property fails to update, capture two screenshots of the **same open process**, the selected KDE color-scheme names, and the terminal output. Note which property remained unchanged.

## Geometry and font caveats

- Kirigami Units are KDE-provided common metrics, not exact radii exported from Ro-Theme Plasma SVG files.
- Changing a Qt Quick Controls application **style** is not guaranteed to work without restarting the process. This test primarily verifies palette changes.
- Changing KDE fonts during an active session is a separate test: some hardcoded pixel font sizes in existing QML may not track system font changes. Do not report font scaling as implemented without verification.
- The offline GitHub Actions smoke test verifies QML startup; it cannot prove a live Plasma theme change. This requires a real KDE desktop session.

## Expected outcomes

- Light theme: legible dark text and accent-appropriate foreground on highlighted controls.
- Dark theme: legible light text and theme-derived accent.
- No fixed hex colors need to be introduced into UI components.
- All app install/remove/update behavior must remain unchanged.
