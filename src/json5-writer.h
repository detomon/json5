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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <sys/types.h>
#include "json5-value.h"

typedef struct json5_writer json5_writer;

/**
 * A callback function definition used by the writer to output data to the user.
 */
typedef int (*json5_writer_func) (uint8_t const * string, size_t size, void * user_info);

/**
 * The writer object to encode a `json5_value` into a string.
 */
struct json5_writer {
	size_t buffer_len;
	size_t buffer_cap;
	uint8_t * buffer;
	json5_writer_func write;
	void * user_info;
};

/**
 * Initialize writer.
 */
extern int json5_writer_init (json5_writer * writer, json5_writer_func callback, void * user_info);

/**
 * Destroy writer.
 */
extern void json5_writer_destroy (json5_writer * writer);

/**
 * Write value and clear previous buffer.
 */
extern int json5_writer_write (json5_writer * writer, json5_value const * value);
