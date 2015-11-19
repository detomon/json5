AM_CFLAGS = -I$(srcdir)/../src -L$(srcdir)/../src
AM_LDADD = $(srcdir)/../src/libjson5.a

check_PROGRAMS = \
	test_json5 \
	test_fail

test_json5_SOURCES = test_json5.c
test_json5_LDADD = $(AM_LDADD)

test_fail_SOURCES = test_fail.c
test_fail_LDADD = $(AM_LDADD)

TESTS_ENVIRONMENT = \
	top_builddir=$(top_builddir); \
	# Enable malloc debugging where available
	export MallocScribble=1; \
	export MallocPreScribble=1; \
	export MallocGuardEdges=1;

TESTS = \
	test_json5 \
	test_fail

XFAIL_TESTS = \
	test_fail
