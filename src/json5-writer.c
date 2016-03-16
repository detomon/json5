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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json5-writer.h"

#define BUFFER_CAP 4096
#define PLACEHOLDER_KEY ((char *) 1)

enum
{
	CHAR_TYPE_ESCAPE = 1 << 0,
};

typedef struct {
	uint8_t flags;
	uint8_t seq;
} json5_char;

static json5_char char_types [128] =
{
	['"']  = {.flags = CHAR_TYPE_ESCAPE, .seq = '"'},
	['\b'] = {.flags = CHAR_TYPE_ESCAPE, .seq = 'b'},
	['\f'] = {.flags = CHAR_TYPE_ESCAPE, .seq = 'f'},
	['\n'] = {.flags = CHAR_TYPE_ESCAPE, .seq = 'n'},
	['\r'] = {.flags = CHAR_TYPE_ESCAPE, .seq = 'r'},
	['\t'] = {.flags = CHAR_TYPE_ESCAPE, .seq = 't'},
	['\v'] = {.flags = CHAR_TYPE_ESCAPE, .seq = 'v'},
};

static int json5_writer_write_value (json5_writer * writer, json5_value const * value);

int json5_writer_init (json5_writer * writer, uint32_t flags, json5_writer_func write, void * user_info) {
	memset (writer, 0, sizeof (*writer));

	writer -> flags = flags;
	writer -> buffer_cap = BUFFER_CAP;
	writer -> buffer = malloc (writer -> buffer_cap);

	if (!writer -> buffer) {
		json5_writer_destroy (writer);
		return -1;
	}

	writer -> write = write;
	writer -> user_info = user_info;

	return 0;
}

void json5_writer_destroy (json5_writer * writer) {
	if (writer -> buffer) {
		free (writer -> buffer);
	}

	memset (writer, 0, sizeof (*writer));
}

static int json5_writer_flush_buffer (json5_writer * writer) {
	int res;

	res = writer -> write (writer -> buffer, writer -> buffer_len, writer -> user_info);
	writer -> buffer_len = 0;

	return res;
}

static int json5_writer_write_byte (json5_writer * writer, int c) {
	int res;

	if (writer -> buffer_len >= writer -> buffer_cap) {
		res = json5_writer_flush_buffer (writer);

		if (res != 0) {
			return res;
		}
	}

	writer -> buffer [writer -> buffer_len ++] = c;

	return 0;
}

static int json5_writer_write_bytes (json5_writer * writer, uint8_t const * string, size_t size) {
	int res;
	size_t free_size;
	size_t write_size;

	while (size) {
		free_size = writer -> buffer_cap - writer -> buffer_len;
		write_size = size < free_size ? size : free_size;

		memcpy (&writer -> buffer [writer -> buffer_len], string, write_size);
		writer -> buffer_len += write_size;
		size -= write_size;
		string += write_size;

		res = json5_writer_flush_buffer (writer);

		if (res != 0) {
			return res;
		}
	}

	return 0;
}

static size_t json5_writer_write_escape_sequence (uint8_t const ** string_ref, uint8_t buffer [], unsigned c)
{
	unsigned count;
	unsigned value = 0;
	unsigned su1, su2;
	size_t len = 0;
	uint8_t const * string = *string_ref;

	if ((c & 0xE0) == 0xC0) {
		value = c & 0x1F;
		count = 1;
	}
	else if ((c & 0xF0) == 0xE0) {
		value = c & 0xF;
		count = 2;
	}
	else if ((c & 0xF8) == 0xF0) {
		value = c & 0x7;
		count = 3;
	}
	else {
		value = 0xDC00 | c;
		count = 0;
	}

	while (count) {
		c = *string ++;
		value = (value << 6) | (c & 0x3F);
		count --;
	}

	if (value <= 0xFFFF) {
		snprintf ((char *) &buffer [len], 8, "\\u%04x", value);
		len += 6;
	}
	// write surrogates
	else {
		value -= 0x10000;
		su1 = ((value >> 10) & 0x3FF) + 0xD800;
		su2 = (value & 0x3FF) + 0xDC00;
		snprintf ((char *) &buffer [len], 8, "\\u%04x", su1);
		len += 6;
		snprintf ((char *) &buffer [len], 8, "\\u%04x", su2);
		len += 6;
	}

	*string_ref = string;

	return len;
}

static int json5_writer_write_escaped_bytes (json5_writer * writer, uint8_t const * string, size_t size) {
	unsigned c;
	unsigned res;
	size_t free_size;
	size_t write_size;
	uint8_t const * string_start;
	uint8_t const * string_end;
	unsigned escape_uc = !(writer -> flags & JSON5_WRITER_FLAG_NO_ESCAPE);

	while (size) {
		free_size = (writer -> buffer_cap - writer -> buffer_len);
		// assume every char has to be escaped with surrogates \uXXXX\uXXXX
		write_size = size < free_size / 12 ? size : free_size / 12;
		string_start = string;
		string_end = &string [write_size];

		while (string < string_end) {
			c = *string ++;

			if (c < 128) {
				if (char_types [c].flags) {
					if (char_types [c].flags & CHAR_TYPE_ESCAPE) {
						writer -> buffer [writer -> buffer_len ++] = '\\';
						writer -> buffer [writer -> buffer_len ++] = char_types [c].seq;
					}
				}
				else {
					writer -> buffer [writer -> buffer_len ++] = c;
				}
			}
			else if (escape_uc) {
				writer -> buffer_len += json5_writer_write_escape_sequence (&string, &writer -> buffer [writer -> buffer_len], c);
			}
			else {
				writer -> buffer [writer -> buffer_len ++] = c;
			}
		}

		size -= (string - string_start);

		res = json5_writer_flush_buffer (writer);

		if (res != 0) {
			return res;
		}
	}

	return 0;
}

