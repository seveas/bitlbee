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

#define BITLBEE_CORE
#include "bitlbee.h"
#include "commands.h"
#include "crypting.h"
#include "protocols/nogaim.h"
#include "help.h"
#include <signal.h>
#include <stdio.h>
#include <errno.h>

GList *connection_list = NULL;

int bitlbee_daemon_init()
{
	struct sockaddr_in listen_addr;
	int i;
	
	log_link( LOGLVL_ERROR, LOGOUTPUT_SYSLOG );
	log_link( LOGLVL_WARNING, LOGOUTPUT_SYSLOG );
	
	global.listen_socket = socket( AF_INET, SOCK_STREAM, 0 );
	if( global.listen_socket == -1 ) {
		log_error("socket");
		return( -1 );
	}
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons( global.conf->port );
	listen_addr.sin_addr.s_addr = inet_addr( global.conf->iface );

	i=bind( global.listen_socket, ( struct sockaddr * )&listen_addr, sizeof( struct sockaddr ) );
	if( i == -1 ) {
		log_error( "bind" );
		return( -1 );
	}

	i = listen( global.listen_socket, 10 );
	if( i == -1 ) {
		log_error( "listen" );
		return( -1 );
	}

#ifndef _WIN32
	if( !global.conf->nofork )
	{
		i = fork();
		if( i == -1 ) {
			log_error( "fork" );
			return( -1 );
		}
		else if( i!=0 ) 
			exit( 0 );
		close(0); close(1); close(2);
		chdir("/");
	}
#endif
	return( 0 );
}
 
int bitlbee_daemon_main_loop()
{
	GList *temp;
	struct timeval tv;
	int i, highest, size, new_socket;
	struct sockaddr_in conn_info;
 
	FD_ZERO( global.readfds );
	FD_ZERO( global.writefds );
	FD_SET( global.listen_socket, global.readfds );
	FD_SET( global.listen_socket, global.writefds );
		
	temp = connection_list;
	highest = global.listen_socket;
	
	while( temp != NULL ) 
	{
		FD_SET( ((irc_t *) temp->data)->fd, global.readfds );
		FD_SET( ((irc_t *) temp->data)->fd, global.writefds );
		if( ((irc_t *) temp->data)->fd > highest )
			highest = ((irc_t *) temp->data)->fd;		
		temp = temp->next;
	} 

	tv.tv_sec = 0;
	tv.tv_usec = 200000;

	if( ( i = select( highest + 1, global.readfds, NULL, NULL, &tv ) ) > 0 )
	{
		if( FD_ISSET( global.listen_socket, global.readfds ) ) 
		{
			size = sizeof( struct sockaddr_in );
			new_socket = accept( global.listen_socket, (struct sockaddr *) &conn_info, 
			                     &size );
			log_message( LOGLVL_INFO, "Creating new connection with fd %d.", new_socket );
			i = bitlbee_connection_create( new_socket );
			if( i != 1 )
				return( -1 );
		}
		temp = connection_list;
		while( temp != NULL ) 
		{
			if( FD_ISSET( ((irc_t *) temp->data)->fd, global.readfds ) )
			{
				irc_t *irc = temp->data;
				if( !irc_fill_buffer( irc ) )
				{
					log_message( LOGLVL_INFO, "Destroying connection with fd %d.", irc->fd );
					temp = bitlbee_connection_destroy( temp );
				}
				else
				{
					if( !irc_process( irc ) )
						temp = bitlbee_connection_destroy( temp );
					if( irc_userping( irc ) > 0 )
						temp = bitlbee_connection_destroy( temp );
				}
			}
			if( temp != NULL )
				temp = temp->next;
		}
	}

	temp = connection_list;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if( ( i = select( highest + 1, NULL, global.writefds, NULL, &tv ) ) > 0 ) {
		while( temp != NULL ) 
		{
			if( FD_ISSET( ( ( irc_t *)( temp->data ) )->fd, global.writefds ) )
				if( !irc_write_buffer(temp->data) )
					temp = bitlbee_connection_destroy( temp );
			if( temp != NULL )
				temp = temp->next;	
		}
	}
	 
	else if( i == -1 )
	{
		log_error( "select" );
		return -1;
	}
	g_main_iteration( FALSE );
	
	return 0;	
}
 
