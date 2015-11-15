/**
 * Copyright (c) 2015 Simon Schoenenberger
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

#ifndef _JSON5_TOKENIZER_H_
#define _JSON5_TOKENIZER_H_

#include <stdint.h>
#include <sys/types.h>

/**
 * Defines tokenizer options
 */
typedef enum {
	/**
	 * The characters in the buffer should be stored as UTF-8 encoded string.
	 * This is the default
	 */
	JSON5_TOK_OPTION_UTF_8 = 1 << 0,
	/**
	 * The characters in the buffer should be stored as native 32-bit integers
	 */
	JSON5_TOK_OPTION_UTF_32 = 1 << 1,
} json5_tok_options;

/**
 * Defines token types returned by the tokenizer
 *
 * Some of these token types are only used internally
 */
typedef enum {
	// external
	JSON5_TOK_NONE = 0,
	JSON5_TOK_OBJ_LEFT,
	JSON5_TOK_OBJ_RIGH,
	JSON5_TOK_ARR_LEFT,
	JSON5_TOK_ARR_RIGHT,
	JSON5_TOK_COMMA,
	JSON5_TOK_COLON,
	JSON5_TOK_STRING,
	JSON5_TOK_NUMBER,
	JSON5_TOK_CONST,
	JSON5_TOK_COMMENT,
	// internal
	JSON5_TOK_ESCAPE,
	JSON5_TOK_PLUS,
	JSON5_TOK_MINUS,
	JSON5_TOK_PERIOD,
	JSON5_TOK_LINEBREAK,
	// special
	JSON5_TOK_END,
	JSON5_TOK_ERROR,
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
	char const * token;
	size_t token_len;
	json5_off offset;
	union {
		int64_t i;
		double f;
	} value;
} json5_token;

/**
 * Defines the tokenizer object
 *
 * It parses the tokens but does not validate the syntax.
 * The tokens are forwarded to `json5_parser` which validates the syntax and
 * builds the syntax tree.
 */
typedef struct {
	size_t buffer_size;
	size_t buffer_cap;
	unsigned char * buffer;
	json5_off offset;
	json5_off token_start;
	int state;
	int token_count;
	int accept_count;
} json5_tokenizer;

/**
 * Initialize a tokenizer
 *
 * Returns 0 on success or -1 if an error occurred
 */
extern int json5_tokenizer_init (json5_tokenizer * tknzr, json5_tok_options options);

/**
 * Reset a tokenizer
 *
 * It then can be used to tokenize a new JSON string. The allocated memory will
 * be preserved.
 *
 * `options` will override the options given to `json5_tokenizer_init`, unless
 * it is set to -1.
 *
 * Returns 0 on success or -1 if an error occurred
 */
extern int json5_tokenizer_reset (json5_tokenizer * tknzr, json5_tok_options options);

/**
 * Destroy a tokenizer
 */
extern void json5_tokenizer_destroy (json5_tokenizer * tknzr);

/**
 * Push single byte to the tokenizer
 *
 * The string the byte is coming from is expected to be UTF-8 encoded
 *
 * Returns 0 on success or -1 if an error occurred
 */
extern int json5_tokenizer_put_byte (json5_tokenizer * tknzr, int c);

/**
 * Push Unicode character to the tokenizer
 *
 * If the character is not in the valid Unicode range, an error is returned
 *
 * Returns 0 on success or -1 if an error occurred
 */
extern int json5_tokenizer_put_char (json5_tokenizer * tknzr, int c);

/**
 * Returns the last error message or NULL if no error is present
 */
extern char const * json5_tokenizer_error (json5_tokenizer const * tknzr);

#endif /* ! _JSON5_TOKENIZER_H_ */
