typedef struct
{
	char *handle;
	int proto;
	char *nick;
	void *next;
} nick_t;

void nick_set( irc_t *irc, char *handle, int proto, char *nick );
char *nick_get( irc_t *irc, char *handle, int proto );
void nick_strip( char *nick );

int nick_ok( char *nick );
int nick_lc( char *nick );
int nick_uc( char *nick );
int nick_cmp( char *a, char *b );
