/*
 * nogaim
 *
 * Gaim without gaim - for BitlBee
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * Copyright 2002 Wilmer van der Gaast <lintux@lintux.cx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "nogaim.h"

struct prpl *proto_prpl[16];
char proto_name[16][8] = { "TOC", "OSCAR", "", "ICQ", "MSN", "", "", "", "JABBER" };

static char *proto_away_alias[6][8] =
{
	{ "Away from computer", "Away", "Extended away", NULL },
	{ "NA", "N/A", "Not available", "Busy", "Do not disturb", "DND", "Occupied", NULL },
	{ "Be right back", "BRB", NULL },
	{ "On the phone", "Phone", NULL },
	{ "Out to lunch", "Lunch", NULL },
	{ NULL }
};
static char *proto_away_alias_find( GList *gcm, char *away );

GSList *connections;


/* nogaim.c */

void nogaim_init()
{
	proto_prpl[PROTO_MSN] = malloc( sizeof( struct prpl ) );
	memset( proto_prpl[PROTO_MSN], 0, sizeof( struct prpl ) );
	msn_init( proto_prpl[PROTO_MSN] );

	proto_prpl[PROTO_OSCAR] = malloc( sizeof( struct prpl ) );
	memset( proto_prpl[PROTO_OSCAR], 0, sizeof( struct prpl ) );
	oscar_init( proto_prpl[PROTO_OSCAR] );
	
	proto_prpl[PROTO_JABBER] = malloc( sizeof( struct prpl ) );
	memset( proto_prpl[PROTO_JABBER], 0, sizeof( struct prpl ) );
	jabber_init( proto_prpl[PROTO_JABBER] );
	
	set_add( IRC, "html", "nostrip", NULL );
}

void strip_html( char *msg )
{
	char *m, *s, *ch;
	int i = 1;
	
	m = s = malloc( strlen( msg ) + 1 );
	memset( m, 0, strlen( msg ) + 1 );
	for( ch = msg; *ch; ch ++ )
		if( *ch == '<' )
			i = 0;
		else if( *ch == '>' && !i )
			i = 1;
		else if( i )
			*(s++) = *ch;
	
	strcpy( msg, m );
	free( m );
} 

struct gaim_connection *gc_nr( int i )
{
	GSList *c = connections;
	
	while( c && i )
	{
		i --;
		c = c->next;
	}

	return( c?c->data:NULL );
}

int proto_away( struct gaim_connection *gc, char *away )
{
	GList *m;
	char *s;
	
	m = gc->prpl->away_states( gc );
	
	while( m )
	{
		if( away && *away )
		{
			if( strcasecmp( m->data, away ) == 0 )
				break;
		}
		else
		{
			if( strcasecmp( m->data, "Available" ) == 0 )
				break;
			if( strcasecmp( m->data, "Online" ) == 0 )
				break;
		}
		m = m->next;
	}
	
	if( m )
	{
		gc->prpl->set_away( gc, m->data, NULL );
	}
	else
	{
		s = proto_away_alias_find( m, away );
		if( s )
			gc->prpl->set_away( gc, s, NULL );
		else
			gc->prpl->set_away( gc, GAIM_AWAY_CUSTOM, away );
	}
	
	g_list_free( m );
	
	return( 1 );
}

static char *proto_away_alias_find( GList *gcm, char *away )
{
	GList *m;
	int i, j;
	
	for( i = 0; *proto_away_alias[i]; i ++ )
		for( j = 0; proto_away_alias[i][j]; j ++ )
			if( strcasecmp( away, proto_away_alias[i][j] ) == 0 )
				goto paaf_found;
	
	return( NULL );
	
	paaf_found:
	for( j = 0; proto_away_alias[i][j]; j ++ )
	{
		m = gcm;
		while( m )
		{
			if( strcasecmp( proto_away_alias[i][j], m->data ) == 0 )
				return( proto_away_alias[i][j] );
			m = m->next;
		}
	}
	
	return( NULL );		/* Shouldn't happen */
}


/* multi.c */

struct gaim_connection *new_gaim_conn( struct aim_user *user )
{
	struct gaim_connection *gc = g_new0( struct gaim_connection, 1 );
	gc->protocol = user->protocol;
	gc->prpl = proto_prpl[gc->protocol];
	g_snprintf( gc->username, sizeof( gc->username ), "%s", user->username );
	g_snprintf( gc->password, sizeof( gc->password ), "%s", user->password );
	gc->inpa = 0;
	gc->permit = NULL;
	gc->deny = NULL;
	gc->irc = IRC;

	connections = g_slist_append(connections, gc);

	user->gc = gc;
	gc->user = user;

	return( gc );
}

void destroy_gaim_conn( struct gaim_connection *gc )
{
	connections = g_slist_remove( connections, gc );
	g_free( gc->user );
	g_free( gc );
}

void set_login_progress( struct gaim_connection *gc, int step, char *msg )
{
	irc_usermsg( gc->irc, "%s - Logging in: %s", proto_name[gc->protocol], msg );
}

