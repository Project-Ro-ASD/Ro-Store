# Contributing

## Scope

Ro-Store is a Qt 6 + QML desktop client for browsing and managing Project Ro applications. Contributions should preserve the current architecture, keep the UI responsive, and avoid introducing generated build artifacts into version control.

## Local setup

```bash
cmake --preset default
cmake --build --preset default
```

If your environment injects `ccache` and it causes permission issues, use:

```bash
env CCACHE_DISABLE=1 cmake --preset default
env CCACHE_DISABLE=1 cmake --build --preset default
```

## Validation

Run these checks before opening a pull request:

```bash
env CCACHE_DISABLE=1 cmake -S . -B build-local
env CCACHE_DISABLE=1 cmake --build build-local
qmllint -I build-local -I build-local/RoStore build-local/RoStore/qml/Main.qml build-local/RoStore/qml/pages/HomePage.qml build-local/RoStore/qml/pages/AppDetailPage.qml build-local/RoStore/qml/components/AppCard.qml
```

## Coding guidelines

- Keep C++ changes in `src/` and UI changes in `qml/`.
- Register QML-exposed C++ types with `QML_ELEMENT`.
- Prefer explicit user-facing error messages over silent failures.
- Do not commit `build/`, `build-local/`, or other generated output.
- Keep comments sparse and technical.

## Pull requests

- Keep PRs focused.
- Describe user-facing behavior changes.
- Mention platform assumptions, especially for Linux-only package management flows.
