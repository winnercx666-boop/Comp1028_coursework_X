/*
 * COMP1028 Task 6: Statistics and Reporting
 * Complete standalone C17 program; no project header files are required.
 *
 * Standard-C build, using a collection list with one document path per line:
 *   gcc -std=c17 -O2 -Wall -Wextra -Wpedantic statistics_standalone.c -o statistics_standalone.exe
 * Optional folder input, when the compiler supplies dirent.h:
 *   gcc -DENABLE_DIRECTORY_INPUT -std=c17 -O2 statistics_standalone.c -o statistics_standalone.exe
 *
 * Load and build an index, display statistics, search, preview documents,
 * and export statistics with optional latest search results as TXT or CSV.
 * Relative document paths are resolved from the collection list's directory.
 * Shared loading and indexing support keeps the reporting example runnable.
 */

/*
 * MIT License
 *
 * Copyright (c) 2026 Seaaaaa
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef ENABLE_DIRECTORY_INPUT
#include <dirent.h>
#endif

/* ================= SHARED TYPES AND DECLARATIONS ================= */

#define MAX_DOCUMENTS 6000
#define MAX_FILENAME 300
#define MAX_DOCUMENT_BYTES 199999
#define MAX_TERM_BYTES 1023
#define MAX_QUERY_BYTES 1023
#define INDEX_BUCKETS 65537

typedef struct text {
    char filename[MAX_FILENAME];
    char *content;
    size_t total_terms;
    size_t unique_terms;
} Document;

typedef struct Posting {
    int docId;                  /* Zero-based document ID. */
    unsigned frequency;
    struct Posting *next;
} Posting;

typedef struct Term {
    Posting *postings;
    struct Term *next;
    unsigned long long lastQuery;
    char word[];
} Term;

typedef struct SearchIndex {
    Document documents[MAX_DOCUMENTS];
    size_t doc_count;
    size_t total_terms;
    size_t term_count;
    size_t posting_count;
    Term *buckets[INDEX_BUCKETS];
    unsigned long long query_number;
    int ready;
} SearchIndex;

typedef struct SearchResult {
    int docId;
    size_t score;
} SearchResult;

void index_init(SearchIndex *index);
void index_clear_terms(SearchIndex *index);
void index_free(SearchIndex *index);
unsigned long index_hash_word(const char *word);
Term *index_find_term(const SearchIndex *index, const char *word);
/* Inserts a normalized word/document/frequency and updates all counters.
   Returns 1 on success, 0 on invalid data, allocation failure or overflow. */
int index_add_posting(SearchIndex *index, const char *word,
                      int doc_id, unsigned frequency);
int index_build(SearchIndex *index);
/* Results are sorted by score descending, then document ID ascending.
   Repeated query words contribute only once. No original documents are read. */
size_t index_search(SearchIndex *index, const char *query,
                    SearchResult *results, size_t capacity);

/* Accepts a collection list, or a folder when ENABLE_DIRECTORY_INPUT is set.
   Failure leaves the destination unchanged. Index building is a separate step. */
int collection_load(SearchIndex *destination, const char *path,
                     char *error, size_t error_size);

/* Prints collection, per-document and index statistics to stream.
   The caller owns stream. An unready index produces a friendly message. */
void statistics_print(const SearchIndex *index, FILE *stream);

/* Exports statistics and optional search results. csv != 0 selects CSV;
   csv == 0 selects text. Pass NULL, 0, NULL for statistics only.
   Document IDs remain zero-based; result ranks start at one.
   Returns 1 on success, 0 on failure and sets error when space is provided. */
int statistics_export(const SearchIndex *index,
                      const SearchResult *results, size_t result_count,
                      const char *query, const char *path, int csv,
                      char *error, size_t error_size);

/* ================= SHARED DOCUMENT INPUT ================= */

/* Existing group folder input is an optional platform adapter.
   The collection-list path uses only the standard C library. */

#define MAX_PATH_BYTES 4095

static int fail(char *error, size_t size, const char *message)
{
    if (error && size)
        snprintf(error, size, "%s", message);
    return 0;
}

