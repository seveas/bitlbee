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

#include "bitlbee.h"
#include "crypting.h"

static char *set_eval_to_char( irc_t *irc, set_t *set, char *value );
static char *set_eval_ops( irc_t *irc, set_t *set, char *value );

irc_t *irc_new( int fd )
{
	irc_t *irc = malloc( sizeof( irc_t ) );
	struct sockaddr_in sock[1];
	struct hostent *peer;
	int i;
	
	memset( irc, 0, sizeof( irc_t ) );
	irc->fd = fd;
	irc->mynick = strdup( ROOT_NICK );
	irc->channel = strdup( ROOT_CHAN );
	strcpy( irc->umode, UMODE );
	
	i = sizeof( *sock );
	if( getsockname( irc->fd, (struct sockaddr*) sock, &i ) == 0 )
		if( ( peer = gethostbyaddr( (char*) &sock->sin_addr, sizeof(sock->sin_addr), AF_INET ) ) )
			irc->myhost = strdup( peer->h_name );
	
	if( getpeername( irc->fd, (struct sockaddr*) sock, &i ) == 0 )
	{
		if( ( peer = gethostbyaddr( (char*) &sock->sin_addr, sizeof(sock->sin_addr), AF_INET ) ) )
			irc->host = strdup( peer->h_name );
#ifndef NO_TCPD
		i = hosts_ctl( "bitlbee", irc->host?irc->host:STRING_UNKNOWN, inet_ntoa( sock->sin_addr ), STRING_UNKNOWN );
	}
	else
	{
		i = 0;
#endif
	}
	
	if( !irc->host ) irc->host = strdup( "localhost." );
	if( !irc->myhost ) irc->myhost = strdup( "localhost." );
	
#ifndef NO_TCPD
	if( !i )
	{
		irc_reply( irc, 463, ":Access denied from your host" );
		if( irc->host ) free( irc->host );
		if( irc->myhost ) free( irc->myhost );
		free( irc );
		return( NULL );
	}
#endif
	
	set_add( irc, "auto_connect", "true", set_eval_bool );
	set_add( irc, "private", "false", set_eval_bool );
#ifdef DEBUG
	set_add( irc, "debug", "true", set_eval_bool );
#else
	set_add( irc, "debug", "false", set_eval_bool );
#endif
	set_add( irc, "to_char", ": ", set_eval_to_char );
	set_add( irc, "ops", "both", set_eval_ops );
	
	irc_write( irc, ":%s NOTICE AUTH :%s", irc->myhost, "BitlBee-IRCd initialized, please go on" );
	
	return( irc );
}

int irc_process( irc_t *irc )
{
	char *line;
	int bytes = 0, size = 256, st;
	struct timeval tv[1];
	fd_set fds[1];
	
	line = malloc( size );
	
	FD_ZERO( fds );
	FD_SET( irc->fd, fds );
	tv->tv_sec = 0;
	tv->tv_usec = 0;
	
	while( select( irc->fd + 1, fds, NULL, NULL, tv ) > 0 )
	{
		st = read( irc->fd, line + bytes, size - bytes );
		if( st <= 0 )
		{
			free( line );
			return( 0 );
		}
		
		bytes += st;
		if( bytes == size )
		{
			size *= 2;
			line = realloc( line, size );
		}
	}
	
	if( bytes > 0 )
		st = irc_process_string( irc, line, bytes );
	else
		st = 1;
	
	free( line );
	return( st );
}

int irc_process_string( irc_t *irc, char *line, int bytes )
{
	char *cmd[IRC_MAX_ARGS+1];
	int i, arg, dosplit;
	
	/* Split it all into separate lines and split the commands as well, then execute them */
	
	memset( cmd, 0, sizeof( cmd ) );
	
	cmd[0] = line;
	arg = dosplit = 1;
	for( i = 0; ( i < bytes ) && ( arg < IRC_MAX_ARGS ); i ++ )
	{
		if( line[i] == ' ' && dosplit )
		{
			line[i] = 0;
			cmd[arg] = line + i + 1;
			if( *cmd[arg] == ':' )
			{
				cmd[arg] ++;
				dosplit = 0;
				arg ++;
			}
			else if( *cmd[arg] && *cmd[arg] != '\r' && *cmd[arg] != '\n' && *cmd[arg] != ' ' )
				arg ++;
		}
		else if( line[i] == '\r' || line[i] == '\n' )
		{
			cmd[arg] = NULL;
			line[i] = 0;
			if( !irc_exec( irc, cmd ) ) return( 0 );
			if( ( ( i + 1 ) < bytes ) && ( line[i+1] == '\n' ) ) i ++;
			memset( cmd, 0, sizeof( cmd ) );
			cmd[0] = line + i + 1;
			arg = dosplit = 1;
		}
	}
	
	return( 1 );
}

