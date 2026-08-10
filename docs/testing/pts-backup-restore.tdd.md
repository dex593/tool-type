# TDD evidence: Pts backup/restore workflows

## User journey and acceptance criteria

- Restore the `Pts` popup for Photoshop settings and font backup/restore using `.afang` archives.
- When several Photoshop versions are installed, backup must let the user choose exactly one source version; restore must let the user choose both the archived source version and the installed destination version.
- Font restore must avoid redundant file writes and repeated `-restored-N` copies, register fonts for the current user, and keep the popup responsive during large scans/registration batches.
- Restore must reject malformed or unsafe paths, preserve an existing destination if a write fails, and never claim CS6 workspace success when conversion is incompatible.

## RED checkpoints

- Commit `4f80a21 test: add RED coverage for restored Pts workflows` introduced the first executable specification while the production symbols were absent; the Pts suite failed to compile as expected.
- Cross-version CS6 workspace tests then exposed unsupported workspace conversion.
- A repeated font restore with an existing different file exposed redundant `-restored-2.ttf` creation.
- Archive validation tests exposed acceptance of `.` path segments and Windows reserved device names.
- An incompatible CS6 workspace archive exposed a false-success/raw-write fallback.
- The final responsiveness regression added a 160-entry archive inspection callback requirement. Before the callback API existed, `run-pts-tests.ps1` failed to compile with “too many arguments to function `InspectPtsArchiveVersions`”.
- Additional filename tests rejected `CON.backup.ttf` and control characters; both represent Windows path rules not covered by the initial validator.

## GREEN implementation

- Restored `.afang` format version 1 with bounded metadata, raw/zlib payloads, exact entry-kind validation, and seek-based payload skipping during preflight.
- Restored installed-Photoshop discovery and explicit version selection for both backup and restore. There is no silent “all versions” backup choice.
- Added cross-version path aliases and explicit CS6 workspace outcomes: not needed, already compatible, converted, or incompatible.
- Restored fonts into `%LOCALAPPDATA%\Microsoft\Windows\Fonts`, reusing identical destination/collision files, batching HKCU Registry updates, and broadcasting `WM_FONTCHANGE` once.
- Writes to existing Photoshop settings are staged in the same directory, flushed, and atomically replaced so a failed write does not truncate the original.
- Progress callbacks now pulse during settings/font discovery, `.afang` inspection, extraction, and font registration. The popup throttles paints/message pumping to approximately 75 ms while disabling its actions during work.
- Windows path validation rejects traversal, empty/dot segments, control characters, trailing dot/space, and reserved device prefixes before the first period.

## Regression specification

| Guarantee | Test target | Result |
|---|---|---|
| Safe archive paths and Windows device names | `TestSafeArchivePaths` | PASS |
| `.afang` compression/decompression round trip | `TestCompressionRoundTrip` | PASS |
| Large metadata inspection reports periodic progress | `TestArchiveInspectionReportsProgress` | PASS |
| Multiple installed Photoshop versions are discovered and one selected version is archived | `TestSettingsBackupArchive` | PASS |
| Modern settings map to CS6 aliases/workspace layout | `TestCrossVersionMappingAndCs6Aliases`, `TestCrossVersionArchiveRestore` | PASS |
| A second identical font restore reuses `-restored-1` and preserves unrelated Registry mappings | `TestIdenticalFontRestoreIsSkipped` | PASS |
| Malformed archives perform no writes | `TestMalformedArchiveDoesNotWriteAnything` | PASS |
| Failed destination replacement preserves original bytes | `TestAtomicRestoreWritePreservesLockedDestination` | PASS |
| Incompatible CS6 workspace data fails without a raw write | `TestIncompatibleCs6WorkspaceFailsInsteadOfClaimingSuccess` | PASS |

Registry-mutating tests use scoped cleanup so their HKCU values are removed even when an assertion throws.

## Fresh verification

- `powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1` — normalization and Pts suites passed.
- `powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1 -Coverage` — suites passed; the extracted normalization module reported **94.85% line coverage**, **100% branches executed**, and **82% branches taken at least once**.
- `powershell -ExecutionPolicy Bypass -File .\build.ps1` — production executable linked successfully with `-Wall -Wextra -Wpedantic` and no warnings.
- `git diff --check` and PowerShell parser checks — passed.

Coverage percentages above apply to `text_normalization.cpp`; the Pts suite is a native Win32 regression/integration suite and does not claim a separate line-coverage percentage. Real-machine performance still depends on archive size, storage, antivirus scanning, and the number of installed fonts, but the implementation removes redundant payload scans/writes and keeps the dialog message queue serviced during long loops.
