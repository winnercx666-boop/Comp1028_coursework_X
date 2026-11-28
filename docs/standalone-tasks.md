# Single-file Statistics, Reporting and Persistent Index

The repository-root `statistics_index.c` combines the two contributions in one C17 program:

| Contribution | Functionality |
| --- | --- |
| Task 6 | Collection, document and index statistics; TXT/CSV reports with optional latest search results |
| Task 9 | Versioned index saving/loading and strict validation; failed loading preserves the current usable index |

The source includes one copy of the shared document-processing/indexing structures and functions, both feature implementations, and one menu entry point. It compiles on its own without project-specific headers or other C sources and retains the full MIT license notice.

## Compile and run

Run these commands from the repository root in PowerShell:

```powershell
New-Item -ItemType Directory -Path build -Force | Out-Null
gcc -std=c17 -O2 -Wall -Wextra -Wpedantic statistics_index.c -o build/statistics_index.exe
.\build\statistics_index.exe
```

The default build uses the standard C library and accepts a collection-list file. Select menu option `1` and enter:

```text
examples/collection.txt
```

Document paths in the list are resolved relative to the list's directory. The supplied sample has 3 documents, including an empty document, 5 word occurrences, 3 unique words and 4 term-document relationships.

Compile this source as its own program. The integrated group application continues to use its explicit five-source build in the root README.

## Menu

| Option | Action |
| --- | --- |
| 1 | Load a collection and build its index |
| 2 | Rebuild the current index |
| 3 | Show collection, document and index statistics |
| 4 | Search; use `/exit` to return to the menu |
| 5 | Preview a document by its zero-based ID |
| 6 | Save a TXT report |
| 7 | Save a CSV report |
| 8 | Save the current index |
| 9 | Load a saved index |
| 0 | Exit |

Enter at the output/input-path prompt to use `report.txt`, `report.csv` or `index.idx`. Relative paths use the program's working directory; run from the repository root to use the examples above. Enter paths containing spaces without additional quotation marks.

## Demonstrate both contributions

1. Choose `1` and load `examples/collection.txt`.
2. Choose `3` to display all statistics.
3. Choose `4`, search for `apple`, press Enter for Top K of 5, then enter `/exit`.
4. Choose `6` and `7` to export text and CSV reports, optionally accepting the default filenames. Reports include the latest displayed search results.
5. Choose `8` to save the index, then `0` to exit.
6. Restart the program and choose `9` to load the saved index without rebuilding or loading the collection first.
7. Choose `3`, `4`, `5`, `6` or `7` to inspect, search, preview or report on the restored index. Saved document text supports these operations even when the original input files are unavailable.

Successfully loading a collection or saved index, or rebuilding, clears obsolete recent-query results. A malformed saved index produces a diagnostic and preserves the current usable index.

## Optional folder input

When the compiler supplies `dirent.h`, enable the existing folder adapter:

```powershell
gcc -DENABLE_DIRECTORY_INPUT -std=c17 -O2 -Wall -Wextra -Wpedantic statistics_index.c -o build/statistics_index_folder.exe
.\build\statistics_index_folder.exe
```

Choose `1` and enter `examples/documents` when running from the repository root.

## Validation

The combined source passed strict C17 compilation with -O2 -Wall -Wextra -Wpedantic -Werror in both standard and folder-input builds. Each build also passed in an isolated directory containing only this source and no project headers or other sources.

The existing feature-contract driver passed 1012 checks with 0 failures against the combined implementation. These are assertions and required allocation/file-I/O checks, rather than 1012 independent scenarios. All 28 checks in the existing CLI driver passed, covering statistics, TXT/CSV export, search, save/restart/load without the original documents, restored preview/reporting, failed-load preservation and a 100,000-token sample.

Final project benchmarking follows the group's evaluation plan.
