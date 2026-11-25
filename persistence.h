#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "search_index.h"

/* Versioned, byte-length-prefixed text format. Returns 1 on success, 0 on
   failure. error receives a readable diagnostic when space is provided. */
int index_save(const SearchIndex *index, const char *path,
               char *error, size_t error_size);

/* destination must have been initialized with index_init. Loading builds a
   temporary index; a failure leaves destination unchanged. */
int index_load(SearchIndex *destination, const char *path,
               char *error, size_t error_size);

#endif
