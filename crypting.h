void setpassnc (irc_t *irc, char *pass); /* USE WITH CAUTION! */
char *passchange (irc_t *irc, void *set, char *value);
int setpass (irc_t *irc, char *pass, char* md5sum);
char *hashpass (irc_t *irc);
char *obfucrypt (irc_t *irc, char *line);
char *deobfucrypt (irc_t *irc, char *line);
