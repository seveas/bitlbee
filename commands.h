#ifndef _COMMANDS_H
#define _COMMANDS_H

#include "bitlbee.h"

/* Hmm... Linked list? Plleeeeaaase?? ;-) */

typedef struct command_t
{
	char *command;
	int required_parameters;
	int (*execute)(irc_t *, char **args);
} command_t;

int cmd_help( irc_t *irc, char **args);
int cmd_login( irc_t *irc, char **args);
int cmd_info( irc_t *irc, char **args);
int cmd_logout( irc_t *irc, char **args );
int cmd_slist( irc_t *irc, char **args );
int cmd_add( irc_t *irc, char **args) ;
int cmd_rename( irc_t *irc, char **args );
int cmd_remove( irc_t *irc, char **args );
int cmd_block( irc_t *irc, char **args );
int cmd_allow( irc_t *irc, char **args );
//int cmd_register( irc_t *irc, char **args );
int cmd_save( irc_t *irc, char **args );
int cmd_set( irc_t *irc, char **args );
int cmd_yesno( irc_t *irc, char **args );
int cmd_identify( irc_t *irc, char **args );
int cmd_register( irc_t *irc, char **args );
int cmd_blist( irc_t *irc, char **cmd );

extern command_t commands[];

#endif
