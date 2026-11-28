#!/usr/bin/env python3
"""Describe the exact unsigned RPM set consumed by Ro-Repo."""

import argparse
import hashlib
import json
import pathlib
import re
import subprocess


def rpm_entry(path):
    query = "%{NAME}\t%{EPOCHNUM}\t%{VERSION}\t%{RELEASE}\t%{ARCH}\t%{SOURCERPM}\t%|SOURCEPACKAGE?{true}:{false}|"
    fields = subprocess.check_output(
        ["rpm", "-qp", "--qf", query, str(path)], text=True
    ).split("\t")
    if len(fields) != 7:
        raise ValueError(f"invalid RPM header: {path.name}")
    name, epoch, version, release, arch, source_rpm, is_source = fields
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    return {
        "filename": path.name,
        "name": name,
        "epoch": int(epoch or 0),
        "version": version,
        "release": release,
        "architecture": "src" if is_source == "true" else arch,
        "source_rpm": None if is_source == "true" else source_rpm,
        "producer_artifact_sha256": digest,
    }


def build_manifest(directory, repository, commit, tag, release_id, workflow_run):
    if repository != "Project-Ro-ASD/Ro-Store":
        raise ValueError("unexpected source repository")
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("invalid source commit")
    match = re.fullmatch(r"v(\d+\.\d+\.\d+)(?:-rc([1-9]\d*))?", tag)
    if match is None:
        raise ValueError("invalid release tag")
    if not all(re.fullmatch(r"[1-9]\d*", value) for value in (release_id, workflow_run)):
        raise ValueError("release ID and workflow run must be positive integers")
    expected_release = f"0.rc{match[2]}.fc44" if match[2] else "1.fc44"
    paths = sorted(directory.glob("*.rpm"))
    if len(paths) != 3 or any(path.is_symlink() or not path.is_file() for path in paths):
        raise ValueError("expected exactly x86_64, aarch64 and source RPMs")
    artifacts = [rpm_entry(path) for path in paths]
    by_arch = {item["architecture"]: item for item in artifacts}
    if set(by_arch) != {"x86_64", "aarch64", "src"}:
        raise ValueError("unexpected RPM architecture set")
    source_name = by_arch["src"]["filename"]
    for item in artifacts:
        arch = item["architecture"]
        expected_name = f"ro-store-{match[1]}-{expected_release}.{arch}.rpm"
        if (item["filename"] != expected_name or item["name"] != "ro-store"
                or item["epoch"] != 0 or item["version"] != match[1]
                or item["release"] != expected_release):
            raise ValueError(f"RPM identity does not match tag: {item['filename']}")
        if arch != "src" and item["source_rpm"] != source_name:
            raise ValueError(f"binary/source RPM mismatch: {item['filename']}")
    return {
        "schema_version": 1,
        "component": "ro-store",
        "source_repository": repository,
        "source_commit": commit,
        "release_tag": tag,
        "release_id": int(release_id),
        "workflow_run": int(workflow_run),
        "fedora_release": 44,
        "artifacts": artifacts,
        "provenance": {"provider": "github-actions", "subject_digest": f"git:{commit}"},
        "attestation": {
            "provider": "github-artifact-attestations",
            "verification": "gh attestation verify with exact repository, commit and signer workflow",
        },
        "sbom": None,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts-dir", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--release-id", required=True)
    parser.add_argument("--workflow-run", required=True)
    args = parser.parse_args()
    manifest = build_manifest(args.artifacts_dir, args.repository, args.commit,
                              args.tag, args.release_id, args.workflow_run)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
