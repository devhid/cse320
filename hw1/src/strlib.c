#include <stdlib.h>
#include <stdio.h>
#include "strlib.h"

/**
    Some of the important string functions in Java reimplemented
    in C to use for CSE320 homework assignments.

    @author Mikey G
**/

int length(char *string) {
    char *ptr;
    int length;

    ptr = string; // equivalent to: ptr = &string[0];
    length = 0;

    while(*ptr++ != '\0') {
        length++;
    }

    return length;
}

char char_at(char *string, int index) {
    if(index < 0 || index >= length(string)) {
        printf("Error (char_at): Index out of bounds: %i\n", index);
        exit(EXIT_FAILURE);
    }

    char ch;
    char *ptr;
    int counter;

    ptr = string;
    counter = 0;

    while(*ptr != '\0') {
        if(counter == index) {
            ch = *ptr;
        }

        ptr++;
        counter++;
    }

    return ch;
}

int index_of(char *string, char ch) {
    char *ptr;
    int index, count;

    ptr = string;
    index = -1;
    count = 0;

    while(*ptr != '\0') {
        if(*ptr == ch) {
            index = count;
            break;
        }

        ptr++;
        count++;
    }

    return index;
}

int equals(char *string, char *compare) {
    int index = 0;

    while(string[index] == compare[index]) {
        if(string[index] != '\0' && compare[index] != '\0') {
            index++;
        } else {
            break;
        }
    }

    if(string[index] == '\0' && compare[index] == '\0') {
        return 1;
    } else {
        return 0;
    }
}

int equals_n(char *string, char *compare) {
    int index = 0;

    while(string[index] == compare[index]) {
        if(string[index] != '\0' && compare[index] != '\0') {
            index++;
        } else {
            break;
        }
    }

    if( (string[index] == '\0' && compare[index] == '\0') || (string[index] == '\n' && compare[index] == '\0') ) {
        return 1;
    } else {
        return 0;
    }
}
