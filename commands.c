#include "commands.h"
#include "bitlbee.h"

command_t commands[] = {
	{ "help", "This information", 0, cmd_help}, 
	{ "identify", "identify <password>", 1, cmd_identify },
	{ "register", "register <password>", 1, cmd_register },
	{ "login", "login <protocol> <username> <password> [<server>]", 3, cmd_login},
	{ "logout",  "logout <protocol>", 1, cmd_logout }, 
	{ "slist", "Server (connection) list", 0, cmd_slist },
	{ "add","add <connection> <handle>", 2, cmd_add },
	{ "rename", "rename <oldnick> <newnick>", 2, cmd_rename },
	{ "remove", "remove <nick>", 1, cmd_remove },
	{ "block", "block <connection> <handle> or block <nick>", 1, cmd_block },
	{ "allow", "allow <connection> <handle> or allow <nick>", 1, cmd_allow },
	{ "save", "Save configuration", 0, cmd_save },
	{ "set", "Set configuration option", 0, cmd_set },
	{ "yes", "Accept request", 0, cmd_yesno },
	{ "no", "Deny request", 0, cmd_yesno },
	{ NULL }
};

int cmd_help( irc_t *irc, char **cmd )
{
	int i;
	
	irc_usermsg( irc, "BitlBee " VERSION " help:" );
	irc_usermsg( irc, " " );
	irc_usermsg( irc, "Commands:" );

	for( i = 0; commands[i].command; i++ )
	{
		irc_usermsg( irc, "  %s    - %s", commands[i].command, commands[i].description );
	}
	return( 1 );
}

int cmd_login( irc_t *irc, char **cmd )
{
	struct aim_user *u;
	int prot = -1;

	if( strcasecmp( cmd[1], "msn" ) == 0 )
		prot = PROTO_MSN;
	else if( strcasecmp( cmd[1], "oscar" ) == 0 )
		prot = PROTO_OSCAR;
	else if( strcasecmp( cmd[1], "jabber" ) == 0 )
		prot = PROTO_JABBER;
	else
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

int cmd_rename( irc_t *irc, char **cmd)
{
	user_t *u;

	if( ( strcasecmp( cmd[1], irc->nick ) == 0 ) || ( strcasecmp( cmd[1], irc->mynick ) == 0 ) )
	{
		irc_usermsg( irc, "Nick '%s' can't be changed", cmd[1] );
		return( 1 );
	}
	if( user_find( irc, cmd[2] ) )
	{
		irc_usermsg( irc, "Nick '%s' already exists", cmd[2] );
		return( 1 );
	}
	if( !( u = user_find( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Nick '%s' does not exist", cmd[1] );
		return( 1 );
	}
	free( u->nick );
	u->nick = strdup( cmd[2] );
	irc_write( irc, ":%s!%s@%s NICK %s", cmd[1], u->user, u->host, cmd[2] );
	nick_set( irc, u->handle, ((struct gaim_connection *)u->gc)->protocol, cmd[2] );
	
	return( 0 );
}

int cmd_remove( irc_t *irc, char **cmd )
{
	user_t *u;

	if( !( u = user_find( irc, cmd[1] ) ) )
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
		if( !u )
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
		if( !u )
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
		irc_usermsg( irc, "%s = `%s'", cmd[1], s );
	}
	else
	{
		set_t *s = irc->set;
		while( s )
		{
			irc_usermsg( irc, "%s = `%s'", s->key, s->value?s->value:s->def );
			s = s->next;
		}
	}
	
	return( 0 );
}

int cmd_save( irc_t *irc, char **cmd )
{
	if( save_config( irc ) )
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
		int checkie = bitlbee_init( irc, cmd[1] );
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
}

int cmd_register( irc_t *irc, char **cmd )
{
	if( !cmd[1] )
	{
		irc_usermsg( irc, "Syntax: register <password>" );
	}
	else
	{
		int checkie = bitlbee_init( irc, cmd[1] ); /* Hmmm.... Not nice.. */
		if( checkie == 0 )
		{
			setpassnc( cmd[1] );
			root_command( irc, "save" );
		}
		else
		{
			irc_usermsg( irc, "Nick is already registered" );
		}
	}
}
