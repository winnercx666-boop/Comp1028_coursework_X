#ifndef COLLECTION_H
#define COLLECTION_H

#include "search_index.h"

/* Accepts a collection list, or a folder when ENABLE_DIRECTORY_INPUT is set.
   Failure leaves the destination unchanged. Index building is a separate step. */
int collection_load(SearchIndex *destination, const char *path,
                     char *error, size_t error_size);

#endif
