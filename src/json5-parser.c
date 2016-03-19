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
	JSON5_STATE_VALUE,
	JSON5_STATE_ARR_VAL,
	JSON5_STATE_ARR_SEP,
	JSON5_STATE_OBJ_KEY,
	JSON5_STATE_OBJ_VAL,
	JSON5_STATE_OBJ_SEP,
	JSON5_STATE_OBJ_KEY_SEP,
	JSON5_STATE_CONTAINER_END,
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
	json5_value * value = NULL;
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

	if (parser -> stack_len) {
		item = &parser -> stack [parser -> stack_len - 1];
		value = item -> value;
	}

	item = &parser -> stack [parser -> stack_len ++];
	memset (item, 0, sizeof (*item));
	item -> value = value;

	return item;
}

static json5_parser_item * json5_parser_stack_top (json5_parser const * parser)
{
	if (parser -> stack_len < 1) {
		return NULL;
	}

	return &parser -> stack [parser -> stack_len - 1];
}

static json5_parser_item * json5_parser_stack_pop (json5_parser * parser)
{
	if (parser -> stack_len <= 0) {
		return NULL;
	}

	parser -> stack_len --;

	if (parser -> stack_len <= 0) {
		return NULL;
	}

	return &parser -> stack [parser -> stack_len - 1];
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
	json5_value_set_null (&parser -> error);

	json5_value_set_null (&parser -> value);
	memset (parser, 0, sizeof (*parser));
}

