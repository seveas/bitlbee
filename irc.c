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
	irc->channel = strdup( "#bitlbee" );
	strcpy( irc->umode, UMODE );
	
	i = sizeof( *sock );
	if( getsockname( irc->fd, (struct sockaddr*) sock, &i ) == 0 )
		if( ( peer = gethostbyaddr( (char*) &sock->sin_addr, i, AF_INET ) ) )
			irc->myhost = strdup( peer->h_name );
	
	if( getpeername( irc->fd, (struct sockaddr*) sock, &i ) == 0 )
	{
		if( ( peer = gethostbyaddr( (char*) &sock->sin_addr, i, AF_INET ) ) )
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
	
	set_add( irc, "private", "false", set_eval_bool );
#ifdef DEBUG
	set_add( irc, "debug", "true", set_eval_bool );
#else
	set_add( irc, "debug", "false", set_eval_bool );
#endif
	set_add( irc, "to_char", ": ", set_eval_to_char );
	set_add( irc, "ops", "both", set_eval_ops );
	
	return( irc );
}

int irc_process( irc_t *irc )
{
	char line[IRC_MAX_LINE];
	int bytes;
	
	if( ( bytes = read( irc->fd, line, IRC_MAX_LINE ) ) <= 0 )
	{
		return( 0 );
	}
	
	return( irc_process_string( irc, line, bytes ) );
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
			if( line[i+1] == '\n' ) i ++;
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
	
	if( strcasecmp( cmd[0], "USER" ) == 0 )
	{
		if( !cmd[4] )
		{
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
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
			irc_reply( irc, 432, "You can't change your nick" );
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
			if( strcasecmp( cmd[1], irc->nick ) == 0 )
			{
				if( cmd[2] )
					irc_umode_set( irc, irc->nick, cmd[2] );
				
				irc_reply( irc, 221, "+%s", irc->umode );
			}
			else
				irc_reply( irc, 502, "Don't touch their modes" );
		}
	}
	else if( strcasecmp( cmd[0], "NAMES" ) == 0 )
	{
		irc_names( irc, cmd[1]?cmd[1]:irc->channel );
	}
	else if( strcasecmp( cmd[0], "PART" ) == 0 )
	{
		if( !cmd[1] )
		{
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
		}
		else if( strcasecmp( cmd[1], irc->channel ) == 0 )
		{
			/* Not allowed to leave control channel */
			irc_write( irc, ":%s!%s@%s PART %s :%s", irc->nick, irc->user, irc->host, irc->channel, cmd[2]?cmd[2]:"" );
			irc_spawn( irc, user_find( irc, irc->nick ) );
		}
		else
		{
			irc_reply( irc, 403, "%s :No such channel", cmd[1] );
		}
	}
/*	else if( strcasecmp( cmd[0], "JOIN" ) == 0 )
	{
		if( cmd[1] )
			irc_reply( irc, 403, "%s :No such channel", cmd[1] );
		else
			irc_reply( irc, 461, "%s :Need more parameters", cmd[0] );
	} */
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
		irc_write( irc, ":%s!%s@%s QUIT :%s", irc->nick, irc->user, irc->host, cmd[1]?cmd[1]:"" );
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
	else if( ( strcasecmp( cmd[0], "NICKSERV" ) == 0 ) || ( strcasecmp( cmd[0], "NS" ) == 0 ) )
	{
		/* [SH] This aliases the NickServ command to PRIVMSG root */
		/* [TV] This aliases the NS command to PRIVMSG root as well */
		root_command( irc, cmd + 1 );
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
	
	va_start( params, format );
	vsnprintf( line, IRC_MAX_LINE - 3, format, params );
	va_end( params );
	strcat( line, "\r\n" );
	return( write( irc->fd, line, strlen( line ) ) == strlen( line ) );
}

void irc_names( irc_t *irc, char *channel )
{
	user_t *u;
	char *s;
	
	u = irc->users;
	while( u )
	{
		if( u->online )
		{
			if( u->gc )
			{
				irc_reply( irc, 353, "@ %s :%s%s", channel, "", u->nick );
			}
			else
			{
				s = "";
				if( strcmp( u->nick, irc->mynick ) == 0 && ( strcmp( set_getstr( irc, "ops" ), "root" ) == 0 || strcmp( set_getstr( irc, "ops" ), "both" ) == 0 ) )
					s = "@";
				else if( strcmp( u->nick, irc->nick ) == 0 && ( strcmp( set_getstr( irc, "ops" ), "user" ) == 0 || strcmp( set_getstr( irc, "ops" ), "both" ) == 0 ) )
					s = "@";
				irc_reply( irc, 353, "@ %s :%s%s", channel, s, u->nick );
			}
		}
		u = u->next;
	}
	irc_reply( irc, 366, "%s :End of /NAMES list", channel );
}

void irc_who( irc_t *irc, char *channel )
{
	user_t *u = irc->users;
	
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
	irc_reply( irc, 422, ":We don't need MOTDs." );
	irc_umode_set( irc, irc->myhost, "+" UMODE );

	u = user_add( irc, irc->mynick );
	u->host = irc->myhost;
	u->realname = ROOT_FN;
	u->online = 1;
	u->send_handler = root_command_string;
	irc_spawn( irc, user_find( irc, irc->mynick ) );
	
	u = user_add( irc, "NickServ" );
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
	irc_spawn( irc, user_find( irc, irc->nick ) );
	
	bitlbee_init( irc );
	
	irc_usermsg( irc, "Welcome to the BitlBee gateway!\n\nIf you've never used BitlBee before, please do read the help information using the help command. Lots of FAQ's are answered there." );
	
	irc->status = USTATUS_LOGGED_IN;
}

void irc_topic( irc_t *irc, char *channel )
{
	if( strcasecmp( channel, irc->channel ) == 0 )
		irc_reply( irc, 332, "%s :%s", irc->channel, CONTROL_TOPIC );
	else
		irc_reply( irc, 331, "%s :No topic", irc->channel );
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
		/* irc_umode_set( irc, irc->myhost, "+a" ); */
		u->away = strdup( away );
	}
	else
	{
		if( u->away ) free( u->away );
		u->away = NULL;
		/* irc_umode_set( irc, irc->myhost, "-a" ); */
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
	if( user_find( irc, irc->nick ) )
		irc_write( irc, ":%s!%s@%s JOIN :%s", u->nick, u->user, u->host, irc->channel );
	if( strcasecmp( u->nick, irc->nick ) == 0 )
	{
		irc_write( irc, ":%s MODE %s +%s", irc->myhost, irc->channel, CMODE );
		irc_names( irc, irc->channel );
		irc_topic( irc, irc->channel );
	}
}

void irc_kill( irc_t *irc, user_t *u )
{
	irc_write( irc, ":%s!%s@%s QUIT :%s", u->nick, u->user, u->host, "Leaving..." );
}

int irc_send( irc_t *irc, char *nick, char *s )
{
	user_t *u = user_find( irc, nick );
	
	if( !u )
	{
		if( irc->private )
			irc_reply( irc, 401, "%s :Nick does not exist", nick );
		else
			irc_usermsg( irc, "Nick '%s' does not exist!", nick );
		return( 0 );
	}
	
	if( *s == 1 && s[strlen(s)-1] == 1 )
	{
		if( strncasecmp( s+1, "ACTION", 6 ) == 0 )
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
		else
		{
			irc_usermsg( irc, "Non-ACTION CTCP's aren't supported" );
			return( 0 );
		}
	}
	
	u->private = irc->private;
	
	if( u->away )
		irc_reply( irc, 301, "%s :%s", u->nick, u->away );
	
	if( u->send_handler )
		u->send_handler( irc, u, s );
	
	return( 1 );
}

int buddy_send_handler( irc_t *irc, user_t *u, char *msg )
{
	if( !u ) return( 0 );
	if( !u->gc ) return( 0 );
	return( ((struct gaim_connection *)u->gc)->prpl->send_im( u->gc, u->handle, msg, strlen( msg ), 0 ) );
}

int irc_msgfrom( irc_t *irc, char *nick, char *msg )
{
	user_t *u = user_find( irc, nick );
	static char *prefix = NULL;
	char *s = msg, *line = msg;
	char last = 0;
	
	if( !u ) return( 0 );
	if( prefix && *prefix ) free( prefix );
	
	if( !u->private && strcasecmp( u->nick, irc->mynick ) != 0 )
	{
		prefix = malloc( strlen( irc->nick) + 3 );
		sprintf( prefix, "%s%s", irc->nick, set_getstr( irc, "to_char" ) );
	}
	else
	{
		prefix = "";
	}
	
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
			if( strncasecmp( line, "/me ", 4 ) == 0 && u->private )
			{
				irc_write( irc, ":%s!%s@%s PRIVMSG %s :\001ACTION %s\001", u->nick, u->user, u->host,
				           irc->nick, line + 4 );
			}
			else
			{
				irc_write( irc, ":%s!%s@%s PRIVMSG %s :%s%s", u->nick, u->user, u->host,
				           u->private ? irc->nick : irc->channel, prefix, line );
			}
			line = s + 1;
		}
		s ++;
	}
	
	return( 1 );
}