int irc_exec( irc_t *irc, char **cmd )
{	
	int i;
	
	if( conf->authmode == CLOSED && irc->status < USTATUS_AUTHORIZED )
	{
		if( strcasecmp( cmd[0], "PASS" ) == 0 )
		{
			if( !cmd[1] )
			{
				irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
			}
			else if( strcmp( cmd[1], conf->password ) == 0 )
			{
				irc->status = USTATUS_AUTHORIZED;
			}
			else
			{
				irc_reply( irc, 464, ":Nope, maybe you should try it again..." );
			}
		}
		else
		{
			irc_reply( irc, 464, ":Uhh, fine, but I want the password first." );
		}
		
		return( 1 );
	}
	
	if( strcasecmp( cmd[0], "USER" ) == 0 )
	{
		if( !cmd[4] )
		{
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
		}
		else if( irc->user )
		{
			irc_reply( irc, 462, ":You can't change your nick/userinfo" );
		}
		else
		{
			irc->user = strdup( cmd[1] );
			irc->realname = strdup( cmd[4] );
			if( irc->nick ) irc_login( irc );
		}
		return( 1 );
	}
	else if( strcasecmp( cmd[0], "NICK" ) == 0 )
	{
		if( !cmd[1] )
		{
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
		}
		else if( irc->nick )
		{
			irc_reply( irc, 438, ":The hand of the deity is upon thee, thy nick may not change" );
		}
		/* This is not clean, but for now it'll have to be like this... */
		else if( ( nick_cmp( cmd[1], irc->mynick ) == 0 ) || ( nick_cmp( cmd[1], NS_NICK ) == 0 ) )
		{
			irc_reply( irc, 433, ":This nick is already in use" );
		}
		else if( !nick_ok( cmd[1] ) )
		{
			/* [SH] Invalid characters. */
			irc_reply( irc, 432, ":This nick contains invalid characters" );
		}
		else
		{
			irc->nick = strdup( cmd[1] );
			if( irc->user ) irc_login( irc );
		}
		return( 1 );
	}
	
	if( !irc->user || !irc->nick ) return( 1 );
	
	if( strcasecmp( cmd[0], "PING" ) == 0 )
	{
		irc_write( irc, ":%s PONG %s :%s", irc->myhost, irc->myhost, cmd[1]?cmd[1]:irc->myhost );
	}
	else if( strcasecmp( cmd[0], "MODE" ) == 0 )
	{
		if( !cmd[1] )
		{
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
		}
		else if( *cmd[1] == '#' )
		{
			if( cmd[2] )
			{
				if( *cmd[2] == '+' || *cmd[2] == '-' )
					irc_reply( irc, 477, "%s :Can't change channel modes", cmd[1] );
				else if( *cmd[2] == 'b' )
					irc_reply( irc, 368, "%s :No bans possible", cmd[1] );
			}
			else
				irc_reply( irc, 324, "%s +%s", cmd[1], CMODE );
		}
		else
		{
			if( nick_cmp( cmd[1], irc->nick ) == 0 )
			{
				if( cmd[2] )
					irc_umode_set( irc, irc->nick, cmd[2] );
				
				irc_reply( irc, 221, "+%s", irc->umode );
			}
			else
				irc_reply( irc, 502, ":Don't touch their modes" );
		}
	}
	else if( strcasecmp( cmd[0], "NAMES" ) == 0 )
	{
		irc_names( irc, cmd[1]?cmd[1]:irc->channel );
	}
	else if( strcasecmp( cmd[0], "PART" ) == 0 )
	{
		struct conversation *c;
		
		if( !cmd[1] )
		{
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
		}
		else if( strcasecmp( cmd[1], irc->channel ) == 0 )
		{
			user_t *u = user_find( irc, irc->nick );
			
			/* Not allowed to leave control channel */
			irc_part( irc, u, irc->channel );
			irc_join( irc, u, irc->channel );
		}
		else if( ( c = conv_findchannel( cmd[1] ) ) )
		{
			user_t *u = user_find( irc, irc->nick );
			
			irc_part( irc, u, c->channel );
			
			if( c->gc && c->gc->prpl )
			{
				c->joined = 0;
				c->gc->prpl->chat_leave( c->gc, c->id );
			}
		}
		else
		{
			irc_reply( irc, 403, "%s :No such channel", cmd[1] );
		}
	}
	else if( strcasecmp( cmd[0], "JOIN" ) == 0 )
	{
		if( strcasecmp( cmd[1], irc->channel ) == 0 )
			; /* Dude, you're already there...
			     RFC doesn't have any reply for that though? */
		else if( cmd[1] )
		{
			if( cmd[1][0] == '#' && cmd[1][1] )
			{
				user_t *u = user_find( irc, cmd[1] + 1 );
				
				if( u && u->gc && u->gc->prpl && u->gc->prpl->convo_open )
				{
					irc_reply( irc, 403, "%s :Initializing groupchat in a different channel", cmd[1] );
					
					if( !u->gc->prpl->convo_open( u->gc, u->handle ) )
					{
						irc_usermsg( irc, "Could not open a groupchat with %s, maybe you don't have a connection to him/her yet?", u->nick );
					}
				}
				else
				{
					irc_reply( irc, 403, "%s :Groupchats are not possible with %s", cmd[1], cmd[1]+1 );
				}
			}
			else
			{
				irc_reply( irc, 403, "%s :No such channel", cmd[1] );
			}
		}
		else
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
	}
	else if( strcasecmp( cmd[0], "INVITE" ) == 0 )
	{
		if( cmd[1] && cmd[2] )
			irc_invite( irc, cmd[1], cmd[2] );
		else
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
	}
	else if( strcasecmp( cmd[0], "PRIVMSG" ) == 0 )
	{
		if( !cmd[2] )
		{
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
		}
		else
		{
			if( strcasecmp( cmd[1], irc->channel ) == 0 )
			{
				unsigned int i;
				
				cmd[1] = irc->mynick;
				for( i = 0; i < strlen( cmd[2] ); i ++ )
				{
					if( cmd[2][i] == ' ' ) break;
					if( cmd[2][i] == ':' || cmd[2][i] == ',' )
					{
						cmd[1] = cmd[2];
						cmd[2] += i;
						*cmd[2] = 0;
						while( *(++cmd[2]) == ' ' );
						break;
					}
				}
				irc->private = 0;
			}
			else
			{
				irc->private = 1;
			}
			irc_send( irc, cmd[1], cmd[2] );
		}
	}
	else if( strcasecmp( cmd[0], "QUIT" ) == 0 )
	{
		irc_write( irc, "ERROR :%s%s", cmd[1]?"Quit: ":"", cmd[1]?cmd[1]:"Client Quit" );
		usleep( 100000 );	/* Give the message a bit time */
		close( irc->fd );
		return( 0 );
	}
	else if( strcasecmp( cmd[0], "WHO" ) == 0 )
	{
		irc_who( irc, cmd[1] );
	}
	else if( strcasecmp( cmd[0], "USERHOST" ) == 0 )
	{
		/* [TV] Usable USERHOST-implementation according to
			RFC1459. Without this, mIRC shows an error
			while connecting, and the used way of rejecting
			breaks standards.
		*/
		user_t *u;
		
		for( i = 1; i < IRC_MAX_ARGS && cmd[i]; i ++ )
			if( ( u = user_find( irc, cmd[i] ) ) )
			{
				if( u->online && u->away )
					irc_reply( irc, 302, ":%s=-%s@%s", u->nick, u->user, u->host );
				else
					irc_reply( irc, 302, ":%s=+%s@%s", u->nick, u->user, u->host );
			}
	}
	else if( strcasecmp( cmd[0], "ISON" ) == 0 )
	{
		user_t *u;
		
		for( i = 1; i < IRC_MAX_ARGS && cmd[i]; i ++ )
			if( ( u = user_find( irc, cmd[i] ) ) && u->online )
				irc_reply( irc, 303, ":%s", u->nick );
	}
	else if( strcasecmp( cmd[0], "TOPIC" ) == 0 )
	{
		if( cmd[2] )
			irc_reply( irc, 482, "%s :Cannot change topic", cmd[1] );
		else if( cmd[1] )
			irc_topic( irc, cmd[1] );
		else
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
	}
	else if( strcasecmp( cmd[0], "AWAY" ) == 0 )
	{
		irc_away( irc, cmd[1] );
	}
	else if( strcasecmp( cmd[0], "WHOIS" ) == 0 )
	{
		if( cmd[1] )
		{
			irc_whois( irc, cmd[1] );
		}
		else
		{
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
		}
	}
	else if( strcasecmp( cmd[0], "WHOWAS" ) == 0 )
	{
		/* For some reason irssi tries a whowas when whois fails. We can
		   ignore this, but then the user never gets a "user not found"
		   message from irssi which is a bit annoying. So just respond
		   with not-found and irssi users will get better error messages */
		
		if( cmd[1] )
		{
			irc_reply( irc, 406, "%s :Nick does not exist", cmd[1] );
			irc_reply( irc, 369, "%s :End of WHOWAS", cmd[1] );
		}
		else
		{
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
		}
	}
	else if( ( strcasecmp( cmd[0], "NICKSERV" ) == 0 ) || ( strcasecmp( cmd[0], "NS" ) == 0 ) )
	{
		/* [SH] This aliases the NickServ command to PRIVMSG root */
		/* [TV] This aliases the NS command to PRIVMSG root as well */
		root_command( irc, cmd + 1 );
	}
	else if( strcasecmp( cmd[0], "MOTD" ) == 0 )
	{
		irc_motd( irc );
	}
	else if( set_getint( irc, "debug" ) )
	{
		irc_usermsg( irc, "\002--- Unknown command:" );
		for( i = 0; i < IRC_MAX_ARGS && cmd[i]; i ++ ) irc_usermsg( irc, "%s", cmd[i] );
		irc_usermsg( irc, "\002--------------------" );
	}
	
	return( 1 );
}

