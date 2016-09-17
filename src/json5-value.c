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

#include <stdlib.h>
#include <string.h>
#include "json5-value.h"

#define ARRAY_MIN_CAP 8
#define OBJECT_MIN_CAP 8
#define PLACEHOLDER_KEY ((uint8_t *) 1)

static json5_hash hash_table_seed = 0XD4244CD25E94BDBBULL;

static uint8_t * string_copy (uint8_t const * str, size_t len) {
	uint8_t * new_str = malloc (len + 1);

	if (!new_str) {
		return NULL;
	}

	memcpy (new_str, str, len);
	new_str [len] = '\0';

	return new_str;
}

static void json5_value_reset (json5_value * value, json5_type type);

/**
 * Delete string `value`.
 */
static void json5_value_delete_string (json5_value * value) {
	if (value -> sval) {
		free (value -> sval);
	}
}

/**
 * Delete items of array `value`.
 */
static void json5_value_delete_array (json5_value * value) {
	for (size_t i = 0; i < value -> len; i ++) {
		json5_value_reset (&value -> items [i], JSON5_TYPE_NULL);
	}

	if (value -> items) {
		free (value -> items);
	}
}

/**
 * Delete key-value pairs object object `value`.
 */
static void json5_value_delete_object (json5_value * value) {
	json5_obj_prop * prop;

	for (size_t i = 0; i < value -> cap; i ++) {
		prop = &value -> props [i];

		if (prop -> key > PLACEHOLDER_KEY) {
			json5_value_reset (&prop -> value, JSON5_TYPE_NULL);
		}
	}

	if (value -> props) {
		free (value -> props);
	}
}

/**
 * Delete current `value` and set new `type`.
 */
static void json5_value_reset (json5_value * value, json5_type type) {
	if (value -> type == type) {
		return;
	}

	switch (value -> type) {
		case JSON5_TYPE_STRING: {
			json5_value_delete_string (value);
			break;
		}
		case JSON5_TYPE_ARRAY: {
			json5_value_delete_array (value);
			break;
		}
		case JSON5_TYPE_OBJECT: {
			json5_value_delete_object (value);
			break;
		}
		default: {
			break;
		}
	}

	*value = JSON5_VALUE_INIT;
	value -> type = type;
}

void json5_value_set_int (json5_value * value, int64_t i) {
	json5_value_reset (value, JSON5_TYPE_INT);
	value -> ival = i;
}

void json5_value_set_float (json5_value * value, double f) {
	json5_value_reset (value, JSON5_TYPE_FLOAT);
	value -> fval = f;
}

void json5_value_set_bool (json5_value * value, int b) {
	json5_value_reset (value, JSON5_TYPE_BOOL);
	value -> ival = b != 0;
}

void json5_value_set_nan (json5_value * value) {
	json5_value_reset (value, JSON5_TYPE_NAN);
}

void json5_value_set_infinity (json5_value * value, int sign) {
	json5_value_reset (value, JSON5_TYPE_INFINITY);
	value -> ival = sign >= 0 ? 1 : -1;
}

void json5_value_set_null (json5_value * value) {
	json5_value_reset (value, JSON5_TYPE_NULL);
}

int json5_value_set_string (json5_value * value, char const * str, size_t len) {
	uint8_t * new_str;

	if (len == (size_t) -1) {
		len = strlen (str);
	}

	new_str = string_copy ((uint8_t const *) str, len);

	if (!new_str) {
		return -1;
	}

	json5_value_reset (value, JSON5_TYPE_STRING);
	value -> len = len;
	value -> cap = len;
	value -> sval = new_str;

	return 0;
}

void json5_value_set_array (json5_value * value) {
	json5_value_reset (value, JSON5_TYPE_ARRAY);
}

void json5_value_set_object (json5_value * value) {
	json5_value_reset (value, JSON5_TYPE_OBJECT);
}

json5_value * json5_value_get_item (json5_value * value, size_t idx) {
	if (value -> type != JSON5_TYPE_ARRAY) {
		return NULL;
	}

	if (idx > value -> len) {
		return NULL;
	}

	return &value -> items [idx];
}

json5_value * json5_value_append_item (json5_value * value) {
	json5_value * item;
	json5_value * new_items;
	size_t new_cap;

	if (value -> type != JSON5_TYPE_ARRAY) {
		return NULL;
	}

	if (value -> len >= value -> cap) {
		new_cap = value -> cap ? value -> cap * 2 : ARRAY_MIN_CAP;
		new_items = realloc (value -> items, new_cap * sizeof (*item));

		if (!new_items) {
			return NULL;
		}

		value -> cap = new_cap;
		value -> items = new_items;
	}

	item = &value -> items [value -> len ++];
	*item = JSON5_VALUE_INIT;

	return item;
}

static json5_hash json5_get_hash (char const * key, size_t key_len) {
	json5_hash hash = hash_table_seed;

	for (size_t i = 0; i < key_len; i ++) {
		hash = (hash * 100003) ^ key [i];
	}

	return hash;
}

