void setpassnc (char *pass); /* USE WITH CAUTION! */
int setpass (char *pass, char* md5sum);
char *hashpass ();
char *obfucrypt (char *line);
char *deobfucrypt (char *line);
