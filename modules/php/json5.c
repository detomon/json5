#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "ext/standard/basic_functions.h"

#include "json5.h"

#define PHP_JSON5_VERSION "1.0"
#define PHP_JSON5_EXTNAME "json5"

extern zend_module_entry json5_module_entry;
#define phpext_json5_ptr &json5_module_entry

static double double_pos_inf;
static double double_neg_inf;
static double double_nan;

PHP_MINIT_FUNCTION(json5);
PHP_FUNCTION(json5_decode);
PHP_FUNCTION(json5_encode);

// list of custom PHP functions provided by this extension
// set {NULL, NULL, NULL} as the last record to mark the end of list
static zend_function_entry json5_functions[] = {
	PHP_FE(json5_decode, NULL)
	PHP_FE(json5_encode, NULL)
	{NULL, NULL, NULL}
};

// the following code creates an entry for the module and registers it with Zend.
zend_module_entry json5_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_JSON5_EXTNAME,
	json5_functions,
	PHP_MINIT(json5), // name of the MINIT function or NULL if not applicable
	NULL, // name of the MSHUTDOWN function or NULL if not applicable
	NULL, // name of the RINIT function or NULL if not applicable
	NULL, // name of the RSHUTDOWN function or NULL if not applicable
	NULL, // name of the MINFO function or NULL if not applicable
#if ZEND_MODULE_API_NO >= 20010901
	PHP_JSON5_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

ZEND_GET_MODULE(json5)

static uint32_t get_rand() {
	return time(0) * getpid();
}

PHP_MINIT_FUNCTION(json5)
{
	uint64_t seed;

	double_pos_inf = php_get_inf();
	double_neg_inf = -double_pos_inf;
	double_nan = php_get_nan();

	seed = get_rand();
	seed = (seed << 32) | get_rand();

	json5_set_hash_seed(seed);

	return SUCCESS;
}

#define FUNCTION_NAME "json5_decode"

static int coder_handle_error(json5_coder *coder) {
	char const *error = json5_tokenizer_get_error(&coder->tknzr);
	json5_value const *error_val = json5_parser_get_error(&coder->parser);

	if (error && !error_val) {
		php_error_docref("function." FUNCTION_NAME TSRMLS_CC, E_WARNING, error);
		return 1;
	}

	if (error_val) {
		php_error_docref("function." FUNCTION_NAME TSRMLS_CC, E_WARNING, (char *) error_val->str.s);
		return 1;
	}

	return 0;
}

static void make_value(json5_value const *value, zval *out_value) {
	switch (value->type) {
		case JSON5_TYPE_NULL: {
			ZVAL_NULL(out_value);
			break;
		}
		case JSON5_TYPE_STRING: {
			ZVAL_STRINGL(out_value, (char *) value->str.s, value->str.len);
			break;
		}
		case JSON5_TYPE_ARRAY: {
			json5_value *item;
			zval arr_item;

			array_init(out_value);

			for (size_t i = 0; i < value->arr.len; i ++) {
				item = &value->arr.itms[i];

				ZVAL_NULL(&arr_item);

				make_value(item, &arr_item);

				// does this make a copy?
				add_next_index_zval(out_value, &arr_item);
			}

			break;
		}
		case JSON5_TYPE_OBJECT: {
			json5_value *item;
			char const *key;
			size_t key_len;
			json5_obj_itor itor;
			zval arr_item;

			json5_obj_itor_init(&itor, value);
			array_init(out_value);

			while (json5_obj_itor_next(&itor, &key, &key_len, &item)) {
				ZVAL_NULL(&arr_item);

				make_value(item, &arr_item);

				// does this make a copy?
				add_assoc_zval(out_value, key, &arr_item);
			}

			break;
		}
		case JSON5_TYPE_INFINITY: {
			ZVAL_DOUBLE(out_value, value->num.i >= 0 ? double_pos_inf : double_neg_inf);
			break;
		}
		case JSON5_TYPE_NAN: {
			ZVAL_DOUBLE(out_value, double_nan);
			break;
		}
		default: {
			switch (value->type) {
				case JSON5_TYPE_INT: {
					ZVAL_LONG(out_value, value->num.i);
					break;
				}
				case JSON5_TYPE_FLOAT: {
					ZVAL_DOUBLE(out_value, value->num.f);
					break;
				}
				case JSON5_TYPE_BOOL: {
					ZVAL_BOOL(out_value, value->num.i);
					break;
				}
				default: {
					break;
				}
			}
		}
	}
}

PHP_FUNCTION(json5_decode)
{
	int res;
	zend_string *string;
	int length;
	json5_coder coder;
	json5_value value;
	zval *z;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &string) == FAILURE) {
		RETURN_NULL();
	}

	if (json5_coder_init(&coder) != 0) {
		php_error_docref("function." FUNCTION_NAME TSRMLS_CC, E_CORE_ERROR, "Internal allocation error");
		goto error;
	}

	json5_value_init(&value);

	res = json5_coder_decode(&coder, (uint8_t *) string->val, string->len, &value);

	if (res != 0) {
		if (coder_handle_error(&coder)) {
			goto error;
		}
	}

	json5_coder_destroy(&coder);

	make_value(&value, return_value);

	json5_value_set_null(&value);

	return;

	error: {
		json5_coder_destroy(&coder);
		json5_value_set_null(&value);

		RETURN_NULL();
	}
}

PHP_FUNCTION(json5_encode)
{
	RETURN_NULL();
}
