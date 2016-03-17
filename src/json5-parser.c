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
#include <stdlib.h>
#include <string.h>
#include "json5-parser.h"

#define INIT_STACK_CAP 32

/**
 * Defines parser states
 */
typedef enum {
	JSON5_STATE_NONE = 0,
	JSON5_STATE_ROOT, // parsed root element
	JSON5_STATE_ARR,
	JSON5_STATE_ARR_BEGIN,
	JSON5_STATE_ARR_END,
	JSON5_STATE_ARR_VAL,
	JSON5_STATE_ARR_SEP,
	JSON5_STATE_OBJ,
	JSON5_STATE_OBJ_BEGIN,
	JSON5_STATE_OBJ_END,
	JSON5_STATE_OBJ_VAL,
	JSON5_STATE_OBJ_SEP,
	JSON5_STATE_OBJ_KEY_SEP,
	JSON5_STATE_STRING,
	JSON5_STATE_NUMBER,
	JSON5_STATE_NAME,
	JSON5_STATE_NAME_SIGN,
	JSON5_STATE_END,
	JSON5_STATE_ERROR,
} json5_parser_state;

static json5_parser_item * json5_parser_stack_push (json5_parser * parser)
{
	json5_parser_item * stack;
	json5_parser_item * item;
	size_t new_cap;

	if (parser -> stack_len >= parser -> stack_cap) {
		new_cap = parser -> stack_cap * 2;

		if (new_cap < INIT_STACK_CAP) {
			new_cap = INIT_STACK_CAP;
		}

		stack = realloc (parser -> stack, new_cap * sizeof (*stack));

		if (!stack) {
			return NULL;
		}

		parser -> stack = stack;
		parser -> stack_cap = new_cap;
	}

	item = &parser -> stack [parser -> stack_len ++];
	memset (item, 0, sizeof (*item));

	return item;
}

static json5_parser_item * json5_parser_stack_pop (json5_parser * parser)
{
	if (!parser -> stack_len) {
		return NULL;
	}

	return &parser -> stack [-- parser -> stack_len];
}

static void json5_parser_set_error (json5_parser * parser, char const * msg, ...)
{
	va_list args;
	char error [1024];
	size_t size;

	va_start (args, msg);
	size = vsnprintf (error, sizeof (error) - 1, msg, args);
	va_end (args);

	if (size > 0) {
		json5_value_set_string (&parser -> error, error, size);
	}
}

int json5_parser_init (json5_parser * parser) {
	json5_parser_item * item;

	memset (parser, 0, sizeof (*parser));

	item = json5_parser_stack_push (parser);

	if (!item) {
		return -1;
	}

	item -> value = &parser -> value;
	item -> state = JSON5_STATE_NONE;

	json5_parser_reset (parser);

	return 0;
}

void json5_parser_reset (json5_parser * parser)
{
	json5_parser_item * stack = parser -> stack;
	json5_parser_item * item;
	size_t stack_cap = parser -> stack_cap;

	json5_value_set_null (&parser -> value);
	json5_value_set_null (&parser -> obj_key);
	json5_value_set_null (&parser -> error);

	memset (parser, 0, sizeof (*parser));
	parser -> stack = stack;
	parser -> stack_cap = stack_cap;

	item = json5_parser_stack_push (parser);

	item -> value = &parser -> value;
	item -> state = JSON5_STATE_NONE;
}

void json5_parser_destroy (json5_parser * parser)
{
	if (parser -> stack) {
		free (parser -> stack);
	}

	json5_value_set_null (&parser -> value);
	json5_value_set_null (&parser -> obj_key);
	json5_value_set_null (&parser -> error);

	json5_value_set_null (&parser -> value);
	memset (parser, 0, sizeof (*parser));
}

int json5_parser_put_tokens (json5_parser * parser, json5_token const * tokens, size_t count)
{
	int res = 0;
	int state;
	json5_token const * token;
	json5_parser_item * item = &parser -> stack [parser -> stack_len - 1];
	json5_value * value;

	state = parser -> state;

	for (size_t i = 0; i < count; i ++) {
		token = &tokens [i];

		// parser states
		switch (state) {
			case JSON5_STATE_NONE: {
				switch (token -> type) {
					case JSON5_TOK_OBJ_OPEN: {
						json5_value_set_object (item -> value);
						state = JSON5_STATE_OBJ;
						break;
					}
					case JSON5_TOK_ARR_OPEN: {
						json5_value_set_array (item -> value);
						state = JSON5_STATE_ARR_BEGIN;
						break;
					}
					case JSON5_TOK_STRING: {
						if ((res = json5_value_set_string (item -> value, (char *) token -> token, token -> length)) != 0) {
							goto error;
						}

						state = JSON5_STATE_STRING;
						break;
					}
					case JSON5_TOK_NUMBER:
					case JSON5_TOK_NUMBER_FLOAT: {
						state = JSON5_STATE_NUMBER;
						break;
					}
					case JSON5_TOK_NAME: {
						state = JSON5_STATE_NAME;
						break;
					}
					case JSON5_TOK_NAME_SIGN: {
						state = JSON5_STATE_NAME_SIGN;
						break;
					}
					default: {
						json5_parser_set_error (parser, "Unexpected token on line %d:%d",
							token -> offset.lineno, token -> offset.colno);
						goto error;
						break;
					}
				}

				item -> state = JSON5_STATE_ROOT;
				break;
			}
			case JSON5_STATE_ROOT: {
				switch (token -> type) {
					case JSON5_TOK_END: {
						state = JSON5_STATE_END;
						break;
					}
					default: {
						json5_parser_set_error (parser, "Extra token in root context on line %d:%d",
							token -> offset.lineno, token -> offset.colno);
						goto error;
						break;
					}
				}
			}
			case JSON5_STATE_ARR: {
				break;
			}
			case JSON5_STATE_ARR_VAL: {
				break;
			}
			case JSON5_STATE_ARR_SEP: {
				break;
			}
			case JSON5_STATE_OBJ: {
				break;
			}
			case JSON5_STATE_OBJ_VAL: {
				break;
			}
			case JSON5_STATE_OBJ_SEP: {
				break;
			}
			case JSON5_STATE_OBJ_KEY_SEP: {
				break;
			}
			case JSON5_STATE_END: {
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

		// state actions
		switch (item -> state) {
			case JSON5_STATE_OBJ_BEGIN: {
				json5_value * value;

				value = item -> value;
				json5_value_set_object (value);
				item = json5_parser_stack_push (parser);

				if (!item) {
					goto alloc_error;
				}

				item -> value = value;
				//item -> state =

				// ...
				break;
			}
			case JSON5_STATE_OBJ_END: {
				break;
			}
			case JSON5_STATE_ARR_BEGIN: {
				item -> state = JSON5_STATE_ARR;
				break;
			}
			case JSON5_STATE_ARR_END: {
				break;
			}
			default: {
				break;
			}
		}
	}

	parser -> state = state;

	alloc_error: {
		json5_parser_set_error (parser, "Allocation error");
		goto error;
	}

	error: {
		item -> state = JSON5_STATE_ERROR;

		return -1;
	}

	return res;
}

int json5_parser_is_finished (json5_parser const * parser)
{
	return parser -> stack [parser -> stack_len - 1].state >= JSON5_STATE_END;
}

int json5_parser_has_error (json5_parser const * parser)
{
	return parser -> stack [parser -> stack_len - 1].state == JSON5_STATE_ERROR;
}

json5_value const * json5_parser_get_error (json5_parser const * parser)
{
	return &parser -> error;
}
