#ifndef _BITLBEE_H
#define _BITLBEE_H

#define BITLBEE_VERSION "0.72"

#include "config.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <glib.h>

#ifndef NO_TCPD
#include <tcpd.h>
#endif

#define _( x ) x

#define ROOT_NICK "root"
#define ROOT_FN "User manager"
#define DEFAULT_AWAY "Away from computer"
#define CONTROL_TOPIC "Welcome to the control channel. Type help for help information."
#define IRCD_INFO "BitlBee <http://www.lintux.cx/bitlbee.html>"

#define MAX_NICK_LENGTH 12

#define HELP_FILE DATADIR "help.txt"

#include "irc.h"
#include "set.h"
#include "protocols/nogaim.h"
#include "commands.h"

int root_command_string( irc_t *irc, user_t *u, char *command );
int root_command( irc_t *irc, char *command[] );
int bitlbee_init( irc_t *irc );
int bitlbee_load( irc_t *irc, char *password );	/* TODO Get rid of password arg? */
int bitlbee_save( irc_t *irc );
void http_encode( char *s );
void http_decode( char *s );

extern irc_t *IRC;

#endif
