# KDE fonts and icon theme QA

Changes are part of PR #12 on `feature/system-theme-integration`.

## Typography

The reference UI uses a 14-pixel application font, but QML labels scale
through `ThemeMetrics.fontPx()` and common semantic text sizes. Font family
comes from `Qt.application.font.family`. The DNF5 technical log uses
`Platform.Theme.fixedWidthFont.family` instead.

1. Open **System Settings → Text & Fonts → Fonts** in KDE Plasma.
2. Select a different general font family and a larger general font size.
3. Start Ro-Store and check Home, Downloads, App Detail, removal dialog,
   transaction log and application cards.
4. Check that the new family and text scale are applied and Turkish characters
   display correctly.
5. While Ro-Store is open, change the general font again. Verify whether
   `Qt.application.font` updates live on the target KDE/Qt deployment.
   Re-launch to distinguish platform font notification limitations from
   Ro-Store's bound typography.
6. Restore the usual font, repeat with large/accessibility font settings, and
   record any clipped text. Detailed geometry fixes belong to responsive QA.

## Theme icons

1. Switch KDE's icon theme among installed themes (Breeze and any available
   RoDark/RoLight icon themes).
2. Inspect **Geri**, **Yenile**, and **Yüklemeler** where present.
3. Confirm each control still shows its text and works when a named icon is
   missing from a particular theme.
4. Switch icon themes with Ro-Store open, and confirm changes propagate on
   the current Qt platform or after relaunch.
5. Verify catalog application artwork still comes from `iconUrl` and does
   not get replaced by a symbolic KDE action icon.

## Regression checks

- Run CMake/Qt QML build and smoke tests for PR #12.
- Check RoDark, RoLight and Breeze Light/Dark colors after changing fonts.
- Inspect 100%, 125% and 150% display scaling in the responsive QA stage.
- Do not merge PR #12 until visual and accessibility checks are done.

## Large-font layout regression — October 2026

A live 18 pt change formerly clipped Back/Downloads, overlaid the App Detail
title and summary, and truncated "Durumu Yenile".

- Compare KDE 10 pt, 12 pt and 18 pt without restarting Ro-Store.
- Confirm the app header and Home hero grow instead of overlaying text.
- Confirm narrow layout engages automatically for large fonts.
- Confirm cards and GridView cells grow together.
- Check the download header, detail footer and action labels.
- Inspect the removal dialog separately before marking all responsive
  layout testing complete.

## Live font shrink while navigating — 2026-10-09 regression

Reproduced visually on Fedora/KDE: while the app was running, KDE font
changed to 18 pt, then reverted to 10 pt. App Detail and Downloads
displayed the small font, but Home retained some large-font geometry until
a complete process restart.

This iteration introduces `SystemFontMonitor`, a singleton registered from
C++ that listens for Qt's `ApplicationFontChange` event, and makes all
`ThemeMetrics` instances share a notifying font property. Home additionally
forces its GridView layout and normalizes its Flickable scroll position when
font scale changes and when StackView reactivates Home.

Manual regression sequence:

1. Open Home at original Noto Sans 10 pt, note row layout and button widths.
2. Without closing Ro-Store, switch to DejaVu Sans 18 pt.
3. Confirm Home changes to narrow layout and content scrolls without overlap.
4. Open Ro Assist detail. Restore Noto Sans 10 pt **while detail is open**.
5. Go back to Home without closing Ro-Store: expect the original wide layout.
6. Repeat 10 → 18 → 10 with Home visible the whole time.
7. Visit Downloads and return to Home, checking card size and scroll origin.
8. Make sure the repo/catalog state is not reset by font changes.

The GitHub Action QML startup smoke test does not prove this live KDE behavior;
it must still be checked on a real Plasma session.

## KDE icon theme live update — October 2026

Problem: after Breeze → Papirus, restarting Ro-Store or opening a fresh
App Detail page uses the new icon theme, but existing Home buttons retain
the old icons. The Qt Quick Controls `icon.name` image can stay cached.

