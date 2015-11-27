#include "test.h"

int main (int argc, char const * argv [])
{
	json5_value value;
	json5_value * item;

	json5_value_init (&value);

	json5_value_set_int (&value, 34);
	assert (value.type == JSON5_TYPE_NUMBER && value.subtype == JSON5_SUBTYPE_INT);
	assert (value.val.num.i == 34);

	json5_value_set_float (&value, -12.5);
	assert (value.type == JSON5_TYPE_NUMBER && value.subtype == JSON5_SUBTYPE_FLOAT);
	assert (value.val.num.f == -12.5);

	json5_value_set_bool (&value, 5);
	assert (value.type == JSON5_TYPE_NUMBER && value.subtype == JSON5_SUBTYPE_BOOL);
	assert (value.val.num.i == 1);

	json5_value_set_bool (&value, 0);
	assert (value.val.num.i == 0);

	json5_value_set_nan (&value);
	assert (value.type == JSON5_TYPE_NAN);

	json5_value_set_infinity (&value, 5);
	assert (value.type == JSON5_TYPE_INFINITY);
	assert (value.val.num.i == 1);

	json5_value_set_infinity (&value, -6);
	assert (value.type == JSON5_TYPE_INFINITY);
	assert (value.val.num.i == -1);

	json5_value_set_string (&value, "akey2", 5);
	assert (value.type == JSON5_TYPE_STRING);
	assert (value.val.str.len == 5);
	assert (strcmp ((void *) value.val.str.s, "akey2") == 0);

	item = json5_value_append_item (&value);
	assert (item == NULL);

	json5_value_set_null (&value);
	assert (value.type == JSON5_TYPE_NULL);

	return RESULT_PASS;
}