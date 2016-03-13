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

#ifdef __cplusplus
extern "C" {
#endif

#include "json5-tokenizer.h"
#include "json5-value.h"
#include "json5-writer.h"

/**
 * Decode a JSON5 string and output value to `out_value`.
 *
 * The value has to be freed with `json5_value_set_null` if not used anymore.
 */
extern int json5_decode (char const * string, size_t size, json5_value * out_value);

/**
 * Encode a `json5_value` into a string.
 *
 * The string can has to be freed with `free` if not used anymore.
 */
extern int json5_encode (json5_value * value, char const ** out_string, size_t * out_size);

#ifdef __cplusplus
}
#endif
