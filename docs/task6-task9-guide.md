# Tasks 6 and 9: A Beginner's Guide from the Menu to the Code

Start by connecting the menu options to the tasks you are responsible for:

| Menu option | Function | Task |
| --- | --- | --- |
| 3 | Display collection, per-document, and index statistics | Task 6 |
| 6 | Export `report.txt`, including the most recent displayed search results when available | Task 6 |
| 7 | Export `report.csv`, including the most recent displayed search results when available | Task 6 |
| 8 | Save the current index; the default filename is `index.idx` | Task 9 |
| 9 | Load a saved index; the default filename is `index.idx` | Task 9 |

Menu option 1 loads a document collection and builds its index. Option 4 performs searches, and option 5 displays a document. These options provide the input and checks needed to demonstrate tasks 6 and 9. Option 2 rebuilds the index by processing the current document text again; you do not need it when demonstrating a saved-index reload.

## Recommended Reading Order

1. **Start with `show_menu` and `main` in `coursework.c`.** Find `case 3`, `case 6`, `case 7`, `case 8`, and `case 9` inside the `switch` statement, and identify the function called by each option. You do not need to read the entire search interaction line by line yet.
2. **Read `search_index.h`.** It defines how documents, terms, posting entries, and the complete index are represented in memory. It also declares the index functions shared by the modules.
3. **Read `statistics.h` and `statistics.c`.** First understand the parameters and return values of `statistics_print` and `statistics_export`. Then examine the text output and CSV field escaping.
4. **Read `persistence.h`, then use the [index format documentation](index-format.md) while reading `persistence.c`.** Begin with `index_save` to understand what is written. Then read how `index_load` restores it.
5. **Finally, read `index_add_posting`, `index_build`, and `index_free` in `index.c`.** These explain where the counters come from, how loading restores records, and how memory is released. Document input is handled by `collection.h` and `collection.c`.

The complete compilation commands and startup instructions are in the project's [README](../README.md). Compile and link the production `.c` files together. Header files provide declarations; do not include a `.c` file with `#include` or try to build only one added module.

## Understand the Four Main Data Types

### `Document`: One Document

A `Document` stores its filename, a `content` pointer to the original text, its total term count, and its unique term count. `documents[0]` is the document with ID 0. Document IDs start at 0; search-result ranks start at 1.

`filename` is a fixed-size array inside the structure. `content` points to separately allocated text storage. Storing a pointer in a structure does not store all the text characters inside that structure.

### `Posting`: One Term-Document Relationship

A `Posting` contains:

- `docId`: the document containing this term.
- `frequency`: how many times the term occurs in that document.
- `next`: the address of the next posting entry, or `NULL` at the end.

If `apple` appears twice in document 0, there is one entry with `docId = 0` and `frequency = 2`. The two occurrences form one term-document relationship.

### `Term`: One Unique Term

A `Term` stores its lowercase `word`. Its `postings` pointer leads to that word's posting list. A `Term` also has a `next` pointer connecting different terms that fall into the same hash bucket. The chain of terms and the posting list for one term serve different purposes.

To traverse a linked list, start at its first node, process the current node, and follow `next` until the pointer is `NULL`:

```c
for (Posting *p = term->postings; p != NULL; p = p->next) {
    /* Use p->docId and p->frequency here. */
}
```

`p->frequency` accesses the `frequency` member of the structure pointed to by `p`. `index->doc_count` works in the same way. When you have the structure variable itself rather than a pointer, use the dot operator, as in `index.doc_count`.

### `SearchIndex`: The Complete Current State

`SearchIndex` holds the document collection, hash buckets, and accumulated counters. Task 6 reads `doc_count`, `total_terms`, `term_count`, `posting_count`, and the per-document counters. Task 9 saves and restores the documents and posting entries. The `ready` flag indicates whether the index is usable. If it is not ready, the menu asks you to build or load an index first.

These shared structures and interfaces were extracted from the original search function. An index available only inside that function could not be accessed by the statistics and persistence modules or restored for a later run. The shared interface allows tasks 6 and 9 to use the same index as searching.

When integrating the change into the group repository, include all production source files: `coursework.c`, `collection.c`, `index.c`, `statistics.c`, and `persistence.c`, together with `collection.h`, `search_index.h`, `statistics.h`, and `persistence.h`. Copying only `statistics.c` and `persistence.c` would leave out shared types, function implementations, or menu entry points. Review the related documentation, build configuration, and tests with the code.

## Work Out the Statistics with Two Documents

Suppose the documents contain:

```text
Document 0: Apple apple banana
Document 1: apple orange
```

The tokenizer accepts consecutive ASCII letters, converts them to lowercase, and treats punctuation and other non-letter characters as separators. A complete word longer than 1023 bytes is skipped rather than split into several shorter words. No stopword removal is performed. In this guide, a word means a normalized searchable term.

