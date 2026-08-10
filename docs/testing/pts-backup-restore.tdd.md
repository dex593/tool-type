# TDD evidence: Pts backup/restore workflows

## User journey and acceptance criteria

- Restore the `Pts` popup for Photoshop settings and font backup/restore using `.afang` archives.
- When several Photoshop versions are installed, backup must let the user choose exactly one source version; restore must let the user choose both the archived source version and the installed destination version.
- Font restore must avoid redundant file writes and repeated `-restored-N` copies, register fonts for the current user, and keep the popup responsive during large scans/registration batches.
- Backup/restore core work must run outside the UI thread, and the user must be able to cancel an active operation without terminating a thread unsafely.
- Restore must reject malformed or unsafe paths, preserve an existing destination if a write fails, and never claim CS6 workspace success when conversion is incompatible.
- A CS6-to-CS6 restore must preserve native workspace bytes and repair only a malformed legacy counterpart alias, without overwriting a valid distinct workspace.
- The Pts popup and its Photoshop-version chooser must be opaque, keep status/percentage/progress/actions in separate regions, and scale their complete layout at the active system DPI.
- Short live status text may be compacted, but omitted text must be marked with `...`; completion/error dialogs must retain the full summary, error code, and recovery path.

## RED checkpoints

- Commit `4f80a21 test: add RED coverage for restored Pts workflows` introduced the first executable specification while the production symbols were absent; the Pts suite failed to compile as expected.
- Cross-version CS6 workspace tests then exposed unsupported workspace conversion.
- A repeated font restore with an existing different file exposed redundant `-restored-2.ttf` creation.
- Archive validation tests exposed acceptance of `.` path segments and Windows reserved device names.
- An incompatible CS6 workspace archive exposed a false-success/raw-write fallback.
- The final responsiveness regression added a 160-entry archive inspection callback requirement. Before the callback API existed, `run-pts-tests.ps1` failed to compile with “too many arguments to function `InspectPtsArchiveVersions`”.
- Additional filename tests rejected `CON.backup.ttf` and control characters; both represent Windows path rules not covered by the initial validator.
- Commit `e99dca4` added a deterministic worker barrier: while the worker is deliberately blocked, the UI must process a heartbeat before completion. It failed to compile because no background-task transport existed.
- Commit `09bb7b6` added the cancellation-token/result contract. Commit `33d1732` extended RED coverage to incomplete-backup cleanup and restore stopping before the next file.
- Commit `80e0d5c` proved that cancelling an overwrite destroyed the previous `.afang`; the regression failed with `cancelled backup destroyed the previous archive`.
- Commit `bdc0779` required a settings backup to fail while Photoshop is running, because the active workspace may not be flushed to disk yet.
- Commit `abe4116` added an injected Registry updater so native tests cannot redirect the real `SettingsFilePath` to a deleted temp fixture.
- Commit `1a2fa98` reproduced same-version CS6 workspace corruption: aliases made `WorkSpaces\Untitled-1` and `WorkSpaces (Modified)\Untitled-1` overwrite each other.
- Commits `4d8bf6e`, `3e1665e`, and `f67029e` covered `WM_QUIT` preservation, documented `ReplaceFileW` partial-failure states 1176/1177, and the late cancellation/commit race.
- Commit `762f732` reproduced the equivalent cross-version corruption when two distinct source workspace variants generated aliases for each other's primary target.
- Commit `4f6980b` showed that incremental alias protection was insufficient: cancelling before the later primary entry left the earlier alias in that future primary path.
- Commit `f205161` reproduced a stale `.previous` safety-copy collision that could otherwise be mistaken for the current rollback file.
- Commit `9b55986` proved that same-version CS6 restore still passed native workspace data through the compatibility normalizer instead of preserving the archive bytes.
- Commit `2eb9674` reproduced the real-machine failure residue: a stale `WorkSpaces\Essentials` alias with a duplicated closing root/trailing XML survived restore and remained available for CS6 to load.
- Commit `4ac1273` expanded that RED contract to prefix garbage, mismatched inner tags, XML comments containing tag-like text, and preservation of a valid unplanned counterpart.
- Commit `59977c1` reproduced the popup defects from the screenshot: the intended Pts modal style/layout APIs did not exist, the popup remained translucent, status spacing was too tight, long live notices discarded their tail silently, completion details were compacted, and partial-cancel wording implied a safer outcome than the non-transactional restore provides.
- Commit `3b20596` added follow-up RED coverage for DPI-scaled dialog fonts, a fully visible non-rollback warning, and cancellation after an identical font was skipped without mutating the machine.

## GREEN implementation