int irc_reply( irc_t *irc, int code, char *format, ... )
{
	char text[IRC_MAX_LINE];
	va_list params;
	
	va_start( params, format );
	vsnprintf( text, IRC_MAX_LINE, format, params );
	va_end( params );
	return( irc_write( irc, ":%s %03d %s %s", irc->myhost, code, irc->nick?irc->nick:"*", text ) );
}

int irc_usermsg( irc_t *irc, char *format, ... )
{
	char text[1024];
	va_list params;
	char private = 0;
	user_t *u;
	
	u = user_find( irc, irc->mynick );
	if( u ) private = u->private;
	
	va_start( params, format );
	vsnprintf( text, 1023, format, params );
	va_end( params );
	
	return( irc_msgfrom( irc, u->nick, text ) );
}

int irc_write( irc_t *irc, char *format, ... )
{
	char line[IRC_MAX_LINE];
	va_list params;
	int n, start = 0;
	
	va_start( params, format );
	vsnprintf( line, IRC_MAX_LINE - 3, format, params );
	va_end( params );
	strcat( line, "\r\n" );
	
	while( line[start] )
	{
		n = write( irc->fd, line + start, strlen( line + start ) );
		if( n <= 0 )
		{
			/* PANIC! But we can't tell the user through IRC,
			   that'd probably cause an infinite loop.. */
			return( 0 );
		}
		start += n;
	}
	
	return( 1 );
}