static json5_obj_prop * json5_prop_lookup (json5_obj_prop * props, size_t mask, json5_hash hash, uint8_t const * key, size_t key_len) {
	json5_hash i = hash, perturb = hash;
	json5_obj_prop * prop = &props [i & mask];

	while (prop -> key) {
		if (prop -> hash == hash) {
			if (prop -> key != PLACEHOLDER_KEY && memcmp (prop -> key, key, key_len) == 0) {
				break;
			}
		}

		i += (perturb >>= 5) + 1;
		prop = &props [i & mask];
	}

	return prop;
}

json5_value * json5_value_get_prop (json5_value * value, char const * key, size_t key_len) {
	json5_hash hash;
	json5_obj_prop * prop;

	if (value -> type != JSON5_TYPE_OBJECT) {
		return NULL;
	}

	if (value -> cap == 0) {
		return NULL;
	}

	if (key_len == (size_t) -1) {
		key_len = strlen (key);
	}

	hash = json5_get_hash (key, key_len);
	prop = json5_prop_lookup (value -> props, value -> cap - 1, hash, (uint8_t const *) key, key_len);

	if (prop -> key > PLACEHOLDER_KEY) {
		return &prop -> value;
	}

	return NULL;
}

static int json5_object_grow (json5_value * value) {
	size_t new_cap = value -> cap * 2;
	json5_obj_prop * prop, * new_prop, * new_props;

	if (new_cap < OBJECT_MIN_CAP) {
		new_cap = OBJECT_MIN_CAP;
	}

	new_props = calloc (new_cap, sizeof (*new_props));

	if (!new_props) {
		return -1;
	}

	for (size_t i = 0; i < value -> cap; i ++) {
		prop = &value -> props [i];

		if (prop -> key > PLACEHOLDER_KEY) {
			new_prop = json5_prop_lookup (new_props, new_cap - 1, prop -> hash, prop -> key, prop -> key_len);
			*new_prop = *prop;
		}
	}

	if (value -> props) {
		free (value -> props);
	}

	value -> cap = new_cap;
	value -> props = new_props;

	return 0;
}

json5_value * json5_value_set_prop (json5_value * value, char const * key, size_t key_len, int replace) {
	json5_hash hash;
	json5_obj_prop * prop;
	uint8_t * new_key;

	if (value -> type != JSON5_TYPE_OBJECT) {
		return NULL;
	}

	if (value -> cap == 0) {
		if (json5_object_grow (value) != 0) {
			return NULL;
		}
	}

	if (key_len == (size_t) -1) {
		key_len = strlen (key);
	}

	hash = json5_get_hash (key, key_len);
	prop = json5_prop_lookup (value -> props, value -> cap - 1, hash, (uint8_t const *) key, key_len);

	if (value -> len + (value -> len / 2) > value -> cap) {
		if (json5_object_grow (value) != 0) {
			return NULL;
		}

		prop = json5_prop_lookup (value -> props, value -> cap - 1, hash, (uint8_t const *)key, key_len);
	}

	if (prop->key > PLACEHOLDER_KEY && !replace) {
		return NULL;
	}

	new_key = string_copy ((uint8_t const *) key, key_len);

	if (!new_key) {
		return NULL;
	}

	if (prop -> key >= PLACEHOLDER_KEY) {
		free (prop -> key);
	}
	else {
		value -> len ++;
	}

	prop -> hash = hash;
	prop -> key = new_key;
	prop -> key_len = key_len;
	json5_value_set_null (&prop -> value);

	return &prop -> value;
}

int json5_value_delete_prop (json5_value * value, char const * key, size_t key_len) {
	json5_hash hash;
	json5_obj_prop * prop;

	if (value -> type != JSON5_TYPE_OBJECT) {
		return 0;
	}

	if (value -> cap == 0) {
		return 0;
	}

	hash = json5_get_hash (key, key_len);
	prop = json5_prop_lookup (value -> props, value -> cap - 1, hash, (uint8_t const *) key, key_len);

	if (prop -> key) {
		free (prop -> key);
		prop -> key = PLACEHOLDER_KEY;
		json5_value_reset (&prop -> value, JSON5_TYPE_NULL);
		value -> len --;

		return 1;
	}

	return 0;
}

void json5_value_transfer (json5_value * target, json5_value * source) {
	json5_value_set_null (target);

	if (source) {
		*target = *source;
		json5_value_set_null (source);
	}
}

int json5_obj_itor_init (json5_obj_itor * itor, json5_value const * obj) {
	if (obj -> type != JSON5_TYPE_OBJECT) {
		return -1;
	}

	itor -> obj = obj;
	itor -> prop = obj -> props;

	return 0;
}

int json5_obj_itor_next (json5_obj_itor * itor, char const ** out_key, size_t * out_key_len, json5_value ** out_value) {
	json5_obj_prop const * end = &itor -> obj -> props [itor -> obj -> cap];

	if (itor -> prop >= end) {
		return 0;
	}

	do {
		if (itor -> prop -> key > PLACEHOLDER_KEY) {
			*out_value = &itor -> prop -> value;
			*out_key = (char const *) itor -> prop -> key;
			*out_key_len = itor -> prop -> key_len;

			itor -> prop ++;

			return 1;
		}

		itor -> prop ++;
	}
	while (itor -> prop < end);

	return 0;
}

void json5_set_hash_seed (json5_hash seed)
{
	hash_table_seed = seed;
}
