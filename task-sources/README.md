# Independent C Sources for Tasks 6 and 9

Each task is a complete program in one C file. Each source includes its own entry point, shared structures and required implementations, so it can be compiled on its own without project-specific header files.

| File | Task and functionality |
| --- | --- |
| `task6.c` | Task 6: collection/document/index statistics and TXT/CSV reports, with document input, index building, search and preview for demonstration |
| `task9.c` | Task 9: versioned index saving/loading and validation, with document input, index building, restored search, document preview and status display |

The Task 6 program contains the statistics/reporting module. The Task 9 program contains the persistence module. Both retain the shared document-processing and index support they need, together with the full MIT license notice.

## Compile and run

Run these commands from the repository root in PowerShell:

```powershell
New-Item -ItemType Directory -Path build -Force | Out-Null
gcc -std=c17 -O2 -Wall -Wextra -Wpedantic task-sources/task6.c -o build/task6.exe
gcc -std=c17 -O2 -Wall -Wextra -Wpedantic task-sources/task9.c -o build/task9.exe
.\build\task6.exe
```

Run Task 9 with:

```powershell
.\build\task9.exe
```

Compile each source separately as its own program. Both files define `main` and include shared implementations.

The default builds use the standard C library and accept a collection-list file. Select menu option `1` and enter:

```text
examples/collection.txt
```

Document paths in the list are resolved relative to the list's directory. The supplied sample has 3 documents, including an empty document, 5 word occurrences, 3 unique words and 4 term-document relationships.

If you run an executable with `task-sources` as the working directory, use `../examples/collection.txt` instead.

## Task 6 menu

| Option | Action |
| --- | --- |
| 1 | Load a collection and build its index |
| 2 | Rebuild the current index |
| 3 | Show collection, document and index statistics |
| 4 | Search; use `/exit` to return to the menu |
| 5 | Preview a document by its zero-based ID |
| 6 | Save a TXT report |
| 7 | Save a CSV report |
| 0 | Exit |

Enter at the report-path prompt to use `report.txt` or `report.csv`. Reports can include the latest displayed search results. Successfully loading a new collection or rebuilding clears old result state.

## Task 9 menu

| Option | Action |
| --- | --- |
| 1 | Load a collection and build its index |
| 2 | Save the current index |
| 3 | Load a saved index |
| 4 | Search the current/restored index |
| 5 | Preview a document by its zero-based ID |
| 6 | Show readiness and index counters |
| 0 | Exit |

To demonstrate persistence, load the sample with option `1`, then save with option `2`. Press Enter to use `index.idx`. Exit, restart Task 9, and use option `3` to load that file. Search and preview work from the restored index without loading the original collection again. Invalid index files produce an error and preserve the current usable index.

## Optional folder input

When the compiler supplies `dirent.h`, enable the existing folder adapter:

```powershell
gcc -DENABLE_DIRECTORY_INPUT -std=c17 -O2 -Wall -Wextra -Wpedantic task-sources/task6.c -o build/task6_folder.exe
gcc -DENABLE_DIRECTORY_INPUT -std=c17 -O2 -Wall -Wextra -Wpedantic task-sources/task9.c -o build/task9_folder.exe
```

At menu option `1`, use `examples/documents` when running from the repository root, or `../examples/documents` when running from this directory.

## Validation

Both sources compiled without diagnostics using strict C17 flags (`-O2 -Wall -Wextra -Wpedantic -Werror`), with and without `ENABLE_DIRECTORY_INPUT`.

The existing feature-contract suite was linked separately against each standalone source's reused core, with its `main` renamed for testing and the other feature supplied by its original module. Each run passed 1012 checks with 0 failures. These are assertion and required allocation/file-I/O checks, not 1012 independent scenarios.

Task 6 also passed 34 standalone menu/process checks covering expected counts, TXT/CSV consistency, quoted query text, filenames with commas, latest-result snapshots, rebuild/no-match clearing, readiness guards and EOF handling. Task 9 was smoke-tested for save/restart/load, search, preview and preservation of the current index after a malformed load.

These checks cover source extraction and functional behaviour. Final project benchmarking follows the group's evaluation plan.

The integrated group application remains available through the repository-root modules and its existing build instructions. These two sources are independently runnable task deliverables.