static int add_document(SearchIndex *index, const char *path,
                         const char *filename)
{
    if (index->doc_count >= MAX_DOCUMENTS || !*filename ||
        strlen(filename) >= MAX_FILENAME) {
        fprintf(stderr, "Skipping '%s': document or filename limit exceeded.\n", path);
        return 0;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Skipping '%s': cannot open file.\n", path);
        return 0;
    }
    char *content = malloc(MAX_DOCUMENT_BYTES + 2);
    if (!content) {
        fclose(file);
        return -1;
    }
    size_t bytes = fread(content, 1, MAX_DOCUMENT_BYTES + 1, file);
    int bad = ferror(file) || bytes > MAX_DOCUMENT_BYTES ||
              memchr(content, '\0', bytes) != NULL;
    if (fclose(file) != 0)
        bad = 1;
    if (bad) {
        fprintf(stderr, "Skipping '%s': unreadable, binary or over %d bytes.\n",
                path, MAX_DOCUMENT_BYTES);
        free(content);
        return 0;
    }
    content[bytes] = '\0';
    char *compact = realloc(content, bytes + 1);
    if (compact)
        content = compact;
    Document *doc = &index->documents[index->doc_count++];
    strcpy(doc->filename, filename);
    doc->content = content;
    return 1;
}

static const char *basename_of(const char *path)
{
    const char *name = path;
    for (const char *p = path; *p; ++p)
        if (*p == '/' || *p == '\\')
            name = p + 1;
    return name;
}

#ifdef ENABLE_DIRECTORY_INPUT
static int text_filename(const char *name)
{
    if (!strcmp(name, ".") || !strcmp(name, ".."))
        return 0;
    const char *ext = strrchr(name, '.');
    return !ext || (ext[0] == '.' && (ext[1] == 't' || ext[1] == 'T') &&
                   (ext[2] == 'x' || ext[2] == 'X') &&
                   (ext[3] == 't' || ext[3] == 'T') && ext[4] == '\0');
}

static int compare_documents(const void *a, const void *b)
{
    return strcmp(((const Document *)a)->filename, ((const Document *)b)->filename);
}
#endif

int collection_load(SearchIndex *destination, const char *path,
                     char *error, size_t error_size)
{
    if (!destination || !path || !*path)
        return fail(error, error_size, "Collection path cannot be empty.");
    SearchIndex *temp = malloc(sizeof *temp);
    if (!temp)
        return fail(error, error_size, "Could not allocate the document collection.");
    index_init(temp);
    int fatal = 0;
    int folder_loaded = 0;

#ifdef ENABLE_DIRECTORY_INPUT
    DIR *dir = opendir(path);
    if (dir) {
        folder_loaded = 1;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!text_filename(entry->d_name))
                continue;
            char full[MAX_PATH_BYTES + 1];
            int length = snprintf(full, sizeof full, "%s/%s", path, entry->d_name);
            if (length < 0 || (size_t)length >= sizeof full) {
                fprintf(stderr, "Skipping '%s': path is too long.\n", entry->d_name);
                continue;
            }
            if (add_document(temp, full, entry->d_name) < 0) {
                fatal = 1;
                break;
            }
        }
        if (closedir(dir) != 0)
            fatal = 1;
        qsort(temp->documents, temp->doc_count, sizeof temp->documents[0],
              compare_documents);
    }
#endif

    if (!folder_loaded) {
        FILE *list = fopen(path, "r");
        if (!list) {
            index_free(temp);
            free(temp);
            return fail(error, error_size, "Cannot open the folder or collection list.");
        }
        char parent[MAX_PATH_BYTES + 1];
        if (strlen(path) > MAX_PATH_BYTES) {
            fclose(list);
            index_free(temp);
            free(temp);
            return fail(error, error_size, "Collection path is too long.");
        }
        strcpy(parent, path);
        size_t parent_length = (size_t)(basename_of(parent) - parent);
        parent[parent_length] = '\0';
        char line[MAX_PATH_BYTES + 2];
        while (fgets(line, sizeof line, list)) {
            if (!strchr(line, '\n') && !feof(list)) {
                int ch = fgetc(list);
                if (ch != '\n' && ch != EOF) {
                    while ((ch = fgetc(list)) != '\n' && ch != EOF)
                        ;
                    fprintf(stderr, "Skipping collection-list entry: path is too long.\n");
                    continue;
                }
            }
            line[strcspn(line, "\r\n")] = '\0';
            if (!*line || *line == '#')
                continue;
            char full[MAX_PATH_BYTES + 1];
            int absolute = line[0] == '/' || line[0] == '\\' ||
                           (line[0] && line[1] == ':');
            int length = snprintf(full, sizeof full, "%s%s", absolute ? "" : parent, line);
            if (length < 0 || (size_t)length >= sizeof full) {
                fprintf(stderr, "Skipping collection-list entry: path is too long.\n");
                continue;
            }
            if (add_document(temp, full, basename_of(line)) < 0) {
                fatal = 1;
                break;
            }
        }
        if (ferror(list))
            fatal = 1;
        if (fclose(list) != 0)
            fatal = 1;
    }
    if (fatal || !temp->doc_count) {
        index_free(temp);
        free(temp);
        return fail(error, error_size, fatal ? "Could not read or allocate the collection." :
                    "No readable text documents found. Empty documents are allowed.");
    }
    index_free(destination);
    *destination = *temp;
    free(temp);
    if (error && error_size)
        error[0] = '\0';
    return 1;
}

