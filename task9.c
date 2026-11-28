/*
 * COMP1028 Task 9: Save and Reload an Inverted Index
 * Standalone C17 source; no project-specific header files are required.
 *
 * Standard-C build (collection-list input):
 *   gcc -std=c17 -O2 -Wall -Wextra -Wpedantic task9.c -o task9.exe
 * Optional folder-input build (requires dirent.h):
 *   gcc -DENABLE_DIRECTORY_INPUT -std=c17 -O2 task9.c -o task9.exe
 *
 * Supporting collection, tokenization, indexing and ranking functions are
 * included so that saving, reloading, searching and previewing can be tried
 * by compiling this file alone. Task 6 reports are not included.
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
 *
 */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_DIRECTORY_INPUT
#include <dirent.h>
#endif

/* Shared structures and function declarations. */

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

/* Collection input leaves the destination unchanged if it fails. */
int collection_load(SearchIndex *destination, const char *path,
                    char *error, size_t error_size);
/* The saved format contains byte lengths, documents, terms and postings. */
int index_save(const SearchIndex *index, const char *path,
               char *error, size_t error_size);
/* destination must be initialized; a failed load retains its current index. */
int index_load(SearchIndex *destination, const char *path,
               char *error, size_t error_size);

/* Collection input (standard C by default). */

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

/* Supporting inverted-index implementation. */

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

/* Task 9 persistence: original versioned format and strict validation. */

#define INDEX_MAGIC "COMP1028_INDEX 1\n"
#define RECORD_LINE_BYTES 256

static void set_error(char *error, size_t size, const char *message)
{
    if (error != NULL && size > 0)
        snprintf(error, size, "%s", message);
}

static size_t bounded_length(const char *string, size_t limit)
{
    size_t length = 0;
    if (string == NULL)
        return 0;
    while (length < limit && string[length] != '\0')
        ++length;
    return length;
}

static int valid_word(const char *word, size_t length)
{
    size_t i;
    if (length == 0 || length > MAX_TERM_BYTES)
        return 0;
    for (i = 0; i < length; ++i) {
        if (word[i] < 'a' || word[i] > 'z')
            return 0;
    }
    return 1;
}

/* Check the in-memory counters against the actual posting lists before
   creating/truncating the output file. */
static int valid_index(const SearchIndex *index)
{
    size_t totals[MAX_DOCUMENTS] = {0};
    size_t unique[MAX_DOCUMENTS] = {0};
    size_t seen[MAX_DOCUMENTS] = {0};
    size_t actual_terms = 0, actual_postings = 0, actual_total = 0;
    size_t i;

    if (index == NULL || !index->ready || index->doc_count > MAX_DOCUMENTS)
        return 0;
    for (i = 0; i < index->doc_count; ++i) {
        size_t name_length = bounded_length(index->documents[i].filename,
                                             MAX_FILENAME);
        size_t content_length = bounded_length(index->documents[i].content,
                                                MAX_DOCUMENT_BYTES + 1U);
        if (name_length == 0 || name_length >= MAX_FILENAME ||
            content_length > MAX_DOCUMENT_BYTES ||
            index->documents[i].total_terms > content_length ||
            index->documents[i].unique_terms > index->documents[i].total_terms)
            return 0;
    }

    for (i = 0; i < INDEX_BUCKETS; ++i) {
        const Term *term;
        for (term = index->buckets[i]; term != NULL; term = term->next) {
            const Posting *posting;
            size_t length = bounded_length(term->word, MAX_TERM_BYTES + 1U);
            if (!valid_word(term->word, length) || term->postings == NULL ||
                index_find_term(index, term->word) != term ||
                actual_terms == SIZE_MAX)
                return 0;
            ++actual_terms;
            for (posting = term->postings; posting != NULL;
                 posting = posting->next) {
                size_t id;
                if (posting->docId < 0 ||
                    (size_t)posting->docId >= index->doc_count ||
                    posting->frequency == 0)
                    return 0;
                id = (size_t)posting->docId;
                if (seen[id] == actual_terms || actual_postings == SIZE_MAX ||
                    totals[id] > SIZE_MAX - posting->frequency ||
                    actual_total > SIZE_MAX - posting->frequency)
                    return 0;
                seen[id] = actual_terms;
                ++unique[id];
                ++actual_postings;
                totals[id] += posting->frequency;
                actual_total += posting->frequency;
            }
        }
    }
    if (actual_terms != index->term_count ||
        actual_postings != index->posting_count ||
        actual_total != index->total_terms)
        return 0;
    for (i = 0; i < index->doc_count; ++i) {
        if (totals[i] != index->documents[i].total_terms ||
            unique[i] != index->documents[i].unique_terms)
            return 0;
    }
    return 1;
}

