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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "json5-tokenizer.h"
#include "unicode-table.h"

#if HAVE_FLOAT_H
#include <float.h>
#else
#define DBL_MAX_10_EXP 307
#define DBL_MIN_10_EXP -308
#endif

#define INIT_BUF_CAP 4096
#define BUF_MIN_FREE_SPACE 256
#define HEX_CHAR_FLAG 16
#define HEX_VAL_MASK (HEX_CHAR_FLAG - 1)

/**
 * Defines number types
 */
enum {
	JSON5_NUM_INT,
	JSON5_NUM_HEX,
	JSON5_NUM_FLOAT,
};

/**
 * Defines tokenizer states
 */
typedef enum {
	JSON5_STATE_NONE = 0,
	JSON5_STATE_SPACE,
	JSON5_STATE_NAME,
	JSON5_STATE_NAME_SIGN,
	JSON5_STATE_STRING,
	JSON5_STATE_STRING_BEGIN,   // '"' or "'"
	JSON5_STATE_STRING_ESCAPE,  // '\'
	JSON5_STATE_STRING_HEXCHAR,
	JSON5_STATE_STRING_HEXCHAR_BEGIN, // 'x', 'u'
	JSON5_STATE_STRING_HEXCHAR_SURR, //
	JSON5_STATE_STRING_HEXCHAR_SURR_SEQ, // '\'
	JSON5_STATE_STRING_HEXCHAR_SURR_ESCAPE, // 'u'
	JSON5_STATE_STRING_HEXCHAR_SURR_BEGIN,
	JSON5_STATE_STRING_MULTILINE,
	JSON5_STATE_STRING_MULTILINE_END,
	JSON5_STATE_NUMBER,
	JSON5_STATE_NUMBER_SIGN,
	JSON5_STATE_NUMBER_START,
	JSON5_STATE_NUMBER_FRAC,
	JSON5_STATE_NUMBER_PERIOD,
	JSON5_STATE_NUMBER_EXP,
	JSON5_STATE_NUMBER_EXP_SIGN,
	JSON5_STATE_NUMBER_EXP_START,
	JSON5_STATE_NUMBER_HEX,
	JSON5_STATE_NUMBER_HEX_BEGIN,
	JSON5_STATE_NUMBER_DONE,
	JSON5_STATE_COMMENT,     // first '/'
	JSON5_STATE_COMMENT_ML,  // multiline comment
	JSON5_STATE_COMMENT_ML2, // ending '*'
	JSON5_STATE_COMMENT_SL,  // single line ocmment
	JSON5_STATE_END,
	JSON5_STATE_ERROR,
} json5_tok_state;

/**
 * Defines character lookup entry
 */
typedef struct {
	uint8_t type;
	uint8_t hex;
	uint8_t seq;
} json5_char;

/**
 * Defines char types and attributes
 */
static json5_char const char_types [128] = {
	[' ']  = {.type = JSON5_TOK_SPACE    },
	['\t'] = {.type = JSON5_TOK_SPACE    },
	['\n'] = {.type = JSON5_TOK_LINEBREAK},
	['\r'] = {.type = JSON5_TOK_LINEBREAK},
	['\f'] = {.type = JSON5_TOK_SPACE    },
	['\v'] = {.type = JSON5_TOK_SPACE    },
	['"']  = {.type = JSON5_TOK_STRING   },
	['\''] = {.type = JSON5_TOK_STRING   },
	['{']  = {.type = JSON5_TOK_OBJ_OPEN },
	['}']  = {.type = JSON5_TOK_OBJ_CLOSE},
	['[']  = {.type = JSON5_TOK_ARR_OPEN },
	[']']  = {.type = JSON5_TOK_ARR_CLOSE},
	['.']  = {.type = JSON5_TOK_PERIOD   },
	[',']  = {.type = JSON5_TOK_COMMA    },
	[':']  = {.type = JSON5_TOK_COLON    },
	['+']  = {.type = JSON5_TOK_SIGN     },
	['-']  = {.type = JSON5_TOK_SIGN     },
	['\\'] = {.type = JSON5_TOK_ESCAPE   },
	['/']  = {.type = JSON5_TOK_COMMENT  },
	['*']  = {.type = JSON5_TOK_COMMENT2 },
	['0']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 0},
	['1']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 1},
	['2']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 2},
	['3']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 3},
	['4']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 4},
	['5']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 5},
	['6']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 6},
	['7']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 7},
	['8']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 8},
	['9']  = {.type = JSON5_TOK_NUMBER   , .hex = HEX_CHAR_FLAG | 9},
	['A']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 10},
	['B']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 11},
	['C']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 12},
	['D']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 13},
	['E']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 14},
	['F']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 15},
	['G']  = {.type = JSON5_TOK_NAME     },
	['H']  = {.type = JSON5_TOK_NAME     },
	['I']  = {.type = JSON5_TOK_NAME     },
	['J']  = {.type = JSON5_TOK_NAME     },
	['K']  = {.type = JSON5_TOK_NAME     },
	['L']  = {.type = JSON5_TOK_NAME     },
	['M']  = {.type = JSON5_TOK_NAME     },
	['N']  = {.type = JSON5_TOK_NAME     },
	['O']  = {.type = JSON5_TOK_NAME     },
	['P']  = {.type = JSON5_TOK_NAME     },
	['Q']  = {.type = JSON5_TOK_NAME     },
	['R']  = {.type = JSON5_TOK_NAME     },
	['S']  = {.type = JSON5_TOK_NAME     },
	['T']  = {.type = JSON5_TOK_NAME     },
	['U']  = {.type = JSON5_TOK_NAME     },
	['V']  = {.type = JSON5_TOK_NAME     },
	['W']  = {.type = JSON5_TOK_NAME     },
	['X']  = {.type = JSON5_TOK_NAME     },
	['Y']  = {.type = JSON5_TOK_NAME     },
	['Z']  = {.type = JSON5_TOK_NAME     },
	['a']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 10},
	['b']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 11, .seq = '\b'},
	['c']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 12},
	['d']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 13},
	['e']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 14},
	['f']  = {.type = JSON5_TOK_NAME     , .hex = HEX_CHAR_FLAG | 15, .seq = '\f'},
	['g']  = {.type = JSON5_TOK_NAME     },
	['h']  = {.type = JSON5_TOK_NAME     },
	['i']  = {.type = JSON5_TOK_NAME     },
	['j']  = {.type = JSON5_TOK_NAME     },
	['k']  = {.type = JSON5_TOK_NAME     },
	['l']  = {.type = JSON5_TOK_NAME     },
	['m']  = {.type = JSON5_TOK_NAME     },
	['n']  = {.type = JSON5_TOK_NAME     , .seq = '\n'},
	['o']  = {.type = JSON5_TOK_NAME     },
	['p']  = {.type = JSON5_TOK_NAME     },
	['q']  = {.type = JSON5_TOK_NAME     },
	['r']  = {.type = JSON5_TOK_NAME     , .seq = '\r'},
	['s']  = {.type = JSON5_TOK_NAME     },
	['t']  = {.type = JSON5_TOK_NAME     , .seq = '\t'},
	['u']  = {.type = JSON5_TOK_NAME     , .seq = 'u'},
	['v']  = {.type = JSON5_TOK_NAME     , .seq = '\v'},
	['w']  = {.type = JSON5_TOK_NAME     },
	['x']  = {.type = JSON5_TOK_NAME     , .seq = 'x'},
	['y']  = {.type = JSON5_TOK_NAME     },
	['z']  = {.type = JSON5_TOK_NAME     },
	['_']  = {.type = JSON5_TOK_NAME     },
	['$']  = {.type = JSON5_TOK_NAME     },
};

