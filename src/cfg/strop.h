/*
 * Copyright (C) 2001 Rok Papez <rok.papez@lugos.si>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * This library is for line oriented processing of config files.
 * The supported syntax is like:
 * var1 = val1:option1:option2:...:optionN, val2:options, ...
 *
 * A character (:) seperates a value from an additional (and optional)
 * option. Variables can span multiple lines if the non-ending lines
 * end with a character (,).
 *
 * Escape character is a back-slash (\). Quotations are supported with
 * double quotation character (").
 *
 * These routines are very flexible but unfortunately also somewhat
 * complicated.
 *
 */

#ifndef __STROP_H__
#define __STROP_H__

typedef struct _strValues {
  char               *val;         // value of the variable being processed.
  int                opt_count;    // number of options
  char               **opt;        // Pointer to an array of strings (options)
  struct _strValues  *next;        // next (optional) value
} strValues;

/*
 * rc <0 -> error.
 * rc =0 -> no error.
 * rc >0 -> more data is needed. Variable spans additional line.
 *
 */
#define defExpectMoreData    1
#define defNoError           0
#define defMallocError      -1
#define defSyntaxError      -2
#define defInvalidParameter -3
int strop_varval(const char *buf,char **var,strValues **val);

/* These functions are private.. used internally by strop_varval */
int  strop_val_copy(const char *buf,int *i,strValues **val);
int  strop_var_copy(const char *buf,char **var);
int  strop_segment_len(const char *buf);
int  strop_segment_copy(const char *buf, char *segment);
void strop_free(strValues *opts);

#else

#ifdef DEBUG
#warning file "strop.h" already included.
#endif /* DEBUG */

#endif /* __STROP_H__ */
