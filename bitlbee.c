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

#include "bitlbee.h"
#include "commands.h"
#include "crypting.h"
#include "protocols/nogaim.h"
#include "help.h"
#include <signal.h>

#ifdef USE_GNUTLS
#include <gnutls/gnutls.h>
#endif

irc_t *IRC;	/* :-( */
conf_t *conf;

static void sighandler( int signal );

int main( int argc, char *argv[] )
{
	irc_t *irc;
	struct timeval tv[1];
	fd_set fds[1];
	int i;

#ifdef USE_GNUTLS
	gnutls_global_init();
#endif
	
	if( 1 )
	{
		/* Catch some signals to tell the user what's happening before quitting */
		struct sigaction sig, old;
		memset( &sig, 0, sizeof( sig ) );
		sig.sa_handler = sighandler;
		sigaction( SIGPIPE, &sig, &old );
		sig.sa_flags = SA_RESETHAND;
		sigaction( SIGINT,  &sig, &old );
		sigaction( SIGILL,  &sig, &old );
		sigaction( SIGBUS,  &sig, &old );
		sigaction( SIGFPE,  &sig, &old );
		sigaction( SIGSEGV, &sig, &old );
		sigaction( SIGTERM, &sig, &old );
		sigaction( SIGQUIT, &sig, &old );
		sigaction( SIGXCPU, &sig, &old );
	}
	
	conf = conf_load( argc, argv );

	if( !conf )
		return( 1 );
	
	if( !( IRC = irc = irc_new( 0 ) ) )
		return( 1 );
	
	nogaim_init();
	set_add( irc, "save_on_quit", "true", set_eval_bool );
	
	conf_loaddefaults( irc );
	
	while( 1 )
	{
		FD_ZERO( fds );
		FD_SET( irc->fd, fds );
		tv->tv_sec = 0;
		tv->tv_usec = 200000;
		if( ( i = select( irc->fd + 1, fds, NULL, NULL, tv ) ) > 0 )
		{
			if( !irc_process( irc ) ) break;
		}
		else if( i == -1 ) break;
		if( irc_userping( irc ) > 0 )
			break;
		g_main_iteration( FALSE );
	}
	
	if( irc->status && set_getint( irc, "save_on_quit" ) )
		if( !bitlbee_save( irc ) )
			irc_usermsg( irc, "Error while saving settings!" );
	
#ifdef USE_GNUTLS
	gnutls_global_deinit();
#endif
	return( 0 );
}

int bitlbee_init( irc_t *irc )
{
	if( !getuid() || !geteuid() )
		irc_usermsg( irc, "You're running BitlBee as root. Why?" );
	
	if( access( CONFIG, F_OK ) != 0 )
		irc_usermsg( irc, "The configuration directory %s does not exist. Configuration won't be saved.", CONFIG );
	else if( access( CONFIG, R_OK ) != 0 || access( CONFIG, W_OK ) != 0 )
		irc_usermsg( irc, "Permission problem: Can't read/write from/to %s", CONFIG );
	
	if( help_init( irc ) == NULL )
		irc_usermsg( irc, "Error opening helpfile: %s", HELP_FILE );
	
	return( 1 );
}

int bitlbee_load( irc_t *irc, char* password )
{
	char s[128];
	char *line;
	int proto;
	char nick[MAX_NICK_LENGTH+1];
	FILE *fp;
	user_t *ru = user_find( irc, ROOT_NICK );
	
	if( irc->status == USTATUS_IDENTIFIED )
		return( 1 );
	
	snprintf( s, 127, "%s%s%s", CONFIG, irc->nick, ".accounts" );
	fp = fopen( s, "r" );
	if( !fp ) return( 0 );

	fscanf( fp, "%32[^\n]s", s );
	if( setpass( irc, password, s ) < 0 ) {
		return( -1 );
	}
	
	/* Do this now. If the user runs with AuthMode = Registered, the
	   account command will not work otherwise. */
	irc->status = USTATUS_IDENTIFIED;
	
	while( fscanf( fp, "%127[^\n]s", s ) > 0 )
	{
		fgetc( fp );
		line = deobfucrypt( irc, s );
		root_command_string( irc, ru, line );
		free( line );
	}
	fclose( fp );
	
	snprintf( s, 127, "%s%s%s", CONFIG, irc->nick, ".nicks" );
	fp = fopen( s, "r" );
	if( !fp ) return( 0 );
	while( fscanf( fp, "%s %d %s", s, &proto, nick ) > 0 )
	{
		http_decode( s );
		nick_set( irc, s, proto, nick );
	}
	fclose( fp );
	
	if( set_getint( IRC, "auto_connect" ) )
	{
		strcpy( s, "account on" );	/* Can't do this directly because r_c_s alters the string */
		root_command_string( irc, ru, s );
	}
	
	return( 1 );
}

