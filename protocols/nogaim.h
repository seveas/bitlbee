  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2003 Wilmer van der Gaast and others                *
  \********************************************************************/

/*
 * nogaim
 *
 * Gaim without gaim - for BitlBee
 *
 * This file contains functions called by the Gaim IM-modules. It's written
 * from scratch for BitlBee and doesn't contain any code from Gaim anymore
 * (except for the function names).
 *
 * This include file contains some struct and type definitions from Gaim.
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * Copyright 2002-2003 Wilmer van der Gaast <lintux@lintux.cx>
 */

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

#ifndef _NOGAIM_H
#define _NOGAIM_H

#include "bitlbee.h"
#include "proxy.h"
#include "md5.h"

#define BUF_LEN MSG_LEN
#define BUF_LONG ( BUF_LEN * 2 )
#define MSG_LEN 2048
#define BUF_LEN MSG_LEN

#define SELF_ALIAS_LEN 400
#define BUDDY_ALIAS_MAXLEN 388   /* because MSN names can be 387 characters */

#define PERMIT_ALL      1
#define PERMIT_NONE     2
#define PERMIT_SOME     3
#define DENY_SOME       4

#define WEBSITE "http://www.lintux.cx/bitlbee.html"
#define IM_FLAG_AWAY 0x0020
#define OPT_CONN_HTML 0x00000001
#define OPT_LOGGED_IN 0x00010000
#define GAIM_AWAY_CUSTOM "Custom"

/* ok. now the fun begins. first we create a connection structure */
struct gaim_connection {
	/* we need to do either oscar or TOC */
	/* we make this as an int in case if we want to add more protocols later */
	int protocol;
	struct prpl *prpl;
	guint32 flags;
	
	/* all connections need an input watcher */
	int inpa;
	
	/* buddy list stuff. there is still a global groups for the buddy list, but
	 * we need to maintain our own set of buddies, and our own permit/deny lists */
	GSList *permit;
	GSList *deny;
	int permdeny;
	
	/* all connections need a list of chats, even if they don't have chat */
	GSList *buddy_chats;
	
	/* each connection then can have its own protocol-specific data */
	void *proto_data;
	
	struct aim_user *user;
	
	char username[64];
	char displayname[128];
	char password[32];
	guint keepalive;
	/* stuff needed for per-connection idle times */
	guint idle_timer;
	time_t login_time;
	time_t lastsent;
	int is_idle;
	
	char *away;
	int is_auto_away;
	
	int evil;
	gboolean wants_to_die; /* defaults to FALSE */
	
	/* BitlBee */
	irc_t *irc;
	
	struct conversation *conversations;
};

/* struct buddy_chat went away and got merged with this. */
struct conversation {
	struct gaim_connection *gc;

	/* stuff used just for chat */
        GList *in_room;
        GList *ignored;
        int id;
        
        /* BitlBee */
        struct conversation *next;
        char *channel;
        char *title;
        char joined;
};

struct buddy {
	char name[80];
	char show[BUDDY_ALIAS_MAXLEN];
        int present;
	int evil;
	time_t signon;
	time_t idle;
        int uc;
	guint caps; /* woohoo! */
	void *proto_data; /* what a hack */
	struct gaim_connection *gc; /* the connection it belongs to */
};

struct aim_user {
	char username[64];
	char alias[SELF_ALIAS_LEN]; 
	char password[32];
	char user_info[2048];
	int options;
	int protocol;
	/* prpls can use this to save information about the user,
	 * like which server to connect to, etc */
	char proto_opt[7][256];

	struct gaim_connection *gc;
};

struct prpl {
	int protocol;
	int options;
	char *(* name)();

	/* for ICQ and Yahoo, who have off/on per-conversation options */
	/* char *checkbox; this should be per-connection */

	GList *(* away_states)(struct gaim_connection *gc);
	GList *(* actions)();
	void   (* do_action)(struct gaim_connection *, char *);
	/* user_opts returns a GList* of g_malloc'd struct proto_user_opts */
	GList *(* user_opts)();
	GList *(* chat_info)(struct gaim_connection *);

	/* all the server-related functions */

	/* a lot of these (like get_dir) are protocol-dependent and should be removed. ones like
	 * set_dir (which is also protocol-dependent) can stay though because there's a dialog
	 * (i.e. the prpl says you can set your dir info, the ui shows a dialog and needs to call
	 * set_dir in order to set it) */

	void (* login)		(struct aim_user *);
	void (* close)		(struct gaim_connection *);
	int  (* send_im)	(struct gaim_connection *, char *who, char *message, int len, int away);
	void (* set_info)	(struct gaim_connection *, char *info);
	void (* get_info)	(struct gaim_connection *, char *who);
	void (* set_away)	(struct gaim_connection *, char *state, char *message);
	void (* get_away)       (struct gaim_connection *, char *who);
	void (* set_idle)	(struct gaim_connection *, int idletime);
	void (* add_buddy)	(struct gaim_connection *, char *name);
	void (* add_buddies)	(struct gaim_connection *, GList *buddies);
	void (* remove_buddy)	(struct gaim_connection *, char *name, char *group);
	void (* remove_buddies)	(struct gaim_connection *, GList *buddies, char *group);
	void (* add_permit)	(struct gaim_connection *, char *name);
	void (* add_deny)	(struct gaim_connection *, char *name);
	void (* rem_permit)	(struct gaim_connection *, char *name);
	void (* rem_deny)	(struct gaim_connection *, char *name);
	void (* set_permit_deny)(struct gaim_connection *);
	void (* join_chat)	(struct gaim_connection *, GList *data);
	void (* chat_invite)	(struct gaim_connection *, int id, char *who, char *message);
	void (* chat_leave)	(struct gaim_connection *, int id);
	void (* chat_whisper)	(struct gaim_connection *, int id, char *who, char *message);
	int  (* chat_send)	(struct gaim_connection *, int id, char *message);
	void (* keepalive)	(struct gaim_connection *);

