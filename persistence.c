#include "persistence.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