- Restored `.afang` format version 1 with bounded metadata, raw/zlib payloads, exact entry-kind validation, and seek-based payload skipping during preflight.
- Restored installed-Photoshop discovery and explicit version selection for both backup and restore. There is no silent “all versions” backup choice.
- Added cross-version path aliases and explicit CS6 workspace outcomes: not needed, already compatible, converted, or incompatible.
- Restored fonts into `%LOCALAPPDATA%\Microsoft\Windows\Fonts`, reusing identical destination/collision files, batching HKCU Registry updates, and broadcasting `WM_FONTCHANGE` once.
- Writes to existing Photoshop settings are staged in the same directory, flushed, and atomically replaced so a failed write does not truncate the original.
- Commit `2938a5c` moved archive creation/restoration, compression, file I/O, font registration, Registry work, and the font-change broadcast to a joinable worker. Progress/completion is marshalled back with `WM_APP` messages; controls and message boxes remain UI-thread-only.
- Progress posts are throttled to approximately 75 ms. The popup remains responsive without re-entering the operation call stack, disables conflicting actions, and changes `Đóng` to `Hủy` while work is active.
- Cancellation is cooperative: backup checks during discovery and between compression/write stages and deletes an incomplete `.afang`; restore checks before additional metadata, writes, aliases, registration and completion. Already completed restore writes are retained and reported rather than rolled back or falsely reported as success.
- Commit `31301f3` stages the complete backup in a same-directory temporary file, flushes it, then replaces the destination. Cancellation or write/commit failure deletes only the temporary file and preserves an existing `.afang` byte-for-byte.
- Commit `bd2ae1e` blocks settings backup while Photoshop is running and explains that closing Photoshop is required to flush the current workspace/layout.
- Commit `9cc37f6` routes Registry updates through an injectable seam in tests; production still updates the selected installed Photoshop version.
- Commit `fd2582a` limits workspace compatibility aliases to real cross-version restores, so same-version `WorkSpaces` and `WorkSpaces (Modified)` files retain their distinct bytes.
- Commits `4077dea`, `ba43695`, and `a1e735b` preserve outer `WM_QUIT`, use a safety backup/rollback for `ReplaceFileW` 1176/1177, clear the temporary file attribute, and arbitrate `Hủy` versus the atomic commit point with a single-winner gate.
- Commit `b2248d0` tracks primary workspace destinations during cross-version restore; a compatibility alias can no longer overwrite a primary entry already restored from the archive.
- Commit `e571385` performs a seek-based metadata planning pass before the first write, so aliases are also blocked from every future primary path during cancelled or failed partial restores.
- Commit `d4f0a7b` aborts before `ReplaceFileW` when its safety-backup path already exists and reports the retained recovery path if rollback itself cannot complete.
- Commit `e9fbc07` limits workspace-format conversion to an actual cross-version restore, keeping CS6-to-CS6 workspace payloads byte-for-byte.
- Commit `71cf7ee` inspects legacy same-version counterpart aliases only when they are absent from the archive plan: valid existing workspaces are preserved, absent aliases are not created, and a malformed stale alias is atomically replaced with the restored valid workspace bytes.
- Commit `bd6f643` replaces the root-string heuristic with a bounded XML structure scanner that handles declarations, comments, CDATA, quoted attributes, balanced nested elements, the required `workspace` element, and clean outer padding before deciding an alias is safe to preserve.
- Windows path validation rejects traversal, empty/dot segments, control characters, trailing dot/space, and reserved device prefixes before the first period.
- Commit `a1379b9` makes both Pts dialogs opaque, introduces one DPI-scaled `520x330` logical layout with dedicated status/percentage/progress regions, clamps dialogs to the monitor work area, and visibly dims disabled owner-drawn buttons.
- Pts actions and progress messages now use consistent Vietnamese labels. Starting a new workflow resets stale progress, partial cancellation keeps the reached percentage and explicitly says applied changes are not rolled back, while full details remain available in the completion/error dialog.
- `CompactPtsNotice` still limits the live popup to two short lines, but now appends `...` whenever details are omitted instead of silently hiding them.
- Commit `08a642a` scales the actual Segoe UI font with the Pts layout, keeps the full “không tự hoàn tác” warning inside two visible lines, and reports “chưa có thay đổi” when cancellation follows only no-op/identical font entries. File writes, font registration, and Registry updates now set the operation mutation state explicitly.

## Regression specification

