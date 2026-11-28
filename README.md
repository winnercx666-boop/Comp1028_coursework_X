# COMP1028: Document Search and Information Retrieval System

Implementation of **Feature 6: Statistics and Reporting** and **Feature 9: Persistent Index**, integrated with the group's original tokenization and term-frequency ranking code at commit `2012ef8`.

## Build and run

A GitHub checkout contains source files rather than executable binaries. Build the program before running it. The separate source-and-run ZIP package includes prebuilt Windows executables.

From the project directory, use the existing GCC toolchain:

```powershell
New-Item -ItemType Directory -Path build -Force
gcc -DENABLE_DIRECTORY_INPUT -std=c17 -O2 -Wall -Wextra -Wpedantic coursework.c collection.c index.c statistics.c persistence.c -o build/coursework.exe
.\build\coursework.exe
```

Alternatively, double-click `run.cmd` after building. It starts the program from the project directory and supports executables in either `build/` or the project root.

`ENABLE_DIRECTORY_INPUT` enables the original group's `dirent.h` folder-scanning adapter. The compiler must provide that platform interface. The indexing, statistics and persistence modules use only standard C libraries.

For a build that uses only standard C library interfaces, omit the adapter and load a configured collection list:

```powershell
New-Item -ItemType Directory -Path build -Force
gcc -std=c17 -O2 -Wall -Wextra -Wpedantic coursework.c collection.c index.c statistics.c persistence.c -o build/coursework_standard.exe
.\build\coursework_standard.exe
```

In this mode, select menu option `1` and enter `examples/collection.txt`. Each non-comment line identifies a document. Relative paths are resolved from the collection list's directory. Empty lines and lines beginning with `#` are ignored. Both input modes produce the same statistics and index format.

If GNU Make is available, use `make`, or `make CPPFLAGS=` for collection-list input without the folder adapter. The Makefile creates `coursework.exe` in the project root; `run.cmd` also supports that location.

## Independent Task 6 and Task 9 files

The independently runnable task sources are available directly in the repository root:

| Source | Purpose |
|---|---|
| [task6.c](task6.c) | Task 6: statistics and TXT/CSV reports |
| [task9.c](task9.c) | Task 9: index saving/loading and validation |

Build each standalone source as its own program. The integrated group application uses the five explicit production sources in the build commands above. See the [standalone task guide](docs/standalone-tasks.md) for compilation, sample input and menu instructions.

## Quick demonstration

1. Select `1` and enter `examples/documents` to load the sample folder and build its index. In the standard-C build, enter `examples/collection.txt` instead.
2. Select `3` to display collection, per-document and index statistics.
3. Select `4`, search for `apple`, and press Enter to use the default Top K of 5. Enter `/exit` to return to the menu.
4. Select `6` to save `report.txt`, or `7` to save `report.csv`. Press Enter at the file prompt to accept the default name.
5. Select `8` and press Enter to save `index.idx`.
6. Select `0` to exit, then start the program again.
7. Select `9` immediately and load `index.idx`. Do not load the collection or rebuild the index first.
8. Use `3`, `4` and `5` to display statistics, search and view restored documents.

Relative output paths use the program's working directory. Paths may contain spaces; enter them without additional quotation marks. Document IDs start at `0`, while result ranks start at `1`.

## Menu and modules

| Menu option | Operation |
|---|---|
| 0 | Exit and release allocated memory |
| 1 | Load a collection and build its index |
| 2 | Rebuild the current index from its stored document text |
| 3 | Display collection, per-document and index statistics |
| 4 | Search and display ranked results |
| 5 | View a document by ID |
| 6 | Export a text report |
| 7 | Export a CSV report |
| 8 | Save the current index |
| 9 | Load a saved index without rebuilding it |

| Module | Responsibility |
|---|---|
| `statistics.c` / `statistics.h` | Feature 6: display statistics and export TXT/CSV reports, optionally including the most recently displayed search results |
| `persistence.c` / `persistence.h` | Feature 9: validate, save and restore document IDs, text, terms, frequencies and counters |
| `search_index.h` / `index.c` | Shared index structures, construction, lookup, search and memory cleanup |
| `collection.c` / `collection.h` | Standard-C collection-list input and the optional original folder adapter |
| `coursework.c` | Menu integration, document preview and recent-query state |

The original function-local index is exposed through `SearchIndex` so search, reporting and persistence can reuse the same posting lists. Compile the five integrated application sources listed in the build commands above; their project headers remain required. The standalone `task6.c` and `task9.c` files are each compiled as a separate program.

## Statistics and ranking definitions

A term consists of consecutive ASCII English letters. Terms are converted to lowercase; other characters are separators. Empty tokens are ignored. Complete terms longer than 1023 bytes are skipped, matching the original engine's rule. No stopword removal is applied.

- **Total terms:** every indexed occurrence, including repeated words.
- **Unique terms:** distinct normalized terms across the collection.
- **Posting relationships:** distinct term-document pairs. A word repeated in one document contributes one relationship with a larger frequency.
- **Per-document statistics:** total occurrences and distinct terms within each document.

For `apple apple banana` and `apple orange`, the collection has 5 occurrences, 3 unique terms and 4 posting relationships. Summing each document's unique-term count would incorrectly count `apple` twice.

A document's score is the sum of its frequencies for the distinct normalized query terms. Results are ordered by descending score, then ascending document ID. Reports can include the most recently displayed Top K results. Loading or rebuilding an index clears obsolete recent-query results.

## Persistence and limits

The default file is `index.idx`. It uses a versioned, length-prefixed text representation, with original filename and document bytes stored alongside terms, document IDs and frequencies. Runtime pointers and query markers are not serialized.

Loading reconstructs posting lists directly rather than calling `index_build()`. Because document text is also restored, preview and reporting remain available when original input files are unavailable. The current index is replaced only after the entire file passes validation. Failed loads leave it usable.

The collection limit is 6000 documents, with at most 199999 bytes per document and 299 bytes per filename. Unreadable, oversized or NUL-containing input files are skipped with an explanation. Empty documents count toward the document total and have zero terms. Output directories must exist; saving to an existing report or index path overwrites that file.

## Documentation and evidence

- [Feature 6 statistics and report format](docs/statistics.md)
- [Feature 9 index file format](docs/index-format.md)
- [Code-reading and demonstration guide](docs/task6-task9-guide.md)
- [Recorded validation and reproduction commands](docs/validation.md)

The functional and synthetic-scale checks are recorded in the validation document. The sample corpus is for demonstration. The group still needs to evaluate the full system on the coursework-prescribed 20 Newsgroups/Cranfield data and the final released benchmark, and complete the remaining group deliverables.

The running C program does not call AI services, retrieval frameworks, or another language to perform its core operations. Python is used only by a separate test driver.

## Original group information

Repository: `WinnerCX666-BOOP/Comp1028_coursework_X`.

Group members listed in the original repository: Chen Xiang, Liu YiLe, Zhong YiLin and Pan WeiLin.
