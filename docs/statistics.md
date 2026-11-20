# Task 6: Statistics and Reports

This module reads a built or loaded `SearchIndex` and reports statistics for the collection, each document, and the inverted index. It can also export the latest search results together with these statistics as TXT or CSV. Generating a report does not reopen the original document files.

## Counting Rules

The index module defines a word as a continuous sequence of ASCII letters (`A-Z` or `a-z`) and converts it to lowercase before indexing. Punctuation, digits, and all other characters act as separators. A complete word longer than 1023 bytes is skipped, following the original implementation; it is not split into shorter words. No stop words are removed, so words such as `a` and `the` are included.

All reported word counts refer to words accepted under these rules. The tokenizer does not simply split on spaces or perform linguistic word segmentation for non-ASCII scripts.

| Statistic | Source field | Definition |
| --- | --- | --- |
| Collection document count | `doc_count` | Number of documents in the current collection; a loaded empty document also has a document ID |
| Collection total word count | `total_terms` | Sum of word occurrences across all documents, including repeated occurrences |
| Collection unique word count | `term_count` | Number of distinct words across the entire collection |
| Document total word count | `documents[id].total_terms` | Number of word occurrences in that document |
| Document unique word count | `documents[id].unique_terms` | Number of distinct words within that document |
| Index unique word count | `term_count` | Number of entries in the index vocabulary |
| Posting relationship count | `posting_count` | Number of distinct word-document pairs, with one posting for each word in each document containing it |

The collection and index unique word counts are equal in this project because they use the same tokenization rules and the index does not remove stop words. The collection unique word count cannot be obtained by adding the unique word counts of individual documents: several documents may contain the same word.

For example, suppose document 0 contains `apple apple banana` and document 1 contains `apple orange`. The collection has 2 documents, 5 word occurrences, and 3 unique words. Each document has 2 unique words, but their sum, 4, is not the collection unique word count. There are 4 posting relationships: `apple-0`, `banana-0`, `apple-1`, and `orange-1`. The `apple-0` posting has a frequency of 2, but still represents only one relationship.

The statistics module uses the counters maintained by the index rather than traversing the posting lists again. During index construction and restoration, the index module updates these fields through `index_add_posting`. After changing the document collection, rebuild the index before reporting statistics. When `ready` is 0, the module displays a message instead of potentially outdated counts.

## Interfaces and Usage

`statistics.h` declares two functions:

```c
void statistics_print(const SearchIndex *index, FILE *stream);

int statistics_export(const SearchIndex *index,
                      const SearchResult *results, size_t result_count,
                      const char *query, const char *path, int csv,
                      char *error, size_t error_size);
```

`statistics_print` writes all three categories of statistics to the supplied stream, such as `stdout`. The caller retains ownership of the stream, and the function does not close it. If no index has been built or loaded, it prints a message explaining that an index is required.

`statistics_export` writes TXT when `csv` is 0 and CSV when `csv` is nonzero. It returns 1 only after the report has been written and the file has been closed successfully; it returns 0 on failure. If an `error` buffer and a nonzero `error_size` are supplied, the buffer receives a diagnostic message on failure and is empty on success. The output file is created or an existing file with the same path is overwritten. Its parent directory must already exist.

The following examples assume that `index` is a `SearchIndex` object that the caller has already built or loaded:

```c
char error[256];

statistics_print(&index, stdout);

if (!statistics_export(&index, NULL, 0, NULL,
                       "report.txt", 0, error, sizeof(error))) {
    fprintf(stderr, "%s\n", error);
}

if (!statistics_export(&index, NULL, 0, NULL,
                       "report.csv", 1, error, sizeof(error))) {
    fprintf(stderr, "%s\n", error);
}
```

If the caller has retained the query string `query`, the result array `results`, and its `result_count`, these can be supplied to the export function:

```c
if (!statistics_export(&index, results, result_count, query,
                       "report.csv", 1, error, sizeof(error))) {
    fprintf(stderr, "%s\n", error);
}
```

Passing `NULL, 0, NULL` for the results, result count, and query exports statistics only. Supplying a query string or result array includes a search summary, even when the result count is 0. The module preserves the supplied result order and `score` values; it does not sort the results or execute the query again. Results must belong to the current index. The caller should clear old search results after rebuilding or loading an index.

Report document IDs match the index and start at 0. Search ranks start at 1.

## TXT and CSV Contents

Both formats obtain their counts from the same index fields. TXT presents the information in readable sections. CSV uses these 11 fixed columns:

```text
section,document_count,document_id,filename,total_words,unique_words,posting_relationships,query,result_count,rank,score
```

The `section` field contains `collection`, `document`, or `index`, with optional `search` and `search_result` rows. Fields that do not apply to a row are empty, while applicable counts equal to zero are written as `0`. This distinguishes an inapplicable field from a zero count. An empty collection still has collection and index rows.

Each CSV record ends with CRLF. A field containing a comma, double quote, carriage return, or newline is enclosed in double quotes, and each embedded double quote is written twice. For example, the filename `notes,"draft".txt` is written as `"notes,""draft"".txt"`. A field containing newlines may span several physical lines while remaining part of one CSV record, so the file should be read with a CSV parser.

The output file is opened in binary mode to prevent Windows from changing the explicit CSV line endings. UTF-8 bytes in filenames and queries are preserved without a byte-order mark (BOM) or encoding conversion; the caller should supply UTF-8 strings. The output path is opened through standard C `fopen`, so supported path encodings depend on the runtime environment.

## Error Handling and Implementation Notes

Before exporting, the function checks that the index is ready, the document count is within its limit, the output path is nonempty, and every result references a valid document ID. Failure to open, write, or close the file returns 0, including a failure while flushing buffered output during closing. A write failure may leave a partial report, so the caller must not treat a failure return as a successful export.

Counts use `size_t`, so their `printf` format is `%zu`. A posting's `docId` is an `int` and is printed with `%d`. File operations require return-value checks: a successful `fprintf` call does not guarantee that buffered output has reached the file, so `fclose` must also be checked.

CSV escaping is implemented in one shared helper so that every field follows the same rules. This keeps filenames and queries containing special characters from introducing unintended columns or records.
