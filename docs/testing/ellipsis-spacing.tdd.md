# TDD evidence: attached ellipsis spacing

## Source and user journey

- Source plan: none. The journey and acceptance criteria were derived from the reported regression.
- User journey: As a ToolType user, I want an ASCII ellipsis that is attached to the following word in the source text to stay attached when pasted, so that ToolType does not silently change `...hôm` into `... hôm`.
- Acceptance criteria:
  - `...hôm` remains `...hôm` through normalization and the full per-line paste pipeline.
  - An existing gap in `... hôm` is normalized to one space rather than removed.
  - Ordinary sentence punctuation and numeric separators keep their previous behavior.

## Task report

### RED: reproduce the defect

- Checkpoint: `6cf60d4 test: add RED reproducer for attached ellipsis spacing`
- Command: `powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1`
- Result: exit code `1`; three assertions failed for leading ellipsis, inline ellipsis, and the marker-stripping paste path.
- Relevant observed difference: expected `...hôm`, actual `... hôm`.
- Guarantee introduced by the test: the reported transformation is executable and fails specifically in punctuation-spacing logic before the production fix.

### GREEN: preserve attached ellipses

- Checkpoint: `bd7b1af fix: preserve attached ASCII ellipses during paste normalization`
- Command: `powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1`
- Result: exit code `0`; `All normalization tests passed.`
- Implementation: `EndsEllipsisRun` recognizes the third and later consecutive ASCII periods, and `NormalizeSpacesAfterPunctuation` no longer treats those periods as standalone sentence terminators.
- Guarantee: ToolType preserves the source's no-space choice after `...` while retaining the existing single-period and numeric-separator rules.

### Refactor and integration verification

- Checkpoint: `b6f9489 refactor: isolate text normalization for regression coverage`
- The pure text pipeline moved from the Win32 entry-point file into `text_normalization.cpp`/`.h`; the production app and tests now compile the same implementation.
- Test command: `powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1`
- Build command: `powershell -ExecutionPolicy Bypass -File .\build.ps1`
- Results: tests exited `0`; production build exited `0` with `Build OK: ...\ToolType.exe` and no compiler warnings from `-Wall -Wextra -Wpedantic`.

### Review hardening

- Checkpoint: `4c4cc53 perf: keep ellipsis detection linear for long runs`
- `EndsEllipsisRun` uses three constant-time neighbor checks rather than rescanning the complete dot run for every period.
- A 100,000-period regression case passes without changing the input.
- `.github/workflows/build.yml` now runs the focused suite with its 80% coverage gate before the production build and packaging steps.

## Test specification

| # | What is guaranteed | Test target | Type | Result | Evidence |
|---|---|---|---|---|---|
| 1 | Leading `...hôm` remains attached | `tests/normalization_tests.cpp:23` | Unit | PASS | `tests/run-tests.ps1` |
| 2 | Inline `Chờ...xem` remains attached | `tests/normalization_tests.cpp:26` | Unit | PASS | `tests/run-tests.ps1` |
| 3 | Existing whitespace after `...` collapses to one space | `tests/normalization_tests.cpp:29` | Unit | PASS | `tests/run-tests.ps1` |
| 4 | A standalone period still receives a missing following space | `tests/normalization_tests.cpp:32` | Regression | PASS | `tests/run-tests.ps1` |
| 5 | Decimal/time numeric separators remain unchanged | `tests/normalization_tests.cpp:35` | Regression | PASS | `tests/run-tests.ps1` |
| 6 | Marker stripping plus paste normalization preserves `...hôm` | `tests/normalization_tests.cpp:38` | Integration | PASS | `tests/run-tests.ps1` |
| 7 | Blank/comment lines, markers, closers, and pasteability remain compatible | `tests/normalization_tests.cpp:41-60` | Regression | PASS | `tests/run-tests.ps1` |
| 8 | The Win32 executable links against the extracted normalization module | `build.ps1` | Build integration | PASS | `build.ps1` |
| 9 | A 100,000-period run remains unchanged and completes without quadratic rescanning | `tests/normalization_tests.cpp:62` | Performance regression | PASS | `tests/run-tests.ps1` |

## Coverage and known gaps

- Command: `powershell -ExecutionPolicy Bypass -File .\tests\run-tests.ps1 -Coverage`
- `text_normalization.cpp` line coverage: **94.85%** (`92/97` executable lines approximately, as reported by gcov).
- Branches executed: **100.00%**; branches taken at least once: **82.00%**; calls executed: **89.02%**.
- The coverage threshold in the runner rejects line coverage below 80%.
- Coverage is intentionally scoped to the extracted text-normalization module. The Win32 UI, keyboard hook, clipboard, network, and document-loading code are not claimed as covered by this focused regression suite.
- No GUI E2E test was added because the defect is fully reproduced before clipboard/keyboard interaction; the production build is the integration check for the Win32 call site.
- GitHub Actions runs the same focused tests with coverage before building and packaging the application.

## Merge evidence

The RED, GREEN, refactor, and review-hardening checkpoints are separate commits on the active `master` branch. If they are later squashed, preserve this report in the resulting commit or pull-request description.
