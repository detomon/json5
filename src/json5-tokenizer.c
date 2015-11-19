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

#include <stdio.h>
#include <string.h>
#include "json5-tokenizer.h"
#include "unicode-table.h"

#define INIT_BUF_CAP 4096

static json5_tok_type const init_chars [256] = {
	[' ']  = JSON5_TOK_SPACE,
	['\t'] = JSON5_TOK_SPACE,
	['\n'] = JSON5_TOK_SPACE,
	['\r'] = JSON5_TOK_SPACE,
	['\f'] = JSON5_TOK_SPACE,
	['\v'] = JSON5_TOK_SPACE,
	['"']  = JSON5_TOK_STRING,
	['\''] = JSON5_TOK_STRING,
	['{']  = JSON5_TOK_OBJ_LEFT,
	['}']  = JSON5_TOK_OBJ_RIGHT,
	['[']  = JSON5_TOK_ARR_LEFT,
	[']']  = JSON5_TOK_ARR_RIGHT,
	['.']  = JSON5_TOK_PERIOD,
	[',']  = JSON5_TOK_COMMA,
	[':']  = JSON5_TOK_COLON,
	['+']  = JSON5_TOK_PLUS,
	['-']  = JSON5_TOK_MINUS,
	['\\'] = JSON5_TOK_ESCAPE,
	['/']  = JSON5_TOK_COMMENT,
	['0']  = JSON5_TOK_NUMBER,
	['1']  = JSON5_TOK_NUMBER,
	['2']  = JSON5_TOK_NUMBER,
	['3']  = JSON5_TOK_NUMBER,
	['4']  = JSON5_TOK_NUMBER,
	['5']  = JSON5_TOK_NUMBER,
	['6']  = JSON5_TOK_NUMBER,
	['7']  = JSON5_TOK_NUMBER,
	['8']  = JSON5_TOK_NUMBER,
	['9']  = JSON5_TOK_NUMBER,
	['A']  = JSON5_TOK_NAME,
	['B']  = JSON5_TOK_NAME,
	['C']  = JSON5_TOK_NAME,
	['D']  = JSON5_TOK_NAME,
	['E']  = JSON5_TOK_NAME,
	['F']  = JSON5_TOK_NAME,
	['G']  = JSON5_TOK_NAME,
	['H']  = JSON5_TOK_NAME,
	['I']  = JSON5_TOK_NAME,
	['J']  = JSON5_TOK_NAME,
	['K']  = JSON5_TOK_NAME,
	['L']  = JSON5_TOK_NAME,
	['M']  = JSON5_TOK_NAME,
	['N']  = JSON5_TOK_NAME,
	['O']  = JSON5_TOK_NAME,
	['P']  = JSON5_TOK_NAME,
	['Q']  = JSON5_TOK_NAME,
	['R']  = JSON5_TOK_NAME,
	['S']  = JSON5_TOK_NAME,
	['T']  = JSON5_TOK_NAME,
	['U']  = JSON5_TOK_NAME,
	['V']  = JSON5_TOK_NAME,
	['W']  = JSON5_TOK_NAME,
	['X']  = JSON5_TOK_NAME,
	['Y']  = JSON5_TOK_NAME,
	['Z']  = JSON5_TOK_NAME,
	['a']  = JSON5_TOK_NAME,
	['b']  = JSON5_TOK_NAME,
	['c']  = JSON5_TOK_NAME,
	['d']  = JSON5_TOK_NAME,
	['e']  = JSON5_TOK_NAME,
	['f']  = JSON5_TOK_NAME,
	['g']  = JSON5_TOK_NAME,
	['h']  = JSON5_TOK_NAME,
	['i']  = JSON5_TOK_NAME,
	['j']  = JSON5_TOK_NAME,
	['k']  = JSON5_TOK_NAME,
	['l']  = JSON5_TOK_NAME,
	['m']  = JSON5_TOK_NAME,
	['n']  = JSON5_TOK_NAME,
	['o']  = JSON5_TOK_NAME,
	['p']  = JSON5_TOK_NAME,
	['q']  = JSON5_TOK_NAME,
	['r']  = JSON5_TOK_NAME,
	['s']  = JSON5_TOK_NAME,
	['t']  = JSON5_TOK_NAME,
	['u']  = JSON5_TOK_NAME,
	['v']  = JSON5_TOK_NAME,
	['w']  = JSON5_TOK_NAME,
	['x']  = JSON5_TOK_NAME,
	['y']  = JSON5_TOK_NAME,
	['z']  = JSON5_TOK_NAME,
};

