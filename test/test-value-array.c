#include "test.h"

int main (int argc, char const * argv [])
{
	json5_value value = JSON5_VALUE_INIT;
	json5_value * item;

	json5_value_set_array (&value);
	assert (value.type == JSON5_TYPE_ARRAY);

	item = json5_value_append_item (&value);
	assert (item != NULL);
	assert (item -> type == JSON5_TYPE_NULL);
	assert (value.len == 1);

	item = json5_value_append_item (&value);
	assert (item != NULL);
	assert (item -> type == JSON5_TYPE_NULL);
	assert (value.len == 2);

	return RESULT_PASS;
}
