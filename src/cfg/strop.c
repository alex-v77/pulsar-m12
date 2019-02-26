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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#define __USE_ISOC99
#define __USE_GNU
#include <ctype.h>
#include <stdlib.h>

#include "strop.h"
#include "util.h"

//------------------------------------------------------------------------------------------------------------
void strop_free(strValues *opts) {
  strValues *tmp;
  int i;
  tmp = opts;
  while(tmp) {
    opts = tmp;
    tmp = opts->next;
    if(opts->opt) {
      for(i=0; i<opts->opt_count; i++)
        if(opts->opt[i])
          free(opts->opt[i]);
      free(opts->opt);
    }
    if(opts->val) free(opts->val);
    free(opts);
  }
}
//------------------------------------------------------------------------------------------------------------
int strop_copy_var(const char *buf,char **var) {
  int i, j, k;
  for(i=0; isblank(buf[i]); i++); // eat blanks.
  for(j=i; isgraph(buf[j]) && '='!=buf[j]; j++); // get size of var.
  for(k=j; isblank(buf[k]); k++); // eat remaining blanks .. if any.
  if('=' != buf[k])
    return defSyntaxError;
  *var = safe_malloc(j-i+1);
  if(!*var)
    return defMallocError;
  memcpy(*var, &buf[i], j-i);
  (*var)[j-i] = '\0';
  k++;
  return k;
}
//------------------------------------------------------------------------------------------------------------
#define defModeESC  0x01
#define defModeDQ   0x02
#define setEsc(a)   ((a)|=defModeESC)
#define setDQ(a)    ((a)|=defModeDQ)
#define clearEsc(a) ((a)&=~defModeESC)
#define clearDQ(a)  ((a)&=~defModeDQ)
#define isEsc(a)    ((a)&defModeESC)
#define isDQ(a)     ((a)&defModeDQ)
//------------------------------------------------------------------------------------------------------------
int strop_segment_len(const char *buf) {
  int i=0;
  int j=0;
  int mode=0;
  for(;isblank(buf[i]);i++); // eat space before value.
  if('"'==buf[i]) {
    setDQ(mode);
    i++;
  }

  for(;buf[i];i++) {

    if('\\'==buf[i] && !isEsc(mode)) { // is this char escaped ?
      setEsc(mode);
      i++;
    }

    if(
       (!isEsc(mode) &&  isDQ(mode) && '"'==buf[i]     ) ||
       (!isEsc(mode) && !isDQ(mode) && !isgraph(buf[i])) ||
       (!isEsc(mode) && !isDQ(mode) && ','==buf[i]     ) ||
       (!isEsc(mode) && !isDQ(mode) && ':'==buf[i]     )
      )
      break;

    j++;
    clearEsc(mode);
  }

  if(isDQ(mode) && '"'!=buf[i])  // if we start with " we must end with "
    return defSyntaxError;

  return j;
}
//------------------------------------------------------------------------------------------------------------
int strop_segment_copy(const char *buf, char *segment) {
  int i=0;
  int j=0;
  int mode=0;
  for(;isblank(buf[i]);i++); // eat space before value.
  if('"'==buf[i]) {
    setDQ(mode);
    i++;
  }

  for(;buf[i];i++) {

    if('\\'==buf[i] && !isEsc(mode)) { // is this char escaped ?
      setEsc(mode);
      i++;
    }

    if(
       (!isEsc(mode) &&  isDQ(mode) && '"'==buf[i]     ) ||
       (!isEsc(mode) && !isDQ(mode) && !isgraph(buf[i])) ||
       (!isEsc(mode) && !isDQ(mode) && ','==buf[i]     ) ||
       (!isEsc(mode) && !isDQ(mode) && ':'==buf[i]     )
      )
      break;

    segment[j]=buf[i];
    j++;
    clearEsc(mode);
  }
  segment[j]='\0';

  if(isDQ(mode)) {
    if('"'!=buf[i])
      return defSyntaxError; // if you start with " you have to end with "
    else
      i++; // caller shouldn't choke on "
  }

  for(;isblank(buf[i]);i++); //eat blanks (if they exist)

  return i; // how many chars were processed.
}

//------------------------------------------------------------------------------------------------------------
int strop_copy_val(const char *buf,int *i,strValues **val) {
  strValues *tmp=NULL;
  strValues **walker=NULL;
  int size;
  char **opt;

  tmp = safe_malloc(sizeof(*tmp));
  if(!tmp)
    return defMallocError;
  memset(tmp, 0x00, sizeof(*tmp));
  size = strop_segment_len(&buf[*i]);
  if(size < 0)
    return size;
  tmp->val = safe_malloc(size+1);
  (*i) += strop_segment_copy(&buf[*i],tmp->val);

  if(isgraph(buf[*i]) && ','!=buf[*i] && ':'!=buf[*i])
    return defSyntaxError;

  while(':'==buf[*i]) { // options...
    (*i)++;
    tmp->opt_count++;
    opt=realloc(tmp->opt,tmp->opt_count*sizeof(*(tmp->opt)));
    if(!opt) {
      tmp->opt_count--;
      return defMallocError;
    }
    tmp->opt=opt;
    size=strop_segment_len(&buf[*i]);
    if(size<0)
      return size;
    tmp->opt[tmp->opt_count-1] = safe_malloc(size+1);
    if(!tmp->opt[tmp->opt_count-1])
      return defMallocError;
    (*i)+=strop_segment_copy(&buf[*i],tmp->opt[tmp->opt_count-1]);
    if((isgraph(buf[*i]) && ','!=buf[*i] && ':'!=buf[*i]))
      return defSyntaxError;
  }

  walker=val;
  while(*walker)
    walker=&((*walker)->next);
  *walker=tmp;

  return 0;
}
//------------------------------------------------------------------------------------------------------------
int strop_varval(const char *buf,char **var,strValues **val) {
  int rc=0;
  int i=0;
  int j;

  if (!buf)
    return defInvalidParameter;

  // if *var exists than we are processing an additional data line
  if(!*var) { 
    i = strop_copy_var(buf,var);
    if(i<0)
      return i;
  }

  do {
    rc = strop_copy_val(buf,&i,val);
    if(',' == buf[i]) i++;
  } while(rc>=0 && buf[i] && !iscntrl(buf[i]));

  for(j=i; j>=0 && !isgraph(buf[j]); j--);
  if(','==buf[j])
    return defExpectMoreData;

  return rc;
}
//------------------------------------------------------------------------------------------------------------
// this is just quick hack for testing routines....
/*
int main() {
  char chars1[]="  var1 = val1:parm11:parm12, val2:parm21,";
  char chars2[]="       val3:parm31:parm32:\"parm33\" ";
  char      *var=NULL;
  int        i;
  strValues *val=NULL;
  strValues *tmp=NULL;
  i=strop_varval(chars1,&var,&val);
  if(i==defExpectMoreData) i=strop_varval(chars2,&var,&val);
  switch (i) {
  case defMallocError:
      printf("MallocError\n");
      break;
  case defSyntaxError:
      printf("SyntaxError\n");
      break;
  case defInvalidParameter:
      printf("SyntaxError\n");
      break;
  }

  if (i)
    return i;

  printf("%s=\n",var);
  tmp=val;
  while(tmp) {
    printf("%s",tmp->val);
    for(i=0;i<tmp->opt_count;i++)
      printf(":%s",tmp->opt[i]);
    printf("\n");
    tmp=tmp->next;
  }

  strop_free(val);
  return 0;
}
*/
