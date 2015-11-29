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

#ifndef _JSON5_WRITER_H_
#define _JSON5_WRITER_H_

#include <stdint.h>
#include <sys/types.h>
#include "json5-value.h"

typedef struct json5_writer json5_writer;
typedef int (* json5_writer_callback) (uint8_t const * string, size_t size, void * user_info);

struct json5_writer {
	size_t buffer_len;
	size_t buffer_cap;
	uint8_t * buffer;
	json5_writer_callback callback;
	void * user_info;
};

/**
 * Initialize writer
 */
extern int json5_writer_init (json5_writer * writer, json5_writer_callback callback, void * user_info);

/**
 * Destroy writer
 */
extern void json5_writer_destroy (json5_writer * writer);

/**
 * Write value and clear previous buffer
 */
extern int json5_writer_write (json5_writer * writer, json5_value const * value);

#endif /* ! _JSON5_WRITER_H_ */
