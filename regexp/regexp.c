/*
   +----------------------------------------------------------------------+
   | This program is free software; you can redistribute it and/or        |
   | modify it under the terms of the GNU General Public License          |
   | as published by the Free Software Foundation; either version 2       |                 
   | of the License, or (at your option) any later version.               | 
   |                                                                      |
   | This program is distributed in the hope that it will be useful,      |
   | but WITHOUT ANY WARRANTY; without even the implied warranty of       |
   | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        |
   | GNU General Public License for more details.                         | 
   |                                                                      |
   | You should have received a copy of the GNU General General Public    |
   | License in the file LICENSE along with this library;                 |
   | if not, write to the                                                 | 
   |                                                                      |
   |   Free Software Foundation, Inc.,                                    |
   |   59 Temple Place, Suite 330,                                        |
   |   Boston, MA  02111-1307  USA                                        |
   +----------------------------------------------------------------------+
   | Authors: Hartmut Holzgraefe <hartmut@mysql.com>                      |
   +----------------------------------------------------------------------+
*/

/* $ Id: $ */ 

// {{{ CREATE and DROP statements for this UDF

/*
register the functions provided by this UDF module using
CREATE FUNCTION regexp_like RETURNS INTEGER SONAME "regexp.so";
CREATE FUNCTION regexp_substr RETURNS STRING SONAME "regexp.so";
CREATE FUNCTION regexp_instr RETURNS INTEGER SONAME "regexp.so";
CREATE FUNCTION regexp_replace RETURNS STRING SONAME "regexp.so";

unregister the functions provided by this UDF module using
DROP FUNCTION regexp_like;
DROP FUNCTION regexp_substr;
DROP FUNCTION regexp_instr;
DROP FUNCTION regexp_replace;
*/
// }}}

// {{{ standard header stuff
#ifdef STANDARD
#include <stdio.h>
#include <string.h>
#ifdef __WIN__
typedef unsigned __int64 ulonglong; /* Microsofts 64 bit types */
typedef __int64 longlong;
#else
typedef unsigned long long ulonglong;
typedef long long longlong;
#endif /*__WIN__*/
#else
#include <my_global.h>
#include <my_sys.h>
#endif
#include <mysql.h>
#include <m_ctype.h>
#include <m_string.h>       // To get strmov()

// }}}

#ifdef HAVE_DLOPEN

#include "udf_regexp.h"

// {{{ prototypes