int bitlbee_save( irc_t *irc )
{
	char s[128];
	char *line;
	nick_t *n = irc->nicks;
	set_t *set = irc->set;
	mode_t ou = umask( 0077 );
	account_t *a;
	FILE *fp;
	
	/*\
	 *  [SH] Nothing should be saved if no password is set, because the
	 *  password is not set if it was wrong, or if one is not identified
	 *  yet. This means that a malicious user could easily overwrite
	 *  files owned by someone else:
	 *  a Bad Thing, methinks
	\*/

	/* [WVG] No? Really? */

	/*\
	 *  [SH] Okay, okay, it wasn't really Wilmer who said that, it was
	 *  me. I just thought it was funny.
	\*/
	
	line = hashpass( irc );
	if( line == NULL )
	{
		irc_usermsg( irc, "Please register yourself if you want to save your settings." );
		return( 0 );
	}
	
	snprintf( s, 127, "%s%s%s", CONFIG, irc->nick, ".nicks" );
	fp = fopen( s, "w" );
	if( !fp ) return( 0 );
	while( n )
	{
		strcpy( s, n->handle );
		s[42] = 0; /* Prevent any overflow (42 == 128 / 3) */
		http_encode( s );
		fprintf( fp, "%s %d %s\n", s, n->proto, n->nick );
		n = n->next;
	}
	fclose( fp );
	
	snprintf( s, 127, "%s%s%s", CONFIG, irc->nick, ".accounts" );
	fp = fopen( s, "w" );
	if( !fp ) return( 0 );
	fprintf( fp, "%s", line );
	free( line );
	
	/* [SH] Making s empty, because if no settings nor accounts are defined
	   the file will contain it's name encrypted. How 'bout redundant
	   information? */
	memset( s, 0, sizeof( s ) );
	for( a = irc->accounts; a; a = a->next )
	{
		if( a->protocol == PROTO_OSCAR || a->protocol == PROTO_ICQ || a->protocol == PROTO_TOC )
			snprintf( s, sizeof( s ), "account add oscar \"%s\" %s %s", a->user, a->pass, a->server );
		else
			snprintf( s, sizeof( s ), "account add %s %s %s", proto_name[a->protocol], a->user, a->pass );
		
		line = obfucrypt( irc, s );
		if( *line ) fprintf( fp, "%s\n", line );
		free( line );
	}
	memset( s, 0, sizeof( s ) );
	while( set )
	{
		if( set->value ) {
			snprintf( s, sizeof( s ), "set %s %s", set->key, set->value );
			line = obfucrypt( irc, s );
			if( *line ) fprintf( fp, "%s\n", line );
			free( line );
		}
		set = set->next;
	}
	if( strcmp( irc->mynick, ROOT_NICK ) != 0 )
	{
		snprintf( s, sizeof( s ), "rename %s %s", ROOT_NICK, irc->mynick );
		line = obfucrypt( irc, s );
		if( *line ) fprintf( fp, "%s\n", line );
		free( line );
	}
	fclose( fp );
	
	umask( ou );
	
	return( 1 );
}

int root_command_string( irc_t *irc, user_t *u, char *command )
{
	char *cmd[IRC_MAX_ARGS];
	char *s;
	int k;
	char q = 0;
	
	memset( cmd, 0, sizeof( cmd ) );
	cmd[0] = command;
	k = 1;
	for( s = command; *s && k < ( IRC_MAX_ARGS - 1 ); s ++ )
		if( *s == ' ' && !q )
		{
			*s = 0;
			while( *++s == ' ' );
			if( *s == '"' || *s == '\'' )
			{
				q = *s;
				s ++;
			}
			if( *s )
			{
				cmd[k++] = s;
				s --;
			}
		}
		else if( *s == q )
		{
			q = *s = 0;
		}
	cmd[k] = NULL;
	
	return( root_command( irc, cmd ) );
}

int root_command( irc_t *irc, char *cmd[] )
{	
	int i;
	
	if( !cmd[0] )
		return( 0 );
	
	for( i = 0; commands[i].command; i++ )
		if( strcasecmp( commands[i].command, cmd[0] ) == 0 )
		{
			if( !cmd[commands[i].required_parameters] )
			{
				irc_usermsg( irc, "Not enough parameters given (need %d)", commands[i].required_parameters );
				return( 0 );
			}
			commands[i].execute( irc, cmd );
			return( 1 );
		}
	
	irc_usermsg( irc, "Unknown command: %s. Please use help commands to get a list of available commands.", cmd[0] );
	
	return( 1 );
}

/* Decode%20a%20file%20name						*/
void http_decode( char *s )
{
	char *t;
	int i, j, k;
	
	t = malloc( strlen( s ) + 1 );
	
	for( i = j = 0; s[i]; i ++, j ++ )
	{
		if( s[i] == '%' )
		{
			if( sscanf( s + i + 1, "%2x", &k ) )
			{
				t[j] = k;
				i += 2;
			}
			else
			{
				*t = 0;
				break;
			}
		}
		else
		{
			t[j] = s[i];
		}
	}
	t[j] = 0;
	
	strcpy( s, t );
	free( t );
}

/* Warning: This one explodes the string. Worst-cases can make the string 3x its original size! */
/* This fuction is safe, but make sure you call it safely as well! */
void http_encode( char *s )
{
	char *t;
	int i, j;
	
	t = malloc( strlen( s ) + 1 );
	strcpy( t, s );
	
	for( i = j = 0; t[i]; i ++, j ++ )
	{
		if( t[i] <= ' ' || ((unsigned char *)t)[i] >= 128 || t[i] == '%' )
		{
			sprintf( s + j, "%%%02X", t[i] );
			j += 2;
		}
		else
		{
			s[j] = t[i];
		}
	}
	s[j] = 0;
	
	free( t );
}

static void sighandler( int signal )
{
	if( signal == SIGPIPE )
	{
		/* SIGPIPE is ignored by Gaim. Looks like we have to do
		   the same, because it causes some nasty hangs. */
		if( set_getint( IRC, "debug" ) )
			irc_usermsg( IRC, "Warning: Caught SIGPIPE, but we probably have to ignore this and pretend nothing happened..." );
		return;
	}
	else
	{
		irc_write( IRC, "ERROR :Fatal signal received: %d. That's probably a bug.. :-/", signal );
#ifdef USE_GNUTLS
		gnutls_global_deinit();
#endif
		raise( signal ); /* Re-raise the signal so the default handler will pick it up, dump core, etc... */
	}
}

double gettime()
{
	struct timeval time[1];
	
	gettimeofday( time, 0 );
	return( (double) time->tv_sec + (double) time->tv_usec / 1000000 );
}
