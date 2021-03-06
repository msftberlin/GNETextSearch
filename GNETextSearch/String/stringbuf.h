//
//  stringbuf.h
//  GNETextSearch
//
//  Created by Anthony Drendel on 11/11/15.
//  Copyright © 2015 Gone East LLC. All rights reserved.
//

#ifndef tsearch_stringbuf_h
#define tsearch_stringbuf_h

#include "GNETextSearchPublic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tsearch_stringbuf * tsearch_stringbuf_ptr;

/// Creates an empty string buffer. Returns a pointer to the string buffer if successful, otherwise NULL.
tsearch_stringbuf_ptr tsearch_stringbuf_init(void);

/// Creates a string buffer containing the specified C char array.
/// Returns a pointer to the string buffer if successful, otherwise NULL.
/// The length parameter refers to the number of chars in cString, but should not include
/// the null terminator.
tsearch_stringbuf_ptr tsearch_stringbuf_init_with_cstring(const char *cString, const size_t length);

void tsearch_stringbuf_free(const tsearch_stringbuf_ptr ptr);

/// Returns the length of the string buffer. The length does not include space for a null terminator.
size_t tsearch_stringbuf_get_len(const tsearch_stringbuf_ptr ptr);

/// Returns the char at the specified index of the string buffer.
/// Returns '\0' if the index is past the bounds of the string or if the string buffer is NULL.
char tsearch_stringbuf_get_char_at_idx(const tsearch_stringbuf_ptr ptr, const size_t index);

/// Appends the specified char to the string buffer. Returns 1 if successful, otherwise 0.
result tsearch_stringbuf_append_char(const tsearch_stringbuf_ptr ptr, const char character);

/// Appends the specified C char array into the string buffer. Returns 1 if successful, otherwise 0.
/// The length parameter refers to the number of chars in cString, but should not include
/// the null terminator.
result tsearch_stringbuf_append_cstring(const tsearch_stringbuf_ptr ptr, const char *cString, const size_t length);

/// Returns a null-terminated char representation of the mutable string's contents.
/// The returned char array must be freed by the caller.
const char * tsearch_stringbuf_copy_cstring(const tsearch_stringbuf_ptr ptr);

void tsearch_stringbuf_print(const tsearch_stringbuf_ptr ptr);

#ifdef __cplusplus
}
#endif

#endif /* tsearch_stringbuf_h */