static int json5_writer_write_number (json5_writer * writer, json5_value const * value) {
	uint8_t number [64];

	switch (value -> type) {
		default:
		case JSON5_TYPE_INT: {
			snprintf ((char *) number, sizeof (number), "%lld", value -> num.i);
			break;
		}
		case JSON5_TYPE_FLOAT: {
			snprintf ((char *) number, sizeof (number), "%.17g", value -> num.f);
			break;
		}
		case JSON5_TYPE_BOOL: {
			strncpy ((char *) number, value -> num.i ? "true" : "false", sizeof (number));
			break;
		}
	}

	return json5_writer_write_bytes (writer, number, strlen ((void *) number));
}

static int json5_writer_write_infinity (json5_writer * writer, json5_value const * value) {
	uint8_t const * string = (uint8_t const *) (value -> num.i > 0 ? "Infinity" : "-Infinity");

	return json5_writer_write_bytes (writer, (uint8_t const *) string, strlen ((void const *) string));
}

static int json5_writer_write_nan (json5_writer * writer, json5_value const * value) {
	return json5_writer_write_bytes (writer, (uint8_t const *) "NaN", 3);
}

static int json5_writer_write_null (json5_writer * writer, json5_value const * value) {
	return json5_writer_write_bytes (writer, (uint8_t const *) "null", 4);
}

static int json5_writer_write_string (json5_writer * writer, json5_value const * value) {
	int res;

	if ((res = json5_writer_write_byte (writer, '"')) != 0) {
		return -1;
	}

	if ((res = json5_writer_write_escaped_bytes (writer, value -> str.s, value -> str.len)) != 0) {
		return res;
	}

	if ((res = json5_writer_write_byte (writer, '"')) != 0) {
		return -1;
	}

	return 0;
}

static int json5_writer_write_array (json5_writer * writer, json5_value const * value) {
	int res;
	json5_value const * item;

	if ((res = json5_writer_write_byte (writer, '[')) != 0) {
		return -1;
	}

	for (size_t i = 0; i < value -> arr.len; i ++) {
		item = &value -> arr.itms [i];

		if ((res = json5_writer_write_value (writer, item)) != 0) {
			return -1;
		}

		if (i < value -> arr.len - 1) {
			if ((res = json5_writer_write_byte (writer, ',')) != 0) {
				return -1;
			}
		}
	}

	if ((res = json5_writer_write_byte (writer, ']')) != 0) {
		return -1;
	}

	return 0;
}

static int json5_writer_write_prop (json5_writer * writer, json5_obj_prop const * prop) {
	int res;

	if ((res = json5_writer_write_byte (writer, '"')) != 0) {
		return -1;
	}

	if ((res = json5_writer_write_escaped_bytes (writer, (void const *) prop -> key, prop -> key_len)) != 0) {
		return -1;
	}

	if ((res = json5_writer_write_byte (writer, '"')) != 0) {
		return -1;
	}

	if ((res = json5_writer_write_byte (writer, ':')) != 0) {
		return -1;
	}

	if ((res = json5_writer_write_value (writer, &prop -> value)) != 0) {
		return -1;
	}

	return 0;
}

static int json5_writer_write_object (json5_writer * writer, json5_value const * value) {
	int res;
	size_t cap = value -> obj.len;
	json5_obj_prop const * item;

	if ((res = json5_writer_write_byte (writer, '{')) != 0) {
		return -1;
	}

	for (size_t i = 0; i < value -> obj.cap; i ++) {
		item = &value -> obj.itms [i];

		if (item -> key > PLACEHOLDER_KEY) {
			if ((res = json5_writer_write_prop (writer, item)) != 0) {
				return -1;
			}

			if (cap -- > 1) {
				if ((res = json5_writer_write_byte (writer, ',')) != 0) {
					return -1;
				}
			}
		}
	}

	if ((res = json5_writer_write_byte (writer, '}')) != 0) {
		return -1;
	}

	return 0;
}

static int json5_writer_write_value (json5_writer * writer, json5_value const * value) {
	switch (value -> type & JSON5_TYPE_MASK) {
		case JSON5_TYPE_NULL: {
			return json5_writer_write_null (writer, value);
			break;
		}
		case JSON5_TYPE_NUMBER: {
			return json5_writer_write_number (writer, value);
			break;
		}
		case JSON5_TYPE_STRING: {
			return json5_writer_write_string (writer, value);
			break;
		}
		case JSON5_TYPE_ARRAY: {
			return json5_writer_write_array (writer, value);
			break;
		}
		case JSON5_TYPE_OBJECT: {
			return json5_writer_write_object (writer, value);
			break;
		}
		case JSON5_TYPE_INFINITY: {
			return json5_writer_write_infinity (writer, value);
			break;
		}
		case JSON5_TYPE_NAN: {
			return json5_writer_write_nan (writer, value);
			break;
		}
	}

	return 0;
}

int json5_writer_write (json5_writer * writer, json5_value const * value) {
	int res;

	writer -> buffer_len = 0;

	res = json5_writer_write_value (writer, value);

	if (res == 0) {
		res = json5_writer_flush_buffer (writer);
	}

	return res;
}
