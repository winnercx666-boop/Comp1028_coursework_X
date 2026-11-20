#include "collection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Existing group folder input is an optional platform adapter.
   The collection-list path uses only the standard C library. */
#ifdef ENABLE_DIRECTORY_INPUT
#include <dirent.h>
#endif

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