/**
 * Defines tokenenizer states
 */
typedef enum {
	JSON5_TOK_STATE_NONE = 0,
	JSON5_TOK_STATE_SPACE,
	JSON5_TOK_STATE_STRING,
	JSON5_TOK_STATE_NUMBER,
	JSON5_TOK_STATE_COMMENT,
	JSON5_TOK_STATE_ERROR,
} json5_tok_state;

static int json5_utf8_decode (json5_utf8_decoder * decoder, unsigned c, unsigned * value)
{
	int byte_count = 0;

	if (c > 0x7F && decoder -> count == 0) {
		if ((c & 0xE0) == 0xC0) {
			c &= 0x1F;
			byte_count = 1;
		}
		else if ((c & 0xF0) == 0xE0) {
			c &= 0xF;
			byte_count = 2;
		}
		else if ((c & 0xF8) == 0xF0) {
			c &= 0x7;
			byte_count = 3;
		}
		else { // invalid byte
			return -1;
		}

		decoder -> value  = c;
		decoder -> count  = byte_count;
		decoder -> length = 0;

		return 0;
	}

	if (decoder -> count) {
		if ((c & 0xC0) != 0x80) {
			return -1;
		}

		c &= 0x3F;
		decoder -> value = (decoder -> value << 6) | c;
		decoder -> length ++;

		if (decoder -> length >= decoder -> count) {
			*value = decoder -> value;
			decoder -> count = 0;

			return 1;
		}
	}
	else {
		*value = c;

		return 1;
	}

	return 0;
}

int json5_tokenizer_init (json5_tokenizer * tknzr)
{
	memset (tknzr, 0, sizeof (*tknzr));

	tknzr -> buffer_cap = INIT_BUF_CAP;
	tknzr -> buffer = malloc (tknzr -> buffer_cap);

	if (!tknzr -> buffer) {
		return -1;
	}

	return 0;
}

int json5_tokenizer_reset (json5_tokenizer * tknzr)
{
	unsigned char * buffer = tknzr -> buffer;
	size_t buffer_cap = tknzr -> buffer_cap;

	memset (tknzr, 0, sizeof (*tknzr));
	tknzr -> buffer = buffer;
	tknzr -> buffer_cap = buffer_cap;
}

void json5_tokenizer_destroy (json5_tokenizer * tknzr)
{
	if (tknzr -> buffer) {
		free (tknzr -> buffer);
	}

	memset (tknzr, 0, sizeof (*tknzr));
}

int json5_tokenizer_put_byte (json5_tokenizer * tknzr, unsigned c)
{
	unsigned value;
	int status;

	status = json5_utf8_decode (&tknzr -> decoder, c, &value);

	if (status < 0) {
		snprintf (tknzr -> buffer, tknzr -> buffer_cap,
			"Invalid byte '%02x' on line %lld",
			c, tknzr -> offset.lineno + 1);
		return -1;
	}

	if (status == 1) {
		return json5_tokenizer_put_char (tknzr, value);
	}

	return 0;
}

int json5_tokenizer_put_char (json5_tokenizer * tknzr, unsigned c)
{
	printf ("> %x\n", c);
}

char const * json5_tokenizer_get_error (json5_tokenizer const * tknzr)
{
	if (tknzr -> state == JSON5_TOK_STATE_ERROR) {
		return tknzr -> buffer;
	}

	return NULL;
}
