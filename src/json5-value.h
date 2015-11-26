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

#ifndef _JSON5_VALUE_H_
#define _JSON5_VALUE_H_

#include <stdint.h>
#include <sys/types.h>

typedef enum json5_type json5_type;
typedef enum json5_subtype json5_subtype;
typedef struct json5_value json5_value;
typedef struct json5_obj_prop json5_obj_prop;
typedef uint64_t json5_hash;

enum json5_type {
	JSON5_TYPE_NULL,
	JSON5_TYPE_NUMBER,
	JSON5_TYPE_STRING,
	JSON5_TYPE_ARRAY,
	JSON5_TYPE_OBJECT,
	JSON5_TYPE_INFINITY,
	JSON5_TYPE_NAN,
};

enum json5_subtype {
	JSON5_SUBTYPE_NONE,
	JSON5_SUBTYPE_INT,
	JSON5_SUBTYPE_FLOAT,
	JSON5_SUBTYPE_BOOL,
};

struct json5_value {
	json5_type type;
	json5_subtype subtype;
	union {
		union {
			int64_t i;
			double f;
		} num;
		struct {
			uint8_t * s;
			size_t len;
		} str;
		struct {
			size_t len;
			size_t cap;
			json5_value * itms;
		} arr;
		struct {
			size_t len;
			size_t cap;
			json5_obj_prop * itms;
		} obj;
	} val;
};

struct json5_obj_prop {
	json5_hash hash;
	char * key;
	size_t key_len;
	json5_value value;
};

/**
 *
 */
extern void json5_value_init (json5_value * value);

/**
 *
 */
extern void json5_value_set_int (json5_value * value, int64_t i);

/**
 *
 */
extern void json5_value_set_float (json5_value * value, double f);

/**
 *
 */
extern void json5_value_set_bool (json5_value * value, int b);

/**
 *
 */
extern void json5_value_set_nan (json5_value * value);

/**
 *
 */
extern void json5_value_set_infinity (json5_value * value, int sign);

/**
 *
 */
extern void json5_value_set_null (json5_value * value);

/**
 *
 */
extern int json5_value_set_string (json5_value * value, char const * str, size_t len);

/**
 *
 */
extern void json5_value_set_array (json5_value * value);

/**
 *
 */
extern void json5_value_set_object (json5_value * value);

/**
 *
 */
extern json5_value * json5_value_get_item (json5_value * value, size_t idx);

/**
 *
 */
extern json5_value * json5_value_append_item (json5_value * value);

/**
 *
 */
extern json5_value * json5_value_get_prop (json5_value * value, char const * key, size_t key_len);

/**
 *
 */
extern json5_value * json5_value_set_prop (json5_value * value, char const * key, size_t key_len);

/**
 *
 */
extern int json5_value_delete_prop (json5_value * value, char const * key, size_t key_len);

#endif /* ! _JSON5_VALUE_H_ */
