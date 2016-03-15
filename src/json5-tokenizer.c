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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
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
	JSON5_STATE_STRING,
	JSON5_STATE_STRING_BEGIN,   // '"' or "'"
	JSON5_STATE_STRING_ESCAPE,  // '\'
	JSON5_STATE_STRING_HEXCHAR, // 'x'
	JSON5_STATE_STRING_HEXCHAR_BEGIN, // 'x'
	JSON5_STATE_NUMBER,
	JSON5_STATE_NUMBER_SIGN,
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
 *
 */
typedef struct {
	uint8_t type;
	uint8_t hex;
	uint8_t seq;
} json5_char;

/**
 *
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

	for (int i = 0; i < tknzr -> token_count; i ++) {
		token = &tknzr -> tokens [i];
		token -> token = new_buf + (token -> token - tknzr -> buffer);
	}
}

static int json5_tokenizer_ensure_buffer (json5_tokenizer * tknzr) {
	size_t new_cap;
	uint8_t * new_buf;

	if (tknzr -> buffer_len + BUF_MIN_FREE_SPACE >= tknzr -> buffer_cap) {
		new_cap = tknzr -> buffer_cap * 2;
		new_buf = realloc (tknzr -> buffer, new_cap);

		if (!new_buf) {
			return -1;
		}

		json5_tokenizer_relocate_token_data (tknzr, new_buf);

		tknzr -> buffer = new_buf;
		tknzr -> buffer_cap = new_cap;
	}

	return 0;
}

static int json5_tokenizer_put_char (json5_tokenizer * tknzr, int c) {
	if (json5_tokenizer_ensure_buffer (tknzr) != 0) {
		return -1;
	}

	tknzr -> buffer [tknzr -> buffer_len ++] = c;

	return 0;
}

static int json5_tokenizer_put_mb_chars (json5_tokenizer * tknzr) {
	if (json5_tokenizer_ensure_buffer (tknzr) != 0) {
		return -1;
	}

	memcpy (&tknzr -> buffer [tknzr -> buffer_len], tknzr -> mb_char.chars, tknzr -> mb_char.length);
	tknzr -> buffer_len += tknzr -> mb_char.length;
	tknzr -> mb_char.length = 0;

	return 0;
}

static void json5_tokenizer_end_buffer (json5_tokenizer * tknzr) {
	// terminate current buffer segment
	tknzr -> buffer [tknzr -> buffer_len ++] = '\0';

	// align next buffer segment to pointer size
	while (tknzr -> buffer_len & (sizeof (void *) - 1)) {
		tknzr -> buffer [tknzr -> buffer_len ++] = '\0';
	}
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
		if (tknzr -> number.mant.u > INT64_MAX / 10) {
			json5_tokenizer_conv_number_float (tknzr);
		}
	}

	if (tknzr -> number.type != JSON5_NUM_FLOAT) {
		tknzr -> number.mant.u = 10 * tknzr -> number.mant.u;

		if (tknzr -> number.mant.u > INT64_MAX - value) {
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
		if (tknzr -> number.mant.u > INT64_MAX / 16) {
			json5_tokenizer_conv_number_float (tknzr);
		}

		tknzr -> number.mant.u = 16 * tknzr -> number.mant.u;

		if (tknzr -> number.mant.u > INT64_MAX - value) {
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
	int n;
	double d;
	double e;

	if (tknzr -> number.type == JSON5_NUM_FLOAT) {
		d = 10.0;
		e = 1.0;
		n = tknzr -> number.exp;

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

int json5_tokenizer_put_chars (json5_tokenizer * tknzr, uint8_t const * chars, size_t size, json5_put_tokens_func put_tokens, void * arg) {
	int c;
	int value;
	int res = 0;
	int state;
	int accept = 0;
	int again;
	int accept_count;
	json5_off offset;
	json5_tok_type char_type;
	json5_token * token;
	json5_ut_info const * info = NULL;
	uint8_t const * end = &chars [size];

	if (tknzr -> state >= JSON5_STATE_END) {
		return 0;
	}

	state = tknzr -> state;
	offset = tknzr -> offset;
	accept_count = tknzr -> accept_count;

	do {
		if (tknzr -> mb_char.count) {
			if (size == 0) {
				goto unexpected_end;
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

		if (chars < end) {
			c = *chars ++;

			if ((c & 0xC0) == 0xC0) {
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
			info = json5_ut_lookup_rune (c);

			switch (info -> category) {
				case JSON5_UT_CATEGORY_LETTER_UPPERCASE:
				case JSON5_UT_CATEGORY_LETTER_LOWERCASE:
				case JSON5_UT_CATEGORY_LETTER_TITLECASE:
				case JSON5_UT_CATEGORY_LETTER_MODIFIER:
				case JSON5_UT_CATEGORY_LETTER_OTHER: {
					char_type = JSON5_TOK_NAME;
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
							json5_number_init (tknzr);
							break;
						}
						case JSON5_TOK_NUMBER: {
							state = JSON5_STATE_NUMBER;
							json5_number_init (tknzr);
							c -= '0';
							break;
						}
						case JSON5_TOK_PERIOD: {
							state = JSON5_STATE_NUMBER_PERIOD;
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
						if (accept_count >= JSON5_NUM_TOKS) {
							if ((res = put_tokens (tknzr -> tokens, accept_count, arg)) != 0) {
								json5_tokenizer_set_error (tknzr, "User error: %d", res);
								goto error;
							}

							memmove (&tknzr -> tokens [0], &tknzr -> tokens [accept_count], (JSON5_NUM_TOKS - accept_count) * sizeof (json5_token));
							tknzr -> token_count -= accept_count;

							if (!tknzr -> token_count) {
								tknzr -> buffer_len = 0;
							}

							accept_count = 0;
						}

						token = &tknzr -> tokens [tknzr -> token_count ++];
						token -> type = char_type;
						token -> token = &tknzr -> buffer [tknzr -> buffer_len];
						token -> offset.lineno = tknzr -> offset.lineno;
						token -> offset.colno = tknzr -> offset.colno + 1;
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
							goto unexpected_end;
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
							goto unexpected_end;
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
				case JSON5_STATE_STRING_HEXCHAR:
				case JSON5_STATE_STRING_HEXCHAR_BEGIN: {
					if (c < 128) {
						if (char_types [c].hex) {
							c = char_types [c].hex & HEX_VAL_MASK;
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
				case JSON5_STATE_NUMBER_SIGN: {
					if (c == '-') {
						tknzr -> number.sign = 1;
					}

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
				case JSON5_STATE_NUMBER: {
					switch (char_type) {
						case JSON5_TOK_NUMBER: {
							c -= '0';
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
							c -= '0';
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
										if (char_type == JSON5_TOK_END) {
											goto unexpected_end;
										}
										else {
											goto unexpected_char;
										}
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
								c = char_types [c].hex & HEX_VAL_MASK;
								state = JSON5_STATE_NUMBER_HEX;
							}
							else {
								goto invalid_hex_char;
							}
							break;
						}
						case JSON5_TOK_END: {
							goto unexpected_end;
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
								c = char_types [c].hex & HEX_VAL_MASK;
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
							c -= '0';
							break;
						}
						default: {
							if (!tknzr -> number.exp_len) {
								if (char_type == JSON5_TOK_END) {
									goto unexpected_end;
								}
								else {
									goto unexpected_char;
								}
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
							c -= '0';
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
						case JSON5_TOK_END: {
							goto unexpected_end;
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
							goto unexpected_end;
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
				case JSON5_STATE_STRING:
				case JSON5_STATE_NAME: {
					if (tknzr -> mb_char.length) {
						if (json5_tokenizer_put_mb_chars (tknzr) != 0) {
							goto alloc_error;
						}
					}
					else {
						if (json5_tokenizer_put_char (tknzr, c) != 0) {
							goto alloc_error;
						}
					}
					break;
				}
				case JSON5_STATE_NUMBER:
				case JSON5_STATE_NUMBER_FRAC: {
					json5_tokenizer_number_add_digit (tknzr, c);
					break;
				}
				case JSON5_STATE_NUMBER_PERIOD: {
					tknzr -> number.dec_pnt = tknzr -> number.length;
					json5_tokenizer_conv_number_float (tknzr);
					state = JSON5_STATE_NUMBER_FRAC;
					break;
				}
				case JSON5_STATE_NUMBER_SIGN: {
					if (c == '-') {
						tknzr -> number.sign = 1;
					}
					state = JSON5_STATE_NUMBER;
					break;
				}
				case JSON5_STATE_NUMBER_EXP: {
					json5_tokenizer_exp_add_digit (tknzr, c);
					break;
				}
				case JSON5_STATE_NUMBER_EXP_SIGN: {
					if (c == '-') {
						tknzr -> number.exp_sign = 1;
					}
					state = JSON5_STATE_NUMBER_EXP;
					break;
				}
				case JSON5_STATE_NUMBER_EXP_START: {
					json5_tokenizer_conv_number_float (tknzr);
					break;
				}
				case JSON5_STATE_NUMBER_HEX: {
					json5_tokenizer_number_add_hex_digit (tknzr, c);
					break;
				}
				case JSON5_STATE_NUMBER_DONE: {
					json5_tokenizer_number_end (tknzr);
					token = &tknzr -> tokens [tknzr -> token_count - 1];
					token -> value.i = tknzr -> number.mant.i;
					state = JSON5_STATE_NONE;
					accept = 1;
					break;
				}
				case JSON5_STATE_STRING_HEXCHAR: {
					tknzr -> seq_value = (tknzr -> seq_value << 4) | c;

					if (-- tknzr -> aux_count == 0) {
						c = tknzr -> seq_value;
						state = JSON5_STATE_STRING;

						if (json5_tokenizer_put_char (tknzr, c) != 0) {
							goto alloc_error;
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
				accept_count ++;

				token = &tknzr -> tokens [tknzr -> token_count - 1];
				token -> length = &tknzr -> buffer [tknzr -> buffer_len] - token -> token;
				json5_tokenizer_end_buffer (tknzr);

				if (state == JSON5_STATE_END) {
					if ((res = put_tokens (tknzr -> tokens, accept_count, arg)) != 0) {
						json5_tokenizer_set_error (tknzr, "User error: %d", res);
						goto error;
					}
				}
			}
		}
		while (again);
	}
	while (chars < end);

	tknzr -> state = state;
	tknzr -> offset = offset;
	tknzr -> accept_count = accept_count;

	return 0;

	unexpected_char: {
		json5_tokenizer_set_error (tknzr, "Unexpected char '\\u%04x' on line %lld offset %lld",
			c, offset.lineno, offset.colno);
		goto error;
	}

	unexpected_end: {
		json5_tokenizer_set_error (tknzr, "Premature end of file");
		goto error;
	}

	invalid_hex_char: {
		json5_tokenizer_set_error (tknzr, "Invalid byte '\\u%02x' on line %lld offset %lld",
			c, offset.lineno, offset.colno);
		goto error;
	}

	invalid_byte: {
		json5_tokenizer_set_error (tknzr, "Invalid byte '\\u%02x' on line %lld offset %lld",
			c, offset.lineno, offset.colno);
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

char const * json5_tokenizer_get_error (json5_tokenizer const * tknzr) {
	if (tknzr -> state == JSON5_STATE_ERROR) {
		return (void *) tknzr -> buffer;
	}

	return NULL;
}
