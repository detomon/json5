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
#define PLACEHOLDER_KEY ((char*) 1)

static void json5_value_delete(json5_value* value, json5_type type);

static void json5_value_delete_string(json5_value* value) {
	if (value -> str.s) {
		free (value -> str.s);
	}
}

static void json5_value_delete_array(json5_value* value) {
	for (size_t i = 0; i < value -> arr.len; i ++) {
		json5_value_delete (&value -> arr.itms [i], JSON5_TYPE_NULL);
	}

	if (value -> arr.itms) {
		free (value -> arr.itms);
	}
}

static void json5_value_delete_object(json5_value* value) {
	json5_obj_prop* prop;

	for (size_t i = 0; i < value -> obj.cap; i ++) {
		prop = &value -> obj.itms [i];

		if (prop -> key > PLACEHOLDER_KEY) {
			json5_value_delete (&prop -> value, JSON5_TYPE_NULL);
		}
	}

	if (value -> obj.itms) {
		free (value -> obj.itms);
	}
}

static void json5_value_delete(json5_value* value, json5_type type) {
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
		case JSON5_TYPE_NUMBER:
		case JSON5_TYPE_NULL:
		case JSON5_TYPE_INFINITY:
		case JSON5_TYPE_NAN: {
			break;
		}
	}

	json5_value_init (value);
	value -> type = type;
}

void json5_value_init(json5_value* value) {
	memset (value, 0, sizeof (*value));
}

void json5_value_set_int(json5_value* value, int64_t i) {
	json5_value_delete (value, JSON5_TYPE_NUMBER);
	value -> subtype = JSON5_SUBTYPE_INT;
	value -> num.i = i;
}

void json5_value_set_float(json5_value* value, double f) {
	json5_value_delete (value, JSON5_TYPE_NUMBER);
	value -> subtype = JSON5_SUBTYPE_FLOAT;
	value -> num.f = f;
}

void json5_value_set_bool(json5_value* value, int b) {
	json5_value_delete (value, JSON5_TYPE_NUMBER);
	value -> subtype = JSON5_SUBTYPE_BOOL;
	value -> num.i = b != 0;
}

void json5_value_set_nan(json5_value* value) {
	json5_value_delete (value, JSON5_TYPE_NAN);
}

void json5_value_set_infinity(json5_value* value, int sign) {
	json5_value_delete (value, JSON5_TYPE_INFINITY);
	value -> num.i = sign >= 0 ? 1 : -1;
}

void json5_value_set_null(json5_value* value) {
	json5_value_delete (value, JSON5_TYPE_NULL);
}

int json5_value_set_string(json5_value* value, char const* str, size_t len) {
	char* new_str = strndup (str, len);

	if (!new_str) {
		return -1;
	}

	json5_value_delete (value, JSON5_TYPE_STRING);
	value -> str.s = (void*) new_str;
	value -> str.len = len;

	return 0;
}

void json5_value_set_array(json5_value* value) {
	json5_value_delete (value, JSON5_TYPE_ARRAY);
}

void json5_value_set_object(json5_value* value) {
	json5_value_delete (value, JSON5_TYPE_OBJECT);
}

json5_value* json5_value_get_item(json5_value* value, size_t idx) {
	if (value -> type != JSON5_TYPE_ARRAY) {
		return NULL;
	}

	if (idx > value -> arr.len) {
		return NULL;
	}

	return &value -> arr.itms [idx];
}

json5_value* json5_value_append_item(json5_value* value) {
	json5_value* item;
	json5_value* new_items;
	size_t new_cap;

	if (value -> type != JSON5_TYPE_ARRAY) {
		return NULL;
	}

	if (value -> arr.len >= value -> arr.cap) {
		new_cap = value -> arr.cap ? value -> arr.cap * 2 : ARRAY_MIN_CAP;
		new_items = realloc (value -> arr.itms, new_cap * sizeof (*item));

		if (!new_items) {
			return NULL;
		}

		value -> arr.cap = new_cap;
		value -> arr.itms = new_items;
	}

	item = &value -> arr.itms [value -> arr.len ++];
	json5_value_init (item);

	return item;
}