int bitlbee_inetd_init()
{
	if( !bitlbee_connection_create( 0 ) )
		return( 1 );

	log_link( LOGLVL_ERROR, LOGOUTPUT_IRC );
	log_link( LOGLVL_WARNING, LOGOUTPUT_IRC );

	return( 0 );
}
 
int bitlbee_inetd_main_loop()
{
	struct timeval tv[1];
	int i;
	irc_t *irc;
	GList *temp;
	
	temp = connection_list;
	irc = temp->data;
	
	FD_ZERO( global.readfds );
	FD_ZERO( global.writefds );
	FD_SET( irc->fd, global.readfds );
	FD_SET( irc->fd, global.writefds );
	tv->tv_sec = 0;
	tv->tv_usec = 200000;
	
	if( ( i = select( ((irc_t *) temp->data)->fd + 1, global.readfds, NULL, NULL, tv ) ) > 0 )
	{
		if( !irc_fill_buffer( (irc_t *) temp->data ) )
		{
			temp = bitlbee_connection_destroy( temp );
			return( 1 );
		}
		else if( !irc_process( (irc_t *) temp->data ) )
		{
			temp = bitlbee_connection_destroy( temp );
			return( 1 );
		}
	}

	tv->tv_sec = 0;
	tv->tv_usec = 0;

	if( ( i = select( ((irc_t *) temp->data)->fd + 1, NULL, global.writefds, NULL, tv ) ) > 0 )
	{
		if( !irc_write_buffer( (irc_t *) temp->data ) )
		{ 
			if( ( ( irc_t * )( temp->data ) )->status && set_getint( ( (irc_t * )( temp->data ) ), "save_on_quit" ) ) 
				if( !bitlbee_save( ( (irc_t * )( temp->data ) ) ) )
					irc_usermsg( ( (irc_t * )( temp->data ) ), "Error while saving settings!" );
			return 1;
		}
	}
	else if( i == -1 ) return( -1 );
	if( irc_userping( temp->data ) > 0 )
		return( 1 );

	g_main_iteration( FALSE );
	
	return( 0 );
}

int bitlbee_connection_create( int fd )
{
	irc_t *newconn;

	newconn = irc_new( fd );
	if( newconn == NULL )
		return( 0 );
	
	connection_list = g_list_append( connection_list, newconn );
	
	set_add( newconn, "away_devoice", "true",  set_eval_away_devoice );
	set_add( newconn, "auto_connect", "true", set_eval_bool );
	set_add( newconn, "auto_reconnect", "false", set_eval_bool );
	set_add( newconn, "auto_reconnect_delay", "300", set_eval_int );
	set_add( newconn, "buddy_sendbuffer", "false", set_eval_bool );
	set_add( newconn, "buddy_sendbuffer_delay", "1", set_eval_int );
	set_add( newconn, "charset", "iso8859-15", set_eval_charset );
	set_add( newconn, "debug", "false", set_eval_bool );
	set_add( newconn, "default_target", "root", NULL );
	set_add( newconn, "display_namechanges", "false", set_eval_bool );
	set_add( newconn, "handle_unknown", "root", NULL );
	set_add( newconn, "html", "nostrip", NULL );
	set_add( newconn, "lcnicks", "true", set_eval_bool );
	set_add( newconn, "ops", "both", set_eval_ops );
	set_add( newconn, "private", "false", set_eval_bool );
	set_add( newconn, "query_order", "lifo", NULL );
	set_add( newconn, "save_on_quit", "1", set_eval_bool );
	set_add( newconn, "to_char", ": ", set_eval_to_char );
	set_add( newconn, "typing_notice", "false", set_eval_bool );
	
	conf_loaddefaults( newconn );
	
	return( 1 );	
} 

