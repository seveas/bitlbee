#include "commands.h"
#include "crypting.h"
#include "bitlbee.h"
#include "help.h"

#include <unistd.h>

command_t commands[] = {
	{ "help",       0, cmd_help }, 
	{ "identify",   1, cmd_identify },
	{ "register",   1, cmd_register },
	{ "login",      3, cmd_login },
	{ "logout",     1, cmd_logout }, 
	{ "slist",      0, cmd_slist },
	{ "add",        2, cmd_add },
	{ "info",       1, cmd_info },
	{ "rename",     2, cmd_rename },
	{ "remove",     1, cmd_remove },
	{ "block",      1, cmd_block },
	{ "allow",      1, cmd_allow },
	{ "save",       0, cmd_save },
	{ "set",        0, cmd_set },
	{ "yes",        0, cmd_yesno },
	{ "no",         0, cmd_yesno },
	{ "blist",      0, cmd_blist },
	{ NULL }
};

int cmd_help( irc_t *irc, char **cmd )
{
	char *s;
	
	if( !cmd[1] ) cmd[1] = "";
	s = help_get( irc, cmd[1] );
	if( !s ) s = help_get( irc, "" );
	
	if( s )
	{
		irc_usermsg( irc, "%s", s );
		free( s );
		return( 1 );
	}
	else
	{
		irc_usermsg( irc, "Error opening helpfile." );
		return( 0 );
	}
}

int cmd_login( irc_t *irc, char **cmd )
{
	struct aim_user *u;
	int prot;
	
	for( prot = 0; prot < PROTO_MAX; prot ++ )
		if( proto_name[prot] && *proto_name[prot] && strcasecmp( proto_name[prot], cmd[1] ) == 0 )
			break;
	
	if( prot == PROTO_MAX )
	{
		irc_usermsg( irc, "Unknown protocol" );
		return( 1 );
	}
	
	u = malloc( sizeof( struct aim_user ) );
	memset( u, 0, sizeof( *u ) );
	strcpy( u->username, cmd[2] );
	strcpy( u->password, cmd[3] );
	u->protocol = prot;
	if( prot == PROTO_OSCAR )
	{
		if( cmd[4] )
			strcpy( u->proto_opt[0], cmd[4] );
		else
		{
			irc_usermsg( irc, "Not enough parameters" );
			free( u );
			return( 1 );
		}
	}
	
	proto_prpl[prot]->login( u );
	
	return( 0 );
}

int cmd_logout( irc_t *irc, char **cmd )
{
	struct gaim_connection *gc;
	int i;
	
	if( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr( i ) ) )
	{
		irc_usermsg( irc, "Incorrect connection number" );
		return( 1 );
	}
	account_offline( gc );
	destroy_gaim_conn( gc );
	
	return( 0 );
}

int cmd_slist( irc_t *irc, char **cmd )
{
	int i = 0;
	struct gaim_connection *gc;
	const GSList *c = connections;
	
	while( c )
	{
		gc = c->data;
		c = c->next;
		
		if( gc->protocol == PROTO_MSN )
			irc_usermsg( irc, "%2d. MSN, %s", i, gc->user->username );
		else if( gc->protocol == PROTO_OSCAR || gc->protocol == PROTO_ICQ || gc->protocol == PROTO_TOC )
			irc_usermsg( irc, "%2d. OSCAR, %s on %s", i, gc->user->username, gc->user->proto_opt[0] );
		else if( gc->protocol == PROTO_JABBER )
			irc_usermsg( irc, "%2d. JABBER, %s", i, gc->user->username );
		i ++;
	}
	irc_usermsg( irc, "End of connection list" );
	
	return( 0 );
}

int cmd_add( irc_t *irc, char **cmd )
{
	int i;
	struct gaim_connection *gc;
	
	if( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr( i ) ) )
	{
		irc_usermsg( irc, "Incorrect connection number" );
		return( 1 );
	}
	gc->prpl->add_buddy( gc, cmd[2] );
	add_buddy( gc, NULL, cmd[2], cmd[2] );
	
	return( 0 );
}

int cmd_info( irc_t *irc, char **cmd )
{
	struct gaim_connection *gc;
	int i;
	
	if( !cmd[2] )
	{
		user_t *u = user_find( irc, cmd[1] );
		if( !u || !u->gc )
		{
			irc_usermsg( irc, "Nick '%s' does not exist", cmd[1] );
			return( 1 );
		}
		gc = u->gc;
		cmd[2] = u->handle;
	}
	else if( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr( i ) ) )
	{
		irc_usermsg( irc, "Incorrect connection number" );
		return( 1 );
	}
	
	if( !gc->prpl->get_info )
	{
		irc_usermsg( irc, "info-command not implemented for this protocol" );
		return( 1 );
	}
	gc->prpl->get_info( gc, cmd[2] );
	
	return( 0 );
}