	/* get "chat buddy" info and away message */
	void (* get_cb_info)	(struct gaim_connection *, int, char *who);
	void (* get_cb_away)	(struct gaim_connection *, int, char *who);

	/* save/store buddy's alias on server list/roster */
	void (* alias_buddy)	(struct gaim_connection *, char *who);

	/* change a buddy's group on a server list/roster */
	void (* group_buddy)	(struct gaim_connection *, char *who, char *old_group, char *new_group);

	void (* buddy_free)	(struct buddy *);

	/* this is really bad. */
	void (* convo_closed)   (struct gaim_connection *, char *who);
	int  (* convo_open)     (struct gaim_connection *, char *who);

	char *(* normalize)(const char *);
};

#define PROTO_TOC	0
#define PROTO_OSCAR	1
#define PROTO_YAHOO	2
#define PROTO_ICQ	3
#define PROTO_MSN	4
#define PROTO_IRC	5
#define PROTO_FTP	6
#define PROTO_VGATE	7
#define PROTO_JABBER	8
#define PROTO_NAPSTER	9
#define PROTO_ZEPHYR	10
#define PROTO_GADUGADU	11
#define PROTO_MAX	16

extern char proto_name[PROTO_MAX][8];

#define UC_UNAVAILABLE  1

#define MSN_ONLINE  1
#define MSN_BUSY    2
#define MSN_IDLE    3
#define MSN_BRB     4
#define MSN_AWAY    5
#define MSN_PHONE   6
#define MSN_LUNCH   7
#define MSN_OFFLINE 8
#define MSN_HIDDEN  9

/* JABBER */
#define UC_AWAY (0x02 | UC_UNAVAILABLE)
#define UC_CHAT  0x04
#define UC_XA   (0x08 | UC_UNAVAILABLE)
#define UC_DND  (0x10 | UC_UNAVAILABLE)

extern GSList *connections;
extern struct prpl *proto_prpl[16];

/* nogaim.c */
void nogaim_init();
struct gaim_connection *gc_nr( int i );
int proto_away( struct gaim_connection *gc, char *away );

/* multi.c */
struct gaim_connection *new_gaim_conn( struct aim_user *user );
void destroy_gaim_conn( struct gaim_connection *gc );
void set_login_progress( struct gaim_connection *gc, int step, char *msg );
void hide_login_progress( struct gaim_connection *gc, char *msg );
void hide_login_progress_error( struct gaim_connection *gc, char *msg );
void account_online( struct gaim_connection *gc );
void account_offline( struct gaim_connection *gc );
void signoff( struct gaim_connection *gc );

/* dialogs.c */
void do_error_dialog( char *msg, char *title );
void do_ask_dialog( char *msg, void *data, void *doit, void *dont );

/* list.c */
int bud_list_cache_exists( struct gaim_connection *gc );
void do_import( struct gaim_connection *gc, void *null );
void add_buddy( struct gaim_connection *gc, char *group, char *handle, char *realname );
struct buddy *find_buddy( struct gaim_connection *gc, char *handle );
void do_export( struct gaim_connection *gc );
void signoff_blocked( struct gaim_connection *gc );

/* buddy.c */
void handle_buddy_rename( struct buddy *buddy, char *handle );

/* buddy_chat.c */
void add_chat_buddy( struct conversation *b, char *handle );
void remove_chat_buddy( struct conversation *b, char *handle, char *reason );

/* prpl.c */
void show_got_added( struct gaim_connection *gc, char *id, char *handle, const char *realname, const char *msg );

/* server.c */                    
void serv_got_update( struct gaim_connection *gc, char *handle, int loggedin, int evil, time_t signon, time_t idle, int type, guint caps );
void serv_got_im( struct gaim_connection *gc, char *handle, char *msg, guint32 flags, time_t mtime, gint len );
void serv_got_typing( struct gaim_connection *gc, char *handle, int timeout );
void serv_got_chat_invite( struct gaim_connection *gc, char *handle, char *who, char *msg, GList *data );
struct conversation *serv_got_joined_chat( struct gaim_connection *gc, int id, char *handle );
void serv_got_chat_in( struct gaim_connection *gc, int id, char *who, int whisper, char *msg, time_t mtime );
void serv_got_chat_left( struct gaim_connection *gc, int id );
// void serv_finish_login( struct gaim_connection *gc );

/* util.c */
unsigned char *utf8_to_str( unsigned char *in );
char *str_to_utf8( unsigned char *in );
void strip_linefeed( gchar *text );
char *add_cr( char *text );
char *tobase64( const char *text );
char *normalize( const char *s );
time_t get_time( int year, int month, int day, int hour, int min, int sec );
void strip_html( char *msg );

/* msn.c */
void msn_init( struct prpl *ret );

/* oscar.c */
void oscar_init( struct prpl *ret );

/* jabber.c */
void jabber_init( struct prpl *ret );

/* yahoo.c */
void yahoo_init( struct prpl *ret );

/* prefs.c */
void build_block_list();
void build_allow_list();

struct conversation *conv_findchannel( char *channel );

#endif
