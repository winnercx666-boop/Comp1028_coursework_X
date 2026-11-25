#include "statistics.h"

#include <errno.h>
#include <string.h>

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
        fputs("No index is ready. Build or load an index first.\n", stream);
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
                  "No index is ready. Build or load an index first.");
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
