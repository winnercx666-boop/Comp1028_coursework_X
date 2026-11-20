#ifndef SEARCH_INDEX_H
#define SEARCH_INDEX_H

#include <stddef.h>

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

#endif