/* Errors *while* logging in */
void hide_login_progress( struct gaim_connection *gc, char *msg )
{
	irc_usermsg( gc->irc, "%s - Login error: %s", proto_name[gc->protocol], msg );
}

/* Errors *after* logging in */
void hide_login_progress_error( struct gaim_connection *gc, char *msg )
{
	irc_usermsg( gc->irc, "%s - Logged out: %s", proto_name[gc->protocol], msg );
}

static gboolean send_keepalive(gpointer d)
{
	struct gaim_connection *gc = d;
	if (gc->prpl && gc->prpl->keepalive)
		gc->prpl->keepalive(gc);
	return TRUE;
}

void account_online( struct gaim_connection *gc )
{
	nick_t *n = gc->irc->nicks;
	user_t *u = user_find( gc->irc, gc->irc->nick );
	
	irc_usermsg( gc->irc, "%s - Logged in", proto_name[gc->protocol] );
	
	gc->keepalive = g_timeout_add( 60000, send_keepalive, gc );
	gc->flags |= OPT_LOGGED_IN;
	
	if( gc->protocol == PROTO_OSCAR || gc->protocol == PROTO_ICQ || gc->protocol == PROTO_TOC )
		while( n )
		{
			if( n->proto == gc->protocol )
			{
				gc->prpl->add_buddy( gc, n->handle );
				add_buddy( gc, NULL, n->handle, NULL );
			}
			n = n->next;
		}
	
	if( u && u->away ) proto_away( gc, u->away );
}

void account_offline( struct gaim_connection *gc )
{
	signoff( gc );
}

void signoff( struct gaim_connection *gc )
{
	irc_t *irc = gc->irc;
	user_t *t, *u = irc->users;
	
	irc_usermsg( gc->irc, "%s - Signing off..", proto_name[gc->protocol] );

	gaim_input_remove( gc->keepalive );
	gc->keepalive = 0;
	gc->prpl->close( gc );
	gaim_input_remove( gc->inpa );
	
	while( u )
	{
		if( u->gc == gc )
		{
			t = u->next;
			user_del( irc, u->nick );
			u = t;
		}
		else
			u = u->next;
	}
	
	destroy_gaim_conn( gc );
}

/* dialogs.c */
void do_error_dialog( char *msg, char *title )
{
	irc_usermsg( IRC, "%s - Error: %s", title, msg );
}

void do_ask_dialog( char *msg, void *data, void *doit, void *dont )
{
	query_t *q = IRC->queries;
	
	if( q )
	{
		while( q->next ) q = q->next;
		q = q->next = malloc( sizeof( query_t ) );
	}
	else
	{
		IRC->queries = q = malloc( sizeof( query_t ) );
	}
	memset( q, 0, sizeof( query_t ) );
	
	q->question = strdup( msg );
	q->yes = doit;
	q->no = dont;
	q->data = data;
	
	if( q == IRC->queries )
	{
		irc_usermsg( IRC, "%s", q->question );
		irc_usermsg( IRC, "Type yes to accept or no to reject" );
	}
}

/* list.c */
int bud_list_cache_exists( struct gaim_connection *gc )
{
	return( 0 );
}

void do_import( struct gaim_connection *gc, void *null )
{
	return;
}

void add_buddy( struct gaim_connection *gc, char *group, char *handle, char *realname )
{
	user_t *u;
	char nick[MAX_NICK_LENGTH+1];
	char *s;
	
	memset( nick, 0, MAX_NICK_LENGTH + 1 );
	strcpy( nick, nick_get( gc->irc, handle, gc->protocol ) );
	
	u = user_add( gc->irc, nick );
	
	if( !realname || !*realname ) realname = nick;
	u->realname = strdup( realname );
	
	if( gc->protocol == PROTO_MSN )
	{
		s = strchr( handle, '@' );
		if( !s ) s = handle; else s ++;
		u->host = strdup( s );
		// u->host = malloc( 5 + strlen( s ) );
		// sprintf( u->host, "msn.%s", s );
		if( s > handle )
		{
			*(s-1) = 0;
			u->user = strdup( handle );
			*(s-1) = '@';
		}
		else
		{
			u->user = strdup( handle );
		}
	}
	else if( gc->protocol == PROTO_OSCAR || gc->protocol == PROTO_ICQ || gc->protocol == PROTO_TOC )
	{
		u->host = strdup( gc->user->proto_opt[0] );
		u->user = strdup( handle );
	}
	else if( gc->protocol == PROTO_JABBER )
	{
		s = strchr( handle, '@' );
		if( !s ) s = handle; else s ++;
		u->host = strdup( s );
		// u->host = malloc( 8 + strlen( s ) );
		// sprintf( u->host, "jabber.%s", s );
		if( s > handle )
		{
			*(s-1) = 0;
			u->user = strdup( handle );
			*(s-1) = '@';
		}
		else
		{
			u->user = strdup( handle );
		}
	}
	
	u->gc = gc;
	u->handle = strdup( handle );
	u->send_handler = buddy_send_handler;
}