#ifdef  __cplusplus
extern "C" {
#endif
/* FUNCTION regexp_like */
my_bool regexp_like_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
long long regexp_like(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
void regexp_like_deinit(UDF_INIT *initid);

/* FUNCTION regexp_substr */
my_bool regexp_substr_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char * regexp_substr(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
void regexp_substr_deinit(UDF_INIT *initid);

/* FUNCTION regexp_instr */
my_bool regexp_instr_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
long long regexp_instr(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
void regexp_instr_deinit(UDF_INIT *initid);

/* FUNCTION regexp_replace */
my_bool regexp_replace_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char * regexp_replace(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
void regexp_replace_deinit(UDF_INIT *initid);

#ifdef  __cplusplus
}
#endif
// }}}

// {{{ UDF functions

// {{{ FUNCTION regexp_like RETURNS INTEGER
struct regexp_like_t {
  my_regex_t expr;
  int dynamic;
};

/* regexp_like init function */
my_bool regexp_like_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    DBUG_ENTER("regexp::regexp_like_init");
    struct regexp_like_t *data = (struct regexp_like_t *)calloc(sizeof(struct regexp_like_t), 1);

    char *text = NULL;
    long long text_len = 0;
    int text_is_null = 1;
    char *pattern = NULL;
    long long pattern_len = 0;
    int pattern_is_null = 1;
    char *mode = NULL;
    long long mode_len = 0;
    int mode_is_null = 1;

    text_is_null = (args->args[0]==NULL);
    text = (char *)args->args[0];
    text_len = (args->args[0] == NULL) ? 0 : args->lengths[0];
    pattern_is_null = (args->args[1]==NULL);
    pattern = (char *)args->args[1];
    pattern_len = (args->args[1] == NULL) ? 0 : args->lengths[1];
    if (args->arg_count > 2) {
        mode_is_null = (args->args[2]==NULL);
        mode = (char *)args->args[2];
        mode_len = (args->args[2] == NULL) ? 0 : args->lengths[2];
    }
    if (!data) {
        strcpy(message, "out of memory in regexp_like()");
        DBUG_RETURN(1);
    }

    initid->ptr = (char *)data;

    initid->maybe_null = 1;

    if ((args->arg_count < 2) || (args->arg_count > 3)) {
        strcpy(message,"wrong number of parameters for regexp_like()");
        DBUG_RETURN(1);
    }
    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    if (args->arg_count > 2) args->arg_type[2] = STRING_RESULT;

	do {
		if (pattern) {
			// static regex pattern -> we can compile it once and reuse it 
			int stat;
			char *copy;

			// we have to make sure we have a NUL terminated C string
			// as argument for my_regcomp           
			copy = strndup(pattern, pattern_len);
			stat  = my_regcomp(&data->expr, copy, parse_mode(mode, mode_len), &my_charset_latin1);
			free(copy);

			if (stat) {
				sprintf(message, "regcomp failed (error: %d)", stat);
				return 1; 
			}

			data->dynamic = 0;
		} else {
			data->dynamic = 1;
		}
	} while (0);

    DBUG_RETURN(0);
}

/* regexp_like deinit function */
void regexp_like_deinit(UDF_INIT *initid)
{
    DBUG_ENTER("regexp::regexp_like_deinit");
    struct regexp_like_t *data = (struct regexp_like_t *)(initid->ptr);

	if (!data->dynamic) {
		// free static compiler pattern
		my_regfree(&data->expr);
	}

    if (initid->ptr) {
        free(initid->ptr);
    }
    DBUG_VOID_RETURN;
}

/* regexp_like actual processing function */
long long regexp_like(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    DBUG_ENTER("regexp::regexp_like");
    struct regexp_like_t *data = (struct regexp_like_t *)initid->ptr;

    char *text = NULL;
    long long text_len = 0;
    int text_is_null = 1;
    char *pattern = NULL;
    long long pattern_len = 0;
    int pattern_is_null = 1;
    char *mode = NULL;
    long long mode_len = 0;
    int mode_is_null = 1;

    text_is_null = (args->args[0]==NULL);
    text = (char *)args->args[0];
    text_len = (args->args[0] == NULL) ? 0 : args->lengths[0];
    pattern_is_null = (args->args[1]==NULL);
    pattern = (char *)args->args[1];
    pattern_len = (args->args[1] == NULL) ? 0 : args->lengths[1];
    if (args->arg_count > 2) {
        mode_is_null = (args->args[2]==NULL);
        mode = (char *)args->args[2];
        mode_len = (args->args[2] == NULL) ? 0 : args->lengths[2];
    }
	do {
		my_regmatch_t match;
		int stat;
		char *copy;
		
		if (data->dynamic) {
			copy = strndup(pattern, pattern_len);
			stat  = my_regcomp(&data->expr, copy, parse_mode(mode, mode_len), &my_charset_latin1);
			free(copy);
			if (stat) {
				// TODO: need ERROR() and WARNING() macro
				RETURN_NULL;
			}
		}

		copy = strndup(text, text_len);
		stat = my_regexec(&data->expr, copy, 1, &match, 0);
		free(copy);

		if (data->dynamic) {
			my_regfree(&data->expr);
		}

		if (stat && (stat != REG_NOMATCH)) {
			fprintf(stderr, "regexec error %d '%s' '%s'\n", stat, pattern, text);
			RETURN_NULL;
		}

		RETURN_INT(stat == REG_NOMATCH ? 0 : 1);
	} while (0);
}

// }}}

// {{{ FUNCTION regexp_substr RETURNS STRING
struct regexp_substr_t {
  char * _resultbuf;
  unsigned long _resultbuf_len;
  my_regex_t expr;
  int dynamic;
};

/* regexp_substr init function */
my_bool regexp_substr_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    DBUG_ENTER("regexp::regexp_substr_init");
    struct regexp_substr_t *data = (struct regexp_substr_t *)calloc(sizeof(struct regexp_substr_t), 1);

    char *text = NULL;
    long long text_len = 0;
    int text_is_null = 1;
    char *pattern = NULL;
    long long pattern_len = 0;
    int pattern_is_null = 1;
    long long position = 1;
    int position_is_null = 0;
    long long occurence = 1;
    int occurence_is_null = 0;
    char *mode = "c";
    long long mode_len = 1;
    int mode_is_null = 0;

    text_is_null = (args->args[0]==NULL);
    text = (char *)args->args[0];
    text_len = (args->args[0] == NULL) ? 0 : args->lengths[0];
    pattern_is_null = (args->args[1]==NULL);
    pattern = (char *)args->args[1];
    pattern_len = (args->args[1] == NULL) ? 0 : args->lengths[1];
    if (args->arg_count > 2) {
        position_is_null = (args->args[2]==NULL);
        position = (args->args[2] == NULL) ? 0 : *((long long *)args->args[2]);
    }
    if (args->arg_count > 3) {
        occurence_is_null = (args->args[3]==NULL);
        occurence = (args->args[3] == NULL) ? 0 : *((long long *)args->args[3]);
    }
    if (args->arg_count > 4) {
        mode_is_null = (args->args[4]==NULL);
        mode = (char *)args->args[4];
        mode_len = (args->args[4] == NULL) ? 0 : args->lengths[4];
    }
    if (!data) {
        strcpy(message, "out of memory in regexp_substr()");
        DBUG_RETURN(1);
    }

    data->_resultbuf = NULL;
    data->_resultbuf_len = 0L;
    initid->ptr = (char *)data;

    initid->maybe_null = 1;
    initid->max_length = 255;

    if ((args->arg_count < 2) || (args->arg_count > 5)) {
        strcpy(message,"wrong number of parameters for regexp_substr()");
        DBUG_RETURN(1);
    }
    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    if (args->arg_count > 2) args->arg_type[2] = INT_RESULT;
    if (args->arg_count > 3) args->arg_type[3] = INT_RESULT;
    if (args->arg_count > 4) args->arg_type[4] = STRING_RESULT;

	do {
		if (pattern) {
			// static regex pattern -> we can compile it once and reuse it 
			int stat;
			char *copy;

			// we have to make sure we have a NUL terminated C string
			// as argument for my_regcomp           
			copy = strndup(pattern, pattern_len);
			stat  = my_regcomp(&data->expr, copy, parse_mode(mode, mode_len), &my_charset_latin1);
			free(copy);

			if (stat) {
				sprintf(message, "regcomp failed (error: %d)", stat);
				return 1; 
			}

			data->dynamic = 0;
		} else {
			data->dynamic = 1;
		}
	} while (0);

    DBUG_RETURN(0);
}

/* regexp_substr deinit function */
void regexp_substr_deinit(UDF_INIT *initid)
{
    DBUG_ENTER("regexp::regexp_substr_deinit");
    struct regexp_substr_t *data = (struct regexp_substr_t *)(initid->ptr);

	if (!data->dynamic) {
		// free static compiler pattern
		my_regfree(&data->expr);
	}

    if (initid->ptr) {
        free(initid->ptr);
    }
    DBUG_VOID_RETURN;
}

/* regexp_substr actual processing function */
char * regexp_substr(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error)
{
    DBUG_ENTER("regexp::regexp_substr");
    struct regexp_substr_t *data = (struct regexp_substr_t *)initid->ptr;

    char *text = NULL;
    long long text_len = 0;
    int text_is_null = 1;
    char *pattern = NULL;
    long long pattern_len = 0;
    int pattern_is_null = 1;
    long long position = 1;
    int position_is_null = 0;
    long long occurence = 1;
    int occurence_is_null = 0;
    char *mode = "c";
    long long mode_len = 1;
    int mode_is_null = 0;

    text_is_null = (args->args[0]==NULL);
    text = (char *)args->args[0];
    text_len = (args->args[0] == NULL) ? 0 : args->lengths[0];
    pattern_is_null = (args->args[1]==NULL);
    pattern = (char *)args->args[1];
    pattern_len = (args->args[1] == NULL) ? 0 : args->lengths[1];
    if (args->arg_count > 2) {
        position_is_null = (args->args[2]==NULL);
        position = (args->args[2] == NULL) ? 0 : *((long long *)args->args[2]);
    }
    if (args->arg_count > 3) {
        occurence_is_null = (args->args[3]==NULL);
        occurence = (args->args[3] == NULL) ? 0 : *((long long *)args->args[3]);
    }
    if (args->arg_count > 4) {
        mode_is_null = (args->args[4]==NULL);
        mode = (char *)args->args[4];
        mode_len = (args->args[4] == NULL) ? 0 : args->lengths[4];
    }
	do {
		my_regmatch_t match;
		int stat = 0;
		char *copy;
		
		if (occurence < 1) {
			RETURN_NULL;
		}

		if (position) {
			position -= 1; /* oracle offsets start at 1, not 0 */
			if (position >= text_len) {
				RETURN_NULL;
			}
		}

		if (data->dynamic) {
			copy = strndup(pattern, pattern_len);
			stat  = my_regcomp(&data->expr, copy, parse_mode(mode, mode_len), &my_charset_latin1);
			free(copy);
			if (stat) {
				// TODO: need ERROR() and WARNING() macro
				RETURN_NULL;
			}
		}

		copy = strndup(text, text_len);

		while (occurence > 0) {
			stat = my_regexec(&data->expr, copy + position, 1, &match, 0);
			if (stat) {
				break;
			}
			if (--occurence) {
				position += match.rm_eo;
			}
		}

		free(copy);

		if (data->dynamic) {
			my_regfree(&data->expr);
		}

		if (stat) {
			if (stat != REG_NOMATCH) {
				fprintf(stderr, "regexec error %d '%s' '%s'\n", stat, pattern, text);
			}
			RETURN_NULL;
		}

		RETURN_STRINGL(text + position + match.rm_so, match.rm_eo - match.rm_so);
	} while (0);
}

// }}}

// {{{ FUNCTION regexp_instr RETURNS INTEGER
struct regexp_instr_t {
  my_regex_t expr;
  int dynamic;
};

/* regexp_instr init function */
my_bool regexp_instr_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    DBUG_ENTER("regexp::regexp_instr_init");
    struct regexp_instr_t *data = (struct regexp_instr_t *)calloc(sizeof(struct regexp_instr_t), 1);

    char *text = NULL;
    long long text_len = 0;
    int text_is_null = 1;
    char *pattern = NULL;
    long long pattern_len = 0;
    int pattern_is_null = 1;
    long long position = 1;
    int position_is_null = 0;
    long long occurrence = 1;
    int occurrence_is_null = 0;
    long long return_end = 0;
    int return_end_is_null = 0;
    char *mode = "c";
    long long mode_len = 1;
    int mode_is_null = 0;

    text_is_null = (args->args[0]==NULL);
    text = (char *)args->args[0];
    text_len = (args->args[0] == NULL) ? 0 : args->lengths[0];
    pattern_is_null = (args->args[1]==NULL);
    pattern = (char *)args->args[1];
    pattern_len = (args->args[1] == NULL) ? 0 : args->lengths[1];
    if (args->arg_count > 2) {
        position_is_null = (args->args[2]==NULL);
        position = (args->args[2] == NULL) ? 0 : *((long long *)args->args[2]);
    }
    if (args->arg_count > 3) {
        occurrence_is_null = (args->args[3]==NULL);
        occurrence = (args->args[3] == NULL) ? 0 : *((long long *)args->args[3]);
    }
    if (args->arg_count > 4) {
        return_end_is_null = (args->args[4]==NULL);
        return_end = (args->args[4] == NULL) ? 0 : *((long long *)args->args[4]);
    }
    if (args->arg_count > 5) {
        mode_is_null = (args->args[5]==NULL);
        mode = (char *)args->args[5];
        mode_len = (args->args[5] == NULL) ? 0 : args->lengths[5];
    }
    if (!data) {
        strcpy(message, "out of memory in regexp_instr()");
        DBUG_RETURN(1);
    }

    initid->ptr = (char *)data;

    initid->maybe_null = 1;

    if ((args->arg_count < 2) || (args->arg_count > 6)) {
        strcpy(message,"wrong number of parameters for regexp_instr()");
        DBUG_RETURN(1);
    }
    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    if (args->arg_count > 2) args->arg_type[2] = INT_RESULT;
    if (args->arg_count > 3) args->arg_type[3] = INT_RESULT;
    if (args->arg_count > 4) args->arg_type[4] = INT_RESULT;
    if (args->arg_count > 5) args->arg_type[5] = STRING_RESULT;

	do {
		if (pattern) {
			// static regex pattern -> we can compile it once and reuse it 
			int stat;
			char *copy;

			// we have to make sure we have a NUL terminated C string
			// as argument for my_regcomp           
			copy = strndup(pattern, pattern_len);
			stat  = my_regcomp(&data->expr, copy, parse_mode(mode, mode_len), &my_charset_latin1);
			free(copy);

			if (stat) {
				sprintf(message, "regcomp failed (error: %d)", stat);
				return 1; 
			}

			data->dynamic = 0;
		} else {
			data->dynamic = 1;
		}
	} while (0);

    DBUG_RETURN(0);
}

/* regexp_instr deinit function */
void regexp_instr_deinit(UDF_INIT *initid)
{
    DBUG_ENTER("regexp::regexp_instr_deinit");
    struct regexp_instr_t *data = (struct regexp_instr_t *)(initid->ptr);

	if (!data->dynamic) {
		// free static compiler pattern
		my_regfree(&data->expr);
	}

    if (initid->ptr) {
        free(initid->ptr);
    }
    DBUG_VOID_RETURN;
}

/* regexp_instr actual processing function */
long long regexp_instr(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    DBUG_ENTER("regexp::regexp_instr");
    struct regexp_instr_t *data = (struct regexp_instr_t *)initid->ptr;

    char *text = NULL;
    long long text_len = 0;
    int text_is_null = 1;
    char *pattern = NULL;
    long long pattern_len = 0;
    int pattern_is_null = 1;
    long long position = 1;
    int position_is_null = 0;
    long long occurrence = 1;
    int occurrence_is_null = 0;
    long long return_end = 0;
    int return_end_is_null = 0;
    char *mode = "c";
    long long mode_len = 1;
    int mode_is_null = 0;

    text_is_null = (args->args[0]==NULL);
    text = (char *)args->args[0];
    text_len = (args->args[0] == NULL) ? 0 : args->lengths[0];
    pattern_is_null = (args->args[1]==NULL);
    pattern = (char *)args->args[1];
    pattern_len = (args->args[1] == NULL) ? 0 : args->lengths[1];
    if (args->arg_count > 2) {
        position_is_null = (args->args[2]==NULL);
        position = (args->args[2] == NULL) ? 0 : *((long long *)args->args[2]);
    }
    if (args->arg_count > 3) {
        occurrence_is_null = (args->args[3]==NULL);
        occurrence = (args->args[3] == NULL) ? 0 : *((long long *)args->args[3]);
    }
    if (args->arg_count > 4) {
        return_end_is_null = (args->args[4]==NULL);
        return_end = (args->args[4] == NULL) ? 0 : *((long long *)args->args[4]);
    }
    if (args->arg_count > 5) {
        mode_is_null = (args->args[5]==NULL);
        mode = (char *)args->args[5];
        mode_len = (args->args[5] == NULL) ? 0 : args->lengths[5];
    }
	do {
		my_regmatch_t match;
		int stat;
		char *copy;

		if (position) {
			position -= 1; /* oracle offsets start at 1, not 0 */
			if (position >= text_len) {
				RETURN_NULL;
			}
		}
 
		if (data->dynamic) {
			copy = strndup(pattern, pattern_len);
			stat  = my_regcomp(&data->expr, copy, parse_mode(mode, mode_len), &my_charset_latin1);
			free(copy);
			if (stat) {
				// TODO: need ERROR() and WARNING() macro
				RETURN_NULL;
			}
		}

		copy = strndup(text, text_len);
		match.rm_eo = 0;
		do {
			position += match.rm_eo;
			stat = my_regexec(&data->expr, copy + (size_t)position, 1, &match, 0);
		} while ((stat == 0) && --occurrence > 0);

		free(copy);

		if (data->dynamic) {
			my_regfree(&data->expr);
		}

		if (stat) {
			fprintf(stderr, "regexec error %d '%s' '%s'\n", stat, pattern, text);
			RETURN_NULL;
		}

		RETURN_INT(position + (return_end ? match.rm_eo : match.rm_so + 1));
	} while (0);
}

// }}}

// {{{ FUNCTION regexp_replace RETURNS STRING
struct regexp_replace_t {
  char * _resultbuf;
  unsigned long _resultbuf_len;
};

/* regexp_replace init function */
my_bool regexp_replace_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    DBUG_ENTER("regexp::regexp_replace_init");
    struct regexp_replace_t *data = (struct regexp_replace_t *)calloc(sizeof(struct regexp_replace_t), 1);

    char *text = NULL;
    long long text_len = 0;
    int text_is_null = 1;
    char *pattern = NULL;
    long long pattern_len = 0;
    int pattern_is_null = 1;
    char *replace = NULL;
    long long replace_len = 0;
    int replace_is_null = 1;
    long long position = 1;
    int position_is_null = 0;
    long long occurence = 0;
    int occurence_is_null = 0;
    char *mode = "c";
    long long mode_len = 1;
    int mode_is_null = 0;

    text_is_null = (args->args[0]==NULL);
    text = (char *)args->args[0];
    text_len = (args->args[0] == NULL) ? 0 : args->lengths[0];
    pattern_is_null = (args->args[1]==NULL);
    pattern = (char *)args->args[1];
    pattern_len = (args->args[1] == NULL) ? 0 : args->lengths[1];
    replace_is_null = (args->args[2]==NULL);
    replace = (char *)args->args[2];
    replace_len = (args->args[2] == NULL) ? 0 : args->lengths[2];
    if (args->arg_count > 3) {
        position_is_null = (args->args[3]==NULL);
        position = (args->args[3] == NULL) ? 0 : *((long long *)args->args[3]);
    }
    if (args->arg_count > 4) {
        occurence_is_null = (args->args[4]==NULL);
        occurence = (args->args[4] == NULL) ? 0 : *((long long *)args->args[4]);
    }
    if (args->arg_count > 5) {
        mode_is_null = (args->args[5]==NULL);
        mode = (char *)args->args[5];
        mode_len = (args->args[5] == NULL) ? 0 : args->lengths[5];
    }
    if (!data) {
        strcpy(message, "out of memory in regexp_replace()");
        DBUG_RETURN(1);
    }

    data->_resultbuf = NULL;
    data->_resultbuf_len = 0L;
    initid->ptr = (char *)data;

    initid->maybe_null = 1;
    initid->max_length = 255;

    if ((args->arg_count < 3) || (args->arg_count > 6)) {
        strcpy(message,"wrong number of parameters for regexp_replace()");
        DBUG_RETURN(1);
    }
    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    args->arg_type[2] = STRING_RESULT;
    if (args->arg_count > 3) args->arg_type[3] = INT_RESULT;
    if (args->arg_count > 4) args->arg_type[4] = INT_RESULT;
    if (args->arg_count > 5) args->arg_type[5] = STRING_RESULT;

    DBUG_RETURN(0);
}