int json5_parser_put_tokens (json5_parser * parser, json5_token const * tokens, size_t count)
{
	int res = 0;
	json5_token const * token = NULL;
	json5_parser_item * item;
	json5_value * value;

	item = json5_parser_stack_top (parser);

	for (size_t i = 0; i < count; i ++) {
		token = &tokens [i];

		// parser states
		switch (item -> state) {
			case JSON5_STATE_NONE: {
				item -> state = JSON5_STATE_ROOT;

				switch (token -> type) {
					case JSON5_TOK_OBJ_OPEN: {
						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_OBJ_KEY;
						break;
					}
					case JSON5_TOK_ARR_OPEN: {
						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_ARR_VAL;
						break;
					}
					case JSON5_TOK_STRING:
					case JSON5_TOK_NUMBER:
					case JSON5_TOK_NUMBER_FLOAT:
					case JSON5_TOK_NUMBER_BOOL:
					case JSON5_TOK_NULL:
					case JSON5_TOK_NAN:
					case JSON5_TOK_INFINITY: {
						item -> state = JSON5_STATE_VALUE;
						break;
					}
					default: {
						goto unexpected_token;
						break;
					}
				}

				if (!(item = json5_parser_stack_push (parser))) {
					goto alloc_error;
				}

				item -> state = JSON5_STATE_VALUE;
				break;
			}
			case JSON5_STATE_ROOT: {
				switch (token -> type) {
					case JSON5_TOK_END: {
						item -> state = JSON5_STATE_END;
						break;
					}
					default: {
						json5_parser_set_error (parser, "Extra token in root context on line %d:%d",
							token -> offset.lineno, token -> offset.colno);
						goto error;
						break;
					}
				}

				break;
			}
			case JSON5_STATE_ARR_VAL: {
				item -> state = JSON5_STATE_ARR_SEP;

				switch (token -> type) {
					case JSON5_TOK_ARR_OPEN: {
						if (!(value = json5_value_append_item (item -> value))) {
							goto alloc_error;
						}

						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_ARR_VAL;
						item -> value = value;

						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_VALUE;
						break;
					}
					case JSON5_TOK_OBJ_OPEN: {
						if (!(value = json5_value_append_item (item -> value))) {
							goto alloc_error;
						}

						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_OBJ_KEY;
						item -> value = value;

						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_VALUE;
						break;
					}
					case JSON5_TOK_STRING:
					case JSON5_TOK_NUMBER:
					case JSON5_TOK_NUMBER_FLOAT:
					case JSON5_TOK_NUMBER_BOOL:
					case JSON5_TOK_NULL:
					case JSON5_TOK_NAN:
					case JSON5_TOK_INFINITY: {
						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						if (!(value = json5_value_append_item (item -> value))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_VALUE;
						item -> value = value;
						break;
					}
					case JSON5_TOK_ARR_CLOSE: {
						item -> state = JSON5_STATE_CONTAINER_END;
						break;
					}
					default: {
						goto unexpected_token;
						break;
					}
				}

				break;
			}
			case JSON5_STATE_ARR_SEP: {
				switch (token -> type) {
					case JSON5_TOK_COMMA: {
						item -> state = JSON5_STATE_ARR_VAL;
						break;
					}
					case JSON5_TOK_ARR_CLOSE: {
						item -> state = JSON5_STATE_CONTAINER_END;
						break;
					}
					default: {
						goto unexpected_token;
						break;
					}
				}

				break;
			}
			case JSON5_STATE_OBJ_KEY: {
				item -> state = JSON5_STATE_OBJ_SEP;

				switch (token -> type) {
					case JSON5_TOK_NAME:
					case JSON5_TOK_STRING:
					case JSON5_TOK_NULL:
					case JSON5_TOK_NAN:
					case JSON5_TOK_INFINITY: {
						if (!(value = json5_value_set_prop (item -> value, (char *) token -> token, token -> length))) {
							goto alloc_error;
						}

						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_OBJ_KEY_SEP;
						item -> value = value;
						break;
					}
					case JSON5_TOK_OBJ_CLOSE: {
						item -> state = JSON5_STATE_CONTAINER_END;
						break;
					}
					default: {
						goto unexpected_token;
						break;
					}
				}

				break;
			}
			case JSON5_STATE_OBJ_KEY_SEP: {
				switch (token -> type) {
					case JSON5_TOK_COLON: {
						item -> state = JSON5_STATE_OBJ_VAL;
						break;
					}
					default: {
						goto unexpected_token;
						break;
					}
				}

				break;
			}
			case JSON5_STATE_OBJ_VAL: {
				item -> state = JSON5_STATE_OBJ_SEP;

				switch (token -> type) {
					case JSON5_TOK_STRING:
					case JSON5_TOK_NUMBER:
					case JSON5_TOK_NUMBER_FLOAT:
					case JSON5_TOK_NUMBER_BOOL:
					case JSON5_TOK_NULL:
					case JSON5_TOK_NAN:
					case JSON5_TOK_INFINITY: {
						item -> state = JSON5_STATE_VALUE;
						break;
					}
					case JSON5_TOK_ARR_OPEN: {
						item -> state = JSON5_STATE_ARR_VAL;

						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_VALUE;
						break;
					}
					case JSON5_TOK_OBJ_OPEN: {
						item -> state = JSON5_STATE_OBJ_KEY;

						if (!(item = json5_parser_stack_push (parser))) {
							goto alloc_error;
						}

						item -> state = JSON5_STATE_VALUE;
						break;
					}
					default: {
						goto unexpected_token;
						break;
					}
				}

				break;
			}
			case JSON5_STATE_OBJ_SEP: {
				switch (token -> type) {
					case JSON5_TOK_COMMA: {
						item -> state = JSON5_STATE_OBJ_KEY;
						break;
					}
					case JSON5_TOK_OBJ_CLOSE: {
						item -> state = JSON5_STATE_CONTAINER_END;
						break;
					}
					default: {
						goto unexpected_token;
						break;
					}
				}
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
			case JSON5_STATE_VALUE: {
				switch (token -> type) {
					case JSON5_TOK_STRING: {
						json5_value_set_string (item -> value, (char *) token -> token, token -> length);
						break;
					}
					case JSON5_TOK_NUMBER: {
						json5_value_set_int (item -> value, token -> value.i);
						break;
					}
					case JSON5_TOK_NUMBER_FLOAT: {
						json5_value_set_float (item -> value, token -> value.f);
						break;
					}
					case JSON5_TOK_NUMBER_BOOL: {
						json5_value_set_bool (item -> value, token -> value.i != 0);
						break;
					}
					case JSON5_TOK_NULL: {
						json5_value_set_null (item -> value);
						break;
					}
					case JSON5_TOK_NAN: {
						json5_value_set_nan (item -> value);
						break;
					}
					case JSON5_TOK_INFINITY: {
						json5_value_set_infinity (item -> value, (int) token -> value.i);
						break;
					}
					case JSON5_TOK_ARR_OPEN: {
						json5_value_set_array (item -> value);
						break;
					}
					case JSON5_TOK_OBJ_OPEN: {
						json5_value_set_object (item -> value);
						break;
					}
					default: {
						break;
					}
				}

				item = json5_parser_stack_pop (parser);
				break;
			}
			case JSON5_STATE_CONTAINER_END: {
				item = json5_parser_stack_pop (parser);
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
	}

	return res;

	unexpected_token: {
		if (token -> type == JSON5_TOK_END) {
			json5_parser_set_error (parser, "Premature end of file");
		}
		else {
			json5_parser_set_error (parser, "Unexpected token on line %d:%d",
				token -> offset.lineno, token -> offset.colno);
		}

		json5_parser_stack_top (parser) -> state = JSON5_STATE_ERROR;
		goto error;
	}

	alloc_error: {
		json5_parser_set_error (parser, "Allocation error");
		json5_parser_stack_top (parser) -> state = JSON5_STATE_ERROR;
		goto error;
	}

	error: {
		item -> state = JSON5_STATE_ERROR;

		return -1;
	}
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
	if (json5_parser_stack_top (parser) -> state != JSON5_STATE_ERROR) {
		return NULL;
	}

	return &parser -> error;
}
