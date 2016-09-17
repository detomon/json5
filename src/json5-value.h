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

typedef enum json5_type json5_type;
typedef struct json5_value json5_value;
typedef struct json5_obj_prop json5_obj_prop;
typedef struct json5_obj_itor json5_obj_itor;
typedef uint64_t json5_hash;

/**
 * Defines value types.
 */
enum json5_type {
	JSON5_TYPE_NULL,
	JSON5_TYPE_BOOL,
	JSON5_TYPE_INT,
	JSON5_TYPE_FLOAT,
	JSON5_TYPE_INFINITY,
	JSON5_TYPE_NAN,
	JSON5_TYPE_STRING,
	JSON5_TYPE_ARRAY,
	JSON5_TYPE_OBJECT,
};

/**
 * Defines value container.
 */
struct json5_value {
	json5_type type;
	union {
		union {
			int64_t i;
			double f;
		} num;
		struct {
			uint8_t * s;
			size_t len;
			size_t cap;
		} str;
		struct {
			json5_value * itms;
			size_t len;
			size_t cap;
		} arr;
		struct {
			json5_obj_prop * itms;
			size_t len;
			size_t cap;
		} obj;
	};
};

/**
 * Defines object property with key and value.
 */
struct json5_obj_prop {
	json5_hash hash;
	uint8_t * key;
	size_t key_len;
	json5_value value;
};

/**
 * Defines an object key-value iterator.
 */
struct json5_obj_itor {
	json5_value const * obj;
	json5_obj_prop * prop;
};

/**
 * Initializes a statically allocated `json5_value` with `null`.
 *
 * Values with cleared memory are also valid.
 */
#define JSON5_VALUE_INIT ((json5_value) {0})

/**
 * Set integer value.
 */
extern void json5_value_set_int (json5_value * value, int64_t i);

/**
 * Set float value.
 */
extern void json5_value_set_float (json5_value * value, double f);

/**
 * Set boolean value.
 *
 * Value is considered `false` if `b` is 0 otherwise `true`.
 * Replaces current value content.
 */
extern void json5_value_set_bool (json5_value * value, int b);

/**
 * Set to `NaN` (Not a Number).
 */
extern void json5_value_set_nan (json5_value * value);

/**
 * Set value to `Infinity`.
 *
 * If sign is >= 0, the value is positive infinite `+Infinity`
 * otherwise negative infinite `-Infinity`.
 */
extern void json5_value_set_infinity (json5_value * value, int sign);

/**
 * Set value to `null`.
 */
extern void json5_value_set_null (json5_value * value);

/**
 * Set string value.
 */
extern int json5_value_set_string (json5_value * value, char const * str, size_t len);

/**
 * Set to empty array.
 *
 * If the value is already an array, nothing is done.
 */
extern void json5_value_set_array (json5_value * value);

/**
 * Set to empty object
 *
 * If the value is already an object, nothing is done.
 */
extern void json5_value_set_object (json5_value * value);

/**
 * Get reference for value at specified array index.
 *
 * If index does not exists or value is not an array, NULL is returned.
 */
extern json5_value * json5_value_get_item (json5_value * value, size_t idx);

/**
 * Append value at end of array.
 *
 * Initializes appended value with `null`.
 * If value is not an array, NULL is returned.
 * Returns reference to the appended value.
 */
extern json5_value * json5_value_append_item (json5_value * value);

/**
 * Get reference to property of object.
 *
 * If no property with given key is found or `value` is not an object, NULL is
 * returned.
 */
extern json5_value * json5_value_get_prop (json5_value * value, char const * key, size_t key_len);

/**
 * Set or replace property with key.
 *
 * Initializes the value with `null`.
 * If `value` is not an object, NULL is returned.
 * If `key_len` is -1, the length is determined with `strlen`.
 * Replaces current value content.
 * Returns reference to inserted property.
 */
extern json5_value * json5_value_set_prop (json5_value * value, char const * key, size_t key_len);

/**
 * Delete property with key.
 *
 * Returns 0 if no property with given key is found or `value` is not an object.
 */
extern int json5_value_delete_prop (json5_value * value, char const * key, size_t key_len);

/**
 * Initialize object iterator.
 *
 * Returns -1 if `obj` is not an object.
 */
extern int json5_obj_itor_init (json5_obj_itor * itor, json5_value const * obj);

/**
 * Get next object key and value.
 *
 * Returns 1 while values are available.
 */
extern int json5_obj_itor_next (json5_obj_itor * itor, char const ** out_key, size_t * out_key_len, json5_value ** out_value);

/**
 * Sets the global hash table seed.
 *
 * To improve security, set this value once to a random integer before creating
 * any object values. This is to prevent malicious hash collision attacks.
 */
extern void json5_set_hash_seed (json5_hash seed);
