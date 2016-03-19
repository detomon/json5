PHP_ARG_ENABLE(json5, whether to enable json5,
[ --enable-json5          Enable json5])

LIB_PATH=json5/src

PHP_ADD_INCLUDE($LIB_PATH)
PHP_ADD_INCLUDE($LIB_PATH/../unicode-table/src)

SOURCES=" \
	json5.c \
	$LIB_PATH/json5-coder.c \
	$LIB_PATH/json5-parser.c \
	$LIB_PATH/json5-tokenizer.c \
	$LIB_PATH/json5-value.c \
	$LIB_PATH/json5-writer.c \
	$LIB_PATH/../unicode-table/src/unicode-table.c"

if test "$PHP_JSON5" = "yes"; then
	AC_DEFINE(HAVE_JSON5, 1, [Whether you have json5])
	PHP_NEW_EXTENSION(json5, $SOURCES, $ext_shared)
fi
