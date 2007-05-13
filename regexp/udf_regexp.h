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

#ifndef UDF_REGEXP_H
#define UDF_REGEXP_H

// {{{ user defined header code

	#include <sys/types.h>
	
	#include <regex/my_regex.h>
	
	// helper function borrowed from PHP, slightly modified
	static char *my_regex_replace(const char *pattern, 
								  const char *replace, 
								  const char *string, 
								  int position,
								  int occurence, 
								  int mode)
	{
		my_regex_t re;
		my_regmatch_t *subs;
	
		char *buf,  /* buf is where we build the replaced string */
			 *nbuf, /* nbuf is used when we grow the buffer */
			 *walkbuf; /* used to walk buf when replacing backrefs */
		const char *walk; /* used to walk replacement string for backrefs */
		int buf_len;
		int pos, tmp, string_len, new_l;
		int err;
		int match_no;
	
		string_len = strlen(string);
	
		err = my_regcomp(&re, pattern, mode, &my_charset_latin1);
		if (err) {
			return NULL;
		}
	
		/* allocate storage for (sub-)expression-matches */
		subs = (my_regmatch_t *)calloc(sizeof(my_regmatch_t),re.re_nsub+1);
	
		/* start with a buffer that is twice the size of the stringo
		   we're doing replacements in */
		buf_len = 2 * string_len + 1;
		buf = calloc(buf_len, sizeof(char));
	
		err = 0;
		match_no = 0;
	
		if (position) {
			// obey request to skip string start
			pos = position;
			strncpy(buf, string, pos);
		} else {
			pos = 0;
			buf[0] = '\0';
		}
	
		while (!err) {
			err = my_regexec(&re, &string[pos], re.re_nsub+1, subs, mode | (pos ? REG_NOTBOL : 0));
	
			if (err && err != REG_NOMATCH) {
				free(subs);
				free(buf);
				my_regfree(&re);
				return NULL;
			}
	
			match_no++;
	
			if ((occurence > 0)) {
				if (match_no < occurence) {
					// append pattern up to the match end 
					// no need to recalculate the buffer size here 
					// as no replaces have occured yet
					strncat(buf, &string[pos], subs[0].rm_eo);
					pos += subs[0].rm_eo;
					continue;
				} else if (match_no > occurence) {
					err = REG_NOMATCH;
				}
			}
	
	
			if (!err) {
				/* backref replacement is done in two passes:
				   1) find out how long the string will be, and allocate buf
				   2) copy the part before match, replacement and backrefs to buf
	
				   Jaakko Hyvätti <Jaakko.Hyvatti@iki.fi>
				   */
	
				new_l = strlen(buf) + subs[0].rm_so; /* part before the match */
				walk = replace;
				while (*walk) {
					if ('\\' == *walk && isdigit((unsigned char)walk[1]) && ((unsigned char)walk[1]) - '0' <= re.re_nsub) {
						if (subs[walk[1] - '0'].rm_so > -1 && subs[walk[1] - '0'].rm_eo > -1) {
							new_l += subs[walk[1] - '0'].rm_eo - subs[walk[1] - '0'].rm_so;
						}    
						walk += 2;
					} else {
						new_l++;
						walk++;
					}
				}
				if (new_l + 1 > buf_len) {
					buf_len = 1 + buf_len + 2 * new_l;
					nbuf = malloc(buf_len);
					strcpy(nbuf, buf);
					free(buf);
					buf = nbuf;
				}
				tmp = strlen(buf);
				/* copy the part of the string before the match */
				strncat(buf, &string[pos], subs[0].rm_so);
	
				/* copy replacement and backrefs */
				walkbuf = &buf[tmp + subs[0].rm_so];
				walk = replace;
				while (*walk) {
					if ('\\' == *walk && isdigit(walk[1]) && walk[1] - '0' <= (int)re.re_nsub) {
						if (subs[walk[1] - '0'].rm_so > -1 && subs[walk[1] - '0'].rm_eo > -1
							/* this next case shouldn't happen. it does. */
							&& subs[walk[1] - '0'].rm_so <= subs[walk[1] - '0'].rm_eo) {
							
							tmp = subs[walk[1] - '0'].rm_eo - subs[walk[1] - '0'].rm_so;
							memcpy (walkbuf, &string[pos + subs[walk[1] - '0'].rm_so], tmp);
							walkbuf += tmp;
						}
						walk += 2;
					} else {
						*walkbuf++ = *walk++;
					}
				}
				*walkbuf = '\0';
	
				/* and get ready to keep looking for replacements */
				if (subs[0].rm_so == subs[0].rm_eo) {
					if (subs[0].rm_so + pos >= string_len) {
						break;
					}
					new_l = strlen (buf) + 1;
					if (new_l + 1 > buf_len) {
						buf_len = 1 + buf_len + 2 * new_l;
						nbuf = calloc(buf_len, sizeof(char));
						strcpy(nbuf, buf);
						free(buf);
						buf = nbuf;
					}
					pos += subs[0].rm_eo + 1;
					buf [new_l-1] = string [pos-1];
					buf [new_l] = '\0';
				} else {
					pos += subs[0].rm_eo;
				}
			} else { /* REG_NOMATCH */
				new_l = strlen(buf) + strlen(&string[pos]);
				if (new_l + 1 > buf_len) {
					buf_len = new_l + 1; /* now we know exactly how long it is */
					nbuf = calloc(buf_len, sizeof(char));
					strcpy(nbuf, buf);
					free(buf);
					buf = nbuf;
				}
				/* stick that last bit of string on our output */
				strcat(buf, &string[pos]);
			}
		}
	
		/* don't want to leak memory .. */
		free(subs);
		my_regfree(&re);
	
		/* whew. */
		return (buf);
	}
	
	static int parse_mode(const char *mode, int len)
	{
		int flags = REG_EXTENDED | REG_NEWLINE;
	
		if (mode) {
			while (len-- > 0) {
				switch (*mode++) {
					case 'i': flags |=  REG_ICASE;   break; /* case insensitive */
					case 'c': flags &= ~REG_ICASE;   break; /* case sensitive   */
					case 'n':  break; /* . matches newline */
					case 'm':  break; /* multiple lines    */
					case 'x':  break; /* ignore whitespace */
					default: break;
				}
			}
		}
	
		return flags;
	}