struct buddy *find_buddy( struct gaim_connection *gc, char *handle )
{
	static struct buddy b[1];
	user_t *u = gc->irc->users;
	
	if( !gc ) return( NULL );
	
	while( u )
	{
		if( u->handle && u->gc == gc && ( strcmp( u->handle, handle ) == 0 ) )
			break;
		u = u->next;
	}
	if( !u ) return( NULL );

	memset( b, 0, sizeof( b ) );
	strncpy( b->name, handle, 80 );
	strncpy( b->show, u->realname, BUDDY_ALIAS_MAXLEN );
	b->present = u->online;
	b->gc = u->gc;
	
	return( b );
}

void do_export( struct gaim_connection *gc )
{
	return;
}

void signoff_blocked( struct gaim_connection *gc )
{
	return; /* Make all blocked users look invisible (TODO?) */
}


/* buddy.c */

void handle_buddy_rename( struct buddy *buddy, char *oldrealname )
{
	return;
}


/* buddy_chat.c */

void add_chat_buddy( struct conversation *b, char *handle )
{
	return;
}

void remove_chat_buddy( struct conversation *b, char *handle, char *reason )
{
	return;
}


/* prpl.c */

void show_got_added( struct gaim_connection *gc, char *id, char *handle, const char *realname, const char *msg )
{
	return;
}


/* server.c */                    

void serv_got_update( struct gaim_connection *gc, char *handle, int loggedin, int evil, time_t signon, time_t idle, int type, guint caps )
{
	user_t *u = gc->irc->users;
	
	while( u->next )
	{
		if( u->handle && u->gc == gc && ( strcmp( u->handle, handle ) == 0 ) )
			break;
		u = u->next;
	}
	if( !u )
	{
		irc_usermsg( gc->irc, "serv_got_update() for unknown handle %s:", handle );
		irc_usermsg( gc->irc, "loggedin = %d, type = %d", loggedin, type );
		return;
	}
	
	if( loggedin && !u->online )
	{
		irc_spawn( gc->irc, u );
		u->online = 1;
	}
	else if( !loggedin && u->online )
	{
		irc_kill( gc->irc, u );
		u->online = 0;
	}
	
	if( ( type & UC_UNAVAILABLE ) && ( gc->protocol == PROTO_MSN ) )
	{
		if( type & ( MSN_BUSY << 1 ) )
			u->away = "Busy";
		else if( type & ( MSN_IDLE << 1 ) )
			u->away = "Idle";
		else if( type & ( MSN_BRB << 1 ) )
			u->away = "Be right back";
		else if( type & ( MSN_PHONE << 1 ) )
			u->away = "On the phone";
		else if( type & ( MSN_LUNCH << 1 ) )
			u->away = "Out to lunch";
		else // if( type & ( MSN_AWAY << 1 ) )
			u->away = "Away from the computer";
	}
	else if( ( type & UC_UNAVAILABLE ) && ( gc->protocol == PROTO_OSCAR || gc->protocol == PROTO_ICQ || gc->protocol == PROTO_TOC ) )
	{
		u->away = "Away";
	}
	else if( ( type & UC_UNAVAILABLE ) && ( gc->protocol == PROTO_JABBER) )
	{
		if( type & UC_DND )
			u->away = "Do Not Disturb";
		else if( type & UC_XA )
			u->away = "Extended Away";
		else // if( type & UC_AWAY )
			u->away = "Away";
	}
	else
		u->away = NULL;
	
	return;
}

void serv_got_im( struct gaim_connection *gc, char *handle, char *msg, guint32 flags, time_t mtime, gint len )
{
	irc_t *irc = gc->irc;
	user_t *u = irc->users;
	
	while( u )
	{
		if( u->handle && u->gc == gc && ( strcmp( u->handle, handle ) == 0 ) )
			break;
		u = u->next;
	}
	if( !u )
	{
		irc_usermsg( irc, "Message from unknown %s handle %s:", proto_name[gc->protocol], handle );
		u = user_find( irc, irc->mynick );
	}
	
	if( strcasecmp( set_getstr( gc->irc, "html" ), "strip" ) == 0 ) strip_html( msg );
	irc_msgfrom( irc, u->nick, msg );
	
	return;
}

void serv_got_typing( struct gaim_connection *gc, char *handle, int timeout )
{
	return;
}

void serv_got_chat_left( struct gaim_connection *gc, int id )
{
	return;
}

void serv_got_chat_in( struct gaim_connection *gc, int id, char *who, int whisper, char *msg, time_t mtime )
{
	return;
}

struct conversation *serv_got_joined_chat( struct gaim_connection *gc, int id, char *handle )
{
	return( NULL );
}

void serv_finish_login( struct gaim_connection *gc )
{
	return;
}

/* prefs.c */

/* Necessary? */
void build_block_list()
{
	return;
}

void build_allow_list()
{
	return;
}
