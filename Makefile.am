lib_LTLIBRARIES = regexp.la
regexp_la_CFLAGS = @MYSQL_CFLAGS@
regexp_la_CXXFLAGS = @MYSQL_CFLAGS@
regexp_la_LDFLAGS = -module -avoid-version -no-undefined
regexp_la_SOURCES = regexp.c udf_regexp.h
test: all
	cd tests; ./test.sh
