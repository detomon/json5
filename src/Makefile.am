AM_CFLAGS = @AM_CFLAGS@ -I$(srcdir)/../unicode-table/src

BUILT_SOURCES = $(srcdir)/../unicode-table/src/unicode-table.c

$(BUILT_SOURCES):
	make -C $(srcdir)/../unicode-table

lib_LIBRARIES = libjson5.a

libjson5_a_SOURCES = \
	json5.c \
	json5-tokenizer.c \
	json5-value.c \
	json5-writer.c

libjson5_a_LIBADD = $(srcdir)/../unicode-table/src/libunicodetable.a

pkginclude_HEADERS = \
	json5.h \
	json5-tokenizer.h \
	json5-value.h \
	json5-writer.h
