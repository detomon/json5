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

#define JSON5_NUM_TOKS 2

#include <stdint.h>
#include <sys/types.h>

/**
 * Defines token types returned by the tokenizer
 *
 * Some of these token types are only used internally
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
	JSON5_TOK_NAME,
	JSON5_TOK_COMMENT,
	// internal
	JSON5_TOK_LINEBREAK,
	JSON5_TOK_ESCAPE,
	JSON5_TOK_SIGN,
	JSON5_TOK_PERIOD,
	JSON5_TOK_SPACE,
	// special
	JSON5_TOK_END,
} json5_tok_type;

/**
 * Defines token specific sub-types
 */
typedef enum {
	// external
	JSON5_SUB_TOK_NONE = 0,
	JSON5_SUB_TOK_NULL,
	JSON5_SUB_TOK_INT,
	JSON5_SUB_TOK_FLOAT,
	JSON5_SUB_TOK_BOOL,
	JSON5_SUB_TOK_NAN,
	JSON5_SUB_TOK_INFINITY,
} json5_tok_sub_type;

/**
 * Defines a token's offset inside the JSON string
 *
 * As the JSON string is expected to be UTF-8 encoded,
 * the offset is given in characters, not bytes.
 */
typedef struct {
	size_t lineno;
	size_t colno;
} json5_off;

/**
 * Defines a token returned by the tokenizer
 */
typedef struct {
	json5_tok_type type;
	json5_tok_sub_type subtype;
	uint8_t* token;
	size_t length;
	json5_off offset;
	union {
		int64_t i;
		double f;
	} value;
} json5_token;

/**
 * Used for decoding UTF-8 byte string
 */
typedef struct {
	size_t length;
	size_t count;
	unsigned value;
} json5_utf8_decoder;

/**
 * Defines the tokenizer object
 *
 * It parses the tokens but does not validate the syntax.
 * The tokens are forwarded to `json5_parser` which validates the syntax and
 * builds the syntax tree.
 */
typedef struct {
	int state;
	int aux_count;
	int token_count;
	int accept_count;
	int aux_value;
	unsigned seq_value;
	struct {
		unsigned sign:1;
		unsigned exp_sign:1;
		unsigned type:4;
		int length;
		int dec_pnt;
		int exp;
		union {
			uint64_t i;
			double f;
		} mant;
	} number;
	size_t buffer_len;
	size_t buffer_cap;
	uint8_t* buffer;
	json5_off offset;
	json5_off token_start;
	json5_utf8_decoder decoder;
	json5_token tokens[JSON5_NUM_TOKS];
} json5_tokenizer;

/**
 * Initialize a tokenizer
 *
 * Returns 0 on success or -1 if an error occurred
 */
extern int json5_tokenizer_init(json5_tokenizer* tknzr);

/**
 * Reset a tokenizer
 *
 * It then can be used to tokenize a new JSON string. The allocated memory will
 * be preserved.
 *
 * Returns 0 on success or -1 if an error occurred
 */
extern void json5_tokenizer_reset(json5_tokenizer* tknzr);

/**
 * Destroy a tokenizer
 */
extern void json5_tokenizer_destroy(json5_tokenizer* tknzr);

/**
 * Push single byte to the tokenizer
 *
 * The string the byte is coming from is expected to be UTF-8 encoded
 *
 * Returns 0 on success or -1 if an error occurred
 */
extern int json5_tokenizer_put_byte(json5_tokenizer* tknzr, int c);

/**
 * Push Unicode character to the tokenizer
 *
 * If the character is not in the valid Unicode range, an error is returned
 *
 * Returns 0 on success or -1 if an error occurred
 */
extern int json5_tokenizer_put_char(json5_tokenizer* tknzr, int c);

/**
 * Returns the last error message or NULL if no error is present
 */
extern char const* json5_tokenizer_get_error(json5_tokenizer const* tknzr);
