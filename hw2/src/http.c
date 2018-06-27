/*
 * Routines for making a HTTP connection to a server on the Internet,
 * sending a simple HTTP GET request, and interpreting the response headers
 * that come back.
 *
 * E. Stark, 11/18/97 for CSE 230
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/socket.h>
#include <assert.h>

#include "debug.h"
#include "url.h"
#include "http.h"

typedef struct HDRNODE *HEADERS;
HEADERS http_parse_headers(HTTP *http);
void http_free_headers(HEADERS env);

/*
 * Routines to manage HTTP connections
 */

typedef enum { ST_REQ, ST_HDRS, ST_BODY, ST_DONE } HTTP_STATE;

struct http {
    FILE *file;             /* Stream to remote server */
    HTTP_STATE state;		/* State of the connection */
    int code;			    /* Response code */
    char version[4];		/* HTTP version from the response */
    char *response;		    /* Response string with message */
    HEADERS headers;		/* Reply headers */
};

/*
 * Open an HTTP connection for a specified IP address and port number
 */

HTTP * http_open(IPADDR *addr, int port) {
    HTTP *http = NULL; // Safety initialization.
    struct sockaddr_in sa = {0}; // Safety initialization.
    int sock = 0; // Safety initialization.

    if(addr == NULL) {
        return(NULL);
    }

    if((http = malloc(sizeof(*http))) == NULL) {
        return(NULL);
    }

    bzero(http, sizeof(*http));
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        free(http);
        return(NULL);
    }

    bzero(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    bcopy(addr, &sa.sin_addr.s_addr, sizeof(struct in_addr));
    if(connect(sock, (struct sockaddr *)(&sa), sizeof(sa)) < 0 || (http->file = fdopen(sock, "w+")) == NULL) {
        free(http);
        close(sock);
        return(NULL);
    }

    http->state = ST_REQ;

    return(http);
}

/*
 * Close an HTTP connection that was previously opened.
 */

int http_close(HTTP *http) {
    if(http == NULL) { // Added NULL check.
        return(EOF);
    }

    int err = 0; // Safety initialization.

    http_free_headers(http->headers);

    err = fclose(http->file);
    free(http->response);
    free(http);

    return(err);
}

/*
 * Obtain the underlying FILE in an HTTP connection.
 * This can be used to issue additional headers after the request.
 */

FILE * http_file(HTTP *http) {
    if(http == NULL) { // Added NULL check.
        return(NULL);
    }

    return(http->file);
}

/*
 * Issue an HTTP GET request for a URL on a connection.
 * After calling this function, the caller may send additional
 * headers, if desired, then must call http_response() before
 * attempting to read any document returned on the connection.
 */

int http_request(HTTP *http, URL *up) {
    void *prev = NULL; // Safety initialization.

    if(http == NULL || up == NULL) { // Added NULL check.
        return(1);
    }

    if(http->state != ST_REQ) {
        return(1);
    }

    /* Ignore SIGPIPE so we don't die while doing this */
    prev = signal(SIGPIPE, SIG_IGN);
    if(fprintf(http->file, "GET %s://%s:%d%s HTTP/1.0\r\nHost: %s\r\n",
	   url_method(up), url_hostname(up), url_port(up),
	   url_path(up), url_hostname(up)) == -1) {
           signal(SIGPIPE, prev);
           return(1);
    }

    http->state = ST_HDRS;
    signal(SIGPIPE, prev);
    return(0);
}

/*
 * Finish outputting an HTTP request and read the reply
 * headers from the response.  After calling this, http_getc()
 * may be used to collect any document returned as part of the
 * response.
 */

int http_response(HTTP *http) {
    void *prev = NULL; // Safety initialization.
    char *response = NULL; // Safety initialization.
    size_t len = 0; // Change type of len to size_t because getline() takes in a size_t parameter.

    if(http == NULL) { // Added NULL check.
        return(1);
    }

    if(http->state != ST_HDRS) {
        return(1);
    }

    /* Ignore SIGPIPE so we don't die while doing this */
    prev = signal(SIGPIPE, SIG_IGN);
    if(fprintf(http->file, "\r\n") == -1 || fflush(http->file) == EOF) {
        signal(SIGPIPE, prev);
        return(1);
    }

    rewind(http->file);
    signal(SIGPIPE, prev);

    len = getline(&response, &len, http->file); // Returns the number of lines read and stores the lines in response. |+

    if(response == NULL || (http->response = malloc(len+1)) == NULL) {
        return(1);
    }

    if((int) len < 0) {
        free(response);
        return(1);
    }

    strncpy(http->response, response, len);
    free(response);

    do {
        http->response[len--] = '\0';
    } while( (int) len >= 0 && (http->response[len] == '\r' || http->response[len] == '\n')); // |+

    if(sscanf(http->response, "HTTP/%3s %d ", http->version, &http->code) != 2) {
        return(1);
    }

    http->headers = http_parse_headers(http);
    http->state = ST_BODY;

    return(0);
}

