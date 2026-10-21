#!/usr/bin/env python3
"""Source-level Flatpak transaction guard regression checks.

These checks are non-mutating; they do NOT replace runtime fault injection.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
BACKEND = (ROOT / "src/flatpak/FlatpakManager.cpp").read_text()
HEADER = (ROOT / "src/flatpak/FlatpakManager.h").read_text()
WINDOW = (ROOT / "qml/Main.qml").read_text()


def method(name):
    match = re.search(r"\bFlatpakManager::" + re.escape(name) + r"\s*\(", BACKEND)
    if not match:
        raise AssertionError(f"Missing {name}")
    return BACKEND[match.start():]


class FlatpakSafetyGuards(unittest.TestCase):
    def test_no_shell_for_mutations(self):
        self.assertNotIn("QProcess::execute(", BACKEND)
        self.assertNotIn("/bin/sh", BACKEND)
        self.assertIn('setProgram(QStringLiteral("/usr/bin/flatpak"))', BACKEND)

    def test_journal_checked_again_under_lock(self):
        body = method("startMutation").split("void FlatpakManager::install(", 1)[0]
        lock = body.index("if (!acquireLock())")
        recheck = body.index("if (QFile::exists(journalPath()))", lock)
        journal = body.index("if (!writeJournal(", recheck)
        execution = body.index("runTask(", journal)
        self.assertLess(lock, recheck)
        self.assertLess(recheck, journal)
        self.assertLess(journal, execution)

    def test_update_commit_read_precedes_journal(self):
        body = method("startMutation").split("void FlatpakManager::install(", 1)[0]
        self.assertLess(body.index("installedCommit(scope, id)"), body.index("writeJournal(kind, scope, id)"))
        self.assertIn("beforeCommit.isEmpty()", body)
        verified = method("verifiedTargetState").split("QString FlatpakManager::installedCommit", 1)[0]
        self.assertIn("after != m_pendingBeforeCommit", verified)

    def test_target_read_must_succeed(self):
        verified = method("verifiedTargetState").split("QString FlatpakManager::installedCommit", 1)[0]
        self.assertIn("m_successfulReadKinds.contains(installedRead)", verified)
        self.assertIn("m_successfulReadKinds.contains(remoteRead)", verified)
        self.assertIn("!isInstalled(m_pendingScope, m_pendingId)", verified)

    def test_failures_preserve_recovery(self):
        finish = method("finishTask").split("void FlatpakManager::parseResponse", 1)[0]
        self.assertIn("m_verifying = successful", finish)
        self.assertIn("m_outcomeUnknown = true", finish)
        complete = method("completeReads").split("bool FlatpakManager::verifiedTargetState", 1)[0]
        self.assertIn("verifiedTargetState()", complete)
        self.assertIn("QFile::remove(journalPath())", complete)

    def test_no_mutation_timeout_and_no_data_deletion(self):
        self.assertIn("if (!task.mutation && task.timeoutMs > 0)", BACKEND)
        self.assertNotIn("--delete-data", BACKEND)
        self.assertIn("--user", BACKEND)
        self.assertIn("--system", BACKEND)
        self.assertIn("m_outcomeUnknown", HEADER)

    def test_app_install_requires_exact_catalog_membership(self):
        install = method("install").split("void FlatpakManager::uninstall", 1)[0]
        self.assertIn("isCatalogApp(scope, remote, id)", install)
        catalog = method("isCatalogApp").split("bool FlatpakManager::isInstalled", 1)[0]
        self.assertIn('QStringLiteral("scope")', catalog)
        self.assertIn('QStringLiteral("origin")', catalog)
        self.assertIn('QStringLiteral("id")', catalog)
        detail = (ROOT / "qml/pages/FlatpakDetailPage.qml").read_text()
        self.assertIn("page.installable", detail)
        self.assertIn('action === "install" && !installable', detail)

    def test_tray_exit_guard(self):
        self.assertIn("transactionManager.busy || flatpakManager.busy || flatpakManager.verifying", WINDOW)
        self.assertIn("enabled: !flatpakManager.busy && !flatpakManager.verifying", WINDOW)


if __name__ == "__main__":
    unittest.main()
