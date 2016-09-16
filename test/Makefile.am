AM_CFLAGS = -I$(srcdir)/../src -L$(srcdir)/../src
LDADD = $(srcdir)/../src/libjson5.a $(srcdir)/../unicode-table/src/libunicodetable.a

check_PROGRAMS = \
	test-value-scalar \
	test-value-array \
	test-value-object

test_value_scalar_SOURCES = test-value-scalar.c
test_value_array_SOURCES = test-value-array.c
test_value_object_SOURCES = test-value-object.c

TESTS_ENVIRONMENT = \
	top_builddir=$(top_builddir); \
	# Enable malloc debugging where available
	export MallocScribble=1; \
	export MallocPreScribble=1; \
	export MallocGuardEdges=1;

TESTS = \
	test-value-scalar \
	test-value-array \
	test-value-object