static void json5_tokenizer_set_error (json5_tokenizer * tknzr, char const * msg, ...) {
	va_list args;

	va_start (args, msg);
	vsnprintf ((void *) tknzr -> buffer, tknzr -> buffer_cap, msg, args);
	va_end (args);
}

/**
 * Relocate data of tokens to new allocated buffer
 */
static void json5_tokenizer_relocate_token_data (json5_tokenizer * tknzr, uint8_t * new_buf) {
	json5_token * token;

	token = &tknzr -> token;
	token -> token = new_buf + (token -> token - tknzr -> buffer);
}

static int json5_tokenizer_ensure_buffer_space (json5_tokenizer * tknzr, size_t size) {
	if (tknzr -> buffer_len + size + BUF_MIN_FREE_SPACE >= tknzr -> buffer_cap) {
		size_t new_cap = tknzr -> buffer_cap * 2;
		uint8_t * new_buf = realloc (tknzr -> buffer, new_cap);

		if (!new_buf) {
			return -1;
		}

		json5_tokenizer_relocate_token_data (tknzr, new_buf);

		tknzr -> buffer = new_buf;
		tknzr -> buffer_cap = new_cap;
	}

	return 0;
}

static void json5_tokenizer_put_char (json5_tokenizer * tknzr, int c) {
	tknzr -> buffer [tknzr -> buffer_len ++] = c;
}

static void json5_tokenizer_put_mb_char (json5_tokenizer * tknzr, unsigned c) {
	redo:

	if (c < 0x80) {
		tknzr -> buffer [tknzr -> buffer_len ++] = c;
	}
	else if (c < 0x800) {
		tknzr -> buffer [tknzr -> buffer_len ++] = 0xC0 | (c >> 6);
		tknzr -> buffer [tknzr -> buffer_len ++] = 0x80 | (c & 0x3F);
	}
	else if (c < 0x10000) {
		tknzr -> buffer [tknzr -> buffer_len ++] = 0xE0 | (c >> 12);
		tknzr -> buffer [tknzr -> buffer_len ++] = 0x80 | ((c >> 6) & 0x3F);
		tknzr -> buffer [tknzr -> buffer_len ++] = 0x80 | (c & 0x3F);
	}
	else if (c <= JSON5_UT_MAX_VALUE) {
		tknzr -> buffer [tknzr -> buffer_len ++] = 0xF0 | (c >> 18);
		tknzr -> buffer [tknzr -> buffer_len ++] = 0x80 | ((c >> 12) & 0x3F);
		tknzr -> buffer [tknzr -> buffer_len ++] = 0x80 | ((c >> 6) & 0x3F);
		tknzr -> buffer [tknzr -> buffer_len ++] = 0x80 | (c & 0x3F);
	}
	else { // invalid glyph
		c = 0xFFFD;
		goto redo;
	}
}

