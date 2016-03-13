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
	JSON5_STATE_STRING_BEGIN,
	JSON5_STATE_STRING_ESCAPE,
	JSON5_STATE_STRING_HEXCHAR,
	JSON5_STATE_NUMBER,
	JSON5_STATE_NUMBER_SIGN,
	JSON5_STATE_NUMBER_FRAC,
	JSON5_STATE_NUMBER_EXP,
	JSON5_STATE_NUMBER_EXP_SIGN,
	JSON5_STATE_NUMBER_EXP_START,
	JSON5_STATE_NUMBER_HEX,
	JSON5_STATE_NUMBER_HEX_BEGIN,
	JSON5_STATE_NUMBER_DONE,
	JSON5_STATE_COMMENT,
	JSON5_STATE_END,
	JSON5_STATE_ERROR,
} json5_tok_state;

/**
 * Defines state of previous character
 */
typedef struct {
	json5_off offset;
	json5_off prev_offset;
	unsigned c;
} json5_char_state;

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
static json5_char const char_types [256] = {
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

static int json5_tokenizer_has_char(json5_char_state* state) {
	return state -> c != EOF;
}

static void json5_tokenizer_push_char(json5_char_state* state, int c) {
	state -> c = c;
}

static int json5_tokenizer_pop_char(json5_char_state* state) {
	int c;

	c = state -> c;
	state -> c = EOF;

	return c;
}

static void json5_tokenizer_push_back(json5_char_state* state, int c) {
	state -> c = c;
	state -> offset = state -> prev_offset;
}

static int json5_utf8_decode(json5_utf8_decoder* decoder, unsigned c, unsigned* value) {
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

static void json5_tokenizer_set_error(json5_tokenizer* tknzr, char const* msg, ...) {
	va_list args;

	va_start (args, msg);
	vsnprintf ((void*) tknzr -> buffer, tknzr -> buffer_cap, msg, args);
	va_end (args);
}

/**
 * Relocate data of tokens to new allocated buffer
 */
static void json5_tokenizer_relocate_token_data(json5_tokenizer* tknzr, uint8_t* new_buf) {
	json5_token* token;

	for (int i = 0; i < tknzr -> token_count; i ++) {
		token = &tknzr -> tokens [i];
		token -> token = new_buf + (token -> token - tknzr -> buffer);
	}
}

static int json5_tokenizer_ensure_buffer(json5_tokenizer* tknzr) {
	size_t new_cap;
	uint8_t* new_buf;

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

static void json5_tokenizer_end_buffer(json5_tokenizer* tknzr) {
	// terminate current buffer segment
	tknzr -> buffer [tknzr -> buffer_len ++] = '\0';

	// align next buffer segment to pointer size
	while (tknzr -> buffer_len & (sizeof (void*) - 1)) {
		tknzr -> buffer [tknzr -> buffer_len ++] = '\0';
	}
}

int json5_tokenizer_init(json5_tokenizer* tknzr) {
	memset (tknzr, 0, sizeof (*tknzr));

	tknzr -> buffer_cap = INIT_BUF_CAP;
	tknzr -> buffer = malloc (tknzr -> buffer_cap);

	if (!tknzr -> buffer) {
		return -1;
	}

	return 0;
}

void json5_tokenizer_reset(json5_tokenizer* tknzr) {
	uint8_t* buffer = tknzr -> buffer;
	size_t buffer_cap = tknzr -> buffer_cap;

	memset (tknzr, 0, sizeof (*tknzr));
	tknzr -> buffer = buffer;
	tknzr -> buffer_cap = buffer_cap;
}

void json5_tokenizer_destroy(json5_tokenizer* tknzr) {
	if (tknzr -> buffer) {
		free (tknzr -> buffer);
	}

	memset (tknzr, 0, sizeof (*tknzr));
}

int json5_tokenizer_put_byte(json5_tokenizer* tknzr, int c) {
	unsigned value;
	int status;

	if (c < 0) {
		return json5_tokenizer_put_char (tknzr, c);
	}

	status = json5_utf8_decode (&tknzr -> decoder, c, &value);

	if (status == 1) {
		return json5_tokenizer_put_char (tknzr, value);
	}
	else if (status < 0) {
		json5_tokenizer_set_error (tknzr, "Invalid byte '%02x' on line %lld",
			c, tknzr -> offset.lineno + 1);
		return -1;
	}

	return 0;
}

static void json5_tokenizer_utf8_encode (char string [], unsigned c) {
	if (c < 0x80) {
		string [0] = c;
		string [1] = '\0';
	}
	else if (c < 0x800) {
		string [0] = 0xC0 | (c >> 6);
		string [1] = 0x80 | (c & 0x3F);
		string [2] = '\0';
	}
	else if (c < 0x10000) {
		string [0] = 0xE0 | (c >> 12);
		string [1] = 0x80 | ((c >> 6) & 0x3F);
		string [2] = 0x80 | (c & 0x3F);
		string [3] = '\0';
	}
	else if (c <= UT_MAX_VALUE) {
		string [0] = 0xF0 | (c >> 18);
		string [1] = 0x80 | ((c >> 12) & 0x3F);
		string [2] = 0x80 | ((c >> 6) & 0x3F);
		string [3] = 0x80 | (c & 0x3F);
		string [4] = '\0';
	}
	else { // Invalid rune
		//c = REPLACEMENT_RUNE;
	}
}

static void json5_tokenizer_conv_number_float(json5_tokenizer* tknzr) {
	if (tknzr -> number.type != JSON5_NUM_FLOAT) {
		tknzr -> number.mant.f = tknzr -> number.mant.i;
		tknzr -> number.type = JSON5_NUM_FLOAT;
	}
}

static void json5_tokenizer_number_add_digit(json5_tokenizer* tknzr, int value) {
	if (tknzr -> number.type != JSON5_NUM_FLOAT) {
		if (tknzr -> number.mant.i > INT64_MAX / 10) {
			json5_tokenizer_conv_number_float (tknzr);
		}
	}

	if (tknzr -> number.type != JSON5_NUM_FLOAT) {
		tknzr -> number.mant.i = 10 * tknzr -> number.mant.i + value;

		if (tknzr -> number.mant.i > INT64_MAX - value) {
			json5_tokenizer_conv_number_float (tknzr);
			tknzr -> number.mant.f += value;
		}
		else {
			tknzr -> number.mant.i += value;
		}
	}
	else {
		tknzr -> number.mant.f = 10.0 * tknzr -> number.mant.f + value;
	}
}

static void json5_tokenizer_exp_add_digit(json5_tokenizer* tknzr, int value) {
	// ignore larger values
	if (tknzr -> number.exp < DBL_MAX_10_EXP) {
		tknzr -> number.exp = 10 * tknzr -> number.exp + value;
	}
}

static void json5_tokenizer_number_end(json5_tokenizer* tknzr) {
	if (tknzr -> number.type == JSON5_NUM_FLOAT) {
		// ...
	}

	// ...
}

int json5_tokenizer_put_char(json5_tokenizer* tknzr, int c) {
	int state;
	int accept;
	int accept_count;
	size_t buffer_len;
	json5_tok_type char_type;
	json5_token* token;
	json5_char_state char_state;
	char char_string [8];
	UTInfo const* info = NULL;

	if (tknzr -> state >= JSON5_STATE_END) {
		return 0;
	}

	if(json5_tokenizer_ensure_buffer (tknzr) != 0) {
		goto alloc_error;
	}

	accept_count = tknzr -> accept_count;

	json5_tokenizer_push_char (&char_state, c);

	if (accept_count) {
		memmove (&tknzr -> tokens [0], &tknzr -> tokens [accept_count], (JSON5_NUM_TOKS - accept_count) * sizeof (json5_token));
		tknzr -> token_count -= accept_count;

		if (!tknzr -> token_count) {
			tknzr -> buffer_len = 0;
		}
	}

	accept_count = 0;

	do {
		accept = 0;
		c = json5_tokenizer_pop_char (&char_state);
		state = tknzr -> state;

		if (c < 0) {
			c = EOF;
			char_type = JSON5_TOK_END;
		}
		else if (c < 256) {
			char_type = char_types [c].type;
		}
		else {
			char_type = JSON5_TOK_OTHER;
			info = UTLookupRune (c);

			if (info -> flags & UTFlagLetter) {
				char_type = JSON5_TOK_NAME;
			}
			else if (info -> flags & UTFlagLinebreak) {
				char_type = JSON5_TOK_LINEBREAK;
			}
			else if (info -> flags & UTFlagSpace) {
				char_type = JSON5_TOK_SPACE;
			}
		}

		if (char_type == JSON5_TOK_LINEBREAK) {
			char_state.offset.lineno ++;
			char_state.offset.colno = 0;
		}
		else {
			char_state.offset.colno ++;
		}

		// tokenizer states
		switch (state) {
			case JSON5_STATE_NONE: {
				switch (char_type) {
					case JSON5_TOK_SPACE:
					case JSON5_TOK_LINEBREAK: {
						state = JSON5_STATE_SPACE;
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
						memset (&tknzr -> number, 0, sizeof (tknzr -> number));

						if (c == '-') {
							tknzr -> number.sign = 1;
						}
						break;
					}
					case JSON5_TOK_NUMBER: {
						state = JSON5_STATE_NUMBER;
						memset (&tknzr -> number, 0, sizeof (tknzr -> number));

						if (c == '-') {
							tknzr -> number.sign = 1;
						}
						break;
					}
					case JSON5_TOK_PERIOD: {
						state = JSON5_STATE_NUMBER_FRAC;
						char_type = JSON5_TOK_NUMBER;

						memset (&tknzr -> number, 0, sizeof (tknzr -> number));
						tknzr -> number.type = JSON5_NUM_FLOAT;
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
					default: {
						goto unexpected_char;
						break;
					}
				}

				token = &tknzr -> tokens [tknzr -> token_count ++];
				token -> type = char_type;
				token -> token = &tknzr -> buffer [tknzr -> buffer_len];
				token -> offset.lineno = tknzr -> offset.lineno;
				token -> offset.colno = tknzr -> offset.colno + 1;

				tknzr -> token_start.lineno = char_state.offset.lineno;
				tknzr -> token_start.colno = char_state.offset.colno + 1;

				break;
			}
			case JSON5_STATE_SPACE: {
				switch (char_type) {
					case JSON5_TOK_SPACE: {
						break;
					}
					default: {
						json5_tokenizer_push_back (&char_state, c);
						accept = 1;
						state = JSON5_STATE_NONE;
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
						if (info -> flags & UTFlagNumber) {
						}
						else {
							accept = 1;
							json5_tokenizer_push_back (&char_state, c);
						}

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
					default: {
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

						if (c < 256) {
							if (char_types [c].seq) {
								c = char_types [c].seq;

								switch (c) {
									case 'u': {
										state = JSON5_STATE_STRING_HEXCHAR;
										tknzr -> aux_count = 4;
										tknzr -> seq_value = 0;
										break;
									}
									case 'x': {
										state = JSON5_STATE_STRING_HEXCHAR;
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
			case JSON5_STATE_STRING_HEXCHAR: {
				if (c < 256) {
					if (char_types [c].hex) {
						c = char_types [c].hex & HEX_VAL_MASK;
					}
					else {
						json5_tokenizer_utf8_encode (char_string, c);
						json5_tokenizer_set_error (tknzr, "Invalid hex char '%s' (\\u%04x) on line %lld offset %lld",
							char_string, c, tknzr -> offset.lineno, tknzr -> offset.colno);

						goto error;
					}
				}
				else {
					json5_tokenizer_utf8_encode (char_string, c);
					json5_tokenizer_set_error (tknzr, "Invalid sequence; unexpected char '%s' (\\u%04x) on line %lld offset %lld",
						char_string, c, tknzr -> offset.lineno, tknzr -> offset.colno);

					goto error;
				}
			}
			case JSON5_STATE_NUMBER_SIGN: {
				switch (char_type) {
					case JSON5_TOK_NUMBER: {
						state = JSON5_STATE_NUMBER;
						break;
					}
					case JSON5_TOK_PERIOD: {
						tknzr -> number.dec_pnt = tknzr -> number.length;
						json5_tokenizer_conv_number_float (tknzr);
						state = JSON5_STATE_NUMBER_FRAC;
						break;
					}
					default: {
						goto unexpected_char;
						break;
					}
				}
				break;
			}
			case JSON5_STATE_NUMBER_FRAC: {
				switch (char_type) {
					case JSON5_TOK_NUMBER: {
						break;
					}
					default: {
						switch (c) {
							case 'e':
							case 'E': {
								state = JSON5_STATE_NUMBER_EXP_START;
								break;
							}
							default: {
								json5_tokenizer_push_back (&char_state, c);
								state = JSON5_STATE_NUMBER_DONE;
							}
						}

						break;
					}
				}
				break;
			}
			case JSON5_STATE_NUMBER: {
				switch (char_type) {
					case JSON5_TOK_NUMBER: {
						break;
					}
					case JSON5_TOK_PERIOD: {
						tknzr -> number.dec_pnt = tknzr -> number.length;
						json5_tokenizer_conv_number_float (tknzr);
						state = JSON5_STATE_NUMBER_FRAC;
						break;
					}
					default: {
						switch (c) {
							case 'e':
							case 'E': {
								state = JSON5_STATE_NUMBER_EXP_START;
								break;
							}
							// check if hex number
							case 'x':
							case 'X': {
								// if number is "0"
								if (tknzr -> number.length == 1 && tknzr -> number.mant.i == 0) {
									tknzr -> number.type = JSON5_NUM_HEX;
									state = JSON5_STATE_NUMBER_HEX_BEGIN;
								}
								else {
									goto unexpected_char;
								}

								break;
							}
							default: {
								json5_tokenizer_push_back (&char_state, c);
								state = JSON5_STATE_NUMBER_DONE;
								break;
							}
						}
						break;
					}
				}

				break;
			}
			case JSON5_STATE_NUMBER_HEX_BEGIN: {
				state = JSON5_STATE_NUMBER_HEX;
				break;
			}
			case JSON5_STATE_NUMBER_HEX: {
				/*if (c < ) {

				}
				else {

				}*/
				break;
			}
			case JSON5_STATE_NUMBER_EXP: {
				break;
			}
			case JSON5_STATE_NUMBER_EXP_START: {
				switch (char_type) {
					case JSON5_TOK_SIGN: {
						if (c == '-') {
							tknzr -> number.exp_sign = 1;
						}
						break;
					}
					case JSON5_TOK_NUMBER: {
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
			default: {
				break;
			}
		}

		// token actions
		switch (state) {
			case JSON5_STATE_SPACE:
			case JSON5_STATE_STRING:
			case JSON5_STATE_NAME: {
				json5_tokenizer_put_char (tknzr, c);
				break;
			}
			case JSON5_STATE_NUMBER: {
				json5_tokenizer_number_add_digit (tknzr, c - '0');
				json5_tokenizer_put_char (tknzr, c);
				break;
			}
			case JSON5_STATE_NUMBER_EXP: {
				json5_tokenizer_exp_add_digit (tknzr, c - '0');
				json5_tokenizer_put_char (tknzr, c);
				break;
			}
			case JSON5_STATE_NUMBER_HEX: {
				json5_tokenizer_number_add_digit (tknzr, char_types [c].hex & 0xF);
				json5_tokenizer_put_char (tknzr, c);
				break;
			}
			case JSON5_STATE_NUMBER_DONE: {
				json5_tokenizer_number_end (tknzr);
				state = JSON5_STATE_NONE;
				accept = 1;
				break;
			}
			case JSON5_STATE_STRING_HEXCHAR: {
				tknzr -> seq_value = (tknzr -> seq_value << 8) | c;

				if (-- tknzr -> aux_count == 0) {
					json5_tokenizer_put_char (tknzr, tknzr -> seq_value);
					state = JSON5_STATE_STRING;
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
			buffer_len = tknzr -> buffer_len;
			token = &tknzr -> tokens [tknzr -> token_count - 1];

			json5_tokenizer_end_buffer (tknzr);
			token -> length = &tknzr -> buffer [buffer_len] - token -> token;

			accept_count ++;
		}

		tknzr -> state = state;
	}
	while(json5_tokenizer_has_char (&char_state));

	tknzr -> accept_count = accept_count;
	tknzr -> offset = char_state.offset;

	return 0;

	unexpected_char: {
		json5_tokenizer_utf8_encode (char_string, c);
		json5_tokenizer_set_error (tknzr, "Unexpected char '%s' (\\u%04x) on line %lld offset %lld",
			char_string, c, tknzr -> offset.lineno, tknzr -> offset.colno);

		goto error;
	}

	unexpected_end: {
		json5_tokenizer_set_error (tknzr, "Premature end of file");

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

char const* json5_tokenizer_get_error(json5_tokenizer const* tknzr) {
	if (tknzr -> state == JSON5_STATE_ERROR) {
		return (void*) tknzr -> buffer;
	}

	return NULL;
}