Solution: a Qt-side `SystemIconMonitor` listens for KDE
`org.kde.KIconLoader.iconChanged(int)` events and watches the KDE
`kdeglobals` configuration (including atomic replacements). It emits
a revision change whenever the icon theme changes/refreshes.
`ThemedIconButton` then briefly clears and restores `icon.name` on
the next event-loop turn without recreating the page or resetting search,
catalog, package transactions, font or color bindings.

Checks on a real KDE Plasma session:
1. With Home open, change Breeze → Papirus; inspect the existing Refresh
   and Downloads buttons without restarting or navigating.
2. Navigate to Ro Assist Detail and Downloads and inspect the Back and
   Downloads buttons.
3. Change Papirus → Breeze *while Home remains visible*. Confirm icons
   revert without restarting.
4. Repeat while Home is behind Detail, then press Back; old theme icons
   must not reappear.
5. Check both narrow and wide Home layouts, and verify icon-less controls
   and catalog app artwork are unaffected.
6. Restore the originally configured theme (Breeze).
7. QML startup smoke tests cannot prove the running KDE refresh behavior.

## Responsive baseline – 2026-10-09

The next patch preserves wide layouts but prevents a hard 320 px content
floor from overflowing very narrow windows. In Home, the narrow search,
filter/refresh and Downloads rows use implicit control heights and actual
spacing instead of fixed y=50/y=100. The Home title/subtitle and Downloads
header use available-width constraints; the Detail "Ro-Store" header label
is hidden in narrow mode to preserve navigation actions.

Local KDE verification is REQUIRED before closing responsive QA:
- Resize Home to 1000x650, 800x600, 600x500 and 400x500; check the
  search/filter/refresh/downloads controls and featured card.
- Check App Detail and Downloads at all four window sizes.
- With 10 pt and 18 pt fonts, repeat 400x500 and 800x600 to find clipping.
- Inspect remove confirmation modal at 400x500 and 18 pt separately.
- Inspect DNF technical log, disabled states, and visible focus indication.
- Run QT_SCALE_FACTOR=1.25 and 1.5 in separate processes for a quick
  Qt-level smoke check (not a substitute for Plasma native display scaling).
- No release/merge before visual inspection and successful CI.

## Remove confirmation and log panel — responsive QA

The removal confirmation previously had a 360px fixed height and hard-coded
text positions (22/54/120/190), warning block height (74), and button row
width (336). Now the modal's height follows the content when possible,
its text and warning wrap, both actions fit the available width, and the
whole content scrolls when even the dynamic dialog exceeds the window.
The Cancel and Remove action handlers are unchanged.

The technical log toggle now elides its long label rather than allowing the
optional status text to overlap it; expanded content begins below the
font-scaled header. The auxiliary DNF5 label is omitted in narrow layouts.

Test on the actual Fedora KDE desktop with an already-installed program:
- At 100% and 150%, open the removal confirmation at 400x500 and 800x600.
- Switch between 10pt and 18pt live with the dialog open.
- Verify the warning is legible and both actions are reachable by scrolling.
- Click ONLY Vazgeç while testing; do not confirm package removal.
- For logs, inspect a genuine existing package transaction if available.
  No extra install/remove action is required merely to exercise the log UI.
- The CI headless QML smoke test cannot verify dialog readability or
  focus behavior, which still requires desktop inspection.

## Dialog compact-height visual correction — 2026-10-09

The 400x500, 800x600, and approximately 1300x700 screenshots confirmed the
modal was functional but had about 90–120 px of unnecessary empty space below
the Cancel/Remove actions due to an unconditional 360 px minimum height.
Use the natural content implicit height plus internal margins, clamped to
the viewport; the existing Flickable is retained for enlarged fonts or
short screens. Reset its scroll origin when opening the dialog.

Re-check the modal at those three window sizes and with 18 pt KDE fonts.
The normal dialog should fit its content; all actions remain reachable on
small screens. Do not press the destructive confirmation during visual QA.
