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