| Statistic | Document 0 | Document 1 | Whole collection |
| --- | --- | --- | --- |
| Number of documents | N/A | N/A | 2 |
| Total term occurrences, including repetitions | 3 | 2 | 5 |
| Number of unique terms | 2 | 2 | 3 |

The collection's unique terms are `apple`, `banana`, and `orange`. Both documents contain `apple`, so adding their individual unique counts gives 4 rather than the correct collection-wide count of 3.

The posting entries can be represented as:

```text
apple  -> (document 0, frequency 2) -> (document 1, frequency 1)
banana -> (document 0, frequency 1)
orange -> (document 1, frequency 1)
```

Therefore, `term_count = 3`, `posting_count = 4`, and `total_terms = 5`. The relationship count is the number of parenthesized entries. The total term count is the sum of their frequencies: `2 + 1 + 1 + 1 = 5`.

`index_add_posting` maintains these counters consistently. An existing term-document combination increases its frequency. A new combination increases the posting count and the document's unique term count. The first occurrence of a new term increases the index's unique term count. Task 6 can read these values without opening the documents again.

## How the C Concepts Are Used

| Concept or function | Purpose in this project |
| --- | --- |
| `struct` / `typedef` | Group the information for a document or record and give the type a convenient name |
| `size_t` | Store counts, lengths, and array capacities; print these values with `%zu` |
| `int` and `%d` | Store and print document IDs |
| `unsigned` and `%u` | Store and print term frequencies in posting entries |
| `malloc` / `free` | Allocate and release text, terms, posting nodes, and other dynamic storage |
| `FILE *` | Refer to a file stream provided by the standard C library |
| `fopen(path, "wb")` | Open an output file; return `NULL` on failure, otherwise create or overwrite the file |
| `fopen(path, "rb")` | Open an existing index file for reading |
| `fprintf` | Write statistics or record headers as formatted text |
| `fwrite` / `fread` | Write or read the exact byte count of filenames, document text, words, and other data blocks |
| `fclose` | Close the stream and flush buffered output; its return value must also be checked |

For example, `fprintf(file, "%zu", index->total_terms)` writes the total term count. `%zu` matches `size_t`; `%ld` matches `long`, which is not guaranteed to be the same type.

Index records and integers use a textual format. The file is opened with `wb` or `rb` to preserve exact byte counts and newline bytes. Binary file mode does not mean that structure memory, including pointer addresses, is written directly to disk.

## Calling Task 6

The main call for menu option 3 is:

```c
statistics_print(index, stdout);
```

`stdout` is the standard output stream, so the statistics appear in the terminal. `statistics_print` reads the counters without changing the index or closing the stream supplied by its caller.

Menu options 6 and 7 use:

```c
statistics_export(index, results, result_count, query,
                  path, csv, error, sizeof error);
```

Set `csv` to 0 for text output or a nonzero value for CSV. The function returns 1 on success or 0 on failure, with a diagnostic stored in the caller's `error` array. For statistics-only output, pass `NULL, 0, NULL` in place of `results, result_count, query`. When a recent search is available, the menu also supplies its saved query string and results.

The menu retains the Top K results that were displayed for that search. The export function preserves their order and scores. Text and CSV reports use the same counters. CSV fields containing commas, double quotes, or newlines must be escaped so that filenames and queries remain in the correct columns.

## What Task 9 Saves and How It Restores the Index

Menu option 8 calls `index_save(index, path, error, sizeof error)`. The saved file contains:

1. A file identifier and format version.
2. The four collection/index counters.
3. Each document's ID, filename, original text, and statistics.
4. Each term and all of its document-ID/frequency posting entries.
5. An end marker.

Byte lengths are recorded before filenames and document text. This allows spaces and newlines in the data to be read accurately. See [index-format.md](index-format.md) for the full specification. The default extension is `.idx`; the extension does not change the file format.

A memory pointer identifies an address within the current program run. After the process exits, that allocation is no longer part of the process, and a later run may allocate nodes at different addresses. The file therefore stores actual words, document IDs, frequencies, and text. Loading allocates new nodes and connects their `next` pointers. Hash bucket locations are recalculated from the terms, and runtime query markers are reset.

Menu option 9 calls `index_load(index, path, error, sizeof error)`. It first creates a temporary index, restores the document text and nodes from the records, and uses `index_add_posting` to restore the posting relationships and counters. It then verifies the counters and the end of the file. Only a fully successful load replaces the current index. A failed load releases the temporary data and preserves the current index.

`index_build` scans and tokenizes document text. `index_load` already has the saved terms and posting entries, so it restores their nodes without calling `index_build` or opening the original document files. Because the format also saves the original text, menu option 5 can still display a document after loading.

## Who Owns the Memory?

Memory ownership means responsibility for eventually releasing an allocation.

