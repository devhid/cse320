#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "url.h"
#include "debug.h"

/*
 * Routines to interpret and manage URL's
 */

struct url {
    char *stuff;			/* Working storage containing all parts */
    char *method;			/* The access method (http, ftp, etc.) */
    char *hostname;		    /* The server host name */
    int port;			    /* The TCP port to contact */
    char *path;			    /* The path of the document on the server */
    int dnsdone;			/* Have we done DNS lookup yet? */
    struct in_addr addr;	/* IP address of the server */
};

/*
 * Parse a URL given as a string into its constituent parts,
 * and return it as a URL object.
 */

// Initialize all struct members to default values to get rid of uninitialized memory error.
void url_init(URL *url) {
    if(url != NULL) { // Added NULL check.
        url->stuff = NULL;
        url->method = NULL;
        url->hostname = NULL;
        url->port = 0;
        url->path = NULL;
        url->dnsdone = 0;
    }
}

URL *url_parse(char *url) {
    URL *up = NULL;
    char *slash, *colon, *cp, c;

    // Initialize local variables to default values.
    cp = slash = colon = NULL;
    c = 0;

    if((up = malloc(sizeof(*up))) == NULL) {
        free(up);
        return(NULL);
    }

    // We can initialize the member variables of the url now because we have allocated space.
    url_init(up);

    /*
     * Make a copy of the argument that we can fiddle with
     */

    if(url == NULL) {
        free(up);
        return(NULL);
    }

    if((up->stuff = strdup(url)) == NULL) {
         free(up);
         return(NULL);
    }

    up->dnsdone = 0;
    bzero(&up->addr, sizeof(struct in_addr));

    /*
     * Now ready to parse the URL
     */
    cp = up->stuff;
    slash = strchr(cp, '/');
    colon = strchr(cp, ':');

    if(slash != NULL && colon != NULL) { // Added null check for slash.
     /*
     * If a colon occurs before any slashes, then we assume the portion
     * of the URL before the colon is the access method.
     */
        if(colon < slash) {
            *colon = '\0';
            up->method = strdup(cp); // The method member needs space which we allocate using strdup.
            cp = colon+1;

            if(!strcasecmp(up->method, "http")) {
	           up->port = 80;
            }
        }

        if(*(slash+1) == '/') {
        /*
         * If there are two slashes, then we have a full, absolute URL,
         * and the following string, up to the next slash, colon or the end
         * of the URL, is the host name.
         */
            for(cp = slash+2; *cp != '\0' && *cp != ':' && *cp != '/'; cp++);
            c = *cp;
            *cp = '\0';

            up->hostname = strdup(slash+2); // Allocate space for host name.

            *cp = c;

            /*
             * If we found a ':', then we have to collect the port number
             */
            if(*cp == ':') {
	             char *cp1;
	             cp1 = ++cp;

                 while(isdigit(*cp)) {
	                cp++;
                 }

	             c = *cp;
	             *cp = '\0';
	             up->port = atoi(cp1);
	             *cp = c;
            }
        }

        if(*cp == '\0') {
            up->path = "/";
        }
        else {
            /*
             * No colon: a relative URL with no method or hostname
             */
            up->path = cp;
        }
    } else {
        up->path = cp; // Not sure what this means, but adding it fixed my crash/test case.
    }

    return(up);
}

/*
 * Free a URL object that was previously created by url_parse()
 */

void url_free(URL *up) {
    if(up != NULL) { // Added NULL check.
        free(up->stuff);
        if(up->method != NULL) {
            free(up->method);
        }

        if(up->hostname != NULL) {
            free(up->hostname);
        }

        free(up);
    }
}

/*
 * Extract the "access method" portion of a URL
 */

char *url_method(URL *up) {
    if(up == NULL) { // Added NULL check.
        return(NULL);
    }

    return(up->method);
}

/*
 * Extract the hostname portion of a URL
 */

char *url_hostname(URL *up) {
    if(up == NULL) { // Added NULL check.
        return(NULL);
    }

    return(up->hostname);
}

/*
 * Obtain the TCP port number portion of a URL
 */

int url_port(URL *up) {
    if(up == NULL) { // Added NULL check.
        return(-1);
    }

    return(up->port);
}

/*
 * Obtain the path portion of a URL
 */

char *url_path(URL *up) {
    if(up == NULL) { // Added NULL check.
        return(NULL);
    }

    return(up->path);
}

/*
 * Obtain the network (IP) address of the host specified in a URL.
 * This will cause a DNS lookup if the address is not already cached
 * in the URL object.
 */

IPADDR *url_address(URL *up) {
    struct hostent *he = NULL; // Safety initialization.

    if(up == NULL) { // If the URL is null, return NULL.
        return(NULL);
    }

    if(!up->dnsdone) {
        if(up->hostname != NULL && *up->hostname != '\0') {
            if((he = gethostbyname(up->hostname)) == NULL) {
	            return(NULL);
            }

            bcopy(he->h_addr, &up->addr, sizeof(struct in_addr));
        }
        up->dnsdone = 1;
    }

    return(&up->addr);
}