static void json5_tokenizer_put_mb_chars (json5_tokenizer * tknzr) {
	memcpy (&tknzr -> buffer [tknzr -> buffer_len], tknzr -> mb_char.chars, tknzr -> mb_char.length);
	tknzr -> buffer_len += tknzr -> mb_char.length;
	tknzr -> mb_char.length = 0;
}

static void json5_tokenizer_end_buffer (json5_tokenizer * tknzr) {
	// terminate current buffer segment
	tknzr -> buffer [tknzr -> buffer_len ++] = '\0';
	// reset buffer
	tknzr -> buffer_len = 0;
}

int json5_tokenizer_init (json5_tokenizer * tknzr) {
	memset (tknzr, 0, sizeof (*tknzr));

	tknzr -> buffer_cap = INIT_BUF_CAP;
	tknzr -> buffer = malloc (tknzr -> buffer_cap);

	if (!tknzr -> buffer) {
		return -1;
	}

	return 0;
}

void json5_tokenizer_reset (json5_tokenizer * tknzr) {
	uint8_t * buffer = tknzr -> buffer;
	size_t buffer_cap = tknzr -> buffer_cap;

	memset (tknzr, 0, sizeof (*tknzr));
	tknzr -> buffer = buffer;
	tknzr -> buffer_cap = buffer_cap;
}

void json5_tokenizer_destroy (json5_tokenizer * tknzr) {
	if (tknzr -> buffer) {
		free (tknzr -> buffer);
	}

	memset (tknzr, 0, sizeof (*tknzr));
}

static void json5_tokenizer_conv_number_float (json5_tokenizer * tknzr) {
	if (tknzr -> number.type != JSON5_NUM_FLOAT) {
		tknzr -> number.mant.f = tknzr -> number.mant.i;
		tknzr -> number.type = JSON5_NUM_FLOAT;
	}
}

static void json5_tokenizer_number_add_digit (json5_tokenizer * tknzr, int value) {
	if (tknzr -> number.type != JSON5_NUM_FLOAT) {
		if (tknzr -> number.mant.u > (uint64_t) INT64_MAX / 10 + 1) {
			json5_tokenizer_conv_number_float (tknzr);
		}
	}

	if (tknzr -> number.type != JSON5_NUM_FLOAT) {
		tknzr -> number.mant.u = 10 * tknzr -> number.mant.u;

		if (tknzr -> number.sign && tknzr -> number.mant.u > (uint64_t) INT64_MAX - value + 1) {
			json5_tokenizer_conv_number_float (tknzr);
			tknzr -> number.mant.f += value;
		}
		else if (!tknzr -> number.sign && tknzr -> number.mant.u > INT64_MAX - value) {
			json5_tokenizer_conv_number_float (tknzr);
			tknzr -> number.mant.f += value;
		}
		else {
			tknzr -> number.mant.u += value;
		}
	}
	else {
		tknzr -> number.mant.f = 10.0 * tknzr -> number.mant.f + value;
	}

	tknzr -> number.length ++;
}


static void json5_tokenizer_number_add_hex_digit (json5_tokenizer * tknzr, int value) {
	if (tknzr -> number.type != JSON5_NUM_FLOAT) {
		if (tknzr -> number.mant.u > (uint64_t) INT64_MAX / 16 + 1) {
			json5_tokenizer_conv_number_float (tknzr);
		}
	}

	if (tknzr -> number.type != JSON5_NUM_FLOAT) {
		tknzr -> number.mant.u = 16 * tknzr -> number.mant.u;

		if (tknzr -> number.sign && tknzr -> number.mant.u > (uint64_t) INT64_MAX - value + 1) {
			json5_tokenizer_conv_number_float (tknzr);
			tknzr -> number.mant.f += value;
		}
		else if (!tknzr -> number.sign && tknzr -> number.mant.u > INT64_MAX - value) {
			json5_tokenizer_conv_number_float (tknzr);
			tknzr -> number.mant.f += value;
		}
		else {
			tknzr -> number.mant.u += value;
			tknzr -> number.length ++;
		}
	}
	else {
		tknzr -> number.mant.f = 16.0 * tknzr -> number.mant.f + value;
	}
}

static void json5_tokenizer_exp_add_digit (json5_tokenizer * tknzr, int value) {
	// ignore larger values
	if (tknzr -> number.exp < DBL_MAX_10_EXP) {
		tknzr -> number.exp = 10 * tknzr -> number.exp + value;
		tknzr -> number.exp_len ++;
	}
}

static void json5_number_init (json5_tokenizer * tknzr)
{
	memset (&tknzr -> number, 0, sizeof (tknzr -> number));
	tknzr -> number.dec_pnt = -1;
}

static void json5_tokenizer_number_end (json5_tokenizer * tknzr) {
	if (tknzr -> number.type == JSON5_NUM_FLOAT) {
		double d = 10.0;
		double e = 1.0;
		int n = tknzr -> number.exp;

		if (tknzr -> number.exp_sign) {
			n = -n;
		}

		if (tknzr -> number.dec_pnt >= 0) {
			n -= tknzr -> number.length - tknzr -> number.dec_pnt;
		}

		if (n < 0) {
			tknzr -> number.exp_sign = 1;
			n = -n;
		}

		while (n) {
			if (n & 1) {
				e *= d;
			}

			n >>= 1;
			d *= d;
		}

		if (tknzr -> number.sign) {
			tknzr -> number.mant.f = -tknzr -> number.mant.f;
		}

		if (tknzr -> number.exp_sign) {
			tknzr -> number.mant.f /= e;
		}
		else {
			tknzr -> number.mant.f *= e;
		}
	}
	else {
		if (tknzr -> number.sign) {
			tknzr -> number.mant.i = -tknzr -> number.mant.i;
		}
	}
}

