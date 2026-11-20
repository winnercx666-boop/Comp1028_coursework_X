/* Contract tests for coursework features 6 and 9.
   Run from the repository root after creating tests/run. */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../search_index.h"
#include "../statistics.h"
#include "../persistence.h"

static unsigned checks;
static unsigned failures;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        ++failures; \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
    } \
} while (0)

#define REQUIRE(condition) do { \
    ++checks; \
    if (!(condition)) { \
        ++failures; \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static char *copy_string(const char *source)
{
    size_t length = strlen(source);
    char *result = malloc(length + 1);
    REQUIRE(result != NULL);
    memcpy(result, source, length + 1);
    return result;
}

static SearchIndex *new_index(void)
{
    SearchIndex *index = malloc(sizeof *index);
    REQUIRE(index != NULL);
    index_init(index);
    return index;
}

static void set_document(SearchIndex *index, size_t id,
                         const char *name, const char *content)
{
    REQUIRE(id < MAX_DOCUMENTS);
    REQUIRE(strlen(name) < MAX_FILENAME);
    strcpy(index->documents[id].filename, name);
    index->documents[id].content = copy_string(content);
}

static SearchIndex *fixture(void)
{
    SearchIndex *index = new_index();
    index->doc_count = 3;
    set_document(index, 0, "a, \"first\".txt", "Apple apple, banana!\n");
    set_document(index, 1, "b with spaces \xe6\xb5\x8b\xe8\xaf\x95.txt", "apple orange");
    set_document(index, 2, "empty.txt", "");
    REQUIRE(index_build(index) == 1);
    return index;
}

static void check_fixture_counts(const SearchIndex *index)
{
    CHECK(index->ready != 0);
    CHECK(index->doc_count == 3);
    CHECK(index->total_terms == 5);
    CHECK(index->term_count == 3);
    CHECK(index->posting_count == 4);
    CHECK(index->documents[0].total_terms == 3);
    CHECK(index->documents[0].unique_terms == 2);
    CHECK(index->documents[1].total_terms == 2);
    CHECK(index->documents[1].unique_terms == 2);
    CHECK(index->documents[2].total_terms == 0);
    CHECK(index->documents[2].unique_terms == 0);
}

static void check_apple_results(SearchIndex *index, const char *query)
{
    SearchResult results[3];
    size_t count = index_search(index, query, results, 3);
    CHECK(count == 2);
    if (count == 2) {
        CHECK(results[0].docId == 0);
        CHECK(results[0].score == 2);
        CHECK(results[1].docId == 1);
        CHECK(results[1].score == 1);
    }
}

static void test_search_and_counts(SearchIndex *index)
{
    SearchResult results[3];
    char *original_content;
    size_t count;
    check_fixture_counts(index);
    check_apple_results(index, "apple");
    check_apple_results(index, "APPLE apple, apple!!!");
    check_apple_results(index, "apple");
    count = index_search(index, "banana ORANGE", results, 3);
    CHECK(count == 2);
    if (count == 2) {
        CHECK(results[0].docId == 0 && results[0].score == 1);
        CHECK(results[1].docId == 1 && results[1].score == 1);
    }
    count = index_search(index, "apple banana", results, 3);
    CHECK(count == 2);
    if (count == 2) {
        CHECK(results[0].docId == 0 && results[0].score == 3);
        CHECK(results[1].docId == 1 && results[1].score == 1);
    }
    CHECK(index_search(index, "quantumbanana", results, 3) == 0);
    CHECK(index_search(index, "", results, 3) == 0);
    CHECK(index_search(index, "123 -- !!!", results, 3) == 0);
    count = index_search(index, "apple", results, 1);
    CHECK(count == 1);
    if (count == 1)
        CHECK(results[0].docId == 0 && results[0].score == 2);

    /* Search must consult postings, even when the original text is unavailable. */
    original_content = index->documents[0].content;
    index->documents[0].content = NULL;
    check_apple_results(index, "apple");
    index->documents[0].content = original_content;

    REQUIRE(index_build(index) == 1);
    check_fixture_counts(index); /* Rebuilding must not double the counters. */
    original_content = index->documents[0].content;
    index_clear_terms(index);
    CHECK(index->documents[0].content == original_content);
    CHECK(index->total_terms == 0 && index->term_count == 0);
    CHECK(index->posting_count == 0 && index->ready == 0);
    CHECK(index->documents[0].total_terms == 0);
    CHECK(index->documents[0].unique_terms == 0);
    REQUIRE(index_build(index) == 1);
    check_fixture_counts(index);
}

static void write_bytes(const char *path, const char *bytes, size_t length)
{
    FILE *file = fopen(path, "wb");
    REQUIRE(file != NULL);
    REQUIRE(fwrite(bytes, 1, length, file) == length);
    REQUIRE(fclose(file) == 0);
}

static char *read_bytes(const char *path, size_t *length)
{
    long file_size;
    char *bytes;
    FILE *file = fopen(path, "rb");
    REQUIRE(file != NULL);
    REQUIRE(fseek(file, 0, SEEK_END) == 0);
    file_size = ftell(file);
    REQUIRE(file_size >= 0);
    REQUIRE(fseek(file, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)file_size + 1);
    REQUIRE(bytes != NULL);
    REQUIRE(fread(bytes, 1, (size_t)file_size, file) == (size_t)file_size);
    REQUIRE(fclose(file) == 0);
    bytes[file_size] = '\0';
    if (length)
        *length = (size_t)file_size;
    return bytes;
}

static void test_reports(SearchIndex *index)
{
    SearchResult results[3];
    char error[256];
    const char *query = "apple, \"banana\"";
    size_t count = index_search(index, query, results, 3);
    char *text;
    FILE *file;

    REQUIRE(statistics_export(index, results, count, query,
                              "tests/run/report.txt", 0,
                              error, sizeof error) == 1);
    text = read_bytes("tests/run/report.txt", NULL);
    CHECK(strstr(text, "Documents: 3") != NULL);
    CHECK(strstr(text, "Total words (occurrences): 5") != NULL);
    CHECK(strstr(text, "Unique words across all documents: 3") != NULL);
    CHECK(strstr(text, "Unique indexed words: 3") != NULL);
    CHECK(strstr(text, "Word-document relationships: 4") != NULL);
    CHECK(strstr(text, "a, \"first\".txt") != NULL);
    CHECK(strstr(text, "empty.txt") != NULL);
    CHECK(strstr(text, query) != NULL);
    free(text);

    REQUIRE(statistics_export(index, results, count, query,
                              "tests/run/report.csv", 1,
                              error, sizeof error) == 1);
    text = read_bytes("tests/run/report.csv", NULL);
    CHECK(strstr(text, "section,document_count,document_id,filename,total_words,") != NULL);
    CHECK(strstr(text, "collection,3,,,5,3,,,,,\r\n") != NULL);
    CHECK(strstr(text, "document,,0,\"a, \"\"first\"\".txt\",3,2,,,,,\r\n") != NULL);
    CHECK(strstr(text, "document,,2,empty.txt,0,0,,,,,\r\n") != NULL);
    CHECK(strstr(text, "index,,,,,3,4,,,,\r\n") != NULL);
    CHECK(strstr(text, "\"a, \"\"first\"\".txt\"") != NULL);
    CHECK(strstr(text, "\"apple, \"\"banana\"\"\"") != NULL);
    CHECK(strstr(text, "b with spaces \xe6\xb5\x8b\xe8\xaf\x95.txt") != NULL);
    CHECK(strstr(text, "empty.txt") != NULL);
    free(text);

    REQUIRE(statistics_export(index, results, count, "apple,\n\"banana\"",
                              "tests/run/multiline-report.csv", 1,
                              error, sizeof error) == 1);
    text = read_bytes("tests/run/multiline-report.csv", NULL);
    CHECK(strstr(text, "\"apple,\n\"\"banana\"\"\"") != NULL);
    free(text);

    file = fopen("tests/run/printed-statistics.txt", "wb");
    REQUIRE(file != NULL);
    statistics_print(index, file);
    REQUIRE(fclose(file) == 0);
    text = read_bytes("tests/run/printed-statistics.txt", NULL);
    CHECK(strstr(text, "a, \"first\".txt") != NULL);
    CHECK(strstr(text, "empty.txt") != NULL);
    free(text);

    CHECK(statistics_export(index, results, count, query,
                           "tests/run/missing-directory/report.csv", 1,
                           error, sizeof error) == 0);
    CHECK(error[0] != '\0');
}

static void test_round_trip(SearchIndex *original)
{
    SearchIndex *loaded = new_index();
    char error[256];
    size_t id;
    REQUIRE(index_save(original, "tests/run/index.txt", error,
                       sizeof error) == 1);
    REQUIRE(index_load(loaded, "tests/run/index.txt", error,
                       sizeof error) == 1);
    check_fixture_counts(loaded);
    check_apple_results(loaded, "APPLE apple");
    for (id = 0; id < original->doc_count; ++id) {
        CHECK(strcmp(loaded->documents[id].filename,
                     original->documents[id].filename) == 0);
        CHECK(strcmp(loaded->documents[id].content,
                     original->documents[id].content) == 0);
    }
    REQUIRE(index_save(loaded, "tests/run/index-reloaded.txt", error,
                       sizeof error) == 1);
    for (id = 0; id < 8; ++id) {
        REQUIRE(index_load(loaded, "tests/run/index.txt", error,
                           sizeof error) == 1);
        check_fixture_counts(loaded);
        check_apple_results(loaded, "apple");
    }
    CHECK(index_save(original, "tests/run/missing-directory/index.txt",
                     error, sizeof error) == 0);
    CHECK(error[0] != '\0');
    index_free(loaded);
    CHECK(loaded->ready == 0 && loaded->doc_count == 0);
    CHECK(loaded->documents[0].content == NULL);
    index_free(loaded); /* Repeated cleanup is safe. */
    free(loaded);
}

/* A hand-written valid file supplies an independent serialization oracle. */
static const char manual_index[] =
    "COMP1028_INDEX 1\n"
    "COUNTS 3 5 3 4\n"
    "D 0 5 18 3 2\n"
    "a.txt\n"
    "apple apple banana\n"
    "D 1 5 12 2 2\n"
    "b.txt\n"
    "apple orange\n"
    "D 2 9 0 0 0\n"
    "empty.txt\n"
    "\n"
    "T 5 2\n"
    "apple\n"
    "P 0 2\n"
    "P 1 1\n"
    "T 6 1\n"
    "banana\n"
    "P 0 1\n"
    "T 6 1\n"
    "orange\n"
    "P 1 1\n"
    "END\n";

static char *replace_first(const char *input, const char *needle,
                            const char *replacement)
{
    const char *found = strstr(input, needle);
    size_t prefix, old_length, new_length, input_length;
    char *result;
    REQUIRE(found != NULL);
    prefix = (size_t)(found - input);
    old_length = strlen(needle);
    new_length = strlen(replacement);
    input_length = strlen(input);
    result = malloc(input_length - old_length + new_length + 1);
    REQUIRE(result != NULL);
    memcpy(result, input, prefix);
    memcpy(result + prefix, replacement, new_length);
    memcpy(result + prefix + new_length, found + old_length,
           input_length - prefix - old_length + 1);
    return result;
}

static void expect_load_failure(SearchIndex *destination, const char *path)
{
    char error[256];
    char *content = destination->documents[0].content;
    Term *term = index_find_term(destination, "apple");
    unsigned long long query_number = destination->query_number;
    CHECK(index_load(destination, path, error, sizeof error) == 0);
    CHECK(error[0] != '\0');
    CHECK(destination->documents[0].content == content);
    CHECK(index_find_term(destination, "apple") == term);
    CHECK(destination->query_number == query_number);
    CHECK(strcmp(destination->documents[0].filename,
                 "a, \"first\".txt") == 0);
    check_fixture_counts(destination);
    check_apple_results(destination, "apple");
}

static void test_invalid_files(SearchIndex *destination)
{
    static const struct {
        const char *label;
        const char *needle;
        const char *replacement;
    } variants[] = {
        {"magic", "COMP1028_INDEX", "OTHER_INDEX"},
        {"version", "COMP1028_INDEX 1", "COMP1028_INDEX 2"},
        {"document limit", "COUNTS 3 5 3 4", "COUNTS 6001 5 3 4"},
        {"integer overflow", "COUNTS 3 5 3 4",
          "COUNTS 999999999999999999999999999999 5 3 4"},
        {"total mismatch", "COUNTS 3 5 3 4", "COUNTS 3 6 3 4"},
        {"postings mismatch", "COUNTS 3 5 3 4", "COUNTS 3 5 3 5"},
        {"filename limit", "D 0 5 18 3 2", "D 0 300 18 3 2"},
        {"content limit", "D 0 5 18 3 2", "D 0 5 200000 3 2"},
        {"document order", "D 1 5 12 2 2", "D 0 5 12 2 2"},
        {"document statistics", "D 0 5 18 3 2", "D 0 5 18 2 2"},
        {"term limit", "T 5 2", "T 1024 2"},
        {"empty postings", "T 5 2", "T 5 0"},
        {"out-of-range ID", "P 1 1\n", "P 3 1\n"},
        {"negative ID", "P 1 1\n", "P -1 1\n"},
        {"zero frequency", "P 0 2\n", "P 0 0\n"},
        {"frequency exceeds document total", "P 0 2\n", "P 0 4\n"},
        {"frequency overflow", "P 0 2\n", "P 0 99999999999999999999\n"},
        {"duplicate posting", "P 0 2\nP 1 1\n", "P 0 2\nP 0 1\n"},
        {"duplicate term", "orange\nP 1 1\n", "banana\nP 1 1\n"},
        {"unnormalized term", "banana\nP 0 1\n", "Banana\nP 0 1\n"},
        {"trailing data", "END\n", "END\nJUNK\n"}
    };
    char path[256];
    size_t id;
    char *bytes;
    SearchIndex *manual = new_index();
    char error[256];

    write_bytes("tests/run/manual-index.txt", manual_index,
                sizeof manual_index - 1);
    REQUIRE(index_load(manual, "tests/run/manual-index.txt", error,
                       sizeof error) == 1);
    check_fixture_counts(manual);
    check_apple_results(manual, "apple");
    CHECK(strcmp(manual->documents[0].content, "apple apple banana") == 0);
    index_free(manual);
    free(manual);

    expect_load_failure(destination, "tests/run/nonexistent-index.txt");
    write_bytes("tests/run/empty-file.txt", "", 0);
    expect_load_failure(destination, "tests/run/empty-file.txt");
    write_bytes("tests/run/truncated-index.txt", manual_index,
                (sizeof manual_index - 1) / 2);
    expect_load_failure(destination, "tests/run/truncated-index.txt");

    bytes = copy_string(manual_index);
    REQUIRE(strstr(bytes, "apple apple banana") != NULL);
    strstr(bytes, "apple apple banana")[3] = '\0';
    write_bytes("tests/run/embedded-nul.txt", bytes, sizeof manual_index - 1);
    expect_load_failure(destination, "tests/run/embedded-nul.txt");
    free(bytes);

    for (id = 0; id < sizeof variants / sizeof variants[0]; ++id) {
        bytes = replace_first(manual_index, variants[id].needle,
                               variants[id].replacement);
        snprintf(path, sizeof path, "tests/run/invalid-%zu.txt", id);
        write_bytes(path, bytes, strlen(bytes));
        printf("Checking invalid file: %s\n", variants[id].label);
        expect_load_failure(destination, path);
        free(bytes);
    }
}

static void test_empty_and_limits(void)
{
    SearchIndex *empty = new_index();
    SearchIndex *loaded = new_index();
    SearchIndex *long_word = new_index();
    char error[256];
    char *content;
    SearchResult results[1];

    CHECK(index_save(empty, "tests/run/unready-index.txt", error,
                     sizeof error) == 0);
    CHECK(statistics_export(empty, NULL, 0, NULL, "tests/run/unready-report.txt",
                           0, error, sizeof error) == 0);
    empty->ready = 1;
    REQUIRE(index_save(empty, "tests/run/empty-index.txt", error,
                       sizeof error) == 1);
    REQUIRE(index_load(loaded, "tests/run/empty-index.txt", error,
                       sizeof error) == 1);
    CHECK(loaded->ready != 0);
    CHECK(loaded->doc_count == 0 && loaded->total_terms == 0);
    CHECK(loaded->term_count == 0 && loaded->posting_count == 0);
    CHECK(index_search(loaded, "apple", results, 1) == 0);
    REQUIRE(statistics_export(loaded, NULL, 0, "", "tests/run/empty-report.csv",
                              1, error, sizeof error) == 1);

    long_word->doc_count = 1;
    content = malloc(MAX_TERM_BYTES + 9);
    REQUIRE(content != NULL);
    memset(content, 'a', MAX_TERM_BYTES + 1);
    strcpy(content + MAX_TERM_BYTES + 1, " apple");
    set_document(long_word, 0, "long-word.txt", content);
    free(content);
    REQUIRE(index_build(long_word) == 1);
    CHECK(long_word->total_terms == 1 && long_word->term_count == 1);
    CHECK(long_word->posting_count == 1);
    CHECK(index_search(long_word, "apple", results, 1) == 1);

    index_free(long_word);
    index_free(loaded);
    index_free(empty);
    free(long_word);
    free(loaded);
    free(empty);
}

int main(void)
{
    SearchIndex *index = fixture();
    test_search_and_counts(index);
    test_reports(index);
    test_round_trip(index);
    test_invalid_files(index);
    test_empty_and_limits();
    index_free(index);
    CHECK(index->doc_count == 0 && index->ready == 0);
    free(index);
    printf("Feature tests: %u checks, %u failures.\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
