# TDD evidence: Pts backup/restore workflows

## User journey and acceptance criteria

- Restore the `Pts` popup for Photoshop settings and font backup/restore using `.afang` archives.
- When several Photoshop versions are installed, backup must let the user choose exactly one source version; restore must let the user choose both the archived source version and the installed destination version.
- Font restore must avoid redundant file writes and repeated `-restored-N` copies, register fonts for the current user, and keep the popup responsive during large scans/registration batches.
- Backup/restore core work must run outside the UI thread, and the user must be able to cancel an active operation without terminating a thread unsafely.
- Restore must reject malformed or unsafe paths, preserve an existing destination if a write fails, and never claim CS6 workspace success when conversion is incompatible.
- A CS6-to-CS6 restore must preserve native workspace bytes and repair only a malformed legacy counterpart alias, without overwriting a valid distinct workspace.
- The Pts popup and its Photoshop-version chooser must be opaque, keep status/percentage/progress/actions in separate regions, and scale their complete layout at the active system DPI.
- Short live status text may be compacted, but omitted text must be marked with `...`; completion summaries must stay concise (`Đã sao lưu, 340 KB, Photoshop CS6` / `Đã sao lưu 24 font, 3,4 MB` and the restore equivalents), while error dialogs must retain the error code and recovery path.
- The progress bar must never move backward inside one operation, must show an indeterminate animation while a phase has no meaningful percentage, must not display a percentage when nothing is running or after a stop, and must read `100%` when an operation completes.

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
- This pass replaces the single raw percentage with a `PtsProgressDisplay` state machine (`Idle`/`Marquee`/`Determinate`/`Done`). Starting an operation animates a two-block marquee (`WM_TIMER`, 30 ms) until the first real percentage arrives; worker percentages are clamped monotonically through `AdvancePtsShownPercent`, so the bar no longer jumps backward when the workflow switches phases (for example settings scan 12% → font scan 8%). Idle and stopped states clear the percentage label instead of showing a stale `0%`/`37%`/`100%`, completed operations pin the bar and label at `100%`, restore preflight no longer inherits the previous run's percentage, and the bar repaints without background erase to stop flicker.
- `kPtsNoticeLineLimit` grows from 40 to 56 characters per line, so typical phase notices such as `Đang nén Photoshop WorkspaceLayout (12/34)...` now fit on one status line instead of being wrapped and cut; two-line compaction with a trailing `...` remains the fallback for longer text.
- Completion summaries are rebuilt through `FormatPtsBackupSummary`/`FormatPtsRestoreSummary`. Backup keeps sizes: settings-only reads `Đã sao lưu, 340 KB, Photoshop CS6.`, fonts-only reads `Đã sao lưu 24 font, 3,4 MB.`, and combined runs join both categories. Restore drops sizes and counts items instead: `Đã khôi phục 24 font.`, `Đã khôi phục 120 file cài đặt, Photoshop 2025 → Photoshop CS6.`, appending the version mapping (`Adobe Photoshop 2025 → Adobe Photoshop CS6`) only for cross-version restores. Routine mechanics notes (workspace conversions, alias counts, `Đã cập nhật SettingsFilePath.`) were dropped; only actionable notes survive — skipped fonts/settings by version, fonts copied but not yet loaded, and Registry paths that could not be updated.
- Font backup now opens a selection popup before the save dialog. The list is gathered through the existing default-Windows-font filter (`IsDefaultWindowsFont`), sorted case-insensitively by display name, and shown as one full scrolling checklist — no pagination. A `Tìm kiếm` box above the list filters as you type (debounced 180 ms like an AJAX autocomplete): `FoldPtsFontSearchKey` folds case and Vietnamese diacritics while keeping spaces meaningful, so `SJ ` matches `SJ abc` but not `SJabc`; `PtsFontMatchesQuery` substring-matches the folded query against the display name or the file name, and a placeholder row appears when nothing matches. Each row is an owner-drawn checkbox with the font name and right-aligned size; the footer shows `Đã chọn N/M font — tổng dung lượng` live, the OK button reads `Sao lưu (N)` and disables at zero. `Chọn tất cả`/`Bỏ chọn` act on the current filter, so a search plus `Chọn tất cả` selects exactly the matching fonts. Chosen fonts are passed to the worker as `PtsBackupOptions::presetFontItems`, so the archive contains exactly the selected fonts (`usePresetFontItems`); the non-interactive path still gathers everything itself.
- The same popup can uninstall fonts: `Xóa (N)` (enabled when at least one font is checked) asks for an explicit `MB_YESNO` confirmation, then `RemoveInstalledFont` calls `RemoveFontResourceW` first (releasing the GDI lock that made plain `DeleteFileW` fail with access-denied code 5), removes the owning user/machine `...\CurrentVersion\Fonts` registry value (reporting admin-right failures for HKLM), deletes the file, and one `WM_FONTCHANGE` broadcast follows the batch. A file Windows still holds after a clean uninstall is reported as pending reboot instead of a failure. The batch pumps dialog messages between fonts and shows `Đang xóa font i/N...`, so the dialog never freezes; buttons are disabled and re-entry guarded while deleting. Deleted fonts are compacted out of the list, fold cache, and search filter immediately; the result dialog reports `Đã xóa N font[, K font không xóa được]` plus pending/error notes.
- Search matching is word-boundary aware: tokens must begin at a word start (space/`-`/`_`/`.`/begin), and a token the user finished with a space must also end on a boundary — `sj ` matches `SJ abc` but neither `SJabc` nor `Boxing Wizards`. Performance: display/file names are folded once into a per-dialog cache, the query is parsed once per keystroke, list rebuilds are wrapped in `WM_SETREDRAW`, and unchanged filter results skip the rebuild entirely. Button captions that outgrow their control now clip with `...` (`DT_END_ELLIPSIS`) instead of spilling, the backup OK button keeps a static `Sao lưu` caption (the live count stays in the summary line), and because `EM_SETCUEBANNER` never rendered on the target setup, the `Tìm kiếm` placeholder is owner-drawn by a search-edit subclass: gray text while the box is empty and unfocused, repainted on focus/text-change notifications.
- Hover highlights can no longer stick after a click: `ButtonHoverEffectActive` suppresses the hover style while the mouse is captured mid-click or no longer over the button, and `ButtonWndProc` drops `hoverButtonId_` on `WM_LBUTTONUP` (after any modal popup opened by the command returns) so a swallowed `WM_MOUSELEAVE` cannot leave a stale highlight.

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
| Progress display is monotonic per operation, hides stale percentages when idle/waiting/stopped, and pins `100%` on completion | `TestPtsProgressDisplayIsMonotonicAndNeverStale` | PASS |
| A typical phase notice fits one status line; longer notices stay within the two-line area | `TestCompactPtsNoticeKeepsTypicalPhaseMessage` | PASS |
| Backup/restore completion summaries follow the concise `size[/font count/size][, version]` shapes, including skip notes | `TestPtsCompletionSummariesAreConcise`, headline assertion in `TestCrossVersionArchiveRestore` | PASS |
| Font selection popup: default Windows fonts never offered, sorted list, space-aware diacritic-insensitive live search over name/file, delete button/summary shapes | `TestFontSelectionPopupHelpers` | PASS |
| Font pick dialog keeps a DPI-scaled, non-overlapping layout including the search box and delete button | `TestFontPickDialogLayoutIsDpiScaledAndSeparated` | PASS |
| Deleting an installed font removes its HKCU registry value and file, and absent files stay a no-op | `TestDeleteInstalledFontRemovesRegistryEntryAndFile` | PASS |
| Hover highlight dies on click capture and never survives a departed cursor | `TestHoverEffectIsNotRetainedAfterClick` | PASS |
| A preset (user-selected) font list is archived exactly as chosen | `TestPresetFontSelectionArchivesOnlySelectedFonts` | PASS |
| Partial restore cancellation states that applied changes are not automatically rolled back | `TestCancelledRestoreStopsBeforeNextFile` | PASS |
| Cancelling after only an identical font no-op reports that no change was applied | `TestIdenticalFontRestoreIsSkipped` | PASS |

