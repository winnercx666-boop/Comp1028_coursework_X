# Single-File Source and Windows Run Package

This directory contains the complete application in one source file, `coursework.c`. Feature 6 (Statistics and Reporting) and Feature 9 (Persistent Index) occupy adjacent labeled sections. Shared structures, document input, indexing, ranking and the console menu are included in the same file.

- Feature 6 starts at line 550: search for `FEATURE 6: STATISTICS AND REPORTING`.
- Feature 9 starts at line 881: search for `FEATURE 9: PERSISTENT INDEX`.
- No project-specific header files or additional production C files are required to compile this source.

## Run the prebuilt Windows package

Download [COMP1028_single_file.zip](COMP1028_single_file.zip), extract the entire archive, open the extracted `COMP1028_single_file` folder and double-click `run.cmd`.

At `Choice:`, enter `1`. At the document-path prompt, enter `examples/documents`. The sample contains 3 documents, 5 term occurrences, 3 unique terms and 4 posting relationships. At the menu, use `3` for statistics, `4` for search and `5` for document preview. Enter `/exit` to leave search mode.

Options `6` and `7` save TXT/CSV reports; option `8` saves `index.idx`. After exiting and restarting, option `9` restores the saved index without rebuilding from the original documents. Press Enter at each output/input-file prompt to use its default filename.

The package includes the single C source, Windows executables, examples, configuration, launcher, license and validation summary.

## Compile the source in this repository

From the repository root, enter this directory:

```powershell
cd single-file
```

Standard-C collection-list build:

```powershell
gcc -std=c17 -O2 -Wall -Wextra -Wpedantic coursework.c -o coursework_standard.exe
.\coursework_standard.exe
```

At menu option 1, enter `../examples/collection.txt`. The collection-list paths are resolved against the list's directory.

Optional original folder-input adapter, when the compiler supplies `dirent.h`:

```powershell
gcc -DENABLE_DIRECTORY_INPUT -std=c17 -O2 -Wall -Wextra -Wpedantic coursework.c -o coursework.exe
.\coursework.exe
```

At menu option 1, enter `../examples/documents`. After compiling, `run.cmd` in this directory also launches the program.

Compile `coursework.c` by itself: all production functions are already included. Repository paths above use `../examples`; the extracted ZIP has its own examples and uses `examples`.

## Recorded validation

Both single-file variants passed strict C17 compilation with `-O2 -Wall -Wextra -Wpedantic -Werror`. Existing contract tests passed 1012 assertion/required-allocation/file-I/O checks with 0 failures, including 21 malformed-index variants. All 28 menu/process checks passed, including restart without the original corpus, failed-load preservation and a synthetic 50-document/100000-occurrence collection.

A static comparison confirmed the five production module bodies and four shared declarations were preserved after consolidating includes and renaming two file-local persistence helpers to avoid name collisions.

These checks establish the tested functional behavior. The prescribed corpus evaluation, final benchmark and remaining course reports/videos are separate deliverables.