/* ================= SHARED TOKENIZATION AND INDEXING ================= */

static int letter(unsigned char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

/* Same rule as the original engine: letters only, lowercase, skip long words. */
static int next_word(const char **cursor, char word[MAX_TERM_BYTES + 1])
{
    while (**cursor) {
        size_t length = 0;
        int too_long = 0;
        while (**cursor && !letter((unsigned char)**cursor))
            ++*cursor;
        while (letter((unsigned char)**cursor)) {
            unsigned char ch = (unsigned char)*(*cursor)++;
            if (length < MAX_TERM_BYTES) {
                if (ch >= 'A' && ch <= 'Z')
                    ch = (unsigned char)(ch - 'A' + 'a');
                word[length++] = (char)ch;
            } else {
                too_long = 1;
            }
        }
        if (length && !too_long) {
            word[length] = '\0';
            return 1;
        }
    }
    return 0;
}

void index_init(SearchIndex *index)
{
    if (index)
        memset(index, 0, sizeof *index);
}

void index_clear_terms(SearchIndex *index)
{
    if (!index)
        return;
    for (size_t i = 0; i < INDEX_BUCKETS; ++i) {
        Term *term = index->buckets[i];
        while (term) {
            Term *next_term = term->next;
            Posting *posting = term->postings;
            while (posting) {
                Posting *next_posting = posting->next;
                free(posting);
                posting = next_posting;
            }
            free(term);
            term = next_term;
        }
        index->buckets[i] = NULL;
    }
    for (size_t i = 0; i < MAX_DOCUMENTS; ++i) {
        index->documents[i].total_terms = 0;
        index->documents[i].unique_terms = 0;
    }
    index->total_terms = 0;
    index->term_count = 0;
    index->posting_count = 0;
    index->query_number = 0;
    index->ready = 0;
}

void index_free(SearchIndex *index)
{
    if (!index)
        return;
    index_clear_terms(index);
    for (size_t i = 0; i < MAX_DOCUMENTS; ++i)
        free(index->documents[i].content);
    index_init(index);
}

unsigned long index_hash_word(const char *word)
{
    unsigned long hash = 0;
    while (*word)
        hash = hash * 31 + (unsigned char)*word++;
    return hash;
}

Term *index_find_term(const SearchIndex *index, const char *word)
{
    if (!index || !word)
        return NULL;
    Term *term = index->buckets[index_hash_word(word) % INDEX_BUCKETS];
    while (term && strcmp(term->word, word) != 0)
        term = term->next;
    return term;
}

int index_add_posting(SearchIndex *index, const char *word,
                      int doc_id, unsigned frequency)
{
    if (!index || !word || !*word || !frequency || doc_id < 0 ||
        index->doc_count > MAX_DOCUMENTS || (size_t)doc_id >= index->doc_count)
        return 0;
    size_t length = strlen(word);
    if (length > MAX_TERM_BYTES)
        return 0;
    for (size_t i = 0; i < length; ++i)
        if (word[i] < 'a' || word[i] > 'z')
            return 0;

    Document *doc = &index->documents[doc_id];
    if (index->total_terms > SIZE_MAX - frequency ||
        doc->total_terms > SIZE_MAX - frequency)
        return 0;
    Term *term = index_find_term(index, word);
    Posting *posting = term ? term->postings : NULL;
    while (posting && posting->docId != doc_id)
        posting = posting->next;
    if (posting) {
        if (posting->frequency > UINT_MAX - frequency)
            return 0;
        posting->frequency += frequency;
    } else {
        if (index->posting_count == SIZE_MAX || doc->unique_terms == SIZE_MAX ||
            (!term && index->term_count == SIZE_MAX))
            return 0;
        Posting *entry = malloc(sizeof *entry);
        if (!entry)
            return 0;
        if (!term) {
            term = malloc(sizeof *term + length + 1);
            if (!term) {
                free(entry);
                return 0;
            }
            strcpy(term->word, word);
            term->postings = NULL;
            term->lastQuery = 0;
            size_t bucket = index_hash_word(word) % INDEX_BUCKETS;
            term->next = index->buckets[bucket];
            index->buckets[bucket] = term;
            ++index->term_count;
        }
        *entry = (Posting){doc_id, frequency, term->postings};
        term->postings = entry;
        ++index->posting_count;
        ++doc->unique_terms;
    }
    index->total_terms += frequency;
    doc->total_terms += frequency;
    return 1;
}

int index_build(SearchIndex *index)
{
    if (!index || index->doc_count > MAX_DOCUMENTS)
        return 0;
    index_clear_terms(index);
    for (size_t id = 0; id < index->doc_count; ++id) {
        const char *cursor = index->documents[id].content;
        char word[MAX_TERM_BYTES + 1];
        if (!cursor)
            goto failure;
        while (next_word(&cursor, word))
            if (!index_add_posting(index, word, (int)id, 1))
                goto failure;
    }
    index->ready = 1;
    return 1;
failure:
    index_clear_terms(index);
    return 0;
}

static int compare_results(const void *left, const void *right)
{
    const SearchResult *a = left;
    const SearchResult *b = right;
    if (a->score != b->score)
        return a->score > b->score ? -1 : 1;
    return (a->docId > b->docId) - (a->docId < b->docId);
}

size_t index_search(SearchIndex *index, const char *query,
                    SearchResult *results, size_t capacity)
{
    size_t scores[MAX_DOCUMENTS] = {0};
    SearchResult ranked[MAX_DOCUMENTS];
    size_t count = 0;
    if (!index || !index->ready || index->doc_count > MAX_DOCUMENTS ||
        !query || !results || !capacity)
        return 0;
    if (++index->query_number == 0) {
        for (size_t i = 0; i < INDEX_BUCKETS; ++i)
            for (Term *t = index->buckets[i]; t; t = t->next)
                t->lastQuery = 0;
        index->query_number = 1;
    }
    char word[MAX_TERM_BYTES + 1];
    const char *cursor = query;
    while (next_word(&cursor, word)) {
        Term *term = index_find_term(index, word);
        if (!term || term->lastQuery == index->query_number)
            continue;
        term->lastQuery = index->query_number;
        for (Posting *p = term->postings; p; p = p->next)
            scores[p->docId] += p->frequency;
    }
    for (size_t id = 0; id < index->doc_count; ++id)
        if (scores[id])
            ranked[count++] = (SearchResult){(int)id, scores[id]};
    qsort(ranked, count, sizeof ranked[0], compare_results);
    if (count > capacity)
        count = capacity;
    memcpy(results, ranked, count * sizeof ranked[0]);
    return count;
}

/* ================= TASK 6: STATISTICS AND REPORTING ================= */

/* Three decimal characters per byte cover any size_t value. */
#define NUMBER_BUFFER_SIZE (3 * sizeof(size_t) + 1)

enum CsvColumn {
    COL_SECTION,
    COL_DOCUMENT_COUNT,
    COL_DOCUMENT_ID,
    COL_FILENAME,
    COL_TOTAL_WORDS,
    COL_UNIQUE_WORDS,
    COL_POSTING_RELATIONSHIPS,
    COL_QUERY,
    COL_RESULT_COUNT,
    COL_RANK,
    COL_SCORE,
    COL_COUNT
};

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
}

