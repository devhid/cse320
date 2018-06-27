#ifndef STRLIB_H_
#define STRLIB_H_

int length(char *string);

char char_at(char *string, int index);

int index_of(char *string, char ch);

int equals(char *string, char *compare);

int equals_n(char *string, char *compare);

#endif // STRLIB_H_