#ifndef STATISTICS_H
#define STATISTICS_H

#include <stddef.h>
#include <stdio.h>

#include "search_index.h"

/* Prints collection, per-document and index statistics to stream.
   The caller owns stream. An unready index produces a friendly message. */
void statistics_print(const SearchIndex *index, FILE *stream);

/* Exports statistics and optional search results. csv != 0 selects CSV;
   csv == 0 selects text. Pass NULL, 0, NULL for statistics only.
   Document IDs remain zero-based; result ranks start at one.
   Returns 1 on success, 0 on failure and sets error when space is provided. */
int statistics_export(const SearchIndex *index,
                      const SearchResult *results, size_t result_count,
                      const char *query, const char *path, int csv,
                      char *error, size_t error_size);

#endif
