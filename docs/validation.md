# Feature 6 and Feature 9: Validation Record

These checks were actually performed on 20 November 2026. They establish the implemented features' behavior; they do not represent the prescribed final coursework benchmark or submission of the complete group assignment. The subsequent English-language revision changes documentation rather than the tested C algorithms.

## Compilation

Both application variants compiled with zero errors and zero warnings using the existing GCC toolchain and:

```text
-std=c17 -O2 -Wall -Wextra -Wpedantic -Werror
```

- `build/coursework.exe`: enables the original folder adapter and also supports collection-list input.
- `build/coursework_standard.exe`: excludes `dirent.h`; the compiled program uses only standard C library interfaces and configured collection-list input.

## Module contract tests

Source: `tests/test_features.c`. The latest actual run reported `1012 checks, 0 failures`. This count includes assertions and required allocation/file-I/O checks; it is not a count of 1012 independent scenarios.

Coverage included:

- Independent expected values for three documents, including an empty document: 5 occurrences, 3 unique terms and 4 posting relationships.
- Per-document counters, case normalization, punctuation, duplicate query terms and deterministic tie ordering.
- TXT/CSV statistics, quoted fields, commas, multiline queries and UTF-8 filenames.
- A hand-written valid index as an independent loading oracle, save/load round trips, document IDs and original text.
- Empty indexes, repeated loading and cleanup.
- 21 malformed-index variants, including invalid format/version, numeric overflow, limits, duplicates, invalid IDs/frequencies and inconsistent counters.
- Missing files, empty files, truncation and embedded NUL. An unsuccessful load preserved the existing index and its search behavior.

Reproduce from the project root:

```powershell
New-Item -ItemType Directory -Path tests/run -Force
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror tests/test_features.c index.c statistics.c persistence.c -o tests/run/test-features.exe
.\tests\run\test-features.exe
```

## Menu and process tests

Source: `tests/test_cli.py`. All 28 process/result checks passed. Python is a verification tool; the application does not call it.

The driver checked:

- Folder loading, empty documents, invalid menu input, operations before an index is ready and punctuation-only queries.
- Search and report export, using Python's standard CSV reader to verify actual fields and counters.
- Saving, terminating the process, renaming the test corpus so its original path was unavailable, and loading in a new process.
- Restored search, document preview and identical statistics without running the collection-loading or index-rebuilding options.
- Identical statistics from the standard-C list input and folder input modes.
- Continued use of the old index after loading a truncated file failed.
- A synthetic collection of 50 documents, 100000 occurrences, 4 unique terms and 200 posting relationships.
- Saving and restoring that collection in a new process, then returning `apple` frequency 500 for every document with ties ordered by document ID.

Measured whole-process wall times were approximately 0.339 seconds for loading/building/reporting/saving and 0.013 seconds for loading/searching. These include startup and output, and the operation sets differ. They do not establish indexed-query latency or a speedup over linear scanning. No prescribed retrieval-quality metrics or final benchmark were measured in these smoke tests.

Raw menu output and `summary.json` remain in the local ignored directory `build/e2e-s89j2rnd/`; module outputs are in `tests/run/`. The source-and-run package includes a concise evidence subset. New test runs create their own directories under `build/`.

To reproduce, first compile both application variants using the README commands, then run:

```powershell
python tests/test_cli.py
```

## Verification after GitHub upload

After the original implementation was uploaded, its branch was fetched again. All 26 repository files matched the local version after normalizing line endings. The freshly fetched source passed strict C17 compilation, sample statistics/save behavior, and loading in a new process. The initial uploaded revision checked was `e411bb73993f5a4de7a627920d10488c0eae67bc`.

This records actual checks. The group must separately collect evidence for the prescribed datasets, full-system evaluation, reports and presentation deliverables.