static int write_block(FILE *file, const char *data, size_t length)
{
    if (length > 0 && fwrite(data, 1, length, file) != length)
        return 0;
    return fputc('\n', file) != EOF;
}

int index_save(const SearchIndex *index, const char *path,
               char *error, size_t error_size)
{
    FILE *file;
    size_t i;
    set_error(error, error_size, "");
    if (path == NULL || path[0] == '\0') {
        set_error(error, error_size, "Index file path cannot be empty.");
        return 0;
    }
    if (!valid_index(index)) {
        set_error(error, error_size,
                  "Build a valid index before saving; counters must match postings.");
        return 0;
    }
    /* Binary file mode preserves the exact byte lengths on Windows, too.
       The records themselves are readable decimal text. */
    file = fopen(path, "wb");
    if (file == NULL) {
        set_error(error, error_size, "Cannot open index file for writing.");
        return 0;
    }
    if (fputs(INDEX_MAGIC, file) == EOF ||
        fprintf(file, "COUNTS %zu %zu %zu %zu\n", index->doc_count,
                index->total_terms, index->term_count, index->posting_count) < 0)
        goto write_error;

    for (i = 0; i < index->doc_count; ++i) {
        const Document *document = &index->documents[i];
        size_t name_length = strlen(document->filename);
        size_t content_length = document->content != NULL ?
                                strlen(document->content) : 0;
        if (fprintf(file, "D %zu %zu %zu %zu %zu\n", i, name_length,
                    content_length, document->total_terms,
                    document->unique_terms) < 0 ||
            !write_block(file, document->filename, name_length) ||
            !write_block(file, document->content, content_length))
            goto write_error;
    }
    for (i = 0; i < INDEX_BUCKETS; ++i) {
        const Term *term;
        for (term = index->buckets[i]; term != NULL; term = term->next) {
            const Posting *posting;
            size_t posting_count = 0;
            for (posting = term->postings; posting != NULL;
                 posting = posting->next)
                ++posting_count;
            if (fprintf(file, "T %zu %zu\n", strlen(term->word),
                        posting_count) < 0 ||
                !write_block(file, term->word, strlen(term->word)))
                goto write_error;
            for (posting = term->postings; posting != NULL;
                 posting = posting->next) {
                if (fprintf(file, "P %d %u\n", posting->docId,
                            posting->frequency) < 0)
                    goto write_error;
            }
        }
    }
    if (fputs("END\n", file) == EOF || ferror(file))
        goto write_error;
    if (fclose(file) != 0) {
        set_error(error, error_size, "Could not finish writing the index file.");
        return 0;
    }
    return 1;

write_error:
    fclose(file);
    set_error(error, error_size,
              "Could not write the complete index file; the output may be incomplete.");
    return 0;
}

static int read_line(FILE *file, char *line, size_t size)
{
    size_t length;
    if (fgets(line, (int)size, file) == NULL)
        return 0;
    length = strlen(line);
    return length > 0 && line[length - 1] == '\n';
}

/* Strict decimal parsing rejects signs, overflow, extra fields and trailing
   junk. Lengths are checked before they are used for any allocation. */
static int read_numbers(FILE *file, const char *label,
                        uintmax_t *numbers, size_t count)
{
    char line[RECORD_LINE_BYTES];
    char *cursor;
    size_t label_length = strlen(label), i;
    if (!read_line(file, line, sizeof line) ||
        strncmp(line, label, label_length) != 0 ||
        line[label_length] != ' ')
        return 0;
    cursor = line + label_length + 1;
    for (i = 0; i < count; ++i) {
        char *end;
        if (*cursor < '0' || *cursor > '9')
            return 0;
        errno = 0;
        numbers[i] = strtoumax(cursor, &end, 10);
        if (errno == ERANGE || end == cursor)
            return 0;
        if (i + 1 < count) {
            if (*end != ' ')
                return 0;
            cursor = end + 1;
        } else if (*end != '\n' || end[1] != '\0') {
            return 0;
        }
    }
    return 1;
}

static int read_block(FILE *file, char *data, size_t length)
{
    if (length > 0 && fread(data, 1, length, file) != length)
        return 0;
    if (fgetc(file) != '\n' || memchr(data, '\0', length) != NULL)
        return 0;
    data[length] = '\0';
    return 1;
}

