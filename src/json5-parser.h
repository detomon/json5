/**
 * Copyright (c) 2016 Simon Schoenenberger
 * https://github.com/detomon/json5
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <sys/types.h>
#include "json5-value.h"
#include "json5-tokenizer.h"

typedef struct {
	int state;
	json5_value * value;
} json5_parser_item;

typedef struct {
	json5_parser_item * stack;
	size_t stack_len;
	size_t stack_cap;
	json5_value value;
	json5_value error;
} json5_parser;

/**
 * Initialize a parser.
 *
 * Returns 0 on success or -1 if an error occurred.
 */
extern int json5_parser_init (json5_parser * parser);

/**
 * Reset a parser.
 *
 * It then can be used to parse new tokens. The allocated memory will
 * be preserved.
 */
extern void json5_parser_reset (json5_parser * parser);

/**
 * Destroy a parser.
 */
extern void json5_parser_destroy (json5_parser * parser);

/**
 * Parser tokens
 */
extern int json5_parser_put_tokens (json5_parser * parser, json5_token const * tokens, size_t count);

/**
 * Check if parser is finished
 *
 * Returns 1 if parser is finished or an error ocurred
 */
extern int json5_parser_is_finished (json5_parser const * parser);

/**
 * Check if parser has encountered an error
 */
extern int json5_parser_has_error (json5_parser const * parser);

/**
 * Returns the last error message or NULL if no error is present.
 */
extern json5_value const * json5_parser_get_error (json5_parser const * parser);
