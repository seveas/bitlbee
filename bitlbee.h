  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2004 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Main file                                                            */

/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License with
  the Debian GNU/Linux distribution in /usr/share/common-licenses/GPL;
  if not, write to the Free Software Foundation, Inc., 59 Temple Place,
  Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _BITLBEE_H
#define _BITLBEE_H

#define _GNU_SOURCE /* Stupid GNU :-P */

#define PACKAGE "BitlBee"
#define BITLBEE_VERSION "0.90"
#define VERSION BITLBEE_VERSION

#define MAX_STRING 128

#include "config.h"

#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#ifndef _WIN32
#include <syslog.h>
#include <errno.h>

#ifndef NO_TCPD
#include <tcpd.h>
#endif
#endif

#include <glib.h>
#include <gmodule.h>

/* The following functions should not be used if we want to maintain Windows compatibility... */
#undef free
#define free		__PLEASE_USE_THE_GLIB_MEMORY_ALLOCATION_SYSTEM_INSTEAD__
#undef malloc
#define malloc		__PLEASE_USE_THE_GLIB_MEMORY_ALLOCATION_SYSTEM_INSTEAD__
#undef calloc
#define calloc		__PLEASE_USE_THE_GLIB_MEMORY_ALLOCATION_SYSTEM_INSTEAD__
#undef realloc
#define realloc		__PLEASE_USE_THE_GLIB_MEMORY_ALLOCATION_SYSTEM_INSTEAD__
#undef strdup
#define strdup		__PLEASE_USE_THE_GLIB_STRDUP_FUNCTIONS_SYSTEM_INSTEAD__
#undef strndup
#define strndup		__PLEASE_USE_THE_GLIB_STRDUP_FUNCTIONS_SYSTEM_INSTEAD__
#undef snprintf
#define snprintf	__PLEASE_USE_G_SNPRINTF_INSTEAD__
#undef strcasecmp
#define strcasecmp	__PLEASE_USE_G_STRCASECMP_INSTEAD__
#undef strncasecmp
#define strncasecmp	__PLEASE_USE_G_STRNCASECMP_INSTEAD__

#define _( x ) x

#define ROOT_NICK "root"
#define ROOT_CHAN "#bitlbee"
#define ROOT_FN "User manager"

#define NS_NICK "NickServ"

#define DEFAULT_AWAY "Away from computer"
#define CONTROL_TOPIC "Welcome to the control channel. Type help for help information."
#define IRCD_INFO "BitlBee <http://www.bitlbee.org/>"

#define MAX_NICK_LENGTH 24

#define HELP_FILE DATADIR "help.txt"
#define CONF_FILE_DEF ETCDIR "bitlbee.conf"

extern char *CONF_FILE;

#include "irc.h"
#include "set.h"
#include "protocols/nogaim.h"
#include "commands.h"
#include "account.h"
#include "conf.h"
#include "log.h"
#include "ini.h"
#include "help.h"
#include "query.h"

#include "sock.h"

typedef struct global_t {
	fd_set readfds[1];
	fd_set writefds[1];
	int listen_socket;
	help_t *help;
	conf_t *conf;
	char *helpfile;
} global_t;

int bitlbee_daemon_main_loop( void );
int bitlbee_inetd_main_loop( void );
int bitlbee_daemon_init( void );
int bitlbee_inetd_init( void );

int bitlbee_connection_create( int fd ); 
GList *bitlbee_connection_destroy( GList *connection );

int root_command_string( irc_t *irc, user_t *u, char *command );
int root_command( irc_t *irc, char *command[] );
int bitlbee_load( irc_t *irc, char *password );
int bitlbee_save( irc_t *irc );
double gettime( void );
G_MODULE_EXPORT void http_encode( char *s );
G_MODULE_EXPORT void http_decode( char *s );

void *bitlbee_alloc(size_t size);
void *bitlbee_realloc(void *oldmem, size_t newsize);

G_MODULE_EXPORT irc_t *get_IRC();

extern irc_t *IRC;
extern global_t global;
extern GList *connection_list;

#endif
