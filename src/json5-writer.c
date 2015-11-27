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
#include <stdlib.h>
#include <string.h>
#include "json5-writer.h"

#define BUFFER_CAP 4096
#define PLACEHOLDER_KEY ((void *) -1)

int json5_writer_init (json5_writer * writer, json5_writer_callback callback, void * user_info)
{
	memset (writer, 0, sizeof (*writer));

	writer -> buffer_cap = BUFFER_CAP;
	writer -> buffer = malloc (writer -> buffer_cap);

	if (!writer -> buffer) {
		json5_writer_destroy (writer);
		return -1;
	}

	writer -> callback = callback;
	writer -> user_info = user_info;

	return 0;
}

void json5_writer_destroy (json5_writer * writer)
{
	if (writer -> buffer) {
		free (writer -> buffer);
	}

	memset (writer, 0, sizeof (*writer));
}

static int json5_writer_flush_buffer (json5_writer * writer)
{
	int res;

	res = writer -> callback (writer -> buffer, writer -> buffer_len, writer -> user_info);
	writer -> buffer_len = 0;

	return res;
}

static int json5_writer_write_byte (json5_writer * writer, int c)
{
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

static int json5_writer_write_bytes (json5_writer * writer, uint8_t const * string, size_t size)
{
	int res;
	size_t free_size;
	size_t write_size;

	while (size) {
		if (writer -> buffer_len >= writer -> buffer_cap) {
			res = json5_writer_flush_buffer (writer);

			if (res != 0) {
				return res;
			}
		}

		free_size = writer -> buffer_cap - writer -> buffer_len;
		write_size = size < free_size ? size : free_size;

		memcpy (&writer -> buffer [writer -> buffer_len], string, write_size);
		writer -> buffer_len += write_size;
		size -= write_size;
		string += write_size;
	}

	return 0;
}

static int json5_writer_write_value (json5_writer * writer, json5_value const * value);

static int json5_writer_write_number (json5_writer * writer, json5_value const * value)
{
	uint8_t number [64];

	switch (value -> subtype) {
		default:
		case JSON5_SUBTYPE_INT: {
			snprintf ((void *) number, sizeof (number), "%lld", value -> val.num.i);
			break;
		}
		case JSON5_SUBTYPE_FLOAT: {
			snprintf ((void *) number, sizeof (number), "%.17g", value -> val.num.f);
			break;
		}
		case JSON5_SUBTYPE_BOOL: {
			strncpy ((void *) number, value -> val.num.i ? "true" : "false", sizeof (number));
			break;
		}
	}

	return json5_writer_write_bytes (writer, number, strlen ((void *) number));
}

static int json5_writer_write_infinity (json5_writer * writer, json5_value const * value)
{
	char const * string = value -> val.num.i > 0 ? "Infinity" : "-Infinity";

	return json5_writer_write_bytes (writer, (void const *) string, strlen ((void const *) string));
}

static int json5_writer_write_nan (json5_writer * writer, json5_value const * value)
{
	return json5_writer_write_bytes (writer, (void const *) "NaN", 3);
}

static int json5_writer_write_null (json5_writer * writer, json5_value const * value)
{
	return json5_writer_write_bytes (writer, (void const *) "null", 4);
}

static int json5_writer_write_string (json5_writer * writer, json5_value const * value)
{
	int res;

	if ((res = json5_writer_write_byte (writer, '"')) != 0) {
		return -1;
	}

	if ((res = json5_writer_write_bytes (writer, value -> val.str.s, value -> val.str.len)) != 0) {
		return res;
	}

	if ((res = json5_writer_write_byte (writer, '"')) != 0) {
		return -1;
	}

	return 0;
}

static int json5_writer_write_array (json5_writer * writer, json5_value const * value)
{
	int res;
	json5_value const * item;

	if ((res = json5_writer_write_byte (writer, '[')) != 0) {
		return -1;
	}

	for (size_t i = 0; i < value -> val.arr.len; i ++) {
		item = &value -> val.arr.itms [i];

		if ((res = json5_writer_write_value (writer, item)) != 0) {
			return -1;
		}

		if (i < value -> val.arr.len - 1) {
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

static int json5_writer_write_prop (json5_writer * writer, json5_obj_prop const * prop)
{
	int res;

	if ((res = json5_writer_write_bytes (writer, (void const *) prop -> key, prop -> key_len)) != 0) {
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

static int json5_writer_write_object (json5_writer * writer, json5_value const * value)
{
	int res;
	size_t cap = value -> val.obj.len;
	json5_obj_prop const * item;

	if ((res = json5_writer_write_byte (writer, '{')) != 0) {
		return -1;
	}

	for (size_t i = 0; i < value -> val.obj.cap; i ++) {
		item = &value -> val.obj.itms [i];

		if (item -> key && item -> key != PLACEHOLDER_KEY) {
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

static int json5_writer_write_value (json5_writer * writer, json5_value const * value)
{
	switch (value -> type) {
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
}

int json5_writer_write (json5_writer * writer, json5_value const * value)
{
	int res;

	writer -> buffer_len = 0;

	res = json5_writer_write_value (writer, value);

	if (res == 0) {
		res = json5_writer_flush_buffer (writer);
	}

	return res;
}