void irc_names( irc_t *irc, char *channel )
{
	user_t *u = irc->users;
	char *s;
	int control = ( strcasecmp( channel, irc->channel ) == 0 );
	struct conversation *c = NULL;
	
	if( !control )
		c = conv_findchannel( channel );
	
	/* RFC's say there is no error reply allowed on NAMES, so when the
	   channel is invalid, just give an empty reply. */
	
	if( control || c ) while( u )
	{
		if( u->online )
		{
			if( u->gc && control )
			{
				if( set_getint( irc, "away_devoice" ) && !u->away )
					s = "+";
				else
					s = "";
				
				irc_reply( irc, 353, "@ %s :%s%s", channel, s, u->nick );
			}
			else if( !u->gc )
			{
				if( strcmp( u->nick, irc->mynick ) == 0 && ( strcmp( set_getstr( irc, "ops" ), "root" ) == 0 || strcmp( set_getstr( irc, "ops" ), "both" ) == 0 ) )
					s = "@";
				else if( strcmp( u->nick, irc->nick ) == 0 && ( strcmp( set_getstr( irc, "ops" ), "user" ) == 0 || strcmp( set_getstr( irc, "ops" ), "both" ) == 0 ) )
					s = "@";
				else
					s = "";
				
				irc_reply( irc, 353, "@ %s :%s%s", channel, s, u->nick );
			}
		}
		
		u = u->next;
	}
	
	/* For non-controlchannel channels (group conversations) only root and
	   you are listed now. Time to show the channel people: */
	if( !control && c )
	{
		GList *l;
		
		for( l = c->in_room; l; l = l->next )
			if( ( u = user_findhandle( c->gc, l->data ) ) )
				irc_reply( irc, 353, "@ %s :%s%s", channel, "", u->nick );
	}
	
	irc_reply( irc, 366, "%s :End of /NAMES list", channel );
}

