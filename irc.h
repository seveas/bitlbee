  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2003 Wilmer van der Gaast and others                *
  \********************************************************************/

/* The big hairy IRCd part of the project                               */

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

#ifndef _IRC_H
#define _IRC_H

#define IRC_MAX_LINE 512
#define IRC_MAX_ARGS 8

#define UMODES "ais"
#define CMODES "nt"
#define CMODE "nt"
#define UMODE "s"

typedef struct query
{
	char *question;
	void (* yes) ( gpointer w, void *data );
	void (* no) ( gpointer w, void *data );
	void *data;
	void *next;
} query_t;

typedef enum
{
	USTATUS_OFFLINE,
	USTATUS_AUTHORIZED,
	USTATUS_LOGGED_IN,
	USTATUS_IDENTIFIED
} irc_status_t;

typedef struct channel
{
	char *name;
} channel_t;

typedef struct irc
{
	int fd;
	
	char *nick;
	char *user;
	char *host;
	char *realname;
	char *password;

	char umode[8];
	
	char *myhost;
	char *mynick;

	char *channel;
	int c_id;
	
	char private;
	irc_status_t status;
	query_t *queries;
	struct account *accounts;
	
	void *users;
	void *nicks;
	void *help;
	void *set;
} irc_t;

#include "user.h"
#include "nick.h"

irc_t *irc_new( int fd );

int irc_exec( irc_t *irc, char **cmd );
int irc_process( irc_t *irc );
int irc_process_string( irc_t *irc, char *line, int bytes );

int irc_write( irc_t *irc, char *format, ... );
int irc_reply( irc_t *irc, int code, char *format, ... );
int irc_usermsg( irc_t *irc, char *format, ... );

void irc_login( irc_t *irc );
void irc_motd( irc_t *irc );
void irc_names( irc_t *irc, char *channel );
void irc_topic( irc_t *irc, char *channel );
void irc_umode_set( irc_t *irc, char *who, char *s );
void irc_who( irc_t *irc, char *channel );
void irc_spawn( irc_t *irc, user_t *u );
void irc_join( irc_t *irc, user_t *u, char *channel );
void irc_part( irc_t *irc, user_t *u, char *channel );
void irc_kill( irc_t *irc, user_t *u );
void irc_invite( irc_t *irc, char *nick, char *channel );
void irc_whois( irc_t *irc, char *nick );
int irc_away( irc_t *irc, char *away );

int irc_send( irc_t *irc, char *nick, char *s );
int irc_privmsg( irc_t *irc, user_t *u, char *type, char *to, char *prefix, char *msg );
int irc_msgfrom( irc_t *irc, char *nick, char *msg );
int irc_noticefrom( irc_t *irc, char *nick, char *msg );

int buddy_send_handler( irc_t *irc, user_t *u, char *msg );

#endif
