# Task 9: Saving and Reloading the Index

`persistence.c` provides two functions implemented with standard C file operations: `fopen`, `fprintf`, `fwrite`, `fgets`, `fread`, `fgetc` and `fclose`.

```c
int index_save(const SearchIndex *index, const char *path,
               char *error, size_t error_size);
int index_load(SearchIndex *destination, const char *path,
               char *error, size_t error_size);
```

Both functions return `1` on success and `0` on failure. When an error buffer is supplied, the caller can display its diagnostic message. Before calling `index_load`, initialize `destination` with `index_init`.

## Purpose and Stored Data

The inverted index maps each term to document IDs and term frequencies. Saving these relationships allows the next program session to restore the index and search immediately, without tokenizing every document and constructing the index again.

The format also stores document names, original document content and document statistics. After loading, the program can therefore search, preview documents and export statistics even when the original input files have been moved or are unavailable.

The file contains actual strings, document IDs and frequencies. It does not contain linked-list `next` pointers, memory addresses, hash-bucket locations or session state such as `lastQuery`. Loading allocates new nodes and calculates each term's hash-bucket location again.

## File Format: Version 1

The menu's default filename is `index.idx`; callers may choose another name, such as `index.txt`. This is a text format with explicit byte lengths. Record fields and integers are readable text, while filenames and document content are stored as their original bytes.

Files are opened in `wb` and `rb` mode. This prevents automatic newline conversion on Windows, which would otherwise change the number of bytes in the data blocks.

Each record line uses one ASCII space between fields and ends with one LF byte (`\n`). Integers are unsigned decimal values without a leading sign. In the table below, `<...>` denotes a field value; the angle brackets are not written to the file.

| Part | Exact format |
|---|---|
| Format identifier and version | `COMP1028_INDEX 1\n` |
| Collection counters | `COUNTS <document_count> <total_terms> <unique_terms> <posting_count>\n` |
| Document descriptor | `D <document_id> <filename_bytes> <content_bytes> <document_total_terms> <document_unique_terms>\n` |
| Filename block | Exactly `filename_bytes` bytes, followed by one `\n` |
| Document content block | Exactly `content_bytes` bytes, followed by one `\n` |
| Term descriptor | `T <term_bytes> <term_posting_count>\n` |
| Term block | Exactly `term_bytes` bytes of a lowercase ASCII term, followed by one `\n` |
| Posting relationship | `P <document_id> <frequency>\n` |
| End marker | `END\n`, followed immediately by end of file |

Document records appear in ID order, from `0` to `document_count - 1`. They are followed by the term records and each term's complete posting list. Terms are written in hash-bucket traversal order and need not be alphabetically sorted. Loading preserves the stored document IDs rather than assigning new IDs based on filenames.

A posting represents one distinct term-document relationship. Multiple occurrences of that term in the same document increase the posting's frequency; they do not create additional relationships.

Byte lengths are calculated with C's `strlen`, rather than by counting displayed characters. A non-ASCII character in a UTF-8 filename may occupy several bytes. Length-prefixed blocks allow filenames to contain spaces, commas, quotation marks and newlines, and allow document content to span multiple lines. Embedded `\0` bytes are rejected because the program stores these blocks as C strings. Newlines belonging to a data block count toward its length; a separate LF byte terminates the block afterward.

### Complete Loadable Example

The documents contain `Apple apple banana` and `apple orange`. After case normalization, they contain five term occurrences, three unique terms and four distinct term-document relationships:

```text
COMP1028_INDEX 1
COUNTS 2 5 3 4
D 0 9 18 3 2
doc 1.txt
Apple apple banana
D 1 8 12 2 2
doc2.txt
apple orange
T 5 2
apple
P 0 2
P 1 1
T 6 1
banana
P 0 1
T 6 1
orange
P 1 1
END
```

The file must retain the newline after `END`, with no additional data after it. An initialized and built empty index is represented by:

```text
COMP1028_INDEX 1
COUNTS 0 0 0 0
END
```

## Loading and Validation

1. Open the file and check its format identifier, version and header counters.
2. Restore document names and content into a temporary `SearchIndex`.
3. Restore each term and posting through `index_add_posting`. That function recalculates the collection and document counters as it inserts the relationships. Loading does not call `index_build` or open the original document files.
4. Compare the reconstructed counters with the stored counters, then check `END` and end of file.
5. Only after every step, including closing the file, succeeds, release the caller's previous index and transfer ownership of the temporary index to the caller.

On failure, temporary documents and nodes are released, and the previous destination index remains usable. Validation covers:

- Unsupported versions, incorrect record labels, missing or extra fields, signed numbers, integer overflow and excessively long record lines.
- More than 6000 documents; empty filenames or filenames longer than 299 bytes; content longer than 199999 bytes per document.
- Empty terms, terms longer than 1023 bytes and term characters outside lowercase ASCII letters.
- Out-of-range document IDs, document descriptors that are not in consecutive ID order, duplicate term records and duplicate document IDs within a term's posting list.
- Frequencies equal to zero or greater than `UINT_MAX`; terms without postings; counters that disagree with the stored records.
- Truncated files, incorrect byte lengths, embedded `\0`, missing end markers and extra data after the end marker.
- File-opening, reading, closing and memory-allocation failures.

A document's total term count must not exceed its content length in bytes, and its unique term count must not exceed its total term count. Each posting is also checked against the remaining declared term count and unique-term allowance for its document before insertion.

The loader validates the file's structure and counters without tokenizing its content again. It therefore preserves the intended ability to reload an index without rebuilding it.

## Saving Behavior

A save request requires a built index. Before opening the output file, the implementation checks the in-memory documents and posting lists to confirm that counters agree and that terms and document IDs are valid. It then writes every record and checks both write operations and file closure.

Opening in `wb` mode replaces an existing file at the specified path. If a disk or file-system error interrupts writing, the function returns failure and the output may contain only part of the index. Such an incomplete file is rejected by the loader. A failed save must not be treated as a successful backup.

## Integration Example

```c
char error[256];

if (!index_save(active_index, "index.idx", error, sizeof error))
    printf("Save failed: %s\n", error);

/* active_index was initialized with index_init. A successful load may
   replace an index that already contains data. */
if (!index_load(active_index, "index.idx", error, sizeof error))
    printf("Load failed: %s\n", error);
```

## Time and Memory Considerations

Saving traverses the documents, hash buckets, terms and posting relationships, and writes filenames and original content. Loading reads the same data and restores nodes through the index insertion interface. Its cost generally grows with the amount of content, the number of terms and the number of postings. Hash collisions and posting-list searches also affect the worst-case running time, so the implementation should not be described as strictly linear without qualification.

During loading, the old index and the temporary new index coexist. This uses additional memory to ensure that a failed load cannot destroy the old index. Statistical counters are reconstructed from the postings, while repeated-query session markers start at zero.

## C Concepts to Explain in a Demonstration

- `FILE *` represents an open file; a `NULL` result from `fopen` must be handled.
- `fprintf` writes record lines, while `fwrite` and `fread` transfer the exact number of bytes specified for each string block.
- `malloc` allocates document content and nodes. Failure paths and final cleanup release owned memory through `index_free`.
- A pointer stores an address in the current process and cannot serve as the persisted contents of a linked list.
- Lengths and counters of type `size_t` use `%zu`; term frequencies of type `unsigned` use `%u`.

Actual test results belong in the verification records. This document describes the file format and implementation; it does not present unexecuted test cases as successful tests.