void irc_who( irc_t *irc, char *channel )
{
	user_t *u = irc->users;
	struct conversation *c;
	GList *l;
	
	if( !channel || *channel == '0' || *channel == '*' || !*channel )
		while( u )
		{
			irc_reply( irc, 352, "%s %s %s %s %s %c :0 %s", u->online ? irc->channel : "*", u->user, u->host, irc->myhost, u->nick, u->online ? ( u->away ? 'G' : 'H' ) : 'G', u->realname );
			u = u->next;
		}
	else if( strcasecmp( channel, irc->channel ) == 0 )
		while( u )
		{
			if( u->online )
				irc_reply( irc, 352, "%s %s %s %s %s %c :0 %s", channel, u->user, u->host, irc->myhost, u->nick, u->away ? 'G' : 'H', u->realname );
			u = u->next;
		}
	else if( ( c = conv_findchannel( channel ) ) )
		for( l = c->in_room; l; l = l->next )
		{
			if( ( u = user_findhandle( c->gc, l->data ) ) )
				irc_reply( irc, 352, "%s %s %s %s %s %c :0 %s", channel, u->user, u->host, irc->myhost, u->nick, u->away ? 'G' : 'H', u->realname );
		}
	else if( ( u = user_find( irc, channel ) ) )
		irc_reply( irc, 352, "%s %s %s %s %s %c :0 %s", channel, u->user, u->host, irc->myhost, u->nick, u->online ? ( u->away ? 'G' : 'H' ) : 'G', u->realname );
	
	irc_reply( irc, 315, "%s :End of /WHO list.", channel );
}

void irc_login( irc_t *irc )
{
	user_t *u;
	
	irc_reply( irc,   1, ":Welcome to the BitlBee gateway, %s", irc->nick );
	irc_reply( irc,   2, ":Host %s is running BitlBee " BITLBEE_VERSION " " ARCH "/" CPU ".", irc->myhost );
	irc_reply( irc,   3, ":%s", IRCD_INFO );
	irc_reply( irc,   4, "%s %s %s %s", irc->myhost, BITLBEE_VERSION, UMODES, CMODES );
	irc_motd( irc );
	irc_umode_set( irc, irc->myhost, "+" UMODE );

	u = user_add( irc, irc->mynick );
	u->host = irc->myhost;
	u->realname = ROOT_FN;
	u->online = 1;
	u->send_handler = root_command_string;
	irc_spawn( irc, u );
	
	u = user_add( irc, NS_NICK );
	u->host = irc->myhost;
	u->realname = ROOT_FN;
	u->online = 0;
	u->send_handler = root_command_string;
	
	u = user_add( irc, irc->nick );
	u->user = irc->user;
	u->host = irc->host;
	u->realname = irc->realname;
	u->online = 1;
//	u->send_handler = msg_echo;
	irc_spawn( irc, u );
	
	bitlbee_init( irc );
	
	irc_usermsg( irc, "Welcome to the BitlBee gateway!\n\nIf you've never used BitlBee before, please do read the help information using the help command. Lots of FAQ's are answered there." );
	
	irc->status = USTATUS_LOGGED_IN;
}