int cmd_rename( irc_t *irc, char **cmd)
{
	user_t *u;
	
	if( ( strcasecmp( cmd[1], irc->nick ) == 0 ) ) // || ( strcasecmp( cmd[1], irc->mynick ) == 0 ) )
	{
		irc_usermsg( irc, "Nick '%s' can't be changed", cmd[1] );
		return( 1 );
	}
	if( user_find( irc, cmd[2] ) )
	{
		irc_usermsg( irc, "Nick '%s' already exists", cmd[2] );
		return( 1 );
	}
	if( !nick_ok( cmd[2] ) )
	{
		irc_usermsg( irc, "Nick '%s' contains invalid characters", cmd[2] );
		return( 1 );
	}
	if( !( u = user_find( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Nick '%s' does not exist", cmd[1] );
		return( 1 );
	}
	/* TODO: Hmm, this might damage u->user */
	free( u->nick );
	u->nick = strdup( cmd[2] );
	irc_write( irc, ":%s!%s@%s NICK %s", cmd[1], u->user, u->host, cmd[2] );
	if( strcasecmp( cmd[1], irc->mynick ) == 0 )
	{
		free( irc->mynick );
		irc->mynick = strdup( cmd[2] );
	}
	else
	{
		nick_set( irc, u->handle, ((struct gaim_connection *)u->gc)->protocol, cmd[2] );
	}
	
	return( 0 );
}

int cmd_remove( irc_t *irc, char **cmd )
{
	user_t *u;
	
	if( !( u = user_find( irc, cmd[1] ) ) || !u->gc )
	{
		irc_usermsg( irc, "Buddy '%s' not found", cmd[1] );
		return( 1 );
	}
	((struct gaim_connection *)u->gc)->prpl->remove_buddy( u->gc, u->handle, NULL );
	user_del( irc, cmd[1] );
	
	return( 0 );
}

int cmd_block( irc_t *irc, char **cmd )
{
	struct gaim_connection *gc;
	int i;
	
	if( !cmd[2] )
	{
		user_t *u = user_find( irc, cmd[1] );
		if( !u || !u->gc )
		{
			irc_usermsg( irc, "Nick '%s' does not exist", cmd[1] );
			return( 1 );
		}
		gc = u->gc;
		cmd[2] = u->handle;
	}
	else if( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr( i ) ) )
	{
		irc_usermsg( irc, "Incorrect connection number" );
		return( 1 );
	}
	if( !gc->prpl->add_deny || !gc->prpl->rem_permit )
	{
		irc_usermsg( irc, "Command not supported by this protocol" );
		return( 1 );
	}
	else
	{
		gc->prpl->rem_permit( gc, cmd[2] );
		gc->prpl->add_deny( gc, cmd[2] );
	}
	
	return( 0 );
}

int cmd_allow( irc_t *irc, char **cmd )
{
	struct gaim_connection *gc;
	int i;
	
	if( !cmd[2] )
	{
		user_t *u = user_find( irc, cmd[1] );
		if( !u || !u->gc )
		{
			irc_usermsg( irc, "Nick '%s' does not exist", cmd[1] );
			return( 1 );
		}
		gc = u->gc;
		cmd[2] = u->handle;
	}
	else if( !cmd[1] || !sscanf( cmd[1], "%d", &i ) || !( gc = gc_nr( i ) ) )
	{
		irc_usermsg( irc, "Incorrect connection number" );
		return( 1 );
	}
	if( !gc->prpl->rem_deny || !gc->prpl->add_permit )
	{
		irc_usermsg( irc, "Command not supported by this protocol" );
		return( 1 );
	}
	else
	{
		gc->prpl->rem_deny( gc, cmd[2] );
		gc->prpl->add_permit( gc, cmd[2] );
	}
	
	return( 0 );
}

int cmd_yesno( irc_t *irc, char **cmd )
{
	query_t *q = irc->queries;
	
	if( !q )
	{
		irc_usermsg( irc, "Did I ask you something?" );
		return( 1 );
	}
	if( !strcasecmp( cmd[0], "yes" ) )
	{
		irc_usermsg( irc, "Accepting: %s", q->question );
		q->yes( NULL, q->data );
	}
	else
	{
		irc_usermsg( irc, "Rejecting: %s", q->question );
		q->no( NULL, q->data );
	}
	irc->queries = irc->queries->next;
	free( q->question );
	/* free( q->data ); */
	free( q );
	if( irc->queries )
	{
		irc_usermsg( irc, "%s", irc->queries->question );
		irc_usermsg( irc, "Type yes to accept or no to reject" );
	}
	
	return( 0 );
}

int cmd_set( irc_t *irc, char **cmd )
{
	if( cmd[2] )
	{
		set_setstr( irc, cmd[1], cmd[2] );
	}
	if( cmd[1] ) /* else 'forgotten' on purpose.. */
	{
		char *s = set_getstr( irc, cmd[1] );
		if( s )
			irc_usermsg( irc, "%s = '%s'", cmd[1], s );
	}
	else
	{
		set_t *s = irc->set;
		while( s )
		{
			if( s->value || s->def )
				irc_usermsg( irc, "%s = `%s'", s->key, s->value?s->value:s->def );
			s = s->next;
		}
	}
	
	return( 0 );
}

int cmd_save( irc_t *irc, char **cmd )
{
	if( bitlbee_save( irc ) )
		irc_usermsg( irc, "Configuration saved" );
	else
		irc_usermsg( irc, "Configuration could not be saved!" );
	
	return( 0 );
}

int cmd_identify( irc_t *irc, char **cmd )
{
	if( !cmd[1] )
	{
		irc_usermsg( irc, "Syntax: identify <password>" );
	}
	else
	{
		int checkie = bitlbee_load( irc, cmd[1] );
		if( checkie == -1 )
		{
			irc_usermsg( irc, "Incorrect password" );
		}
		else if( checkie == 0 )
		{
			irc_usermsg( irc, "The nickname is (probably) not registered" );
		}
		else if( checkie == 1 )
		{
			irc_usermsg( irc, "Password accepted" );
		}
		else
		{
			irc_usermsg( irc, "Something very weird happened" );
		}
	}
	
	return( 0 );
}

int cmd_register( irc_t *irc, char **cmd )
{
	if( !cmd[1] )
	{
		irc_usermsg( irc, "Syntax: register <password>" );
	}
	else
	{
		int checkie;
		
		char *str = (char *) malloc( strlen( irc->nick ) +
			strlen( CONFIG ) +
			strlen( ".accounts" ) + 1 );
		
		strcpy( str, CONFIG );
		strcat( str, irc->nick );
		strcat( str, ".accounts" );
		
		checkie = access( str, F_OK );
		
		strcpy( str, CONFIG );
		strcat( str, irc->nick );
		strcat( str, ".nicks" );
		
		checkie += access( str, F_OK );
	
		if( checkie == -2 )
		{
			setpassnc( irc, cmd[1] );
			root_command_string( irc, user_find( irc, irc->mynick ), "save" );
		}
		else
		{
			irc_usermsg( irc, "Nick is already registered" );
		}
	}
	
	return( 0 );
}

int cmd_blist( irc_t *irc, char **cmd )
{
	int all = 0;
	user_t *u;
	char s[31];
	int n_online = 0, n_away = 0, n_offline = 0;
	
	if( cmd[1] && strcasecmp( cmd[1], "all" ) == 0 )
		all = 1;
	
	irc_usermsg( irc, "%-16.16s  %-40.40s  %s", "Nickname", "User/Host/Network", "Status" );
	
	for( u = irc->users; u; u = u->next ) if( u->gc && u->online && !u->away )
	{
		snprintf( s, 30, "%s@%s (%s)", u->user, u->host, proto_name[u->gc->user->protocol] );
		irc_usermsg( irc, "%-16.16s  %-40.40s  %s", u->nick, s, "Online" );
		n_online ++;
	}
	
	for( u = irc->users; u; u = u->next ) if( u->gc && u->online && u->away )
	{
		snprintf( s, 30, "%s@%s (%s)", u->user, u->host, proto_name[u->gc->user->protocol] );
		irc_usermsg( irc, "%-16.16s  %-40.40s  %s", u->nick, s, u->away );
		n_away ++;
	}
	
	if( all ) for( u = irc->users; u; u = u->next ) if( u->gc && !u->online )
	{
		snprintf( s, 30, "%s@%s (%s)", u->user, u->host, proto_name[u->gc->user->protocol] );
		irc_usermsg( irc, "%-16.16s  %-40.40s  %s", u->nick, s, "Offline" );
		n_offline ++;
	}
	
	irc_usermsg( irc, "%d buddies (%d available, %d away, %d offline)", n_online, n_away, n_offline );
	
	return( 0 );
}
