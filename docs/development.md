# Development

## Repository conventions

- Source code lives in `src/`.
- QML UI files live in `qml/`.
- Generated artifacts must stay out of version control.

## Recommended workflow

```bash
cmake --preset default
cmake --build --preset default
```

## Lint and verification

```bash
qmllint -I build-local -I build-local/RoStore build-local/RoStore/qml/Main.qml build-local/RoStore/qml/pages/HomePage.qml build-local/RoStore/qml/pages/AppDetailPage.qml build-local/RoStore/qml/components/AppCard.qml
```

## Release readiness

- Build succeeds from a clean directory.
- QML lint passes.
- README and changelog reflect user-facing changes.
- No generated files are staged.