void irc_motd( irc_t *irc )
{
	int fd;
	
	fd = open( MOTD_FILE, O_RDONLY );
	if( fd == -1 )
	{
		irc_reply( irc, 422, ":We don't need MOTDs." );
	}
	else
	{
		char linebuf[80];	/* Max. line length for MOTD's is 79 chars. It's what most IRC networks seem to do. */
		char *add, max;
		int len;
		
		linebuf[79] = len = 0;
		max = sizeof( linebuf ) - 1;
		
		irc_reply( irc, 375, ":- %s Message Of The Day - ", irc->myhost );
		while( read( fd, linebuf + len, 1 ) == 1 )
		{
			if( linebuf[len] == '\n' || len == max )
			{
				linebuf[len] = 0;
				irc_reply( irc, 372, ":- %s", linebuf );
				len = 0;
			}
			else if( linebuf[len] == '%' )
			{
				read( fd, linebuf + len, 1 );
				if( linebuf[len] == 'h' )
					add = irc->myhost;
				else if( linebuf[len] == 'v' )
					add = BITLBEE_VERSION;
				else if( linebuf[len] == 'n' )
					add = irc->nick;
				else
					add = "%";
				
				strncpy( linebuf + len, add, max - len );
				while( linebuf[++len] );
			}
			else if( len < max )
			{
				len ++;
			}
		}
		irc_reply( irc, 376, ":End of MOTD" );
		close( fd );
	}
}

void irc_topic( irc_t *irc, char *channel )
{
	if( strcasecmp( channel, irc->channel ) == 0 )
	{
		irc_reply( irc, 332, "%s :%s", channel, CONTROL_TOPIC );
	}
	else
	{
		struct conversation *c = conv_findchannel( channel );
		
		if( c )
			irc_reply( irc, 332, "%s :BitlBee groupchat: \"%s\". Please keep in mind that root-commands won't work here. Have fun!", channel, c->title );
		else
			irc_reply( irc, 331, "%s :No topic for this channel" );
	}
}

void irc_whois( irc_t *irc, char *nick )
{
	user_t *u = user_find( irc, nick );
	
	if( u )
	{
		irc_reply( irc, 311, "%s %s %s * :%s", u->nick, u->user, u->host, u->realname );
		
		if( u->gc )
			irc_reply( irc, 312, "%s %s.%s :%s network", u->nick, u->gc->user->username,
			           *u->gc->user->proto_opt[0] ? u->gc->user->proto_opt[0] : "", proto_name[u->gc->user->protocol] );
		else
			irc_reply( irc, 312, "%s %s :%s", u->nick, irc->myhost, IRCD_INFO );
		
		if( !u->online )
			irc_reply( irc, 301, "%s :%s", u->nick, "User is offline" );
		else if( u->away )
			irc_reply( irc, 301, "%s :%s", u->nick, u->away );
		
		irc_reply( irc, 318, "%s :End of /WHOIS list", nick );
	}
	else
	{
		irc_reply( irc, 401, "%s :Nick does not exist", nick );
	}
}

void irc_umode_set( irc_t *irc, char *who, char *s )
{
	char m[256], st = 1, *t;
	int i;
	
	memset( m, 0, sizeof( m ) );
	
	for( t = irc->umode; *t; t ++ )
		m[(int)*t] = 1;
	
	for( t = s; *t; t ++ )
	{
		if( *t == '+' || *t == '-' )
			st = *t == '+';
		else
			m[(int)*t] = st;
	}
	
	memset( irc->umode, 0, sizeof( irc->umode ) );
	
	for( i = 0; i < 256 && strlen( irc->umode ) < ( sizeof( irc->umode ) - 1 ); i ++ )
		if( m[i] && strchr( UMODES, i ) )
			irc->umode[strlen(irc->umode)] = i;
}