int index_load(SearchIndex *destination, const char *path,
               char *error, size_t error_size)
{
    FILE *file = NULL;
    SearchIndex *temporary = NULL;
    uintmax_t counts[4], fields[5];
    size_t expected_total[MAX_DOCUMENTS] = {0};
    size_t expected_unique[MAX_DOCUMENTS] = {0};
    size_t seen[MAX_DOCUMENTS] = {0};
    size_t expected_terms, expected_postings, i;
    char line[RECORD_LINE_BYTES];

    set_error(error, error_size, "");
    if (destination == NULL || path == NULL || path[0] == '\0') {
        set_error(error, error_size, "Provide an initialized index and a file path.");
        return 0;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        set_error(error, error_size, "Cannot open index file for reading.");
        return 0;
    }
    if (!read_line(file, line, sizeof line) || strcmp(line, INDEX_MAGIC) != 0) {
        set_error(error, error_size, "Invalid index file format or unsupported version.");
        goto failure;
    }
    if (!read_numbers(file, "COUNTS", counts, 4) ||
        counts[0] > MAX_DOCUMENTS || counts[1] > SIZE_MAX ||
        counts[2] > SIZE_MAX || counts[3] > SIZE_MAX ||
        counts[1] > counts[0] * MAX_DOCUMENT_BYTES ||
        counts[2] > counts[3] || counts[3] > counts[1] ||
        (counts[2] == 0 && counts[3] != 0)) {
        set_error(error, error_size, "Invalid index counts.");
        goto failure;
    }
    temporary = malloc(sizeof *temporary);
    if (temporary == NULL) {
        set_error(error, error_size, "Could not allocate the loaded index.");
        goto failure;
    }
    index_init(temporary);
    temporary->doc_count = (size_t)counts[0];
    expected_terms = (size_t)counts[2];
    expected_postings = (size_t)counts[3];

    for (i = 0; i < temporary->doc_count; ++i) {
        Document *document = &temporary->documents[i];
        if (!read_numbers(file, "D", fields, 5) || fields[0] != i ||
            fields[1] == 0 || fields[1] >= MAX_FILENAME ||
            fields[2] > MAX_DOCUMENT_BYTES || fields[3] > fields[2] ||
            fields[4] > fields[3]) {
            set_error(error, error_size, "Invalid or truncated document record.");
            goto failure;
        }
        expected_total[i] = (size_t)fields[3];
        expected_unique[i] = (size_t)fields[4];
        document->content = malloc((size_t)fields[2] + 1);
        if (document->content == NULL) {
            set_error(error, error_size, "Could not allocate document content.");
            goto failure;
        }
        if (!read_block(file, document->filename, (size_t)fields[1]) ||
            !read_block(file, document->content, (size_t)fields[2])) {
            set_error(error, error_size, "Invalid or truncated document data.");
            goto failure;
        }
    }
    for (i = 0; i < expected_terms; ++i) {
        char word[MAX_TERM_BYTES + 1];
        size_t j, posting_count;
        if (!read_numbers(file, "T", fields, 2) ||
            fields[0] == 0 || fields[0] > MAX_TERM_BYTES ||
            fields[1] == 0 || fields[1] > temporary->doc_count ||
            temporary->posting_count > expected_postings ||
            fields[1] > expected_postings - temporary->posting_count ||
            !read_block(file, word, (size_t)fields[0]) ||
            !valid_word(word, (size_t)fields[0]) ||
            index_find_term(temporary, word) != NULL) {
            set_error(error, error_size, "Invalid, duplicate or truncated term record.");
            goto failure;
        }
        posting_count = (size_t)fields[1];
        for (j = 0; j < posting_count; ++j) {
            size_t id;
            if (!read_numbers(file, "P", fields, 2) ||
                fields[0] >= temporary->doc_count ||
                fields[1] == 0 || fields[1] > UINT_MAX) {
                set_error(error, error_size, "Invalid or truncated posting record.");
                goto failure;
            }
            id = (size_t)fields[0];
            if (seen[id] == i + 1) {
                set_error(error, error_size, "Duplicate document ID in a posting list.");
                goto failure;
            }
            seen[id] = i + 1;
            if (temporary->documents[id].total_terms > expected_total[id] ||
                fields[1] > expected_total[id] -
                            temporary->documents[id].total_terms ||
                temporary->documents[id].unique_terms >= expected_unique[id]) {
                set_error(error, error_size,
                          "Posting data exceeds the document counters.");
                goto failure;
            }
            if (!index_add_posting(temporary, word, (int)id,
                                   (unsigned)fields[1])) {
                set_error(error, error_size, "Could not restore posting data safely.");
                goto failure;
            }
        }
    }
    if (temporary->total_terms != (size_t)counts[1] ||
        temporary->term_count != expected_terms ||
        temporary->posting_count != expected_postings) {
        set_error(error, error_size, "Index counters do not match the saved posting data.");
        goto failure;
    }
    for (i = 0; i < temporary->doc_count; ++i) {
        if (temporary->documents[i].total_terms != expected_total[i] ||
            temporary->documents[i].unique_terms != expected_unique[i]) {
            set_error(error, error_size, "Document counters do not match the posting data.");
            goto failure;
        }
    }
    if (!read_line(file, line, sizeof line) || strcmp(line, "END\n") != 0 ||
        fgetc(file) != EOF || ferror(file)) {
        set_error(error, error_size, "Invalid index ending, extra data or read failure.");
        goto failure;
    }
    if (fclose(file) != 0) {
        file = NULL;
        set_error(error, error_size, "Could not finish reading the index file.");
        goto failure;
    }
    file = NULL;
    temporary->ready = 1;
    /* Move ownership only after parsing and checking the entire file. */
    index_free(destination);
    *destination = *temporary;
    free(temporary);
    return 1;

failure:
    if (file != NULL)
        fclose(file);
    if (temporary != NULL) {
        index_free(temporary);
        free(temporary);
    }
    return 0;
}