static int json5_tokenizer_put_chars_chunk (json5_tokenizer * tknzr, uint8_t const * chars, size_t size, json5_put_token_func put_token, void * arg) {
	int c = 0;
	int state = 0;
	int value = 0;
	json5_off offset;
	json5_tok_type char_type = 0;
	json5_token * token;
	json5_ut_info const * info = NULL;
	uint8_t const * end = &chars [size];

	if (tknzr -> state >= JSON5_STATE_END) {
		return 0;
	}

	state = tknzr -> state;
	offset = tknzr -> offset;

	// ensure buffer space for worst case
	if (json5_tokenizer_ensure_buffer_space (tknzr, size * 2) != 0) {
		goto alloc_error;
	}

	do {
		if (tknzr -> mb_char.count) {
			if (size == 0) {
				goto unexpected_char;
			}
			else if (chars >= end) {
				break;
			}
			else {
				c = *chars ++;
			}

			if ((c & 0xC0) != 0x80) {
				goto invalid_byte;
			}

			tknzr -> mb_char.chars [tknzr -> mb_char.length ++] = c;
			value = c & 0x3F;
			tknzr -> mb_char.value = (tknzr -> mb_char.value << 6) | value;

			if (-- tknzr -> mb_char.count == 0) {
				c = tknzr -> mb_char.value;
				goto handle_char;
			}
			else {
				continue;
			}
		}
		else if (chars < end) {
			c = *chars ++;

			if (c >= 128) {
				if ((c & 0xE0) == 0xC0) {
					value = c & 0x1F;
					tknzr -> mb_char.count = 1;
				}
				else if ((c & 0xF0) == 0xE0) {
					value = c & 0xF;
					tknzr -> mb_char.count = 2;
				}
				else if ((c & 0xF8) == 0xF0) {
					value = c & 0x7;
					tknzr -> mb_char.count = 3;
				}
				else {
					goto invalid_byte;
				}

				tknzr -> mb_char.length = 0;
				tknzr -> mb_char.value = value;
				tknzr -> mb_char.chars [tknzr -> mb_char.length ++] = c;

				continue;
			}
		}
		else if (size == 0) {
			c = -1;
		}
		else {
			// need new data
			break;
		}

		handle_char:

		if (c < 0) {
			char_type = JSON5_TOK_END;
		}
		else if (c < 128) {
			char_type = char_types [c].type;
		}
		else {
			char_type = JSON5_TOK_OTHER;
			info = json5_ut_lookup_glyph (c);

			switch (info -> category) {
				case JSON5_UT_CATEGORY_LETTER_UPPERCASE:
				case JSON5_UT_CATEGORY_LETTER_LOWERCASE:
				case JSON5_UT_CATEGORY_LETTER_TITLECASE:
				case JSON5_UT_CATEGORY_LETTER_MODIFIER:
				case JSON5_UT_CATEGORY_LETTER_OTHER:
				case JSON5_UT_CATEGORY_NUMBER_LETTER: {
					char_type = JSON5_TOK_NAME;
					break;
				}
				case JSON5_UT_CATEGORY_NUMBER_DECIMAL_DIGIT:
				case JSON5_UT_CATEGORY_MARK_NONSPACING:
				case JSON5_UT_CATEGORY_MARK_SPACING_COMBINING: {
					char_type = JSON5_TOK_NAME_OTHER;
					break;
				}
				case JSON5_UT_CATEGORY_SEPARATOR_PARAGRAPH: {
					char_type = JSON5_TOK_LINEBREAK;
					break;
				}
				case JSON5_UT_CATEGORY_SEPARATOR_SPACE: {
					char_type = JSON5_TOK_SPACE;
					break;
				}
			}
		}

		if (char_type == JSON5_TOK_LINEBREAK) {
			offset.lineno ++;
			offset.colno = 0;
		}
		else {
			offset.colno ++;
		}

		int again;
		int accept;

		do {
			again = 0;
			accept = 0;

			// tokenizer states
			switch (state) {
				case JSON5_STATE_NONE: {
					int capture = 1;

					switch (char_type) {
						case JSON5_TOK_SPACE:
						case JSON5_TOK_LINEBREAK: {
							state = JSON5_STATE_SPACE;
							capture = 0;
							break;
						}
						case JSON5_TOK_STRING: {
							state = JSON5_STATE_STRING_BEGIN;
							tknzr -> aux_value = c;
							break;
						}
						case JSON5_TOK_OBJ_OPEN: {
							char_type = JSON5_TOK_OBJ_OPEN;
							accept = 1;
							break;
						}
						case JSON5_TOK_OBJ_CLOSE: {
							char_type = JSON5_TOK_OBJ_CLOSE;
							accept = 1;
							break;
						}
						case JSON5_TOK_ARR_OPEN: {
							char_type = JSON5_TOK_ARR_OPEN;
							accept = 1;
							break;
						}
						case JSON5_TOK_ARR_CLOSE: {
							char_type = JSON5_TOK_ARR_CLOSE;
							accept = 1;
							break;
						}
						case JSON5_TOK_SIGN: {
							state = JSON5_STATE_NUMBER_SIGN;
							char_type = JSON5_TOK_NUMBER;
							json5_number_init (tknzr);
							break;
						}
						case JSON5_TOK_NUMBER: {
							state = JSON5_STATE_NUMBER;
							json5_number_init (tknzr);
							value = c - '0';
							break;
						}
						case JSON5_TOK_PERIOD: {
							state = JSON5_STATE_NUMBER_PERIOD;
							char_type = JSON5_TOK_NUMBER;
							json5_number_init (tknzr);
							break;
						}
						case JSON5_TOK_COMMA: {
							accept = 1;
							break;
						}
						case JSON5_TOK_COLON: {
							accept = 1;
							break;
						}
						case JSON5_TOK_NAME: {
							state = JSON5_STATE_NAME;
							break;
						}
						case JSON5_TOK_COMMENT: {
							state = JSON5_STATE_COMMENT;
							capture = 0;
							break;
						}
						case JSON5_TOK_END: {
							state = JSON5_STATE_END;
							accept = 1;
							break;
						}
						default: {
							goto unexpected_char;
							break;
						}
					}

					if (capture) {
						token = &tknzr -> token;
						token -> type = char_type;
						token -> token = &tknzr -> buffer [tknzr -> buffer_len];
						token -> offset = offset;
					}

					break;
				}
				case JSON5_STATE_SPACE: {
					switch (char_type) {
						case JSON5_TOK_SPACE: {
							break;
						}
						default: {
							state = JSON5_STATE_NONE;
							again = 1;
						}
					}

					break;
				}
				case JSON5_STATE_NAME: {
					switch (char_type) {
						case JSON5_TOK_NAME:
						case JSON5_TOK_NAME_OTHER:
						case JSON5_TOK_NUMBER: {
							break;
						}
						default: {
							state = JSON5_STATE_NONE;
							again = 1;
							accept = 1;
							break;
						}
					}

					break;
				}
				case JSON5_STATE_NAME_SIGN: {
					switch (char_type) {
						case JSON5_TOK_NAME:
							break;
						default: {
							goto unexpected_char;
							break;
						}
					}

					state = JSON5_STATE_NAME;
					break;
				}
				case JSON5_STATE_STRING:
				case JSON5_STATE_STRING_BEGIN: {
					switch (char_type) {
						case JSON5_TOK_ESCAPE: {
							state = JSON5_STATE_STRING_ESCAPE;
							break;
						}
						case JSON5_TOK_STRING: {
							if (c == tknzr -> aux_value) {
								state = JSON5_STATE_NONE;
								accept = 1;
							}
							break;
						}
						case JSON5_TOK_END: {
							goto unexpected_end_starting;
							break;
						}
						default: {
							state = JSON5_STATE_STRING;
							break;
						}
					}

					break;
				}
				case JSON5_STATE_STRING_ESCAPE: {
					switch (char_type) {
						case JSON5_TOK_END: {
							goto unexpected_end_starting;
							break;
						}
						case JSON5_TOK_SPACE:
						case JSON5_TOK_LINEBREAK: {
							state = JSON5_STATE_STRING_MULTILINE;
							again = 1;
							break;
						}
						default: {
							state = JSON5_STATE_STRING;

							if (c < 128) {
								if (char_types [c].seq) {
									c = char_types [c].seq;

									switch (c) {
										case 'u': {
											state = JSON5_STATE_STRING_HEXCHAR_BEGIN;
											tknzr -> aux_count = 4;
											tknzr -> seq_value = 0;
											break;
										}
										case 'x': {
											state = JSON5_STATE_STRING_HEXCHAR_BEGIN;
											tknzr -> aux_count = 2;
											tknzr -> seq_value = 0;
											break;
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
				case JSON5_STATE_STRING_MULTILINE: {
					switch (char_type) {
						case JSON5_TOK_SPACE: {
							break;
						}
						case JSON5_TOK_LINEBREAK: {
							// read until linebreak, but ignore '\r'
							if (c != '\r') {
								state = JSON5_STATE_STRING_MULTILINE_END;
							}
							break;
						}
						default: {
							goto unexpected_char;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_STRING_HEXCHAR:
				case JSON5_STATE_STRING_HEXCHAR_BEGIN: {
					if (c < 128) {
						if (char_types [c].hex) {
							value = char_types [c].hex & HEX_VAL_MASK;
						}
						else {
							goto invalid_hex_char;
						}
					}
					else {
						goto invalid_hex_char;
					}

					state = JSON5_STATE_STRING_HEXCHAR;
					break;
				}
				case JSON5_STATE_STRING_HEXCHAR_SURR_SEQ: {
					switch (char_type) {
						case JSON5_TOK_ESCAPE: {
							state = JSON5_STATE_STRING_HEXCHAR_SURR_ESCAPE;
							break;
						}
						default: {
							goto expected_low_surrogate;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_STRING_HEXCHAR_SURR_ESCAPE: {
					if (c == 'u') {
						state = JSON5_STATE_STRING_HEXCHAR_SURR_BEGIN;
						tknzr -> aux_count = 4;
					}
					else {
						goto unexpected_char;
					}
					break;
				}
				case JSON5_STATE_STRING_HEXCHAR_SURR:
				case JSON5_STATE_STRING_HEXCHAR_SURR_BEGIN: {
					if (c < 128) {
						if (char_types [c].hex) {
							value = char_types [c].hex & HEX_VAL_MASK;
						}
						else {
							goto invalid_hex_char;
						}
					}
					else {
						goto invalid_hex_char;
					}

					state = JSON5_STATE_STRING_HEXCHAR_SURR;
					break;
				}
				case JSON5_STATE_NUMBER_SIGN: {
					switch (char_type) {
						case JSON5_TOK_NUMBER: {
							state = JSON5_STATE_NUMBER;
							break;
						}
						case JSON5_TOK_PERIOD: {
							state = JSON5_STATE_NUMBER_PERIOD;
							break;
						}
						default: {
							goto unexpected_char;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_NUMBER_START: {
					switch (char_type) {
						case JSON5_TOK_NUMBER: {
							value = c - '0';
							state = JSON5_STATE_NUMBER;
							break;
						}
						case JSON5_TOK_PERIOD: {
							state = JSON5_STATE_NUMBER_PERIOD;
							break;
						}
						case JSON5_TOK_NAME: {
							token = &tknzr -> token;
							token -> type = JSON5_TOK_NAME_SIGN;
							token -> value.i = tknzr -> number.sign ? -1 : 0;
							state = JSON5_STATE_NAME_SIGN;
							break;
						}
						default: {
							goto unexpected_char;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_NUMBER: {
					switch (char_type) {
						case JSON5_TOK_NUMBER: {
							value = c - '0';
							break;
						}
						case JSON5_TOK_PERIOD: {
							state = JSON5_STATE_NUMBER_PERIOD;
							break;
						}
						default: {
							switch (c) {
								case 'e':
								case 'E': {
									if (tknzr -> number.length) {
										state = JSON5_STATE_NUMBER_EXP_START;
									}
									else {
										goto unexpected_char;
									}
									break;
								}
								// check if hex number
								case 'x':
								case 'X': {
									// if number is "0"
									if (tknzr -> number.length == 1 && tknzr -> number.mant.u == 0) {
										tknzr -> number.type = JSON5_NUM_HEX;
										state = JSON5_STATE_NUMBER_HEX_BEGIN;
									}
									else {
										goto unexpected_char;
									}

									break;
								}
								default: {
									state = JSON5_STATE_NUMBER_DONE;
									again = 1;
									break;
								}
							}
							break;
						}
					}

					break;
				}
				case JSON5_STATE_NUMBER_FRAC: {
					switch (char_type) {
						case JSON5_TOK_NUMBER: {
							value = c - '0';
							break;
						}
						default: {
							switch (c) {
								case 'e':
								case 'E': {
									if (tknzr -> number.length) {
										state = JSON5_STATE_NUMBER_EXP_START;
									}
									else {
										goto unexpected_char;
									}
									break;
								}
								default: {
									if (!tknzr -> number.length) {
										goto unexpected_char;
									}

									state = JSON5_STATE_NUMBER_DONE;
									again = 1;
									break;
								}
							}
							break;
						}
					}
					break;
				}
				case JSON5_STATE_NUMBER_HEX_BEGIN: {
					switch (char_type) {
						case JSON5_TOK_NAME:
						case JSON5_TOK_NUMBER: {
							if (c < 128 && char_types [c].hex) {
								value = char_types [c].hex & HEX_VAL_MASK;
								state = JSON5_STATE_NUMBER_HEX;
							}
							else {
								goto invalid_hex_char;
							}
							break;
						}
						default: {
							goto invalid_hex_char;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_NUMBER_HEX: {
					switch (char_type) {
						case JSON5_TOK_NAME:
						case JSON5_TOK_NUMBER: {
							if (c < 128 && char_types [c].hex) {
								value = char_types [c].hex & HEX_VAL_MASK;
							}
							else {
								goto invalid_hex_char;
							}
							break;
						}
						default: {
							state = JSON5_STATE_NUMBER_DONE;
							again = 1;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_NUMBER_EXP: {
					switch (char_type) {
						case JSON5_TOK_NUMBER: {
							value = c - '0';
							break;
						}
						default: {
							if (!tknzr -> number.exp_len) {
								goto unexpected_char;
							}

							state = JSON5_STATE_NUMBER_DONE;
							again = 1;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_NUMBER_EXP_START: {
					switch (char_type) {
						case JSON5_TOK_SIGN: {
							state = JSON5_STATE_NUMBER_EXP_SIGN;
							break;
						}
						case JSON5_TOK_NUMBER: {
							value = c - '0';
							state = JSON5_STATE_NUMBER_EXP;
							break;
						}
						default: {
							goto unexpected_char;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_COMMENT: {
					switch (char_type) {
						case JSON5_TOK_COMMENT: {
							state = JSON5_STATE_COMMENT_SL;
							break;
						}
						case JSON5_TOK_COMMENT2: {
							state = JSON5_STATE_COMMENT_ML;
							break;
						}
						default: {
							goto unexpected_char;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_COMMENT_ML: {
					switch (char_type) {
						case JSON5_TOK_COMMENT2: {
							state = JSON5_STATE_COMMENT_ML2;
							break;
						}
						case JSON5_TOK_END: {
							goto unexpected_char;
							break;
						}
						default: {
							break;
						}
					}
					break;
				}
				case JSON5_STATE_COMMENT_ML2: {
					switch (char_type) {
						case JSON5_TOK_COMMENT: {
							state = JSON5_STATE_NONE;
							break;
						}
						default: {
							state = JSON5_STATE_COMMENT_ML;
							break;
						}
					}
					break;
				}
				case JSON5_STATE_COMMENT_SL: {
					if (char_type == JSON5_TOK_LINEBREAK) {
						state = JSON5_STATE_NONE;
					}
					break;
				}
				default: {
					break;
				}
			}

			// token actions
			switch (state) {
				case JSON5_STATE_NAME:
				case JSON5_STATE_NAME_SIGN:
				case JSON5_STATE_STRING: {
					if (tknzr -> mb_char.length) {
						json5_tokenizer_put_mb_chars (tknzr);
					}
					else {
						json5_tokenizer_put_char (tknzr, c);
					}
					break;
				}
				case JSON5_STATE_STRING_MULTILINE_END: {
					state = JSON5_STATE_STRING;
					break;
				}
				case JSON5_STATE_NUMBER:
				case JSON5_STATE_NUMBER_FRAC: {
					json5_tokenizer_number_add_digit (tknzr, value);
					break;
				}
				case JSON5_STATE_NUMBER_PERIOD: {
					tknzr -> number.dec_pnt = tknzr -> number.length;
					json5_tokenizer_conv_number_float (tknzr);
					state = JSON5_STATE_NUMBER_FRAC;
					break;
				}
				case JSON5_STATE_NUMBER_SIGN: {
					tknzr -> number.sign = (c == '-');
					state = JSON5_STATE_NUMBER_START;
					break;
				}
				case JSON5_STATE_NUMBER_EXP: {
					json5_tokenizer_exp_add_digit (tknzr, value);
					break;
				}
				case JSON5_STATE_NUMBER_EXP_SIGN: {
					tknzr -> number.exp_sign = (c == '-');
					state = JSON5_STATE_NUMBER_EXP;
					break;
				}
				case JSON5_STATE_NUMBER_EXP_START: {
					json5_tokenizer_conv_number_float (tknzr);
					break;
				}
				case JSON5_STATE_NUMBER_HEX: {
					json5_tokenizer_number_add_hex_digit (tknzr, value);
					break;
				}
				case JSON5_STATE_NUMBER_DONE: {
					json5_tokenizer_number_end (tknzr);
					token = &tknzr -> token;
					token -> value.i = tknzr -> number.mant.i;

					switch (tknzr -> number.type) {
						default:
						case JSON5_NUM_INT:
						case JSON5_NUM_HEX: {
							token -> type = JSON5_TOK_NUMBER;
							break;
						}
						case JSON5_NUM_FLOAT: {
							token -> type = JSON5_TOK_NUMBER_FLOAT;
							break;
						}
					}

					state = JSON5_STATE_NONE;
					accept = 1;
					break;
				}
				case JSON5_STATE_STRING_HEXCHAR: {
					tknzr -> seq_value = (tknzr -> seq_value << 4) | value;

					if (-- tknzr -> aux_count == 0) {
						value = tknzr -> seq_value;

						// high surrogate
						if ((value & 0xFC00) == 0xD800) {
							tknzr -> seq_value = (value - 0xD800) << 16; // save value
							state = JSON5_STATE_STRING_HEXCHAR_SURR_SEQ;
						}
						else {
							state = JSON5_STATE_STRING;
							json5_tokenizer_put_mb_char (tknzr, value);
						}
					}
					break;
				}
				case JSON5_STATE_STRING_HEXCHAR_SURR: {
					tknzr -> seq_value = (tknzr -> seq_value & ~0xFFFF) | ((tknzr -> seq_value & 0xFFFF) << 4) | value;

					if (-- tknzr -> aux_count == 0) {
						value = tknzr -> seq_value;

						// low surrogate
						if ((value & 0xFC00) == 0xDC00) {
							value = 0x10000 + (value >> 16) * 0x400 + ((value & 0xFFFF) - 0xDC00);
							state = JSON5_STATE_STRING;
							json5_tokenizer_put_mb_char (tknzr, value);
						}
						else {
							goto expected_low_surrogate;
						}
					}
					break;
				}
				case JSON5_STATE_END: {
					char_type = JSON5_TOK_END;
					accept = 1;
					break;
				}
				case JSON5_STATE_ERROR: {
					goto error;
					break;
				}
				default: {
					break;
				}
			}

			if (accept) {
				int res;

				token = &tknzr -> token;
				token -> length = &tknzr -> buffer [tknzr -> buffer_len] - token -> token;

				json5_tokenizer_end_buffer (tknzr);

				switch (token -> type) {
					case JSON5_TOK_NAME: {
						if (strcmp ((char *) token -> token, "true") == 0) {
							token -> type = JSON5_TOK_NUMBER_BOOL;
							token -> value.i = 1;
						}
						else if (strcmp ((char *) token -> token, "false") == 0) {
							token -> type = JSON5_TOK_NUMBER_BOOL;
							token -> value.i = 0;
						}
						else if (strcmp ((char *) token -> token, "null") == 0) {
							token -> type = JSON5_TOK_NULL;
						}
						else if (strcmp ((char *) token -> token, "NaN") == 0) {
							token -> type = JSON5_TOK_NAN;
						}
						else if (strcmp ((char *) token -> token, "Infinity") == 0) {
							token -> type = JSON5_TOK_INFINITY;
						}

						break;
					}
					case JSON5_TOK_NAME_SIGN: {
						if (strcmp ((char *) token -> token, "null") == 0) {
							token -> type = JSON5_TOK_NULL;
						}
						else if (strcmp ((char *) token -> token, "NaN") == 0) {
							token -> type = JSON5_TOK_NAN;
						}
						else if (strcmp ((char *) token -> token, "Infinity") == 0) {
							token -> type = JSON5_TOK_INFINITY;
						}
						else {
							goto invalid_token;
						}

						break;
					}
					default: {
						break;
					}
				}

				if ((res = put_token (&tknzr -> token, arg)) != 0) {
					json5_tokenizer_set_error (tknzr, "User error: %d", res);
					goto error;
				}
			}
		}
		while (again);
	}
	while (chars < end);

	tknzr -> state = state;
	tknzr -> offset = offset;

	return 0;

	invalid_token: {
		json5_tokenizer_set_error (tknzr, "Invalid token on line %d:%d",
			offset.lineno + 1, offset.colno);
		goto error;
	}

	unexpected_char: {
		if (char_type == JSON5_TOK_END) {
			json5_tokenizer_set_error (tknzr, "Premature end of file");
		}
		else if (char_type == JSON5_TOK_LINEBREAK) {
			json5_tokenizer_set_error (tknzr, "Unexpected linebreak on line %d:%d",
				offset.lineno + 1, offset.colno);
		}
		else if (c >= ' ' && c < 127) {
			json5_tokenizer_set_error (tknzr, "Unexpected character '%c' on line %d:%d",
				c, offset.lineno + 1, offset.colno);
		}
		else {
			json5_tokenizer_set_error (tknzr, "Unexpected character '\\u%04x' on line %d:%d",
				c, offset.lineno + 1, offset.colno);
		}

		goto error;
	}

	unexpected_end_starting: {
		json5_tokenizer_set_error (tknzr, "Premature end of file for string starting on line %d:%d",
			tknzr -> token.offset.lineno + 1, tknzr -> token.offset.colno);
		goto error;
	}

	invalid_hex_char: {
		if (char_type == JSON5_TOK_END) {
			json5_tokenizer_set_error (tknzr, "Premature end of hex sequence");
		}
		else if (char_type == JSON5_TOK_LINEBREAK) {
			json5_tokenizer_set_error (tknzr, "Unexpected linebreak on line %d:%d",
				offset.lineno + 1, offset.colno);
		}
		else if (c >= ' ' && c < 127) {
			json5_tokenizer_set_error (tknzr, "Invalid hex character '%c' on line %d:%d",
				c, offset.lineno + 1, offset.colno);
		}
		else {
			json5_tokenizer_set_error (tknzr, "Invalid hex character '\\u%04x' on line %d:%d",
				c, offset.lineno + 1, offset.colno);
		}

		goto error;
	}

	invalid_byte: {
		if (char_type == JSON5_TOK_END) {
			json5_tokenizer_set_error (tknzr, "Premature end of Unicode sequence");
		}
		else if (char_type == JSON5_TOK_LINEBREAK) {
			json5_tokenizer_set_error (tknzr, "Unexpected linebreak on line %d:%d",
				offset.lineno + 1, offset.colno);
		}
		else if (c >= ' ' && c < 127) {
			json5_tokenizer_set_error (tknzr, "Invalid character '%c' for Unicode sequence on line %d:%d",
				c, offset.lineno + 1, offset.colno);
		}
		else {
			json5_tokenizer_set_error (tknzr, "Invalid byte '\\x%02x' for Unicode sequence on line %d:%d",
				c, offset.lineno + 1, offset.colno);
		}

		goto error;
	}

	expected_low_surrogate: {
		json5_tokenizer_set_error (tknzr, "Unicode error: Expected low surrogate sequence on line %d:%d",
			offset.lineno + 1, offset.colno);

		goto error;
	}

	alloc_error: {
		json5_tokenizer_set_error (tknzr, "Allocation error");
		goto error;
	}

	error: {
		tknzr -> state = JSON5_STATE_ERROR;

		return -1;
	}
}

/**
 * Splits input chars into smaller chunks for better buffer usage
 */
int json5_tokenizer_put_chars (json5_tokenizer * tknzr, uint8_t const * chars, size_t size, json5_put_token_func put_token, void * arg) {
	int res = 0;

	do {
		size_t const max_size = 1024;
		size_t chunk_size = size > max_size ? max_size : size;

		if ((res = json5_tokenizer_put_chars_chunk (tknzr, chars, chunk_size, put_token, arg)) != 0) {
			break;
		}

		chars += chunk_size;
		size -= chunk_size;
	}
	while (size);

	return res;
}

char const * json5_tokenizer_get_error (json5_tokenizer const * tknzr) {
	if (tknzr -> state == JSON5_STATE_ERROR) {
		return (void *) tknzr -> buffer;
	}

	return NULL;
}