static void set_io_error(char *error, size_t error_size,
                         const char *message, int error_code)
{
    if (error == NULL || error_size == 0) {
        return;
    }
    if (error_code != 0) {
        snprintf(error, error_size, "%s: %s", message, strerror(error_code));
    } else {
        snprintf(error, error_size, "%s", message);
    }
}

static int write_text_statistics(const SearchIndex *index, FILE *stream)
{
    size_t i;

    if (fprintf(stream,
                "Search Index Report\n"
                "===================\n\n"
                "Collection statistics\n"
                "Documents: %zu\n"
                "Total words (occurrences): %zu\n"
                "Unique words across all documents: %zu\n\n"
                "Per-document statistics\n",
                index->doc_count, index->total_terms, index->term_count) < 0) {
        return 0;
    }

    if (index->doc_count == 0 && fputs("No documents.\n", stream) == EOF) {
        return 0;
    }
    for (i = 0; i < index->doc_count; ++i) {
        const Document *document = &index->documents[i];
        if (fprintf(stream,
                    "Document ID: %zu\n"
                    "File: %s\n"
                    "Total words (occurrences): %zu\n"
                    "Unique words: %zu\n\n",
                    i, document->filename, document->total_terms,
                    document->unique_terms) < 0) {
            return 0;
        }
    }

    return fprintf(stream,
                   "\nIndex statistics\n"
                   "Unique indexed words: %zu\n"
                   "Word-document relationships: %zu\n",
                   index->term_count, index->posting_count) >= 0;
}

