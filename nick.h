typedef struct
{
	char *handle;
	int proto;
	char *nick;
	void *next;
} nick_t;

void nick_set( irc_t *irc, char *handle, int proto, char *nick );
char *nick_get( irc_t *irc, char *handle, int proto );