/* regexp_replace deinit function */
void regexp_replace_deinit(UDF_INIT *initid)
{
    DBUG_ENTER("regexp::regexp_replace_deinit");
    struct regexp_replace_t *data = (struct regexp_replace_t *)(initid->ptr);

    if (initid->ptr) {
        free(initid->ptr);
    }
    DBUG_VOID_RETURN;
}

/* regexp_replace actual processing function */
char * regexp_replace(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error)
{
    DBUG_ENTER("regexp::regexp_replace");
    struct regexp_replace_t *data = (struct regexp_replace_t *)initid->ptr;

    char *text = NULL;
    long long text_len = 0;
    int text_is_null = 1;
    char *pattern = NULL;
    long long pattern_len = 0;
    int pattern_is_null = 1;
    char *replace = NULL;
    long long replace_len = 0;
    int replace_is_null = 1;
    long long position = 1;
    int position_is_null = 0;
    long long occurence = 0;
    int occurence_is_null = 0;
    char *mode = "c";
    long long mode_len = 1;
    int mode_is_null = 0;

    text_is_null = (args->args[0]==NULL);
    text = (char *)args->args[0];
    text_len = (args->args[0] == NULL) ? 0 : args->lengths[0];
    pattern_is_null = (args->args[1]==NULL);
    pattern = (char *)args->args[1];
    pattern_len = (args->args[1] == NULL) ? 0 : args->lengths[1];
    replace_is_null = (args->args[2]==NULL);
    replace = (char *)args->args[2];
    replace_len = (args->args[2] == NULL) ? 0 : args->lengths[2];
    if (args->arg_count > 3) {
        position_is_null = (args->args[3]==NULL);
        position = (args->args[3] == NULL) ? 0 : *((long long *)args->args[3]);
    }
    if (args->arg_count > 4) {
        occurence_is_null = (args->args[4]==NULL);
        occurence = (args->args[4] == NULL) ? 0 : *((long long *)args->args[4]);
    }
    if (args->arg_count > 5) {
        mode_is_null = (args->args[5]==NULL);
        mode = (char *)args->args[5];
        mode_len = (args->args[5] == NULL) ? 0 : args->lengths[5];
    }
	do {
		char *c_pattern, *c_replace, *c_text;
		char *result;

		if (position) {
			position -= 1; /* oracle offsets start at 1, not 0 */
			if (position >= text_len) {
				RETURN_NULL;
			}
		}

		c_pattern = strndup(pattern, pattern_len);
		c_replace = strndup(replace, replace_len);
		c_text    = strndup(text, text_len);

		result = my_regex_replace(c_pattern, c_replace, c_text, position, occurence, parse_mode(mode, mode_len));

		free(c_pattern);
		free(c_replace);
		free(c_text);

		if (result) {
			RETURN_STRING(result);
		} else {
			RETURN_NULL;
		}
	} while (0);
}

// }}}

// }}}

#else
#error your installation does not support loading UDFs
#endif /* HAVE_DLOPEN */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
