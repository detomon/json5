#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "ext/standard/basic_functions.h"

#include "json5.h"

#define PHP_JSON5_VERSION "1.0"
#define PHP_JSON5_EXTNAME "json5"

#define PHP_JSON5_STACK_INIT_SIZE 64

typedef struct {
	size_t len;
	size_t cap;
	struct _json5_item {
		zval *value;
	} *stack;
} _json5_stack;

extern zend_module_entry json5_module_entry;

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

static int _json5_stack_init(_json5_stack *stack) {
	memset(stack, 0, sizeof(*stack));

	stack->len = 0;
	stack->cap = PHP_JSON5_STACK_INIT_SIZE;
	stack->stack = safe_emalloc(stack->cap, sizeof(zval*), 0);

	if (!stack->stack) {
		return -1;
	}

	return 0;
}

static struct _json5_item *_json5_stack_top(_json5_stack *stack) {
	if (stack->len < 1) {
		return NULL;
	}

	return &stack->stack[stack->len - 1];
}

static struct _json5_item *_json5_stack_push(_json5_stack *stack) {
	size_t new_cap;
	struct _json5_item *new_stack;
	struct _json5_item *top = _json5_stack_top(stack);
	struct _json5_item *next;

	if (stack->len >= stack->cap) {
		new_cap = stack->cap * 2;
		new_stack = erealloc(stack->stack, new_cap * sizeof(*new_stack));

		if (!new_stack) {
			return NULL;
		}

		stack->cap = new_cap;
		stack->stack = new_stack;
	}

	next = &stack->stack[stack->len ++];

	if (top) {
		next->value = top->value;
	}

	return next;
}

static struct _json5_item *_json5_stack_pop(_json5_stack *stack) {
	struct _json5_item *top = NULL;

	if (stack->len > 0) {
		top = &stack->stack[stack->len - 1];
		//zval_ptr_dtor(&top->value);
		stack->len --;
	}

	if (stack->len > 0) {
		return &stack->stack[stack->len - 1];
	}

	return NULL;
}

static void _json5_stack_destroy(_json5_stack *stack) {
	while (_json5_stack_pop(stack)) {
		;
	}

	if (stack->stack) {
		efree(stack->stack);
	}
}

static int begin_arr_or_obj(json5_token const *token, _json5_stack *stack) {
	struct _json5_item *top;

	top = _json5_stack_top(stack);
	array_init(top->value);

	top = _json5_stack_push(stack);

	if (!top) {
		return -1;
	}

	return 0;
}

static int end_container(json5_token const *token, _json5_stack *stack) {
	_json5_stack_pop(stack);
	_json5_stack_pop(stack);

	return 0;
}

static int begin_key(json5_token const *token, _json5_stack *stack) {
	zval *value;
	struct _json5_item *top;

 	top = _json5_stack_push(stack);

	if (!top) {
		return -1;
	}

	// insert dummy empty string to get reference to array item
	top->value = add_get_assoc_stringl_ex(top->value, (char *) token->token, token->length, "", 0);

	return 0;
}

static int begin_index(json5_token const *token, _json5_stack *stack) {
	HashTable *arr_hash;
	struct _json5_item *top;
	int array_count;

	top = _json5_stack_push(stack);

	if (!top) {
		return -1;
	}

	// insert dummy long to get reference to array item
 	arr_hash = Z_ARRVAL_P(top->value);
    array_count = zend_hash_num_elements(arr_hash);
	top->value = add_get_index_long(top->value, array_count, 0);

	return 0;
}

static int set_value(json5_token const *token, _json5_stack *stack) {
	struct _json5_item *top = _json5_stack_top(stack);
	zval *value = top->value;

	switch (token->type) {
		case JSON5_TOK_NULL: {
			ZVAL_NULL(value);
			break;
		}
		case JSON5_TOK_STRING: {
			ZVAL_STRINGL(value, (char *) token->token, token->length);
			break;
		}
		case JSON5_TOK_INFINITY: {
			ZVAL_DOUBLE(value, token->value.i >= 0 ? double_pos_inf : double_neg_inf);
			break;
		}
		case JSON5_TOK_NAN: {
			ZVAL_DOUBLE(value, double_nan);
			break;
		}
		default: {
			switch (token->type) {
				case JSON5_TOK_NUMBER: {
					ZVAL_LONG(value, token->value.i);
					break;
				}
				case JSON5_TOK_NUMBER_FLOAT: {
					ZVAL_DOUBLE(value, token->value.f);
					break;
				}
				case JSON5_TOK_NUMBER_BOOL: {
					ZVAL_BOOL(value, token->value.i);
					break;
				}
				default: {
					break;
				}
			}

			break;
		}
	}

	_json5_stack_pop(stack);

	return 0;
}

static json5_parser_funcs funcs = {
	.begin_arr     = (void *) begin_arr_or_obj,
	.begin_obj     = (void *) begin_arr_or_obj,
	.end_container = (void *) end_container,
	.begin_key     = (void *) begin_key,
	.begin_index   = (void *) begin_index,
	.set_value     = (void *) set_value,
};

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

PHP_FUNCTION(json5_decode)
{
	int res;
	zend_string *string;
	json5_coder coder;
	json5_value value;
	struct _json5_item *top;
	_json5_stack stack;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &string) == FAILURE) {
		RETURN_NULL();
	}

	if (json5_coder_init(&coder) != 0) {
		php_error_docref("function." FUNCTION_NAME TSRMLS_CC, E_ERROR, "Allocation error");
		goto error;
	}

	if (_json5_stack_init(&stack) != 0) {
		php_error_docref("function." FUNCTION_NAME TSRMLS_CC, E_ERROR, "Allocation error");
		goto error;
	}

	coder.parser.funcs = &funcs;
	coder.parser.funcs_arg = &stack;

	top = _json5_stack_push(&stack); // root
	top->value = return_value;

	json5_value_init(&value);

	res = json5_coder_decode(&coder, (uint8_t *) string->val, string->len, &value);

	if (res != 0) {
		if (coder_handle_error(&coder)) {
			goto error;
		}
	}

	json5_coder_destroy(&coder);

	json5_value_set_null(&value);
	_json5_stack_destroy(&stack);

	return;

	error: {
		json5_coder_destroy(&coder);
		json5_value_set_null(&value);
		_json5_stack_destroy(&stack);

		RETURN_NULL();
	}
}

PHP_FUNCTION(json5_encode)
{
	RETURN_NULL();
}
