#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "json5-value.h"
#include "json5-writer.h"
#include "json5-tokenizer.h"

/*static void print_value (json5_value * value, int indent) {
	char ind [10];

	for (int i = 0; i < indent; i ++) {
		ind [i] = '\t';
	}

	ind [indent] = '\0';

	switch (value -> type) {
		case JSON5_TYPE_NULL: {
			printf ("null");
			break;
		}
		case JSON5_TYPE_NUMBER: {
			switch (value -> subtype) {
				case JSON5_SUBTYPE_NONE:
				case JSON5_SUBTYPE_INT: {
					printf ("%lld,\n", value -> num.i);
					break;
				}
				case JSON5_SUBTYPE_FLOAT: {
					printf ("%.17g,\n", value -> num.f);
					break;
				}
				case JSON5_SUBTYPE_BOOL: {
					printf ("%s,\n", value -> num.i ? "true" : "false");
					break;
				}
			}
			break;
		}
		case JSON5_TYPE_STRING: {
			printf ("\"%s\",\n", value -> str.s);
			break;
		}
		case JSON5_TYPE_ARRAY: {
			printf ("[\n");
			for (int i = 0; i < value -> arr.len; i ++) {
				printf ("%s", ind);
				print_value (&value -> arr.itms [i], indent + 1);
			}
			printf ("%s],\n", ind);
			break;
		}
		case JSON5_TYPE_OBJECT: {
			1 * prop;

			printf ("{\n");
			for (int i = 0; i < value -> obj.cap; i ++) {
				prop = &value -> obj.itms [i];

				if (prop -> key && prop -> key != ((void *) -1)) {
					printf ("%s\t", ind);
					printf ("\"%s\": ", prop -> key);
					print_value (&value -> obj.itms [i].value, indent + 1);
				}
			}
			printf ("%s},\n", ind);
			break;
		}
		case JSON5_TYPE_INFINITY: {
			printf ("%cInfinity,\n", value -> num.i > 0 ? '+' : '-');
			break;
		}
		case JSON5_TYPE_NAN: {
			printf ("NaN,\n");
			break;
		}
	}
}*/

static int write_json (uint8_t const * string, size_t size, void * info) {
	fwrite (string, 1, size, stdout);

	return 0;
}

static int put_tokens (json5_token const * token, void * arg) {
	switch (token -> type) {
		case JSON5_TOK_OBJ_OPEN: {
			printf (">> {\n");
			break;
		}
		case JSON5_TOK_OBJ_CLOSE: {
			printf (">> }\n");
			break;
		}
		case JSON5_TOK_ARR_OPEN: {
			printf (">> [\n");
			break;
		}
		case JSON5_TOK_ARR_CLOSE: {
			printf (">> ]\n");
			break;
		}
		case JSON5_TOK_COMMA: {
			printf (">> ,\n");
			break;
		}
		case JSON5_TOK_COLON: {
			printf (">> :\n");
			break;
		}
		case JSON5_TOK_STRING: {
			printf (">> str: \"%s\"\n", token -> token);
			break;
		}
		case JSON5_TOK_NUMBER: {
			printf (">> i: %lld\n", token -> value.i);
			break;
		}
		case JSON5_TOK_NUMBER_FLOAT: {
			printf (">> f: %lf\n", token -> value.f);
			break;
		}
		case JSON5_TOK_NAME: {
			printf (">> name: \"%s\"\n", token -> token);
			break;
		}
		case JSON5_TOK_NAME_SIGN: {
			printf (">> sign name: \"%s\" %lld\n", token -> token, token -> value.i);
			break;
		}
		case JSON5_TOK_END: {
			printf (">> END\n");
			break;
		}
		default: {
			break;
		}
	}


	return 0;
}