/*
 * Retrieve the HTTP status line and code returned as the
 * first line of the response from the server
 */

char * http_status(HTTP *http, int *code) {
    if(http == NULL || code == NULL) { // Added NULL check.
        return(NULL);
    }

    if(http->state != ST_BODY) {
        return(NULL);
    }

    if(code != NULL) {
        *code = http->code;
    }

    return(http->response);
}

/*
 * Read the next character of a document from an HTTP connection
 */

int http_getc(HTTP *http) {
    if(http == NULL) { // Added NULL check.
        return(EOF);
    }

    if(http->state != ST_BODY) {
        return(EOF);
    }

    return(fgetc(http->file));
}

/*
 * Routines for parsing the RFC822-style headers that come back
 * as part of the response to an HTTP request.
 */

typedef struct HDRNODE {
    char *key;
    char *value;
    struct HDRNODE *next;
} HDRNODE;

/*
 * Function for parsing RFC 822 header lines directly from input stream.
 */

HEADERS http_parse_headers(HTTP *http) {
    if(http == NULL) { // Added NULL check.
        return(NULL);
    }

    FILE *f = http->file;
    HEADERS env = NULL, last = NULL; // Safety initialization.
    HDRNODE *node = NULL; // Safety initialization.
    ssize_t read = 0; // Safety initialization.
    size_t len = 0; // Safety initialization.
    char *line, *l, *ll, *cp;
    line = l = ll = cp = NULL; // Safety initialization.

    while((read = getline(&ll, &len, f)) != -1) { // Store the number of lines read in "read".
        len = read; // Set the number of lines read to len.
        line = l = malloc(len+1);
    	l[len] = '\0';
    	strncpy(l, ll, len);

    	while((int) len > 0 && (l[len-1] == '\n' || l[len-1] == '\r')) { // |+
    	   l[--len] = '\0';
        }

    	if(len == 0) {
    	    free(line);
    	    break;
    	}

    	node = malloc(sizeof(HDRNODE)); // Allocate space for the new node.
    	node->next = NULL; // Set the node's next to null because there is no "next" yet.

        if(last != NULL) { // If the last node in the linked list isn't null, set the next node of last to node.
            last->next = node;
        } else { // Else, the head of the linked list equals the newly allocated node.
            env = node;
        }

        for(cp = l; *cp == ' '; cp++); // Increase cp until we reach a space.
    	l = cp;

        for( ; *cp != ':' && *cp != '\0'; cp++); // Increase cp until we reach a null terminator.

        if(*cp == '\0' || *(cp+1) != ' ') {
    	    free(line);
    	    free(node);
    	    continue;
    	}

    	*cp++ = '\0';
    	node->key = strdup(l);

    	while(*cp == ' ') {
    	    cp++;
        }

    	node->value = strdup(cp);

    	for(cp = node->key; *cp != '\0'; cp++) { // We want to check when we reach a null terminator, not NULL.
    	    /*if(isupper(*cp)) {
    		     *cp = tolower(*cp);
             }*/
        }

    	last = node; // We reset last or the "tail" of the linked list to be the newly allocated node.
    	node = node->next; // Set the node equal to it's next node.

    	free(line); // Free the line. Not sure why we even need this variable though.
    }

    free(ll); // Free ll. Again, not sure why we really need this.
    return(env); // Return a pointer to the head of the linked list.
}

/*
 * Free headers previously created by http_parse_headers()
 */

void http_free_headers(HEADERS env) {
    if(env != NULL) { // Added NULL check.
        HEADERS next = NULL; // Safety initialization.

        while(env != NULL) {
    	    free(env->key);
    	    free(env->value);
    	    next = env->next;
    	    free(env);
    	    env = next;
        }
    }
}

/*
 * Find the value corresponding to a given key in the headers
 */

char * http_headers_lookup(HTTP *http, char *key) {
    if(http == NULL || key == NULL) { // Added NULL check.
        return(NULL);
    }
    HEADERS env = http->headers;
    while(env != NULL) {
	    if(!strcasecmp(env->key, key)) // Change to strcasecmp because we want to match the key regardless of the case.
	       return(env->value);
	    env = env->next;
    }

    return(NULL);
}

char * http_header_key(HTTP *http, char *key) {
    if(http == NULL || key == NULL) { // Added NULL check.
        return(NULL);
    }
    HEADERS env = http->headers;
    while(env != NULL) {
	    if(!strcasecmp(env->key, key)) {
            return(env->key);
        }
	    env = env->next;
    }

    return(NULL);
}
