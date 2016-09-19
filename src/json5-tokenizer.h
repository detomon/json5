/*
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

/**
 * Defines token types returned by the tokenizer.
 *
 * Some of these token types are only used internally.
 */
typedef enum {
	// external
	JSON5_TOK_OTHER = 0,
	JSON5_TOK_OBJ_OPEN,
	JSON5_TOK_OBJ_CLOSE,
	JSON5_TOK_ARR_OPEN,
	JSON5_TOK_ARR_CLOSE,
	JSON5_TOK_COMMA,
	JSON5_TOK_COLON,
	JSON5_TOK_STRING,
	JSON5_TOK_NUMBER,
	JSON5_TOK_NUMBER_FLOAT,
	JSON5_TOK_NUMBER_BOOL,
	JSON5_TOK_NAME,
	JSON5_TOK_INFINITY,
	JSON5_TOK_NAN,
	JSON5_TOK_NULL,
	// internal
	JSON5_TOK_NAME_OTHER,
	JSON5_TOK_NAME_SIGN,
	JSON5_TOK_COMMENT,
	JSON5_TOK_COMMENT2,
	JSON5_TOK_LINEBREAK,
	JSON5_TOK_ESCAPE,
	JSON5_TOK_SIGN,
	JSON5_TOK_PERIOD,
	JSON5_TOK_SPACE,
	// special
	JSON5_TOK_END,
} json5_tok_type;

/**
 * Defines a token's offset inside the JSON string.
 *
 * As the JSON string is expected to be UTF-8 encoded,
 * the offset is given in characters, not bytes.
 */
typedef struct {
	int lineno;
	int colno;
} json5_off;

/**
 * Defines a token returned by the tokenizer.
 */
typedef struct {
	json5_tok_type type;
	uint8_t * token;
	size_t length;
	json5_off offset;
	union {
		int64_t i;
		double f;
	} value;
} json5_token;

/**
 * Defines the tokenizer object.
 *
 * It is used to parses the tokens but does not validate the syntax.
 * The tokens are forwarded to `json5_parser` which validates the syntax and
 * builds the syntax tree.
 */
typedef struct {
	int state;
	int aux_count;
	int aux_value;
	unsigned seq_value;
	struct {
		unsigned sign:1;
		unsigned exp_sign:1;
		unsigned type:4;
		uint16_t length;
		uint16_t exp_len;
		int dec_pnt;
		int exp;
		union {
			uint64_t u;
			int64_t i;
			double f;
		} mant;
	} number;
	size_t buffer_len;
	size_t buffer_cap;
	uint8_t * buffer;
	json5_off offset;
	struct {
		uint8_t length;
		uint8_t count;
		unsigned value;
		uint8_t chars [4];
	} mb_char;
	json5_token token;
} json5_tokenizer;

/**
 * A callback function definition used to receive parsed tokens by the tokenizer.
 */
typedef int (*json5_put_token_func) (json5_token const * token, void * arg);

/**
 * Initialize a tokenizer.
 *
 * Returns 0 on success or -1 if an error occurred.
 */
extern int json5_tokenizer_init (json5_tokenizer * tknzr);

/**
 * Reset a tokenizer.
 *
 * It then can be used to tokenize a new JSON string. The allocated memory will
 * be preserved.
 */
extern void json5_tokenizer_reset (json5_tokenizer * tknzr);

/**
 * Destroy a tokenizer.
 */
extern void json5_tokenizer_destroy (json5_tokenizer * tknzr);

/**
 * Push Unicode characters to the tokenizer.
 *
 * Returns 0 on success or -1 if an error occurred.
 */
extern int json5_tokenizer_put_chars (json5_tokenizer * tknzr, uint8_t const * chars, size_t size, json5_put_token_func put_token, void * arg);

/**
 * Returns the last error message or NULL if no error is present.
 */
extern char const * json5_tokenizer_get_error (json5_tokenizer const * tknzr);
