#ifndef _BITLBEE_H
#define _BITLBEE_H

#include "config.h"

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <glib.h>

#ifndef NO_TCPD
#include <tcpd.h>
#endif

#define _( x ) x

#define VERSION "0.7"

#define ROOT_FN "User manager"
#define DEFAULT_AWAY "Away from computer"
#define CONTROL_TOPIC "Welcome to the control channel. Type help for help information."
#define IRCD_INFO "BitlBee <http://www.lintux.cx/bitlbee.html>"

#define MAX_NICK_LENGTH 12

#include "irc.h"
#include "set.h"
#include "protocols/nogaim.h"
#include "commands.h"

int root_command( irc_t *irc, char *command );
int bitlbee_init( irc_t *irc, char *password );
int save_config( irc_t *irc );

extern irc_t *IRC;

#endif
