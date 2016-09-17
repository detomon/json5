#include "test.h"

int main (int argc, char const * argv [])
{
	json5_value value = JSON5_VALUE_INIT;
	json5_value * item, * item2;

	json5_value_set_object (&value);
	assert (value.type == JSON5_TYPE_OBJECT);

	item = json5_value_set_prop (&value, "akey34", 6);
	assert (item -> type == JSON5_TYPE_NULL);
	assert (item != NULL);

	json5_value_set_float (item, -34.3);
	assert (item -> type == JSON5_TYPE_FLOAT);

	item2 = json5_value_set_prop (&value, "akey34", 6);
	assert (item2 != NULL);
	assert (item2 == item);
	assert (item2 -> type == JSON5_TYPE_NULL);
	assert (value.len == 1);

	json5_value_set_int (item, 45457);
	assert (item -> type == JSON5_TYPE_INT);

	item2 = json5_value_set_prop (&value, "somkey44", 8);
	assert (item2 != item);
	assert (item2 -> type == JSON5_TYPE_NULL);
	assert (value.len == 2);

	json5_value_set_string (item2, "astring", 7);
	assert (item2 -> type == JSON5_TYPE_STRING);

	item = json5_value_get_prop (&value, "akey34", 6);
	assert (item != NULL);
	assert (item -> type == JSON5_TYPE_INT);

	item = json5_value_get_prop (&value, "somkey44", 8);
	assert (item != NULL);
	assert (item -> type == JSON5_TYPE_STRING);

	assert (json5_value_delete_prop (&value, "akey34", 6) == 1);
	assert (value.len == 1);

	assert (json5_value_delete_prop (&value, "somkey44", 8) == 1);
	assert (value.len == 0);

	return RESULT_PASS;
}
