typedef struct
{
	char *key;
	char *value;
	char *def;	/* Default */
	/* Eval: Returns NULL if the value is incorrect. Can return a
	   corrected value. set_setstr() should be able to free() this
	   string! */
	char *(*eval) ( irc_t *irc, void *set, char *value );
	void *next;
} set_t;

set_t *set_add( irc_t *irc, char *key, char *def, void *eval );
set_t *set_find( irc_t *irc, char *key );
char *set_getstr( irc_t *irc, char *key );
int set_getint( irc_t *irc, char *key );
int set_setstr( irc_t *irc, char *key, char *value );
int set_setint( irc_t *irc, char *key, int value );
void set_del( irc_t *irc, char *key );
char *set_eval_int( irc_t *irc, set_t *set, char *value );
char *set_eval_bool( irc_t *irc, set_t *set, char *value );