/* Console demonstration. cli_ names are distinct from file-format helpers. */

/* Read and drain a complete input line: 1 = success, 0 = EOF, -1 = error.
   Reject embedded NUL bytes so numeric/path input cannot hide trailing data. */
static int cli_read_line(char *buffer, size_t capacity)
{
    size_t length = 0;
    int ch, too_long = 0, contains_nul = 0;
    if (buffer == NULL || capacity < 2)
        return -1;
    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (ch == '\0')
            contains_nul = 1;
        if (length + 1 < capacity)
            buffer[length++] = (char)ch;
        else
            too_long = 1;
    }
    buffer[length] = '\0';
    if (ferror(stdin)) {
        fputs("Could not read console input.\n", stderr);
        return -1;
    }
    if (ch == EOF && length == 0 && !too_long && !contains_nul)
        return 0;
    if (too_long || contains_nul) {
        puts(too_long ? "Input is too long. Please try again." :
                        "Input must be text without embedded NUL bytes.");
        return -1;
    }
    if (length > 0 && buffer[length - 1] == '\r')
        buffer[length - 1] = '\0';
    return 1;
}

static int cli_parse_number(const char *text, long minimum, long maximum,
                            long *value)
{
    char *end;
    long number;
    errno = 0;
    number = strtol(text, &end, 10);
    if (end == text || errno == ERANGE)
        return 0;
    while (isspace((unsigned char)*end))
        ++end;
    if (*end != '\0' || number < minimum || number > maximum)
        return 0;
    *value = number;
    return 1;
}

static int cli_index_path(char *path, size_t capacity)
{
    int length;
    printf("Index file path (Enter for index.idx): ");
    fflush(stdout);
    if (cli_read_line(path, capacity) != 1)
        return 0;
    if (*path == '\0') {
        length = snprintf(path, capacity, "%s", "index.idx");
        if (length < 0 || (size_t)length >= capacity) {
            puts("Index path is too long.");
            return 0;
        }
    }
    return 1;
}

static int cli_require_index(const SearchIndex *index)
{
    if (index->ready)
        return 1;
    puts("Load a collection and build an index, or load a saved index first.");
    return 0;
}

static void cli_status(const SearchIndex *index)
{
    printf("Index ready: %s\n", index->ready ? "yes" : "no");
    printf("Documents: %zu\n", index->doc_count);
    printf("Total term occurrences: %zu\n", index->total_terms);
    printf("Unique terms: %zu\n", index->term_count);
    printf("Term-document postings: %zu\n", index->posting_count);
}

static void cli_load_collection(SearchIndex *active)
{
    char path[MAX_PATH_BYTES + 1], error[256];
    SearchIndex *candidate;
#ifdef ENABLE_DIRECTORY_INPUT
    printf("Folder or collection-list path: ");
#else
    printf("Collection-list path (one document path per line): ");
#endif
    fflush(stdout);
    if (cli_read_line(path, sizeof path) != 1)
        return;
    candidate = malloc(sizeof *candidate);
    if (candidate == NULL) {
        puts("Could not allocate a new collection.");
        return;
    }
    index_init(candidate);
    if (!collection_load(candidate, path, error, sizeof error)) {
        printf("Collection not loaded: %s\n", error);
        index_free(candidate);
        free(candidate);
        return;
    }
    if (!index_build(candidate)) {
        puts("Could not build the index. The previous index is unchanged.");
        index_free(candidate);
        free(candidate);
        return;
    }
    index_free(active);
    *active = *candidate;
    free(candidate);
    puts("Collection loaded and index built.");
    cli_status(active);
}