GList *bitlbee_connection_destroy( GList *node )
{
	GList *returnval;

	log_message(LOGLVL_INFO, "Destroying connection with fd %d", ( (irc_t * )( node->data ) )->fd); 
	
	if( ( (irc_t * )( node->data ) )->status && set_getint( (irc_t *)( node->data ), "save_on_quit" ) ) 
		if( !bitlbee_save( node->data ) )
			irc_usermsg( node->data, "Error while saving settings!" );

	FD_CLR( ( (irc_t * )( node->data ) )->fd, global.readfds ); 
	FD_CLR( ( (irc_t * )( node->data ) )->fd, global.writefds ); 
	
	closesocket( ( (irc_t * )( node->data ) )->fd );

	returnval=node->next;

	connection_list=g_list_remove_link(connection_list, node);
	irc_free(node->data);
	g_list_free(node);

	return returnval;
}


int bitlbee_load( irc_t *irc, char* password )
{
	char s[512];
	char *line;
	int proto;
	char nick[MAX_NICK_LENGTH+1];
	FILE *fp;
	user_t *ru = user_find( irc, ROOT_NICK );
	
	if( irc->status == USTATUS_IDENTIFIED )
		return( 1 );
	
	g_snprintf( s, 511, "%s%s%s", global.conf->configdir, irc->nick, ".accounts" );
   	fp = fopen( s, "r" );
   	if( !fp ) return( 0 );
	
	fscanf( fp, "%32[^\n]s", s );
	if( setpass( irc, password, s ) < 0 )
		return( -1 );
	
	/* Do this now. If the user runs with AuthMode = Registered, the
	   account command will not work otherwise. */
	irc->status = USTATUS_IDENTIFIED;
	
	while( fscanf( fp, "%511[^\n]s", s ) > 0 )
	{
		fgetc( fp );
		line = deobfucrypt( irc, s );
		root_command_string( irc, ru, line );
		g_free( line );
	}
	fclose( fp );
	
	g_snprintf( s, 511, "%s%s%s", global.conf->configdir, irc->nick, ".nicks" );
	fp = fopen( s, "r" );
	if( !fp ) return( 0 );
	while( fscanf( fp, "%s %d %s", s, &proto, nick ) > 0 )
	{
		http_decode( s );
		nick_set( irc, s, proto, nick );
	}
	fclose( fp );
	
	if( set_getint( irc, "auto_connect" ) )
	{
		strcpy( s, "account on" );	/* Can't do this directly because r_c_s alters the string */
		root_command_string( irc, ru, s );
	}
	
	return( 1 );
}

