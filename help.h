typedef union
{
	off_t file_offset;
	char *mem_offset;
} help_off_t;

typedef struct
{
	int fd;
	time_t mtime;
	char *string;
	help_off_t offset;
	int length;
	void *next;
} help_t;

help_t *help_init( irc_t *irc );
char *help_get( irc_t *irc, char *string );

