#include "bitlbee.h"
#include "commands.h"
#include "crypting.h"
#include "protocols/nogaim.h"
#include "help.h"

irc_t *IRC;	/* :-( */

int main( int argc, char *argv[] )
{
	irc_t *irc;
	struct timeval tv[1];
	fd_set fds[1];
	int i;
	
	if( !( IRC = irc = irc_new( 0 ) ) )
		return( 1 );
	
	nogaim_init();
	set_add( irc, "save_on_quit", "true", set_eval_bool );
	
	while( 1 )
	{
		FD_ZERO( fds );
		FD_SET( irc->fd, fds );
		tv->tv_sec = 1;
		tv->tv_usec = 0;
		if( ( i = select( irc->fd + 1, fds, NULL, NULL, tv ) ) > 0 )
		{
			if( !irc_process( irc ) ) break;
		}
		else if( i == -1 ) break;
		g_main_iteration( FALSE );
	}
	
	if( irc->status && set_getint( irc, "save_on_quit" ) )
		if( !bitlbee_save( irc ) )
			irc_usermsg( irc, "Error while saving settings!" );
	
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
	
	irc->status = USTATUS_IDENTIFIED;
	
	return( 1 );
}

int bitlbee_save( irc_t *irc )
{
	char s[128];
	char *line;
	nick_t *n = irc->nicks;
	GSList *c = connections;
	set_t *set = irc->set;
	mode_t ou = umask( 0077 );
	FILE *fp;
	
	/*\
	 *  [SH] Nothing should be save if no password is set,
	 *  because the password is not set if it was wrong, or if one
	 *  is not identified yet. This means that a malicious user could
	 *  easily overwrite files owned by someone else:
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
		irc_usermsg( irc, "Please register yourself with NickServ if you want to save your settings." );
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
	while( c )
	{
		struct gaim_connection *gc = c->data;
		c = c->next;
		
		if( gc->protocol == PROTO_MSN )
			snprintf( s, sizeof( s ), "login msn %s %s", gc->user->username, gc->user->password );
		else if( gc->protocol == PROTO_OSCAR || gc->protocol == PROTO_ICQ || gc->protocol == PROTO_TOC )
			snprintf( s, sizeof( s ), "login oscar \"%s\" %s %s", gc->user->username, gc->user->password, gc->user->proto_opt[0] );
		else if( gc->protocol == PROTO_JABBER )
			snprintf( s, sizeof( s ), "login jabber %s %s", gc->user->username, gc->user->password );
		
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

void http_encode( char *s )
{
	char *t;
	int i, j;
	
	t = malloc( strlen( s ) * 3 + 1 );
	
	for( i = j = 0; s[i]; i ++, j ++ )
	{
		if( s[i] <= ' ' || ((unsigned char *)s)[i] >= 128 || s[i] == '%' )
		{
			sprintf( t + j, "%%%02X", s[i] );
			j += 2;
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