// }}} 

        

#define RETURN_NULL          { *is_null = 1; DBUG_RETURN(0); }

#define RETURN_INT(x)        { *is_null = 0; DBUG_RETURN(x); }

#define RETURN_REAL(x)       { *is_null = 0; DBUG_RETURN(x); }

#define RETURN_STRINGL(s, l) { \
  if (s == NULL) { \
    *is_null = 1; \
    DBUG_RETURN(NULL); \
  } \
  *is_null = 0; \
  *length = l; \
  if (l < 255) { \
    memcpy(result, s, l); \
    DBUG_RETURN(result); \
  } \
  if (l > data->_resultbuf_len) { \
    data->_resultbuf = realloc(data->_resultbuf, l); \
    if (!data->_resultbuf) { \
      *error = 1; \
      DBUG_RETURN(NULL); \
    } \
    data->_resultbuf_len = l; \
  } \
  memcpy(data->_resultbuf, s, l); \
  DBUG_RETURN(data->_resultbuf); \
}

#define RETURN_STRING(s) { \
  if (s == NULL) { \
    *is_null = 1; \
    DBUG_RETURN(NULL); \
  } \
  RETURN_STRINGL(s, strlen(s)); \
}

#define RETURN_DATETIME(d)   { *length = my_datetime_to_str(d, result); *is_null = 0; DBUG_RETURN(result); }


#endif /* UDF_REGEXP_H */