int irc_away( irc_t *irc, char *away )
{
	user_t *u = user_find( irc, irc->nick );
	GSList *c = connections;
	
	if( !u ) return( 0 );

	if( away && *away )
	{
		irc_reply( irc, 306, ":You're now away: %s", away );
		irc_umode_set( irc, irc->myhost, "+a" );
		u->away = strdup( away );
	}
	else
	{
		if( u->away ) free( u->away );
		u->away = NULL;
		irc_umode_set( irc, irc->myhost, "-a" );
		irc_reply( irc, 305, ":Welcome back" );
	}
	
	while( c )
	{
		if( ((struct gaim_connection *)c->data)->flags & OPT_LOGGED_IN )
			proto_away( c->data, away );
		
		c = c->next;
	}
	
	return( 1 );
}

void irc_spawn( irc_t *irc, user_t *u )
{
	irc_join( irc, u, irc->channel );
}

void irc_join( irc_t *irc, user_t *u, char *channel )
{
	if( ( strcasecmp( channel, irc->channel ) != 0 ) || user_find( irc, irc->nick ) )
		irc_write( irc, ":%s!%s@%s JOIN :%s", u->nick, u->user, u->host, channel );
	
	if( nick_cmp( u->nick, irc->nick ) == 0 )
	{
		irc_write( irc, ":%s MODE %s +%s", irc->myhost, channel, CMODE );
		irc_names( irc, channel );
		irc_topic( irc, channel );
	}
}

void irc_part( irc_t *irc, user_t *u, char *channel )
{
	irc_write( irc, ":%s!%s@%s PART %s :%s", u->nick, u->user, u->host, channel, "" );
}

void irc_kick( irc_t *irc, user_t *u, char *channel, user_t *kicker )
{
	irc_write( irc, ":%s!%s@%s KICK %s %s :%s", kicker->nick, kicker->user, kicker->host, channel, u->nick, "" );
}

void irc_kill( irc_t *irc, user_t *u )
{
	irc_write( irc, ":%s!%s@%s QUIT :%s", u->nick, u->user, u->host, "Leaving..." );
}

void irc_invite( irc_t *irc, char *nick, char *channel )
{
	struct conversation *c = conv_findchannel( channel );
	user_t *u = user_find( irc, nick );
	
	if( u && c && ( u->gc == c->gc ) )
		if( c->gc && c->gc->prpl && c->gc->prpl->chat_invite )
		{
			c->gc->prpl->chat_invite( c->gc, c->id, "", u->handle );
			irc_reply( irc, 341, "%s %s", nick, channel );
			return;
		}
	
	irc_reply( irc, 482, "%s :Invite impossible; User/Channel non-existent or incompatible", channel );
}

int irc_send( irc_t *irc, char *nick, char *s )
{
	struct conversation *c = NULL;
	user_t *u = NULL;
	
	if( *nick == '#' )
	{
		if( !( c = conv_findchannel( nick ) ) )
		{
			irc_reply( irc, 403, "%s :Channel does not exist", nick );
			return( 0 );
		}
	}
	else
	{
		u = user_find( irc, nick );
		
		if( !u )
		{
			if( irc->private )
				irc_reply( irc, 401, "%s :Nick does not exist", nick );
			else
				irc_usermsg( irc, "Nick '%s' does not exist!", nick );
			return( 0 );
		}
		
		u->private = irc->private;
		
		if( u->away )
			irc_reply( irc, 301, "%s :%s", u->nick, u->away );
	}
	
	if( *s == 1 && s[strlen(s)-1] == 1 )
	{
		if( strncasecmp( s + 1, "ACTION", 6 ) == 0 )
		{
			if( s[7] == ' ' ) s ++;
			s += 3;
			*(s++) = '/';
			*(s++) = 'm';
			*(s++) = 'e';
			*(s++) = ' ';
			s -= 4;
			s[strlen(s)-1] = 0;
		}
		else if( strncasecmp( s + 1, "VERSION", 7 ) == 0 )
		{
			irc_privmsg( irc, u, "NOTICE", irc->nick, "", "\001VERSION BitlBee " BITLBEE_VERSION " " ARCH "/" CPU "\001" );
			return( 0 );
		}
		else
		{
			irc_usermsg( irc, "Non-ACTION CTCP's aren't supported" );
			return( 0 );
		}
	}
	
	if( u && u->send_handler )
		return( u->send_handler( irc, u, s ) );
	else if( c && c->gc && c->gc->prpl )
		return( serv_send_chat( irc, c->gc, c->id, s ) );
	
	return( 1 );
}

