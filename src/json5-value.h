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
	JSON5_TYPE_NULL,     ///< null
	JSON5_TYPE_BOOL,     ///< bool
	JSON5_TYPE_INT,      ///< 42
	JSON5_TYPE_FLOAT,    ///< 42.3
	JSON5_TYPE_INFINITY, ///< +Infinity or -Infinity
	JSON5_TYPE_NAN,      ///< NaN
	JSON5_TYPE_STRING,   ///< "abc"
	JSON5_TYPE_ARRAY,    ///< [...]
	JSON5_TYPE_OBJECT,   ///< {...}
};

/**
 * Defines a value container.
 */
struct json5_value {
	json5_type type; ///< Value type.
	struct {
		union {
			int64_t ival;           ///< Integer value.
			double fval;            ///< Float value.
			uint8_t * sval;         ///< String value.
			json5_value * items;    ///< Array items.
			json5_obj_prop * props; ///< Object properties.
		};
		size_t len; ///< Number of container item.
		size_t cap; ///< Container capacity.
	};
};

/**
 * Defines object property with key and value.
 */
struct json5_obj_prop {
	json5_hash hash;   ///< The property hash.
	uint8_t * key;     ///< The property key.
	size_t key_len;    ///< The property key length in bytes.
	json5_value value; ///< The property value.
};

/**
 * Defines an object key-value iterator.
 */
struct json5_obj_itor {
	json5_value const * obj; ///< The object to iterator
	json5_obj_prop * prop;   ///< The current property.
};

/**
 * Constant to initialize a statically allocated `json5_value` with `null`.
 * Values with cleared memory are also valid.
 */
#define JSON5_VALUE_INIT ((json5_value) {0})

/**
 * Set integer value.
 *
 * @param value The value to set to an integer.
 * @param i The integer value to set.
 */
static inline void json5_value_set_int (json5_value * value, int64_t i);

/**
 * Set float value.
 *
 * @param value The value to set to a float.
 * @param f The float value to set.
 */
static inline void json5_value_set_float (json5_value * value, double f);

/**
 * Set boolean value. Value is considered `false` if @p b is 0 otherwise `true`.
 *
 * @param value The value to set to a bool.
 * @param b The bool value to set.
 */
static inline void json5_value_set_bool (json5_value * value, int b);

/**
 * Set to `NaN` (Not a Number).
 *
 * @param value The value to set to NaN.
 */
static inline void json5_value_set_nan (json5_value * value);

/**
 * Set value to `Infinity`. If @p sign is >= 0, the value is positive infinite
 * `+Infinity` otherwise negative infinite `-Infinity`.
 *
 * @param value The value to set to infinity.
 * @param sign If >= 0, the value is positive infinite `+Infinity` otherwise
 * negative infinite `-Infinity`.
 */
static inline void json5_value_set_infinity (json5_value * value, int sign);

/**
 * Set value to `null`.
 *
 * @param value The value to set to `null`.
 */
static inline void json5_value_set_null (json5_value * value);

/**
 * Set string value.
 *
 * @param value The value to set to a string.
 * @param str The string to set. Can contain `NUL` bytes.
 * @param len The string length in bytes.
 *
 * @return 0 on success otherwise a value != 0 indicating an allocation error.
 */
extern int json5_value_set_string (json5_value * value, char const * str, size_t len);

/**
 * Set to empty array. If the value is already an array, nothing is done.
 *
 * @param value Set value to set to an empty array.
 */
static inline void json5_value_set_array (json5_value * value);

/**
 * Set to empty object. If the value is already an object, nothing is done.
 *
 * @param value Set value to set to an empty object.
 */
static inline void json5_value_set_object (json5_value * value);

/**
 * Set value type and and reset current value.
 *
 * @param value The value to set to a new type.
 * @param type The new type to set.
 */
extern void json5_value_reset (json5_value * value, json5_type type);

/**
 * Get array item at specified index.
 *
 * @param value The array value.
 * @param idx The index of the array item to retreive.
 *
 * @return The item at the specified index otherwise `NULL` if @p value is not
 * an array @p idx is out of bounds.
 */
extern json5_value * json5_value_get_item (json5_value * value, size_t idx);

/**
 * Append value at end of array and return its pointer. Initializes appended
 * item with `null`.
 *
 * @param value The array value to append an item to.
 *
 * @return The appended item otherwise `NULL` if @p value is not an array value
 * or an allocation error occured.
 */
extern json5_value * json5_value_append_item (json5_value * value);

/**
 * Get property of object @p value with given @p key.
 *
 * @param value The object value to retrieve the property.
 * @param key The key of the property to retrieve.
 * @param key_len The property key length in bytes.
 *
 * @return The property value otherwise `NULL` if no property with specified @p
 * key exists or @p value is not an object value.
 */
extern json5_value * json5_value_get_prop (json5_value * value, char const * key, size_t key_len);