static void cli_search(SearchIndex *index, SearchResult *results)
{
    char query[MAX_QUERY_BYTES + 1], input[128];
    size_t count;
    long requested = 5;
    int has_letter = 0;
    if (!cli_require_index(index))
        return;
    printf("Search query: ");
    fflush(stdout);
    if (cli_read_line(query, sizeof query) != 1)
        return;
    for (size_t i = 0; query[i] != '\0'; ++i) {
        if (letter((unsigned char)query[i]))
            has_letter = 1;
    }
    if (!has_letter) {
        puts("Enter a query containing at least one English letter.");
        return;
    }
    count = index_search(index, query, results, MAX_DOCUMENTS);
    if (count == 0) {
        puts("No matching documents.");
        return;
    }
    printf("Top K (Enter for 5): ");
    fflush(stdout);
    if (cli_read_line(input, sizeof input) != 1)
        return;
    if (*input != '\0' &&
        !cli_parse_number(input, 1, MAX_DOCUMENTS, &requested)) {
        printf("Enter a whole number from 1 to %d.\n", MAX_DOCUMENTS);
        return;
    }
    if (count > (size_t)requested)
        count = (size_t)requested;
    puts("Rank\tDocument ID\tFile\tScore");
    for (size_t rank = 0; rank < count; ++rank) {
        printf("%zu\t%d\t%s\t%zu\n", rank + 1, results[rank].docId,
               index->documents[results[rank].docId].filename,
               results[rank].score);
    }
}

static void cli_preview(const SearchIndex *index)
{
    char input[128];
    long id;
    if (!cli_require_index(index))
        return;
    if (index->doc_count == 0) {
        puts("The index contains no documents.");
        return;
    }
    printf("Document ID (0-%zu): ", index->doc_count - 1);
    fflush(stdout);
    if (cli_read_line(input, sizeof input) != 1)
        return;
    if (!cli_parse_number(input, 0, (long)index->doc_count - 1, &id)) {
        puts("Invalid document ID.");
        return;
    }
    printf("\n%s\n%s\n", index->documents[id].filename,
           index->documents[id].content != NULL ?
           index->documents[id].content : "");
}

static void cli_menu(void)
{
    puts("\nTask 9: Save and Reload an Inverted Index\n"
         "1. Load collection + build index\n"
         "2. Save index\n"
         "3. Load saved index\n"
         "4. Search\n"
         "5. Preview document\n"
         "6. Show index status\n"
         "0. Exit");
    printf("Choice: ");
    fflush(stdout);
}

int main(void)
{
    SearchIndex *index = malloc(sizeof *index);
    SearchResult *results = malloc(MAX_DOCUMENTS * sizeof *results);
    char input[128], path[MAX_PATH_BYTES + 1], error[256];
    int done = 0, exit_status;
    if (index == NULL || results == NULL) {
        free(index);
        free(results);
        fputs("Could not allocate the application.\n", stderr);
        return EXIT_FAILURE;
    }
    index_init(index);
    while (!done) {
        int status;
        long choice;
        cli_menu();
        status = cli_read_line(input, sizeof input);
        if (status == 0)
            break;
        if (status < 0) {
            if (ferror(stdin))
                break;
            continue;
        }
        if (!cli_parse_number(input, 0, 6, &choice)) {
            puts("Invalid menu choice. Enter a number from 0 to 6.");
            continue;
        }
        switch (choice) {
        case 0:
            done = 1;
            break;
        case 1:
            cli_load_collection(index);
            break;
        case 2:
            if (!cli_require_index(index) ||
                !cli_index_path(path, sizeof path))
                break;
            if (index_save(index, path, error, sizeof error))
                printf("Index saved to %s\n", path);
            else
                printf("Index not saved: %s\n", error);
            break;
        case 3:
            if (!cli_index_path(path, sizeof path))
                break;
            if (index_load(index, path, error, sizeof error)) {
                puts("Index loaded without rebuilding.");
                cli_status(index);
            } else {
                printf("Index not loaded: %s\n", error);
                puts("The previous index is unchanged.");
            }
            break;
        case 4:
            cli_search(index, results);
            break;
        case 5:
            cli_preview(index);
            break;
        case 6:
            cli_status(index);
            break;
        }
        if (ferror(stdin))
            break;
    }
    exit_status = ferror(stdin) ? EXIT_FAILURE : EXIT_SUCCESS;
    index_free(index);
    free(index);
    free(results);
    return exit_status;
}
