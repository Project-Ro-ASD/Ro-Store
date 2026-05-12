# Architecture

## Overview

Ro-Store is a Qt Quick application with a thin C++ backend and a QML-first interface.

## Layout

- `src/`: C++ application logic and QML-exposed service objects
- `qml/`: user interface pages and reusable components
- `.github/`: repository automation and collaboration templates
- `docs/`: project documentation

## Runtime flow

1. `main.cpp` boots `QQmlApplicationEngine` and loads the `RoStore` QML module.
2. `CatalogModel` downloads and parses the remote catalog.
3. `HomePage.qml` renders app cards from model roles.
4. `AppDetailPage.qml` coordinates install, update, remove, and launch actions.
5. `PackageStatus`, `PackageInstaller`, and `AppLauncher` bridge system behavior into QML.

## Package management assumptions

- Install/update/remove flows assume Linux with `pkexec` and `pkcon`.
- Installation state checks assume `rpm`.
- Non-Linux or incomplete environments should fail with explicit status messages rather than undefined behavior.