int bitlbee_save( irc_t *irc )
{
	char s[512];
	char path[512], new_path[512];
	char *line;
	nick_t *n;
	set_t *set;
	mode_t ou = umask( 0077 );
	account_t *a;
	FILE *fp;
	char *hash;
	
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
	
	hash = hashpass( irc );
	if( hash == NULL )
	{
		irc_usermsg( irc, "Please register yourself if you want to save your settings." );
		return( 0 );
	}
	
	g_snprintf( path, 511, "%s%s%s", global.conf->configdir, irc->nick, ".nicks~" );
	fp = fopen( path, "w" );
	if( !fp ) return( 0 );
	for( n = irc->nicks; n; n = n->next )
	{
		strcpy( s, n->handle );
		s[169] = 0; /* Prevent any overflow (169 ~ 512 / 3) */
		http_encode( s );
		g_snprintf( s + strlen( s ), 510 - strlen( s ), " %d %s", n->proto, n->nick );
		if( fprintf( fp, "%s\n", s ) != strlen( s ) + 1 )
		{
			irc_usermsg( irc, "fprintf() wrote too little. Disk full?" );
			fclose( fp );
			return( 0 );
		}
	}
	fclose( fp );
  
	g_snprintf( new_path, 512, "%s%s%s", global.conf->configdir, irc->nick, ".nicks" );
	if( unlink( new_path ) != 0 )
	{
		if( errno != ENOENT )
		{
			irc_usermsg( irc, "Error while removing old .nicks file" );
			return( 0 );
		}
	}
	if( rename( path, new_path ) != 0 )
	{
		irc_usermsg( irc, "Error while renaming new .nicks file" );
		return( 0 );
	}
	
	g_snprintf( path, 511, "%s%s%s", global.conf->configdir, irc->nick, ".accounts~" );
	fp = fopen( path, "w" );
	if( !fp ) return( 0 );
	if( fprintf( fp, "%s", hash ) != strlen( hash ) )
	{
		irc_usermsg( irc, "fprintf() wrote too little. Disk full?" );
		fclose( fp );
		return( 0 );
	}
	g_free( hash );

	for( a = irc->accounts; a; a = a->next )
	{
		if( a->protocol == PROTO_OSCAR || a->protocol == PROTO_ICQ || a->protocol == PROTO_TOC )
			g_snprintf( s, sizeof( s ), "account add oscar \"%s\" \"%s\" %s", a->user, a->pass, a->server );
		else
			g_snprintf( s, sizeof( s ), "account add %s \"%s\" \"%s\" \"%s\"",
			            proto_name[a->protocol], a->user, a->pass, a->server ? a->server : "" );
		
		line = obfucrypt( irc, s );
		if( *line )
		{
			if( fprintf( fp, "%s\n", line ) != strlen( line ) + 1 )
			{
				irc_usermsg( irc, "fprintf() wrote too little. Disk full?" );
				fclose( fp );
				return( 0 );
			}
		}
		g_free( line );
	}
	
	for( set = irc->set; set; set = set->next )
	{
		if( set->value && set->def )
		{
			g_snprintf( s, sizeof( s ), "set %s \"%s\"", set->key, set->value );
			line = obfucrypt( irc, s );
			if( *line )
			{
				if( fprintf( fp, "%s\n", line ) != strlen( line ) + 1 )
				{
					irc_usermsg( irc, "fprintf() wrote too little. Disk full?" );
					fclose( fp );
					return( 0 );
				}
			}
			g_free( line );
		}
	}
	
	if( strcmp( irc->mynick, ROOT_NICK ) != 0 )
	{
		g_snprintf( s, sizeof( s ), "rename %s %s", ROOT_NICK, irc->mynick );
		line = obfucrypt( irc, s );
		if( *line )
		{
			if( fprintf( fp, "%s\n", line ) != strlen( line ) + 1 )
			{
				irc_usermsg( irc, "fprintf() wrote too little. Disk full?" );
				fclose( fp );
				return( 0 );
			}
		}
		g_free( line );
	}
	fclose( fp );
	
 	g_snprintf( new_path, 512, "%s%s%s", global.conf->configdir, irc->nick, ".accounts" );
 	if( unlink( new_path ) != 0 )
	{
		if( errno != ENOENT )
		{
			irc_usermsg( irc, "Error while removing old .accounts file" );
			return( 0 );
		}
	}
	if( rename( path, new_path ) != 0 )
	{
		irc_usermsg( irc, "Error while renaming new .accounts file" );
		return( 0 );
	}
	
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
		if( g_strcasecmp( commands[i].command, cmd[0] ) == 0 )
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
	
	t = bitlbee_alloc( strlen( s ) + 1 );
	
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
	g_free( t );
}

/* Warning: This one explodes the string. Worst-cases can make the string 3x its original size! */
/* This fuction is safe, but make sure you call it safely as well! */
void http_encode( char *s )
{
	char *t;
	int i, j;
	
	t = g_strdup( s );
	
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
	
	g_free( t );
}


void *bitlbee_alloc( size_t size )
{
	void *mem;

	mem=g_malloc(size);
	if(mem==NULL) {
		log_error("g_malloc");
		exit(1);
	}
	
	return(mem);
}

void *bitlbee_realloc( void *oldmem, size_t newsize )
{
	void *newmem;

	newmem=g_realloc(oldmem, newsize);
	if(newmem==NULL) {
		log_error("realloc");
		exit(1);
	}
	
	return(newmem);
}
