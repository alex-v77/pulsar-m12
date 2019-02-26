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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sysexits.h>
#include <netinet/in.h>

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif /* HAVE_CRYPT_H */

#include "md5.h"

char table[]    = "./0123456789abcdefghijklnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
char md5_salt[] = "$1$12345678";
char des_salt[] = "xy";

//------------------------------------------------------------------------------------------------------------
void help(const char *filename) {
  printf("%s [ -c <password> [ -d | -m | <salt>]]  |  [ -h <to_md5_hash> ]\n"
         "-----------------------------------------------------------------------------------\n"
         " -c <password>   - <password> to be crypted\n"
         " -d              - random generate DES salt\n"
         " -m              - random generate MD5 salt\n"
         " <salt>          - DES: specify 2 salt characters from ./a..zA..Z0..9\n"
         "                 - MD5: specify \"\\$1\\$\" + 8 salt characters from ./a..zA..Z0..9\n"
         "-----------------------------------------------------------------------------------\n"
         " -h <string>     - string to MD5 hash\n"
         "-----------------------------------------------------------------------------------\n"
         ,filename
        );
  return;
}
//------------------------------------------------------------------------------------------------------------
char *generate_md5_salt() {
  long int index;
  int      i;

  for(i=3; i<(sizeof(md5_salt) - 1); i++) {
    index = random() + getpid();
    index = index % (sizeof(table) - 2);
    md5_salt[i] = table[index];
  }

  printf("Using random MD5 salt.\n");
  return md5_salt;
}
//------------------------------------------------------------------------------------------------------------
char *generate_des_salt() {
  long int index;
  int      i;

  for(i=0; i<(sizeof(des_salt) - 1); i++) {
    index = random() + getpid();
    index = index % (sizeof(table) - 2);
    des_salt[i] = table[index];
  }

  printf("Using random DES salt.\n");
  return des_salt;
}
//------------------------------------------------------------------------------------------------------------
int tool_hash(int argc, char *argv[], const char *string) {
  int   md5hash[4]; // 16x4x8 = 128b

  md5_buffer(string, strlen(string), md5hash);

  printf("String: \"%s\"\n", string);
  printf("Hash  : \"%x%x%x%x\"\n",
         ntohl(md5hash[0]),
         ntohl(md5hash[1]),
         ntohl(md5hash[2]),
         ntohl(md5hash[3])
        );
  return EX_OK;
}
//------------------------------------------------------------------------------------------------------------
int tool_crypt(int argc, char *argv[], const char *pass) {
  char *salt = NULL;
  int   rc;

  srandom(time(NULL));

  while(1) {
    rc = getopt(argc, argv, "dm");
    if(-1 == rc) // no more options
      break;

    switch(rc) {
    case 'd': // DES
        if(salt) {
          help(argv[0]);
          return EX_USAGE;
        }
        salt = generate_des_salt();
        break;
    case 'm': // MD5
        if(salt) {
          help(argv[0]);
          return EX_USAGE;
        }
        salt = generate_md5_salt();
        break;
    default:
        help(argv[0]);
        return EX_USAGE;
    }

  } // ~while


  if(!salt) { // get user supplied salt or get MD5 random salt.
    if(optind >= argc) {
      salt = generate_md5_salt();
    } else {
      salt = argv[optind];
    }
  }

  printf("Password: \"%s\"\n", pass);
  printf("Salt    : \"%s\"\n", salt);
  printf("Hash    : \"%s\"\n", crypt(pass, salt));
  return EX_OK;
}
//------------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  int   rc;

  // Select utility to "use"
  while(1) {
    rc = getopt(argc, argv, "h:c:");
    if(-1 == rc) // no more options
      break;

    switch(rc) {
    case 'c': // Crypt tool
        return tool_crypt(argc, argv, optarg);
        break;
    case 'h': // MD5 hash tool
        return tool_hash(argc, argv, optarg);
        break;
    }

  } // ~while


  help(argv[0]);
  return EX_USAGE;
}
//------------------------------------------------------------------------------------------------------------