Registry-mutating tests use scoped cleanup so their HKCU values are removed even when an assertion throws.

## Fresh verification

- `powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1` — normalization and Pts suites passed, including the progress-display, phase-notice, concise-summary, and font-selection regressions.
- The production `build.ps1` linked `ToolType.exe` successfully with `-Wall -Wextra -Wpedantic` and zero warnings.
- A Win32 smoke launch started the production binary and it stayed alive; the process was then terminated by hand. The marquee/Done visuals were verified through the paint/state-machine code paths compiled by the Pts suite rather than a scripted screenshot.
- `git diff --check` — passed.
- Earlier passes on this document: `run-tests.ps1 -Coverage` reported **94.85% line coverage** for `text_normalization.cpp` with **100% branches executed** (module untouched by this change), and a captured 96-DPI smoke launch showed the `526x359` popup opaque with separated regions; both remain valid because the `520x330` logical layout and dialog styles are unchanged.

Coverage percentages above apply to `text_normalization.cpp`; the Pts suite is a native Win32 regression/integration suite and does not claim a separate line-coverage percentage. Real-machine performance still depends on archive size, storage, antivirus scanning, and the number of installed fonts. A single blocking Windows/font API call cannot be interrupted internally, so `Hủy` takes effect immediately after that safe call returns; the UI thread itself remains available because the call runs on the worker.
