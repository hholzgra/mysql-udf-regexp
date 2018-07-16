mysql-udf-regexp
================

This package implements regular expression functions as MySQL User Defined Functions (UDFs).

The functions implemented by this package are:

    REGEXP_LIKE(text, pattern [, mode])
    REGEXP_SUBSTR(text, pattern [,position [,occurence [,mode]]])
    REGEXP_INSTR?(text, pattern [,position [,occurence [,return_end [,mode]]]])
    REGEXP_REPLACE?(text, pattern, replace [,position [,occurence [,return_end [,mode]]])

The functions support the same regular expression syntax as the MySQL REGEXP operator as documented in the Regular Expressions appendix of the MySQL manual.

These functions are very similar to the Oracle SQL functions by the same name. They are not 100% compatible but should be good enough to act as replacements in most common use cases.