static int write_text_results(const SearchIndex *index,
                              const SearchResult *results,
                              size_t result_count, const char *query,
                              FILE *stream)
{
    size_t i;

    if (fprintf(stream,
                "\nSearch results\n"
                "Query: %s\n"
                "Result count: %zu\n",
                query != NULL ? query : "", result_count) < 0) {
        return 0;
    }
    if (result_count == 0 && fputs("No matching documents.\n", stream) == EOF) {
        return 0;
    }
    for (i = 0; i < result_count; ++i) {
        const SearchResult *result = &results[i];
        if (fprintf(stream,
                    "Rank: %zu | Document ID: %d | File: %s | Score: %zu\n",
                    i + 1, result->docId,
                    index->documents[result->docId].filename,
                    result->score) < 0) {
            return 0;
        }
    }
    return 1;
}

/* Quote fields containing CSV separators and double every embedded quote. */
static int write_csv_field(FILE *stream, const char *value)
{
    const unsigned char *p;

    if (strpbrk(value, ",\"\r\n") == NULL) {
        return fputs(value, stream) != EOF;
    }
    if (fputc('"', stream) == EOF) {
        return 0;
    }
    for (p = (const unsigned char *)value; *p != '\0'; ++p) {
        if (*p == '"' && fputc('"', stream) == EOF) {
            return 0;
        }
        if (fputc(*p, stream) == EOF) {
            return 0;
        }
    }
    return fputc('"', stream) != EOF;
}

static int write_csv_row(FILE *stream, const char *const fields[COL_COUNT])
{
    size_t i;

    for (i = 0; i < COL_COUNT; ++i) {
        if (i != 0 && fputc(',', stream) == EOF) {
            return 0;
        }
        if (!write_csv_field(stream, fields[i] != NULL ? fields[i] : "")) {
            return 0;
        }
    }
    return fputs("\r\n", stream) != EOF;
}

