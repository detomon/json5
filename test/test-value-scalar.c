#include "test.h"

int main (int argc, char const * argv []) {
	json5_value value = JSON5_VALUE_INIT;
	json5_value * item;

	json5_value_set_int (&value, 34);
	assert (value.type == JSON5_TYPE_INT);
	assert (value.ival == 34);

	json5_value_set_float (&value, -12.5);
	assert (value.type == JSON5_TYPE_FLOAT);
	assert (value.fval == -12.5);

	json5_value_set_bool (&value, 5);
	assert (value.type == JSON5_TYPE_BOOL);
	assert (value.ival == 1);

	json5_value_set_bool (&value, 0);
	assert (value.ival == 0);

	json5_value_set_nan (&value);
	assert (value.type == JSON5_TYPE_NAN);

	json5_value_set_infinity (&value, 5);
	assert (value.type == JSON5_TYPE_INFINITY);
	assert (value.ival == 0);

	json5_value_set_infinity (&value, -6);
	assert (value.type == JSON5_TYPE_INFINITY);
	assert (value.ival == 1);

	json5_value_set_string (&value, "akey2", 5);
	assert (value.type == JSON5_TYPE_STRING);
	assert (value.len == 5);
	assert (strcmp ((void *) value.sval, "akey2") == 0);

	item = json5_value_append_item (&value);
	assert (item == NULL);

	json5_value_set_null (&value);
	assert (value.type == JSON5_TYPE_NULL);

	return RESULT_PASS;
}
