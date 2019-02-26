/*
 * Copyright (C) 2001 Rok Papez <rok.papez@lugos.si>
 * Rok Papez
 * Hribovska pot 17
 * 1231 Ljubljana - Crnuce
 * EUROPE, Slovenia
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

#ifdef   HAVE_CONFIG_H
#include "config.h"
#endif

/* Compile this file only if PAM is included */
#ifdef WITH_PAM

#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>

#include "pulsar.h"
#include "users_pam.h"

#include "util.h"
#include "error_facility.h"
//------------------------------------------------------------------------------------------------------------
int users_pam_auth_cb( int num_msg,
                       const struct pam_message **msg,
                       struct pam_response **resp,
                       void *appdata_ptr
                     )
{
  struct pam_response *data;
  int                  i;

  data = safe_calloc(sizeof(*data), num_msg);
  if(!data)
    return PAM_CONV_ERR; //error code.

  for(i=0; i<num_msg; i++) {
    switch(msg[i]->msg_style) {
    case PAM_PROMPT_ECHO_ON: // username
        data[i].resp = strdup(g.user);
        data[i].resp_retcode = PAM_SUCCESS;
        break;
    case PAM_PROMPT_ECHO_OFF: // password
        data[i].resp = strdup(appdata_ptr);
        data[i].resp_retcode = PAM_SUCCESS;
        break;
    default:
        data[i].resp = NULL;
        data[i].resp_retcode = PAM_SUCCESS;
        break;
    }
  }

  *resp = data;
  return PAM_SUCCESS;
}
//------------------------------------------------------------------------------------------------------------
int users_pam_auth(const int   type,
                   const char *credentials
                  )
{
  struct pam_conv  pamcnv;
  pam_handle_t    *pamh = NULL;
  int              rc;

  err_debug_function();



  if(defCredPass != type) {
    err_debug(5, "PAM authentication only works with PASS");
    return defERR;
  }



  pamcnv.conv = users_pam_auth_cb;
  pamcnv.appdata_ptr = (void *)credentials;

  err_suspend(); // suspend our error logging since PAM may walk all over us.
  rc = pam_start("pulsar", g.user, &pamcnv, &pamh);
  if(rc != PAM_SUCCESS) {
    err_resume();
    err_error(EX_OSERR,"pam_start failed: \"%s\"",pam_strerror(pamh,rc));
    rc = defAuth;
    goto error;
  }

  rc = pam_authenticate(pamh, PAM_SILENT | PAM_DISALLOW_NULL_AUTHTOK);
  if(rc != PAM_SUCCESS) {
    err_resume();
    err_error(EX_OSERR,
              "PAM authentication for user \"%s\" FAILED; Reason: \"%s\"",
              g.user,
              pam_strerror(pamh,rc)
             );
    rc = defAuth;
    goto error;
  }

  err_resume();
  err_debug(5,"PAM authentication for user \"%s\" OK.", g.user);
  pam_end(pamh,PAM_SUCCESS);

  rc = users_pam_getinfo();
  if(defOK != rc) { // if users_pam_getinfo failes we abort!
    err_debug(2,
              "WARNING: User \"%s\" information unavailable. You PAM authentication and "
              "user information retreival is misconfigured.",
              g.user
             );
    return rc;
  }
  return defOK;

error:
  pam_end(pamh, PAM_PERM_DENIED);
  return rc;
}
//------------------------------------------------------------------------------------------------------------
int users_pam_getinfo() {
  struct passwd *pw;

  pw = getpwnam(g.user);
  if(!pw) // this is not an error (yet)
    return defOK; 
  if(g.homedir)
    free(g.homedir);
  g.homedir = strdup(pw->pw_dir);
  if(!g.homedir) {
    err_malloc_error();
    return defMalloc;
  }
  g.owner.uid = pw->pw_uid;
  g.owner.uid_set = 1;
  g.owner.gid = pw->pw_gid;
  g.owner.gid_set = 1;

  return defOK;
}
//------------------------------------------------------------------------------------------------------------
#endif /* WITH PAM */
