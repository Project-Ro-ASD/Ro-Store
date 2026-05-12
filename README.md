# Ro-Store

Ro-Store is a Qt 6 + QML desktop client for browsing official Project Ro applications and managing package lifecycle actions on Fedora systems.

## Highlights

- Remote catalog loading with filtering and search
- Responsive QML interface
- RPM-based installed-version detection
- PackageKit-backed install, update, and remove flows
- Explicit failure states for unsupported or incomplete host environments

## Repository layout

- `src/`: C++ backend and QML-exposed service objects
- `qml/`: application UI, pages, and reusable components
- `docs/`: architecture and development notes
- `.github/`: CI, templates, and repository automation

## Requirements

- CMake 3.21+
- Qt 6 with `Core`, `Quick`, and `Network`
- Fedora 43 for the packaged target environment
- `rpm`, `pkexec`, and `pkcon` for runtime package operations

Note: GitHub Actions builds RPM packages targeted at Fedora 43 systems running KDE Plasma Desktop.

## Build

```bash
cmake --preset default
cmake --build --preset default
```

If your environment injects `ccache` and it causes permission issues:

```bash
env CCACHE_DISABLE=1 cmake --preset default
env CCACHE_DISABLE=1 cmake --build --preset default
```

## Run

```bash
./build-local/ro-store
```

## Quality checks

```bash
env CCACHE_DISABLE=1 cmake -S . -B build-local
env CCACHE_DISABLE=1 cmake --build build-local
qmllint -I build-local -I build-local/RoStore build-local/RoStore/qml/Main.qml build-local/RoStore/qml/pages/HomePage.qml build-local/RoStore/qml/pages/AppDetailPage.qml build-local/RoStore/qml/components/AppCard.qml
```

## GitHub Actions outputs

The repository CI produces only two RPM artifacts:

- `ro-store-<version>-x86_64.rpm`
- `ro-store-<version>-aarch64.rpm`

These packages are built in Fedora 43 environments and are intended for Fedora 43 systems with KDE Plasma Desktop.

## Maintenance

- See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution rules.
- See [docs/architecture.md](docs/architecture.md) for system structure.
- See [CHANGELOG.md](CHANGELOG.md) for tracked changes.