/**
 * Set or replace object property with key. An existing property value is set to
 * `null` if @p replace is 1. If a property with the given @p key already exists
 * and @p replace is 0 `NULL` is returned.
 *
 * @param value The object value to set a property.
 * @param key The property key to set.
 * @param key_len The property key length in bytes.
 * @param replace If an existing property should be replaced.
 *
 * @return The property with the given @p key.
 */
extern json5_value * json5_value_set_prop (json5_value * value, char const * key, size_t key_len, int replace);

/**
 * Delete object property with key.
 *
 * @param value The object value to delete a property from.
 * @param key The property key to delete.
 * @param key_len The property key length in bytes.
 *
 * @return 1 if the property was deleted otherwise 0 if no property with given
 * @p key was found or @p value is not an object.
 */
extern int json5_value_delete_prop (json5_value * value, char const * key, size_t key_len);

/**
 * Transfers the value of @p source to @p target and clears @p source. If @p
 * source is `NULL` @p target is set to `null`.
 *
 * @param target The value to transfer a value to
 * @param source The value to transfer a value from
 */
extern void json5_value_transfer (json5_value * target, json5_value * source);

/**
 * Initialize an object iterator.
 *
 * @param itor The iterator to initialize.
 * @param obj The object value to iterate.
 *
 * @return 0 on success otherwise a value != 0 if @obj is not an object value.
 */
extern int json5_obj_itor_init (json5_obj_itor * itor, json5_value const * obj);

/**
 * Get next key-value pair.
 *
 * @param itor Iterator to iterate.
 * @param out_key A reference set to the current property key.
 * @param out_key_len A reference set to the current property key length in bytes.
 * @param out_value A reference set to the current property value.
 *
 * @return 1 as long as more properties exist.
 */
extern int json5_obj_itor_next (json5_obj_itor * itor, char const ** out_key, size_t * out_key_len, json5_value ** out_value);

/**
 * Sets the global hash table seed. To improve security, set this value once to
 * a random integer before creating any object values. This is to prevent
 * malicious hash collision attacks.
 *
 * @param A random integer.
 */
extern void json5_set_hash_seed (json5_hash seed);


static inline void json5_value_set_int (json5_value * value, int64_t i) {
	if (value -> type != JSON5_TYPE_INT) {
		if (value -> type >= JSON5_TYPE_STRING) {
			json5_value_reset (value, JSON5_TYPE_INT);
		}
		else {
			value -> type = JSON5_TYPE_INT;
		}
	}

	value -> ival = i;
}

static inline void json5_value_set_float (json5_value * value, double f) {
	if (value -> type != JSON5_TYPE_FLOAT) {
		if (value -> type >= JSON5_TYPE_STRING) {
			json5_value_reset (value, JSON5_TYPE_FLOAT);
		}
		else {
			value -> type = JSON5_TYPE_FLOAT;
		}
	}

	value -> fval = f;
}

static inline void json5_value_set_bool (json5_value * value, int b) {
	if (value -> type != JSON5_TYPE_BOOL) {
		if (value -> type >= JSON5_TYPE_STRING) {
			json5_value_reset (value, JSON5_TYPE_BOOL);
		}
		else {
			value -> type = JSON5_TYPE_BOOL;
		}
	}

	value -> ival = b != 0;
}

static inline void json5_value_set_nan (json5_value * value) {
	if (value -> type != JSON5_TYPE_NAN) {
		if (value -> type >= JSON5_TYPE_STRING) {
			json5_value_reset (value, JSON5_TYPE_NAN);
		}
		else {
			value -> type = JSON5_TYPE_NAN;
		}
	}
}

static inline void json5_value_set_infinity (json5_value * value, int sign) {
	if (value -> type != JSON5_TYPE_INFINITY) {
		if (value -> type >= JSON5_TYPE_STRING) {
			json5_value_reset (value, JSON5_TYPE_INFINITY);
		}
		else {
			value -> type = JSON5_TYPE_INFINITY;
		}
	}

	value -> ival = sign < 0;
}

static inline void json5_value_set_null (json5_value * value) {
	if (value -> type != JSON5_TYPE_NULL) {
		if (value -> type >= JSON5_TYPE_STRING) {
			json5_value_reset (value, JSON5_TYPE_NULL);
		}
		else {
			value -> type = JSON5_TYPE_NULL;
		}
	}
}

static inline void json5_value_set_array (json5_value * value) {
	if (value -> type != JSON5_TYPE_ARRAY) {
		if (value -> type >= JSON5_TYPE_STRING) {
			json5_value_reset (value, JSON5_TYPE_ARRAY);
		}
		else {
			value -> type = JSON5_TYPE_ARRAY;
			value -> items = NULL;
		}
	}
}

static inline void json5_value_set_object (json5_value * value) {
	if (value -> type != JSON5_TYPE_OBJECT) {
		if (value -> type >= JSON5_TYPE_STRING) {
			json5_value_reset (value, JSON5_TYPE_OBJECT);
		}
		else {
			value -> type = JSON5_TYPE_OBJECT;
			value -> props = NULL;
		}
	}
}