| Object or operation | Allocation and release responsibilities |
| --- | --- |
| The `SearchIndex` object itself | `main` allocates it; on exit, call `index_free(index)` and then `free(index)` |
| Document text, terms, and posting nodes | The collection or index module allocates them; the current `SearchIndex` owns them, and `index_free` releases them |
| The most recent search-result array | `main` allocates it separately and frees it separately on exit |
| Printing or exporting statistics | Borrows the index, results, and query without taking ownership; the export function opens and closes its own output file |
| Saving the index | Borrows the current index without modifying or freeing it; opens and closes its own file |
| A successful index load | Transfers document text and nodes from the temporary index to the destination; frees only the temporary outer object, not the transferred data |
| A failed index load | Releases the temporary text and nodes; the destination continues to own its original data |

`index_free` releases the object's internal allocations and resets its state. It does not free the outer `SearchIndex` object allocated by the caller. That is why shutdown requires both `index_free(index)` and `free(index)`.

The destination passed to `index_load` must first be initialized with `index_init`, so its pointers can be checked and released safely. After a successful reload or rebuild, the menu clears the previous search results to prevent old document IDs from being interpreted against a different index.

## Document Input and Standard C

The original `dirent` folder-reading interface is retained as an optional platform adapter. It is compiled only when `ENABLE_DIRECTORY_INPUT` is enabled and the build environment provides the required header and interfaces.

Without that option, use standard-C collection-list input: a plain text file containing one document path per line. Relative paths are resolved against the list file's directory. Blank lines and lines beginning with `#` are skipped. The list configures which documents are loaded; it is not a previously saved index.

Collection-list input lets you demonstrate tasks 6 and 9 without installing an additional directory-reading interface. If the group uses folder input, select the corresponding build command in the README.

## Demonstrate the Features Yourself

The following steps are a demonstration you can perform, not a record of actions you have already completed.

1. Create a `demo` folder in the project root. Create `doc0.txt` and `doc1.txt` inside it, containing `Apple apple banana` and `apple orange`, respectively.
2. Create `demo/collection.txt` with two lines: `doc0.txt` and `doc1.txt`. Compile according to the README and start the program from the project root.
3. Select menu option 1 and enter `demo/collection.txt`. After a successful load, select option 3. Check for 2 documents, 5 term occurrences, 3 unique terms, and 4 posting relationships.
4. Select option 4, search for `apple`, and enter 2 for Top K. Document 0 should have score 2, and document 1 should have score 1. Enter `/exit` to return to the main menu.
5. Select options 6 and 7. Press Enter at each output-path prompt to use the defaults. Open `report.txt` and `report.csv` in the project root and check the statistics and results. You can inspect CSV with a text editor or a CSV reader.
6. Select option 8 and press Enter to save the current index as `index.idx` in the project root. Then select option 0 to exit the entire program.
7. After the program exits, temporarily rename the test-created `demo` folder to `demo-hidden`, leaving `index.idx` in the project root. Restart the program, go directly to option 9, and press Enter to load `index.idx`. Do not choose option 1 or 2 during this reload demonstration.
8. Check option 3 again, repeat the `apple` search, and use option 5 to display document 0. The statistics, search results, and original text should remain available. This demonstrates that the saved data supports these operations even when the original input paths are unavailable.
9. Restore the folder's name after the demonstration. To export search results again, first run a new query: loading an index clears the previous process's or previous index's recent search state.

You can also demonstrate failure handling by loading a nonexistent file or a separately prepared incomplete copy of an index file. Check the error message and confirm that the current valid index can still be searched. Keep the correctly saved index intact.

## Outline for a 3-5 Minute Individual Explanation Video

Adapt this outline to your own understanding and the actions you actually demonstrate. Identify your modules and explain the observed behavior. Do not describe an unperformed operation as personal experience or claim that preparing an outline means the video or complete coursework has already been recorded or submitted.

| Suggested time | What to show | Points to explain |
| --- | --- | --- |
| 0:00-0:30 | Menu and file list | "My assigned areas are statistics and reporting in task 6, and saving and loading the index in task 9. Options 3, 6, and 7 cover reporting; options 8 and 9 cover persistence." |
| 0:30-1:15 | Two sample documents and the statistics screen | Use the `apple` example to explain 5 total occurrences, 3 unique terms, and 4 relationships; explain why per-document unique counts cannot simply be added |
| 1:15-2:00 | One search and the text/CSV reports | Show the query and exported results; explain how CSV handles commas, quotes, and newlines in filenames or queries |
| 2:00-3:00 | Saving, exiting, restarting, and loading | Show the process exit clearly; rename the test input folder, load the index directly, then search and display a document without rebuilding |
| 3:00-4:00 | Key functions in the two modules | Explain shared counters, saving actual records, temporary-index loading, preservation of the old index on failure, and why pointer addresses cannot be reused across runs |
| 4:00-4:30 | A simple error case and closing explanation | Based on the actual demonstration, explain a file-open failure and how files and memory are closed or released |

You do not need to explain every helper function within a few minutes. Prioritize the input data, the resulting output, restoration after process exit, and failure handling. Use a small amount of code to support those explanations.