static int write_csv_report(const SearchIndex *index,
                             const SearchResult *results,
                             size_t result_count, const char *query,
                             int include_results, FILE *stream)
{
    const char *header[COL_COUNT] = {
        "section", "document_count", "document_id", "filename",
        "total_words", "unique_words", "posting_relationships", "query",
        "result_count", "rank", "score"
    };
    const char *fields[COL_COUNT] = {0};
    char document_count[NUMBER_BUFFER_SIZE];
    char document_id[NUMBER_BUFFER_SIZE];
    char total_words[NUMBER_BUFFER_SIZE];
    char unique_words[NUMBER_BUFFER_SIZE];
    char relationships[NUMBER_BUFFER_SIZE];
    char result_total[NUMBER_BUFFER_SIZE];
    char rank[NUMBER_BUFFER_SIZE];
    char score[NUMBER_BUFFER_SIZE];
    size_t i;

    if (!write_csv_row(stream, header)) {
        return 0;
    }

    snprintf(document_count, sizeof(document_count), "%zu", index->doc_count);
    snprintf(total_words, sizeof(total_words), "%zu", index->total_terms);
    snprintf(unique_words, sizeof(unique_words), "%zu", index->term_count);
    fields[COL_SECTION] = "collection";
    fields[COL_DOCUMENT_COUNT] = document_count;
    fields[COL_TOTAL_WORDS] = total_words;
    fields[COL_UNIQUE_WORDS] = unique_words;
    if (!write_csv_row(stream, fields)) {
        return 0;
    }

    fields[COL_SECTION] = "document";
    fields[COL_DOCUMENT_COUNT] = NULL;
    fields[COL_DOCUMENT_ID] = document_id;
    for (i = 0; i < index->doc_count; ++i) {
        const Document *document = &index->documents[i];
        snprintf(document_id, sizeof(document_id), "%zu", i);
        snprintf(total_words, sizeof(total_words), "%zu", document->total_terms);
        snprintf(unique_words, sizeof(unique_words), "%zu", document->unique_terms);
        fields[COL_FILENAME] = document->filename;
        if (!write_csv_row(stream, fields)) {
            return 0;
        }
    }

    memset(fields, 0, sizeof(fields));
    snprintf(unique_words, sizeof(unique_words), "%zu", index->term_count);
    snprintf(relationships, sizeof(relationships), "%zu", index->posting_count);
    fields[COL_SECTION] = "index";
    fields[COL_UNIQUE_WORDS] = unique_words;
    fields[COL_POSTING_RELATIONSHIPS] = relationships;
    if (!write_csv_row(stream, fields)) {
        return 0;
    }

    if (!include_results) {
        return 1;
    }
    memset(fields, 0, sizeof(fields));
    snprintf(result_total, sizeof(result_total), "%zu", result_count);
    fields[COL_SECTION] = "search";
    fields[COL_QUERY] = query != NULL ? query : "";
    fields[COL_RESULT_COUNT] = result_total;
    if (!write_csv_row(stream, fields)) {
        return 0;
    }

    fields[COL_SECTION] = "search_result";
    fields[COL_RESULT_COUNT] = NULL;
    fields[COL_DOCUMENT_ID] = document_id;
    fields[COL_RANK] = rank;
    fields[COL_SCORE] = score;
    for (i = 0; i < result_count; ++i) {
        snprintf(document_id, sizeof(document_id), "%d", results[i].docId);
        snprintf(rank, sizeof(rank), "%zu", i + 1);
        snprintf(score, sizeof(score), "%zu", results[i].score);
        fields[COL_FILENAME] = index->documents[results[i].docId].filename;
        if (!write_csv_row(stream, fields)) {
            return 0;
        }
    }
    return 1;
}

void statistics_print(const SearchIndex *index, FILE *stream)
{
    if (stream == NULL) {
        return;
    }
    if (index == NULL || !index->ready) {
        fputs("No index is ready. Load a collection and build an index first.\n", stream);
        return;
    }
    if (index->doc_count > MAX_DOCUMENTS) {
        fputs("Invalid index: document count exceeds the limit.\n", stream);
        return;
    }
    (void)write_text_statistics(index, stream);
}

int statistics_export(const SearchIndex *index,
                      const SearchResult *results, size_t result_count,
                      const char *query, const char *path, int csv,
                      char *error, size_t error_size)
{
    FILE *stream;
    size_t i;
    int include_results;
    int write_ok;
    int write_error;
    int close_status;

    set_error(error, error_size, "");
    if (index == NULL || !index->ready) {
        set_error(error, error_size,
                  "No index is ready. Load a collection and build an index first.");
        return 0;
    }
    if (index->doc_count > MAX_DOCUMENTS) {
        set_error(error, error_size,
                  "Invalid index: document count exceeds the limit.");
        return 0;
    }
    if (path == NULL || path[0] == '\0') {
        set_error(error, error_size, "Please provide a report file path.");
        return 0;
    }
    if (result_count != 0 && results == NULL) {
        set_error(error, error_size, "Search results are missing.");
        return 0;
    }
    for (i = 0; i < result_count; ++i) {
        if (results[i].docId < 0 || (size_t)results[i].docId >= index->doc_count) {
            set_error(error, error_size,
                      "A search result refers to an invalid document ID.");
            return 0;
        }
    }

    /* Binary mode preserves UTF-8 bytes and our explicit CSV CRLF endings. */
    stream = fopen(path, "wb");
    if (stream == NULL) {
        set_io_error(error, error_size, "Could not open the report file", errno);
        return 0;
    }
    include_results = query != NULL || results != NULL || result_count != 0;
    errno = 0;
    if (csv) {
        write_ok = write_csv_report(index, results, result_count, query,
                                   include_results, stream);
    } else {
        write_ok = write_text_statistics(index, stream);
        if (write_ok && include_results) {
            write_ok = write_text_results(index, results, result_count,
                                          query, stream);
        }
    }
    if (ferror(stream)) {
        write_ok = 0;
    }
    write_error = errno;
    close_status = fclose(stream);
    if (!write_ok) {
        set_io_error(error, error_size, "Could not write the report", write_error);
        return 0;
    }
    if (close_status != 0) {
        set_io_error(error, error_size, "Could not finish the report file", errno);
        return 0;
    }
    return 1;
}