/* Sort-of same as irc_msgfrom, but limited because notices don't know
   CTCP ACTION and because this one lacks multiline and in-channel support. */
int irc_noticefrom( irc_t *irc, char *nick, char *msg )
{
	user_t *u = user_find( irc, nick );

	if( u )
		irc_write( irc, ":%s!%s@%s NOTICE %s :%s", u->nick, u->user, u->host, irc->nick, msg );
	else
		return( 0 );
	
	return( 1 );
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
		irc_write( irc, ":%s MODE %s %s %s %s", irc->myhost, irc->channel, "+o-o", irc->nick, irc->mynick );
		return( value );
	}
	else if( strcasecmp( value, "root" ) == 0 )
	{
		irc_write( irc, ":%s MODE %s %s %s %s", irc->myhost, irc->channel, "-o+o", irc->nick, irc->mynick );
		return( value );
	}
	else if( strcasecmp( value, "both" ) == 0 )
	{
		irc_write( irc, ":%s MODE %s %s %s %s", irc->myhost, irc->channel, "+oo", irc->nick, irc->mynick );
		return( value );
	}
	else if( strcasecmp( value, "none" ) == 0 )
	{
		irc_write( irc, ":%s MODE %s %s %s %s", irc->myhost, irc->channel, "-oo", irc->nick, irc->mynick );
		return( value );
	}
	
	return( NULL );
}