| Guarantee | Test target | Result |
|---|---|---|
| Safe archive paths and Windows device names | `TestSafeArchivePaths` | PASS |
| `.afang` compression/decompression round trip | `TestCompressionRoundTrip` | PASS |
| Large metadata inspection reports periodic progress | `TestArchiveInspectionReportsProgress` | PASS |
| A blocked Pts worker does not block UI heartbeat/progress/completion dispatch | `TestPtsBackgroundTaskKeepsUiThreadResponsive` | PASS |
| A cancellation request reaches the worker and completion reports cancelled, not success | `TestPtsBackgroundTaskCanBeCancelled` | PASS |
| Multiple installed Photoshop versions are discovered and one selected version is archived | `TestSettingsBackupArchive` | PASS |
| Saved `WorkSpaces (Modified)` data is included, and settings backup is refused while Photoshop is running | `TestSettingsBackupArchive`, `TestSettingsBackupRequiresPhotoshopToBeClosed` | PASS |
| Cancelled backup leaves no incomplete `.afang` and preserves an existing archive when overwriting | `TestCancelledBackupDeletesIncompleteArchive` | PASS |
| Cancelled restore stops before the next file and preserves a completed prior file | `TestCancelledRestoreStopsBeforeNextFile` | PASS |
| Modern settings map to CS6 aliases/workspace layout | `TestCrossVersionMappingAndCs6Aliases`, `TestCrossVersionArchiveRestore` | PASS |
| Same-version workspace variants keep their own bytes instead of overwriting through aliases | `TestSameVersionWorkspaceRestoreKeepsDistinctVariants` | PASS |
| Native CS6-to-CS6 workspace payloads bypass cross-version normalization | `TestSameVersionCs6WorkspaceRestoreKeepsNativeBytes` | PASS |
| A malformed legacy counterpart alias is repaired while archive-planned primary variants remain protected | `TestSameVersionCs6RestoreRepairsMalformedLegacyAlias` | PASS |
| Structurally corrupt workspace XML is rejected while a valid comment and a valid unplanned counterpart are preserved | `TestCs6WorkspaceStructuralValidation`, `TestSameVersionCs6RestoreRepairsMalformedLegacyAlias` | PASS |
| Cross-version workspace aliases cannot overwrite distinct primary workspace variants | `TestCrossVersionWorkspaceAliasesDoNotOverwritePrimaryEntries` | PASS |
| Cancelled cross-version restore cannot leave an alias in a future primary workspace path | `TestCancelledCrossVersionRestoreDoesNotAliasOverFuturePrimary` | PASS |
| Native restore tests request but do not write the real Photoshop Registry path | injected updater in `TestCrossVersionArchiveRestore` | PASS |
| `WM_QUIT` destroys the Pts dialog and is reposted to the outer app loop | `TestPtsDialogLoopPreservesQuitAndDestroysWindow` | PASS |
| Replace failures 1176/1177 restore the old destination, stale safety paths are rejected, and committed files lose the temporary attribute | `TestAtomicCommitPreservesDestinationAcrossReplaceFailures` | PASS |
| Cancellation and atomic backup commit have exactly one winner | `TestPtsCancellationCommitGateHasSingleWinner`, `TestCancelledBackupDeletesIncompleteArchive` | PASS |
| A second identical font restore reuses `-restored-1` and preserves unrelated Registry mappings | `TestIdenticalFontRestoreIsSkipped` | PASS |
| Malformed archives perform no writes | `TestMalformedArchiveDoesNotWriteAnything` | PASS |
| Failed destination replacement preserves original bytes | `TestAtomicRestoreWritePreservesLockedDestination` | PASS |
| Incompatible CS6 workspace data fails without a raw write | `TestIncompatibleCs6WorkspaceFailsInsteadOfClaimingSuccess` | PASS |
| Pts popup is opaque; every control and its font stay separated/scaled at 96/120/144/192 DPI | `TestPtsPopupLayoutIsOpaqueReadableAndDpiScaled` | PASS |
| Live notices mark omitted content and full restore summaries reach the completion dialog | `TestCompactPtsNoticeMarksOmittedDetails`, summary assertion in `TestCrossVersionArchiveRestore` | PASS |
| Partial restore cancellation states that applied changes are not automatically rolled back | `TestCancelledRestoreStopsBeforeNextFile` | PASS |
| Cancelling after only an identical font no-op reports that no change was applied | `TestIdenticalFontRestoreIsSkipped` | PASS |

Registry-mutating tests use scoped cleanup so their HKCU values are removed even when an assertion throws.

## Fresh verification

- `powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1` — normalization and Pts suites passed.
- `powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1 -Coverage` — suites passed; the extracted normalization module reported **94.85% line coverage**, **100% branches executed**, and **82% branches taken at least once**.
- The production `build.ps1` linked `ToolType.exe` successfully with `-Wall -Wextra -Wpedantic` and no warnings.
- A Win32 smoke launch opened the production Pts popup at `526x359` outer pixels on the 96-DPI test desktop. The captured popup was opaque, showed no owner controls through its client area, and kept actions, status/percentage, progress bar, and close button visibly separated.
- `git diff --check` and PowerShell parser checks — passed.

Coverage percentages above apply to `text_normalization.cpp`; the Pts suite is a native Win32 regression/integration suite and does not claim a separate line-coverage percentage. Real-machine performance still depends on archive size, storage, antivirus scanning, and the number of installed fonts. A single blocking Windows/font API call cannot be interrupted internally, so `Hủy` takes effect immediately after that safe call returns; the UI thread itself remains available because the call runs on the worker.
