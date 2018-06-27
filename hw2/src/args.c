#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "debug.h"
#include "snarf.h"

int opterr = 0;
int optopt = 0;
int optind = 0;
char *optarg = NULL;
char *url_to_snarf = NULL;
char *output_file = NULL;

char *keyPtr = NULL;
char keywords[1024];

void parse_args(int argc, char *argv[]) {
    int i = 0;
    char option = 0;

    for(int i = 0; i < argc; i++) {
        if(!strcmp(argv[i], "-h")) {
            USAGE(argv[0]);
            exit(0);
        }
    }

    for (i = 0; optind < argc; i++) {
        debug("%d opterr: %d", i, opterr);
        debug("%d optind: %d", i, optind);
        debug("%d optopt: %d", i, optopt);
        debug("%d argv[optind]: %s", i, argv[optind]);

        if ((option = getopt(argc, argv, "+q:o:")) != -1) {
            switch (option) {
                case 'q':
                    info("Query header: %s", optarg);
                    if(!strcmp(optarg, "-o") || !strcmp(optarg, "-q")) { // Check if the argument for -q is a flag, if not, exit with error.
                        USAGE(argv[0]);
                        exit(-1);
                    }

                    if(*(optarg - 1) != '\0') { // Check if there is a space between the flag and argument, if not, exit with error.
                        USAGE(argv[0]);
                        exit(-1);
                    }

                    if(keyPtr == NULL) {
                        keyPtr = keywords; // Have a pointer to keywords if we don't have one already.
                    }

                    strcat(keywords, optarg); // Concatenate the keyword into the buffer.
                    strcat(keywords, " "); // Concenate a space into the buffer.

                    break;
                case 'o':
                    info("Output file: %s", optarg);

                    if(!strcmp(optarg, "-o") || !strcmp(optarg, "-q")) { // Check if the argument for -o is a flag, if not, exit with error.
                        USAGE(argv[0]);
                        exit(-1);
                    }

                    if(*(optarg - 1) != '\0') { // Check if there is a space between the flag and argument, if not, exit with error.
                        USAGE(argv[0]);
                        exit(-1);
                    }

                    if(output_file == NULL) {
                        output_file = optarg; // Have output_file have a pointer to the file argument.
                    } else { // There can only be one output file.
                        USAGE(argv[0]);
                        exit(-1);
                    }

                    break;
                case '?':
                    if (optopt != 'h') {
                        if(optopt != 'q' && optopt != 'o') {
                            fprintf(stderr, KRED "-%c is not a supported argument\n" KNRM, optopt);
                        }
                        USAGE(argv[0]);
                        exit(-1); // Exit with -1 because it's an error.
                    }

                    USAGE(argv[0]);
                    exit(0);
                    break;
                default:
                    break;
            }
        }

        else if(argv[optind] != NULL) {
            if(optind != argc - 1) { // If the URL is not the last argument, exit with -1 status.
                USAGE(argv[0]);
                exit(-1);
            } else {
                info("URL to snarf: %s", argv[optind]);
                url_to_snarf = argv[optind];
                optind++;
            }
        }
    }
}
