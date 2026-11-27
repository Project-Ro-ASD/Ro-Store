# Ro-Store after v0.9.0

v0.9.0 adds bounded catalog caches, combined RPM/Flatpak search, Flatpak permission previews, background reads and executable fault tests. The supported Flatpak mutation scope remains user Flathub app installation and removal.

## v1.0 release candidate

1. Test clean installation and upgrade from v0.7.0, v0.8.0 and v0.9.0 on Fedora 44 and Ro-ASD. Exercise the RPM install/remove/upgrade plan, offline system update recovery and the enabled user Flathub flows.
2. Run a real user Flatpak app update and compare the installed OSTree commit before and after. Keep update controls disabled if no suitable update can be validated. System-scope Flatpak changes, runtimes, drivers, codecs and extensions need separate qualification.
3. Complete keyboard, KDE light/dark, high DPI and compact-window acceptance. Review permission-query retry and privilege-denial messages on live systems. Repeat isolated network, disk and process-failure tests; do not induce destructive failures on an active workstation.
4. Establish RPM signing identity and release key handling. Verify repository metadata signatures, source checksums, package signatures and the downloaded release assets on both architectures.
5. Finish Turkish and English user documentation, installation and upgrade instructions, troubleshooting and known limits. Publish a release candidate, resolve blocking findings, and rerun workstation plus both architecture CI checks.

## v1.0.0 stable

Merge the reviewed release candidate, tag the merge commit, verify both RPM assets and `SHA256SUMS.txt`, then publish the stable release with the final validation record.

Fedora distribution upgrades, automatic update schedules, advisory-based classification and broader rollback integration remain post-1.0 work.
