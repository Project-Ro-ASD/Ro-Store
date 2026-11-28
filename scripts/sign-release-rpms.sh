#!/usr/bin/env bash
set -euo pipefail

if (( $# != 5 )); then
    echo "usage: sign-release-rpms.sh RPM_DIR PUBLIC_KEY PRIVATE_KEY PASSPHRASE_FILE SIGNING_FINGERPRINT" >&2
    exit 2
fi

rpm_dir=$1
public_key=$2
private_key=$3
passphrase_file=$4
fingerprint=$5

[[ $fingerprint =~ ^[A-F0-9]{40}$ ]] || exit 2
[[ -s $public_key && -s $private_key && -s $passphrase_file ]] || exit 2
shopt -s nullglob
rpms=("$rpm_dir"/*.rpm)
(( ${#rpms[@]} == 2 )) || {
    echo "Expected exactly two RPMs" >&2
    exit 1
}

temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
export GNUPGHOME="$temporary/gnupg"
mkdir -m 700 "$GNUPGHOME" "$temporary/rpmdb"

public_fingerprints=$(gpg --batch --show-keys --with-colons "$public_key" |
    awk -F: '$1 == "fpr" {print $10}')
grep -Fxq "$fingerprint" <<< "$public_fingerprints" || {
    echo "Signing fingerprint is absent from the published RPM key" >&2
    exit 1
}

gpg --batch --quiet --import "$private_key"
secret_fingerprints=$(gpg --batch --list-secret-keys --with-colons |
    awk -F: '$1 == "fpr" {print $10}')
grep -Fxq "$fingerprint" <<< "$secret_fingerprints" || {
    echo "Matching private signing subkey is unavailable" >&2
    exit 1
}

for package in "${rpms[@]}"; do
    rpmsign --addsign --key-id "${fingerprint}!" \
        --define "_gpg_sign_cmd_extra_args --batch --pinentry-mode loopback --passphrase-file $passphrase_file" \
        "$package"
done

rpmkeys --dbpath "$temporary/rpmdb" --import "$public_key"
for package in "${rpms[@]}"; do
    verification=$(rpmkeys --dbpath "$temporary/rpmdb" --checksig --verbose "$package")
    grep -Eiq 'signature.*: OK' <<< "$verification" || {
        printf '%s\n' "$verification" >&2
        echo "RPM signature verification failed: $package" >&2
        exit 1
    }
    printf '%s\n' "$verification"
done