int main (int argc, const char * argv []) {
	/*json5_value arr;
	json5_value obj;
	json5_value * item, * item2;

	json5_value_init (&arr);
	json5_value_init (&obj);

	json5_value_set_array (&arr);
	json5_value_set_object (&obj);

	item = json5_value_append_item (&arr);
	json5_value_set_float (item, -345.4);

	for (int i = 0; i < 100; i ++) {
		char key [20];
		sprintf (key, "bla%u", i);

		item2 = json5_value_set_prop (&obj, key, strlen (key));
		json5_value_set_array (item2);
		item = json5_value_append_item (item2);
		json5_value_set_object (item);

		item = json5_value_set_prop (item, "hallo", 5);
		json5_value_set_string(item, "test", 4);

		item = json5_value_append_item (item2);
		json5_value_set_object (item);

		item = json5_value_set_prop (item, "hallo", 5);
		json5_value_set_nan(item);
	}

	item = json5_value_set_prop (&obj, "bla", 3);
	json5_value_set_bool (item, 2);

	item = json5_value_set_prop (&obj, "blub", 4);
	json5_value_set_array (item);
	item = json5_value_append_item (item);
	json5_value_set_infinity (item, -1);

	item = json5_value_get_prop (&obj, "bla", 3);
	json5_value_set_float (item, 17.6e4);

	//json5_value_set_int (&arr, 2);

	print_value (&obj, 0);*/

	json5_value value;
	json5_value * item, * item2;

	json5_value_set_object (&value);
	assert (value.type == JSON5_TYPE_OBJECT);

	item = json5_value_set_prop (&value, "ä💀key \n4", -1);
	assert (item -> type == JSON5_TYPE_NULL);
	assert (item != NULL);

	json5_value_set_float (item, -34.3);
	assert (item -> type == JSON5_TYPE_FLOAT);

	item2 = json5_value_set_prop (&value, "ä💀key \n4", -1);
	assert (item2 != NULL);
	assert (item2 == item);
	assert (item2 -> type == JSON5_TYPE_NULL);
	assert (value.obj.len == 1);

	json5_value_set_float (item, -4e6);
	//assert (item -> type == JSON5_TYPE_INT);

	item2 = json5_value_set_prop (&value, "somkey44", -1);
	assert (item2 != item);
	assert (item2 -> type == JSON5_TYPE_NULL);
	assert (value.obj.len == 2);

	json5_value_set_string (item2, "astring", 7);
	assert (item2 -> type == JSON5_TYPE_STRING);

	item = json5_value_get_prop (&value, "ä💀key \n4", -1);
	assert (item != NULL);
	//assert (item -> type == JSON5_TYPE_INT);

	item = json5_value_get_prop (&value, "somkey44", -1);
	assert (item != NULL);
	assert (item -> type == JSON5_TYPE_STRING);

	item2 = json5_value_set_prop (&value, "arr", -1);
	json5_value_set_array (item2);

	item = json5_value_append_item (item2);
	json5_value_set_infinity (item, 0);

	item = json5_value_append_item (item2);
	json5_value_set_string (item, "\"\n", 2);

	/*assert (json5_value_delete_prop (&value, "akey34", 6) == 1);
	assert (value.val.obj.len == 1);

	assert (json5_value_delete_prop (&value, "somkey44", 8) == 1);
	assert (value.val.obj.len == 0);*/


	json5_writer writer;
	json5_writer_init (&writer, JSON5_WRITER_FLAG_NO_ESCAPE, write_json, NULL);

	json5_writer_write (&writer, &value);

	printf ("\n\n");

	json5_tokenizer tknzr;

	json5_tokenizer_init (&tknzr);

	//char const * string = "{'bla':\"key\\x40\",e:1e-2,}";
	char const * string = "-NaN, {, ᛮtⅧ: 'aöäü', [-4.e-5, -0xfff.6]}";
	size_t size = strlen (string);
	json5_tokenizer_put_chars (&tknzr, (uint8_t *) string, size, put_tokens, NULL);

	json5_tokenizer_put_chars (&tknzr, (uint8_t *) "", 0, put_tokens, NULL);

	printf ("\n!! %s\n", tknzr.buffer);

	return 0;
}
