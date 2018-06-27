/*
 * Example of the use of the HTTP package for "Internet Bumbler"
 * E. Stark, 11/18/97 for CSE 230
 *
 * This program takes a single argument, which is interpreted as the
 * URL of a document to be retrieved from the Web.  It uses the HTTP
 * package to parse the URL, make an HTTP connection to the remote
 * server, retrieve the document, and display the result code and string,
 * the response headers, and the data portion of the document.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "debug.h"
#include "http.h"
#include "url.h"
#include "snarf.h"

int main(int argc, char *argv[]) {
    URL *up = NULL; // Safety initialization.
    HTTP *http = NULL; // Safety initialization.
    IPADDR *addr = NULL; // Safety initialization.

    int port, c, code;
    port = c = 0; // Safety initialization.
    code = -1; // Safety initialization.

    char *status, *method;
    status = method = NULL; // Safety initialization.

    parse_args(argc, argv);
    if((up = url_parse(url_to_snarf)) == NULL) {
        fprintf(stderr, "Illegal URL: '%s'\n", argv[1]);
        url_free(up);
        exit(-1); // Exit with -1 because the URL was null.
    }

    method = url_method(up);
    addr = url_address(up);
    port = url_port(up);
    if(method == NULL || strcasecmp(method, "http")) {
        fprintf(stderr, "Only HTTP access method is supported\n");
        url_free(up);
        exit(-1); // Exit with -1 because the method is not provided or the method isn't http.
    }

    if((http = http_open(addr, port)) == NULL) {
        fprintf(stderr, "Unable to contact host '%s', port %d\n",
	    url_hostname(up) != NULL ? url_hostname(up) : "(NULL)", port);
        url_free(up);
        exit(-1); // Exit with -1 because the link might not exist (which is why we cannot contact it).
    }

    http_request(http, up);
  /*
   * Additional RFC822-style headers can be sent at this point,
   * if desired, by outputting to url_file(up).  For example:
   *
   *     fprintf(url_file(up), "If-modified-since: 10 Jul 1997\r\n");
   *
   * would activate "Conditional GET" on most HTTP servers.
   */
    http_response(http);
  /*
   * At this point, response status and headers are available for querying.
   *
   * Some of the possible HTTP response status codes are as follows:
   *
   *	200	Success -- document follows
   *	302	Document moved
   *	400	Request error
   *	401	Authentication required
   *	403	Access denied
   *	404	Document not found
   *	500	Server error
   *
   * You probably want to examine the "Content-type" http_status_code(http)header.
   * Two possibilities are:
   *    text/html	The body of the document is HTML code
   *	text/plain	The body of the document is plain ASCII text
   */
    status = http_status(http, &code);

#ifdef DEBUG
    debug("%s", status);
#else
    (void) status;
#endif

    if(keyPtr != NULL) {
        if(keyPtr[strlen(keywords) - 1] == ' ') {
            keyPtr[strlen(keywords) - 1] = '\0'; // There's an extra space at the end so we null terminate it.
        }

        char *key = NULL;
        char *token = NULL;

        char *search = " "; // Searching for a whitespace between header keys.

        while((token = strtok(keyPtr, search)) != NULL) {
            if((key = http_header_key(http, token)) != NULL) {
                fprintf(stderr, "%s", key);
                fprintf(stderr, "%s", ": ");
                fprintf(stderr, "%s\n", http_headers_lookup(http, token)); // Print the header value to stderr.
            }
            keyPtr = NULL;
        }
    }

  /*
   * At this point, we can retrieve the body of the document,
   * character by character, using http_getc()
   */

   FILE *file = NULL;
   if(output_file != NULL) { // If the output file is null, then there was no argument supplied to -o or -o wasn't specified.
       file = fopen(output_file, "w"); // Open the file in write mode or create the file if it doesn't exist.

       if(file == NULL) {
           exit(-1); // If the file is invalid, exit with -1 status.
       }
   }

    while((c = http_getc(http)) != EOF) {
        if(file != NULL) { // If we specified the file, print each char into the output file.
            fprintf(file, "%c", c);
        } else {
            putchar(c); // Else, just print each char into stdout.
        }
    }

    if(file != NULL) { // If the file was not null, close it.
        fclose(file);
    }

    http_close(http);
    url_free(up);
    exit(code == 200 ? 0 : code); // If the exit status was not 200, then exit with the code, otherwise exit with 0.
}
