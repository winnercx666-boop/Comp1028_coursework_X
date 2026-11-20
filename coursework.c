#include "collection.h"
#include "persistence.h"
#include "statistics.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    puts("Load a collection and build an index, or load a saved index first.");
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
    puts("\nDocument Search and Information Retrieval System\n"
         "1. Load collection + build index\n"
         "2. Rebuild current index\n"
         "3. Show collection, document and index statistics\n"
         "4. Search\n"
         "5. View document\n"
         "6. Save text report\n"
         "7. Save CSV report\n"
         "8. Save index\n"
         "9. Load saved index\n"
         "0. Exit");
    printf("Choice: ");
    fflush(stdout);
}

int main(void)
{
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
        int status = read_line(input, sizeof input);
        if (status == 0)
            break;
        if (status < 0) {
            if (ferror(stdin))
                break;
            continue;
        }
        long choice;
        if (!parse_number(input, 0, 9, &choice)) {
            puts("Invalid menu choice. Enter a number from 0 to 9.");
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
        case 8:
            if (!require_index(index) || !read_path(path, sizeof path, "index.idx"))
                break;
            if (index_save(index, path, error, sizeof error))
                printf("Index saved to %s\n", path);
            else
                printf("Index not saved: %s\n", error);
            break;
        case 9:
            if (!read_path(path, sizeof path, "index.idx"))
                break;
            if (index_load(index, path, error, sizeof error)) {
                printf("Index loaded without rebuilding: %zu documents, %zu terms.\n",
                       index->doc_count, index->term_count);
                last_count = 0;
                last_query[0] = '\0';
            } else {
                printf("Index not loaded: %s\n", error);
            }
            break;
        }
    }
    int status = ferror(stdin) ? 1 : 0;
    index_free(index);
    free(index);
    free(last_results);
    return status;
}
