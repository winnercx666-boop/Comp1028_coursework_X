#include "search_index.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