/* ================= TASK 6: CONSOLE AND MAIN ================= */

/* Read and drain a whole line: 1 = success, 0 = EOF, -1 = overlong/error. */
static int read_line(char *buffer, size_t capacity)
{
    if (!fgets(buffer, (int)capacity, stdin))
        return ferror(stdin) ? -1 : 0;
    if (!strchr(buffer, '\n')) {
        int ch = getchar();
        if (ch != '\n' && ch != EOF) {
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;
            puts("Input is too long. Please try again.");
            return -1;
        }
    }
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

static int parse_number(const char *text, long minimum, long maximum, long *value)
{
    char *end;
    errno = 0;
    long number = strtol(text, &end, 10);
    if (end == text || errno == ERANGE)
        return 0;
    while (isspace((unsigned char)*end))
        ++end;
    if (*end || number < minimum || number > maximum)
        return 0;
    *value = number;
    return 1;
}

static int read_path(char *path, size_t size, const char *default_path)
{
    printf("Output/input file (Enter for %s): ", default_path);
    fflush(stdout);
    if (read_line(path, size) != 1)
        return 0;
    if (!*path)
        snprintf(path, size, "%s", default_path);
    return 1;
}

static int require_index(const SearchIndex *index)
{
    if (index->ready)
        return 1;
    puts("Load a collection and build an index first.");
    return 0;
}

static int readFile(SearchIndex *active)
{
    char path[4096], error[256];
#ifdef ENABLE_DIRECTORY_INPUT
    printf("Folder or collection-list path: ");
#else
    printf("Collection-list path (one document path per line): ");
#endif
    fflush(stdout);
    if (read_line(path, sizeof path) != 1)
        return 0;
    SearchIndex *candidate = malloc(sizeof *candidate);
    if (!candidate) {
        puts("Could not allocate a new collection.");
        return 0;
    }
    index_init(candidate);
    clock_t start = clock();
    if (!collection_load(candidate, path, error, sizeof error)) {
        printf("Collection not loaded: %s\n", error);
        free(candidate);
        return 0;
    }
    if (!index_build(candidate)) {
        puts("Could not build the index. The previous collection is still available.");
        index_free(candidate);
        free(candidate);
        return 0;
    }
    clock_t end = clock();
    index_free(active);
    *active = *candidate;
    free(candidate);
    printf("Loaded %zu documents and built %zu terms / %zu postings.\n",
           active->doc_count, active->term_count, active->posting_count);
    if (start != (clock_t)-1 && end != (clock_t)-1)
        printf("Load + build CPU time: %.6f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);
    return 1;
}

static void searchEngine(SearchIndex *index, SearchResult *last_results,
                         size_t *last_count, char *last_query)
{
    if (!require_index(index))
        return;
    char query[MAX_QUERY_BYTES + 1];
    char input[128];
    SearchResult results[MAX_DOCUMENTS];
    for (;;) {
        printf("\nSearch query (/exit to return): ");
        fflush(stdout);
        int status = read_line(query, sizeof query);
        if (status == 0)
            return;
        if (status < 0) {
            if (ferror(stdin))
                return;
            continue;
        }
        if (!strcmp(query, "/exit"))
            return;
        int has_letter = 0;
        for (size_t i = 0; query[i]; ++i)
            if ((query[i] >= 'A' && query[i] <= 'Z') ||
                (query[i] >= 'a' && query[i] <= 'z'))
                has_letter = 1;
        if (!has_letter) {
            puts("Enter a query containing at least one English letter.");
            continue;
        }
        size_t count = index_search(index, query, results, MAX_DOCUMENTS);
        if (!count) {
            strcpy(last_query, query);
            *last_count = 0;
            puts("No matching documents.");
            continue;
        }
        printf("Top K (Enter for 5): ");
        fflush(stdout);
        if (read_line(input, sizeof input) != 1)
            return;
        long requested = 5;
        if (*input && !parse_number(input, 1, MAX_DOCUMENTS, &requested)) {
            puts("Enter a positive whole number between 1 and 6000.");
            continue;
        }
        if (count > (size_t)requested)
            count = (size_t)requested;
        puts("Rank\tDocument ID\tFile\tScore");
        for (size_t rank = 0; rank < count; ++rank)
            printf("%zu\t%d\t%s\t%zu\n", rank + 1, results[rank].docId,
                   index->documents[results[rank].docId].filename, results[rank].score);
        memcpy(last_results, results, count * sizeof *results);
        *last_count = count;
        strcpy(last_query, query);
    }
}

static void view_document(const SearchIndex *index)
{
    if (!require_index(index))
        return;
    if (!index->doc_count) {
        puts("The collection contains no documents.");
        return;
    }
    char input[128];
    printf("Document ID (0-%zu): ", index->doc_count - 1);
    fflush(stdout);
    if (read_line(input, sizeof input) != 1)
        return;
    long id;
    if (!parse_number(input, 0, (long)index->doc_count - 1, &id)) {
        puts("Invalid document ID.");
        return;
    }
    printf("\n%s\n%s\n", index->documents[id].filename,
           index->documents[id].content ? index->documents[id].content : "");
}

static void show_menu(void)
{
    puts("\nTask 6: Statistics and Reporting\n"
         "1. Load collection and build index\n"
         "2. Rebuild current index\n"
         "3. Show collection, document and index statistics\n"
         "4. Search\n"
         "5. Preview document\n"
         "6. Export TXT report\n"
         "7. Export CSV report\n"
         "0. Exit");
    printf("Choice: ");
    fflush(stdout);
}

int main(void)
{
    /* Large collections belong on the heap rather than the stack. */
    SearchIndex *index = malloc(sizeof *index);
    SearchResult *last_results = malloc(MAX_DOCUMENTS * sizeof *last_results);
    if (!index || !last_results) {
        free(index);
        free(last_results);
        fputs("Could not allocate the application.\n", stderr);
        return 1;
    }
    index_init(index);
    size_t last_count = 0;
    char last_query[MAX_QUERY_BYTES + 1] = "";
    char input[128], path[4096], error[256];
    int done = 0;

    while (!done) {
        show_menu();
        int input_status = read_line(input, sizeof input);
        if (input_status == 0)
            break;
        if (input_status < 0) {
            if (ferror(stdin))
                break;
            continue;
        }
        long choice;
        if (!parse_number(input, 0, 7, &choice)) {
            puts("Invalid menu choice. Enter a number from 0 to 7.");
            continue;
        }
        switch (choice) {
        case 0:
            done = 1;
            break;
        case 1:
            if (readFile(index)) {
                last_count = 0;
                last_query[0] = '\0';
            }
            break;
        case 2:
            if (!index->doc_count) {
                puts("No documents loaded.");
                break;
            }
            if (index_build(index))
                puts("Index rebuilt.");
            else
                puts("Could not rebuild the index.");
            /* A rebuild invalidates the previous result snapshot. */
            last_count = 0;
            last_query[0] = '\0';
            break;
        case 3:
            statistics_print(index, stdout);
            break;
        case 4:
            searchEngine(index, last_results, &last_count, last_query);
            break;
        case 5:
            view_document(index);
            break;
        case 6:
        case 7:
            if (!require_index(index) ||
                !read_path(path, sizeof path, choice == 6 ? "report.txt" : "report.csv"))
                break;
            if (statistics_export(index, *last_query ? last_results : NULL,
                                  last_count, *last_query ? last_query : NULL,
                                  path, choice == 7, error, sizeof error))
                printf("Report saved to %s\n", path);
            else
                printf("Report not saved: %s\n", error);
            break;
        }
    }

    int exit_status = ferror(stdin) ? 1 : 0;
    index_free(index);
    free(index);
    free(last_results);
    return exit_status;
}
