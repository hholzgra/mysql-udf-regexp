lib_LTLIBRARIES = regexp.la

regexp_la_SOURCES = regexp.c

EXTRA_DIST     = udf_regexp.h
noinst_HEADERS = udf_regexp.h


regexp_la_CFLAGS = @MYSQL_CFLAGS@
regexp_la_CXXFLAGS = @MYSQL_CXXFLAGS@ 
regexp_la_LDFLAGS = -module -avoid-version -no-undefined

test: all
	cd tests; ./test.sh


pdf: manual.pdf

manual.pdf: manual.xml
	docbook2pdf manual.xml

html: manual.html

manual.html: manual.xml
	docbook2html -u manual.xml