static json5_hash json5_get_hash (char const* key, size_t key_len) {
	json5_hash hash = 0;

	for (size_t i = 0; i < key_len; i ++) {
		hash = (hash * 100003) ^ key [i];
	}

	return hash;
}

static json5_obj_prop* json5_prop_lookup(json5_obj_prop* props, size_t mask, json5_hash hash, char const* key, size_t key_len) {
	json5_hash i = hash, perturb = hash;
	json5_obj_prop* prop = &props [i & mask];

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

json5_value* json5_value_get_prop(json5_value* value, char const* key, size_t key_len) {
	json5_hash hash;
	json5_obj_prop* prop;

	if (value -> type != JSON5_TYPE_OBJECT) {
		return NULL;
	}

	if (value -> obj.cap == 0) {
		return NULL;
	}

	hash = json5_get_hash (key, key_len);
	prop = json5_prop_lookup (value -> obj.itms, value -> obj.cap - 1, hash, key, key_len);

	if (prop -> key) {
		return &prop -> value;
	}

	return NULL;
}

static int json5_object_grow(json5_value* value) {
	size_t new_cap = value -> obj.cap * 2;
	json5_obj_prop* prop, * new_prop, * new_props;

	if (new_cap < OBJECT_MIN_CAP) {
		new_cap = OBJECT_MIN_CAP;
	}

	new_props = calloc (new_cap, sizeof (*new_props));

	if (!new_props) {
		return -1;
	}

	for (size_t i = 0; i < value -> obj.cap; i ++) {
		prop = &value -> obj.itms [i];

		if (prop -> key > PLACEHOLDER_KEY) {
			new_prop = json5_prop_lookup (new_props, new_cap - 1, prop -> hash, prop -> key, prop -> key_len);
			*new_prop = *prop;
		}
	}

	if (value -> obj.itms) {
		free (value -> obj.itms);
	}

	value -> obj.cap = new_cap;
	value -> obj.itms = new_props;

	return 0;
}

json5_value* json5_value_set_prop(json5_value* value, char const* key, size_t key_len) {
	json5_hash hash;
	json5_obj_prop* prop;
	char* new_key;

	if (value -> type != JSON5_TYPE_OBJECT) {
		return NULL;
	}

	if (value -> obj.cap == 0) {
		if(json5_object_grow (value) != 0) {
			return NULL;
		}
	}

	hash = json5_get_hash (key, key_len);
	prop = json5_prop_lookup (value -> obj.itms, value -> obj.cap - 1, hash, key, key_len);

	if (value -> obj.len + (value -> obj.len / 2) > value -> obj.cap) {
		if(json5_object_grow (value) != 0) {
			return NULL;
		}

		prop = json5_prop_lookup (value -> obj.itms, value -> obj.cap - 1, hash, key, key_len);
	}

	new_key = strndup (key, key_len);

	if (!new_key) {
		return NULL;
	}

	if (!prop -> key) {
		value -> obj.len ++;
	}

	prop -> hash = hash;
	prop -> key = new_key;
	prop -> key_len = key_len;
	json5_value_set_null (&prop -> value);

	return &prop -> value;
}

int json5_value_delete_prop(json5_value* value, char const* key, size_t key_len) {
	json5_hash hash;
	json5_obj_prop* prop;

	if (value -> type != JSON5_TYPE_OBJECT) {
		return 0;
	}

	if (value -> obj.cap == 0) {
		return 0;
	}

	hash = json5_get_hash (key, key_len);
	prop = json5_prop_lookup (value -> obj.itms, value -> obj.cap - 1, hash, key, key_len);

	if (prop -> key) {
		free (prop -> key);
		prop -> key = PLACEHOLDER_KEY;
		json5_value_delete (&prop -> value, JSON5_TYPE_NULL);
		value -> obj.len --;

		return 1;
	}

	return 0;
}
