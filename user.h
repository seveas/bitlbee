typedef struct user
{
	char *nick;
	char *user;
	char *host;
	char *realname;
	
	char *away;
	
	char private;
	char online;
	
	char *handle;
	struct gaim_connection *gc;
	
	int (*send_handler) ( irc_t *irc, struct user *u, char *msg );
	
	void *next;
} user_t;

user_t *user_add( struct irc *irc, char *nick );
int user_del( irc_t *irc, char *nick );
user_t *user_find( irc_t *irc, char *nick );
