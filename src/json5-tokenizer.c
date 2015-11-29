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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include "json5-tokenizer.h"
#include "unicode-table.h"

#define INIT_BUF_CAP 4096
#define BUF_MIN_FREE_SPACE 256

/**
 * Defines number types
 */
enum {
	JSON5_NUMBER_INT,
	JSON5_NUMBER_HEX,
	JSON5_NUMBER_FLOAT,
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
 * Defines initial characters of tokens
 */
static json5_tok_type const init_chars [256] = {
	[' ']  = JSON5_TOK_SPACE,
	['\t'] = JSON5_TOK_SPACE,
	['\n'] = JSON5_TOK_LINEBREAK,
	['\r'] = JSON5_TOK_LINEBREAK,
	['\f'] = JSON5_TOK_SPACE,
	['\v'] = JSON5_TOK_SPACE,
	['"']  = JSON5_TOK_STRING,
	['\''] = JSON5_TOK_STRING,
	['{']  = JSON5_TOK_OBJ_OPEN,
	['}']  = JSON5_TOK_OBJ_CLOSE,
	['[']  = JSON5_TOK_ARR_OPEN,
	[']']  = JSON5_TOK_ARR_CLOSE,
	['.']  = JSON5_TOK_PERIOD,
	[',']  = JSON5_TOK_COMMA,
	[':']  = JSON5_TOK_COLON,
	['+']  = JSON5_TOK_SIGN,
	['-']  = JSON5_TOK_SIGN,
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
	['_']  = JSON5_TOK_NAME,
};

/**
 * Defines hexadecimal chars
 */
static unsigned const hex_chars [256] = {
	['0'] = 16 | 0,
	['1'] = 16 | 1,
	['2'] = 16 | 2,
	['3'] = 16 | 3,
	['4'] = 16 | 4,
	['5'] = 16 | 5,
	['6'] = 16 | 6,
	['7'] = 16 | 7,
	['8'] = 16 | 8,
	['9'] = 16 | 9,
	['A'] = 16 | 10,
	['B'] = 16 | 11,
	['C'] = 16 | 12,
	['D'] = 16 | 13,
	['E'] = 16 | 14,
	['F'] = 16 | 15,
	['a'] = 16 | 10,
	['b'] = 16 | 11,
	['c'] = 16 | 12,
	['d'] = 16 | 13,
	['e'] = 16 | 14,
	['f'] = 16 | 15,
};

/**
 * Defines escape sequences
 */
static unsigned const escape_chars [256] = {
	['b'] = '\b',
	['f'] = '\f',
	['n'] = '\n',
	['r'] = '\r',
	['t'] = '\t',
	['u'] = 'u',
	['v'] = '\v',
	['x'] = 'x',
};

static int json5_tokenizer_has_char (json5_char_state * state) {
	return state -> c != EOF;
}

static void json5_tokenizer_push_char (json5_char_state * state, int c) {
	state -> c = c;
}

static int json5_tokenizer_pop_char (json5_char_state * state) {
	int c;

	c = state -> c;
	state -> c = EOF;

	return c;
}

static void json5_tokenizer_push_back (json5_char_state * state, int c) {
	state -> c = c;
	state -> offset = state -> prev_offset;
}

static int json5_utf8_decode (json5_utf8_decoder * decoder, unsigned c, unsigned * value) {
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

static void json5_tokenizer_set_error (json5_tokenizer * tknzr, char const * msg, ...) {
	va_list args;

	va_start (args, msg);
	vsnprintf ((void *) tknzr -> buffer, tknzr -> buffer_cap, msg, args);
	va_end (args);
}

/**
 * Relocate data of tokens to new allocated buffer
 */
static void json5_tokenizer_relocate_token_data (json5_tokenizer * tknzr, unsigned char * new_buf) {
	json5_token * token;

	for (int i = 0; i < tknzr -> token_count; i ++) {
		token = &tknzr -> tokens [i];
		token -> token = new_buf + (token -> token - tknzr -> buffer);
	}
}

static int json5_tokenizer_ensure_buffer (json5_tokenizer * tknzr) {
	size_t new_cap;
	unsigned char * new_buf;

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
	unsigned char * buffer = tknzr -> buffer;
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

int json5_tokenizer_put_byte (json5_tokenizer * tknzr, int c) {
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
	string [0] = '\0';

	fprintf (stderr, "Uninplemented json5_tokenizer_utf8_encode\n");
	abort ();
}

static void json5_tokenizer_conv_number_float (json5_tokenizer * tknzr) {
	if (tknzr -> number.type != JSON5_NUMBER_FLOAT) {
		tknzr -> number.mantissa.f = tknzr -> number.mantissa.i;
		tknzr -> number.type = JSON5_NUMBER_FLOAT;
	}
}

static void json5_tokenizer_number_add_digit (json5_tokenizer * tknzr, int value) {
	if (tknzr -> number.type != JSON5_NUMBER_FLOAT) {
		if (tknzr -> number.mantissa.i > INT64_MAX / 10) {
			json5_tokenizer_conv_number_float (tknzr);
		}
	}

	if (tknzr -> number.type != JSON5_NUMBER_FLOAT) {
		tknzr -> number.mantissa.i = 10 * tknzr -> number.mantissa.i + value;

		if (tknzr -> number.mantissa.i > INT64_MAX - value) {
			json5_tokenizer_conv_number_float (tknzr);
			tknzr -> number.mantissa.f += value;
		}
		else {
			tknzr -> number.mantissa.i += value;
		}
	}
	else {
		tknzr -> number.mantissa.f = 10.0 * tknzr -> number.mantissa.f + value;
	}
}

static void json5_tokenizer_exp_add_digit (json5_tokenizer * tknzr, int value) {
	// ignore larger values
	if (tknzr -> number.exp < DBL_MAX_10_EXP) {
		tknzr -> number.exp = 10 * tknzr -> number.exp + value;
	}
}

static void json5_tokenizer_number_end (json5_tokenizer * tknzr) {
	if (tknzr -> number.type == JSON5_NUMBER_FLOAT) {
		// ...
	}

	// ...
}

int json5_tokenizer_put_char (json5_tokenizer * tknzr, int c) {
	int state;
	int accept;
	int accept_count;
	size_t buffer_len;
	json5_tok_type char_type;
	json5_token * token;
	json5_char_state char_state;
	char char_string [8];
	UTInfo const * info;

	if (tknzr -> state >= JSON5_STATE_END) {
		return 0;
	}

	if (json5_tokenizer_ensure_buffer (tknzr) != 0) {
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

		info = UTLookupRune (c);

		if (c < 0) {
			c = EOF;
			char_type = JSON5_TOK_END;
		}
		else if (c < 256) {
			char_type = init_chars [c];
		}
		else {
			char_type = JSON5_TOK_OTHER;

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
						tknzr -> number.type = JSON5_NUMBER_FLOAT;
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
					case JSON5_TOK_NAME: {
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
							if (escape_chars [c]) {
								c = escape_chars [c];

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
					if (hex_chars [c]) {
						c = hex_chars [c] & 0xF;
					}
					else {
						json5_tokenizer_utf8_encode (char_string, c);
						json5_tokenizer_set_error (tknzr, "Invalid hex char '%s' on line %lld offset %lld",
							char_string, tknzr -> offset.lineno, tknzr -> offset.colno);

						goto error;
					}
				}
				else {
					json5_tokenizer_utf8_encode (char_string, c);
					json5_tokenizer_set_error (tknzr, "Invalid sequence; unexpected char '%s' on line %lld offset %lld",
						char_string, tknzr -> offset.lineno, tknzr -> offset.colno);

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
								if (tknzr -> number.length == 1 && tknzr -> number.mantissa.i == 0) {
									tknzr -> number.type = JSON5_NUMBER_HEX;
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
				json5_tokenizer_number_add_digit (tknzr, info -> number.i);
				json5_tokenizer_put_char (tknzr, c);
				break;
			}
			case JSON5_STATE_NUMBER_EXP: {
				json5_tokenizer_exp_add_digit (tknzr, info -> number.i);
				json5_tokenizer_put_char (tknzr, c);
				break;
			}
			case JSON5_STATE_NUMBER_HEX: {
				json5_tokenizer_number_add_digit (tknzr, hex_chars [c] & 0xF);
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
	while (json5_tokenizer_has_char (&char_state));

	tknzr -> accept_count = accept_count;
	tknzr -> offset = char_state.offset;

	return 0;

	unexpected_char: {
		json5_tokenizer_utf8_encode (char_string, c);
		json5_tokenizer_set_error (tknzr, "Unexpected char '%s' on line %lld offset %lld",
			char_string, tknzr -> offset.lineno, tknzr -> offset.colno);

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

char const * json5_tokenizer_get_error (json5_tokenizer const * tknzr) {
	if (tknzr -> state == JSON5_STATE_ERROR) {
		return (void *) tknzr -> buffer;
	}

	return NULL;
}
