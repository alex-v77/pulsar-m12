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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <string.h>

#include "cfg.h"
#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_MailspoolOwner(strValues *val, strOptionsRealm *realm) {
  struct passwd *pw;
  struct group  *gr;
  int            i;

  if (val->opt_count>1 || val->next || !val->val)
    return 6;

  realm->mailspool_owner.uid_set = 0;
  realm->mailspool_owner.gid_set = 0;

  if(strlen(val->val)) {
    pw = getpwnam(val->val);
    if(!pw) {
      for(i=0; isdigit(val->val[i]); i++);
      if(!val->val[i]) { // it must be all numbers! We don't want atoi() to give us 0 becouse of garbage.
        err_debug(0, "Bad uid specification at \"%s\".", val->val);
        return 6;
      }
      pw = getpwuid(atoi(val->val));
      if(!pw) {
        err_debug(0, "Unable to locate user with uid == %s", val->val);
        return 6;
      }
    }
    realm->mailspool_owner.uid = pw->pw_uid;
    realm->mailspool_owner.uid_set = 1;
    realm->mailspool_owner.gid = pw->pw_gid;
    realm->mailspool_owner.gid_set = 1;
  }

  if(1 != val->opt_count)
    return 0;

  if(!strlen(val->opt[0])) {
    realm->mailspool_owner.gid_set = 0;
    return 0;
  }

  gr = getgrnam(val->opt[0]);
  if(!gr) { // fallback
    for(i=0; isdigit(val->opt[0][i]); i++);
    if(!val->opt[0][i]) { // it must be all numbers! We don't want atoi() to give us 0 becouse of garbage.
      err_debug(0, "Bad gid specification at \"%s\".", val->opt[0]);
      return 6;
    }
    gr = getgrgid(atoi(val->opt[0]));
    if(!gr) {
      err_debug(0, "Unable to locate group with gid == %s", val->opt[0]);
      return 6;
    }
  }
  realm->mailspool_owner.gid = gr->gr_gid;
  realm->mailspool_owner.gid_set = 1;

  return 0;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_MailspoolOwner(FILE *fd, const char *fill, strOptionsRealm *realm) {
  if(!fd || !realm)
    return;

  if(!realm->mailspool_owner.uid_set && !realm->mailspool_owner.gid_set) {
    fprintf(fd,"#%smailspool_owner =\n", fill);
    return;
  }

  fprintf(fd,"%smailspool_owner = ", fill);
  if(realm->mailspool_owner.uid_set)
    fprintf(fd,"%d", realm->mailspool_owner.uid);
  if(realm->mailspool_owner.gid_set)
    fprintf(fd,":%d", realm->mailspool_owner.gid);
  fprintf(fd,"\n");
  return;
}
//------------------------------------------------------------------------------------------------------------
int add_realm_MailspoolOwner(strOptionsRealm *dst, strOptionsRealm *src) {
  if(!dst)
    return 0;

  if(!src) { // creation of default realm.
    dst->mailspool_owner.uid_set = 0;
    dst->mailspool_owner.uid = 0;
    dst->mailspool_owner.gid_set = 0;
    dst->mailspool_owner.gid = 0;
    return 1;
  }

  dst->mailspool_owner.uid_set = src->mailspool_owner.uid_set;
  dst->mailspool_owner.uid = src->mailspool_owner.uid;
  dst->mailspool_owner.gid_set = src->mailspool_owner.gid_set;
  dst->mailspool_owner.gid = src->mailspool_owner.gid;
  return 1;
}
//------------------------------------------------------------------------------------------------------------