int buddy_send_handler( irc_t *irc, user_t *u, char *msg )
{
	if( !u || !u->gc ) return( 0 );
	return( serv_send_im( irc, u, msg ) );
}

int irc_privmsg( irc_t *irc, user_t *u, char *type, char *to, char *prefix, char *msg )
{
	char last = 0;
	char *s = msg, *line = msg;
	
	/* The almighty linesplitter .. woohoo!! */
	while( !last )
	{
		if( *s == '\r' && *(s+1) == '\n' )
			*(s++) = 0;
		if( *s == '\n' )
		{
			last = s[1] == 0;
			*s = 0;
		}
		else
		{
			last = s[0] == 0;
		}
		if( *s == 0 )
		{
			if( strncasecmp( line, "/me ", 4 ) == 0 && ( !prefix || !*prefix ) && strcasecmp( type, "PRIVMSG" ) == 0 )
			{
				irc_write( irc, ":%s!%s@%s %s %s :\001ACTION %s\001", u->nick, u->user, u->host,
				           type, to, line + 4 );
			}
			else
			{
				irc_write( irc, ":%s!%s@%s %s %s :%s%s", u->nick, u->user, u->host,
				           type, to, prefix, line );
			}
			line = s + 1;
		}
		s ++;
	}
	
	return( 1 );
}

int irc_msgfrom( irc_t *irc, char *nick, char *msg )
{
	user_t *u = user_find( irc, nick );
	static char *prefix = NULL;
	
	if( !u ) return( 0 );
	if( prefix && *prefix ) free( prefix );
	
	if( !u->private && nick_cmp( u->nick, irc->mynick ) != 0 )
	{
		int len = strlen( irc->nick) + 3;
		prefix = malloc( len );
		snprintf( prefix, len, "%s%s", irc->nick, set_getstr( irc, "to_char" ) );
		prefix[len-1] = 0;
	}
	else
	{
		prefix = "";
	}
	
	return( irc_privmsg( irc, u, "PRIVMSG", u->private ? irc->nick : irc->channel, prefix, msg ) );
}

int irc_noticefrom( irc_t *irc, char *nick, char *msg )
{
	user_t *u = user_find( irc, nick );
	
	if( u )
		return( irc_privmsg( irc, u, "NOTICE", irc->nick, "", msg ) );
	else
		return( 0 );
}

static char *set_eval_to_char( irc_t *irc, set_t *set, char *value )
{
	char *s = malloc( 3 );
	
	if( *value == ' ' )
		strcpy( s, " " );
	else
		sprintf( s, "%c ", *value );
	
	return( s );
}

static char *set_eval_ops( irc_t *irc, set_t *set, char *value )
{
	if( strcasecmp( value, "user" ) == 0 )
	{
		irc_write( irc, ":%s!%s@%s MODE %s %s %s %s", irc->mynick, irc->mynick, irc->myhost,
		                                              irc->channel, "+o-o", irc->nick, irc->mynick );
		return( value );
	}
	else if( strcasecmp( value, "root" ) == 0 )
	{
		irc_write( irc, ":%s!%s@%s MODE %s %s %s %s", irc->mynick, irc->mynick, irc->myhost,
		                                              irc->channel, "-o+o", irc->nick, irc->mynick );
		return( value );
	}
	else if( strcasecmp( value, "both" ) == 0 )
	{
		irc_write( irc, ":%s!%s@%s MODE %s %s %s %s", irc->mynick, irc->mynick, irc->myhost,
		                                              irc->channel, "+oo", irc->nick, irc->mynick );
		return( value );
	}
	else if( strcasecmp( value, "none" ) == 0 )
	{
		irc_write( irc, ":%s!%s@%s MODE %s %s %s %s", irc->mynick, irc->mynick, irc->myhost,
		                                              irc->channel, "-oo", irc->nick, irc->mynick );
		return( value );
	}
	
	return( NULL );
}
