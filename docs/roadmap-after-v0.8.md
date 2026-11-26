# Ro-Store after v0.8.0

v0.8.0 ships the tested installed-software inventory, read-only history and clearer removal/update presentation. The following work remains before a stable 1.0 release.

## v0.9 — Performance, security and stability

1. Cache AppStream metadata and measure search and first-screen latency on Fedora and Ro-ASD. Refresh catalogs and update candidates in the background without blocking navigation.
2. Complete catalog-wide RPM/Flatpak source comparison and unified search. Explain source, version and permissions before a user chooses an installation.
3. Add executable tests around Flatpak argument validation, transaction journal lifecycle, interrupted operations, read failures and recovery. Test with a disposable Fedora 44 environment or isolated user data, including network loss, low disk, corrupt journal and daemon failure.
4. Verify authorization denial, simultaneous store instances and RPM/Flatpak coordinator lock contention. Run a genuine user-scope Flatpak app update and verify the installed OSTree commit changed before considering update controls. Keep system-scope mutations, drivers, runtimes, codecs and extensions gated until separately qualified.
5. Improve diagnostics for failed RPM/Flatpak operations and group system updates by useful package categories. Retain original error details without exposing credentials.
6. Review keyboard navigation, 400×500 / 600×500 / 1000×650 layouts, screen scaling and KDE light/dark themes. Expand regression and integration coverage around confirmed issues.

Release `v0.9.0` only after the final source tree passes both architecture builds and the validated scenarios are recorded.

## v1.0 — Release candidate and stable release

1. Test clean install and upgrade from v0.7.0, v0.8.0 and v0.9.0 on Fedora and Ro-ASD. Exercise RPM install/remove/system update and the Flatpak flows actually enabled for users.
2. Check RPM signatures, repository metadata and release asset checksums. Document signing identity and publication steps; ensure release automation only uses reviewed sources.
3. Finish user documentation, troubleshooting guidance, limitations and release notes in Turkish and English.
4. Publish a 1.0 release candidate, resolve its blocking findings, rerun both architecture builds and host acceptance tests, then publish `v1.0.0`.

Fedora distribution upgrades, automatic update schedules, advisory-based classification and broader rollback integration remain post-1.0 work.
