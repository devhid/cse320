/*
 * Ecran: A program that (if properly completed) supports the multiplexing
 * of multiple virtual terminal sessions onto a single physical terminal.
 */

#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>
#include <sys/signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "ecran.h"
#include "debug.h"

static void initialize();
static void curses_init(void);
static void curses_fini(void);
static void parse_args(int argc, char *argv[]);
static void check_program(int end);

char **cmd = NULL;

int main(int argc, char *argv[]) {
    parse_args(argc, argv);

    initialize();
    mainloop();

    // NOT REACHED
}

void cleanup(void) {
    if(cmd != NULL) {
        free(cmd);
    }

    for(int i = 0; i < MAX_SESSIONS; i++) {
        if(sessions[i] != NULL) {
            session_kill(sessions[i]);
        }
    }
}

static void parse_args(int argc, char *argv[]) {
    if(argc > 3 && !strcmp(argv[1], "-o")) {
        cmd = calloc(sizeof(char *), argc - 3 + 1);
        for(int i = 0; i < argc - 3; i++) {
            cmd[i] = argv[i+3];
        }

        cmd[argc - 3] = NULL;
        check_program(argc - 3);
    } else {
        if(argc > 1 && strcmp(argv[1], "-o")) {
            cmd = calloc(sizeof(char *), argc - 1 + 1);
            for(int i = 0; i < argc - 1; i++) {
                cmd[i] = argv[i+1];
            }

            cmd[argc - 1] = NULL;
            check_program(argc - 1);
        }
    }

    // Handle redirection from standard error input to file.
    const char* opstring = "+o:";
    int opt = 0;

    while((opt = getopt(argc, argv, opstring)) != -1) {
        if(opt == 'o') {

            // Open the file in with read/write, truncated to 0, and append mode.
            int fd = open(optarg, O_RDWR | O_CREAT | O_TRUNC | O_APPEND, S_IRUSR | S_IWUSR);

            // Handle if error occurred in opening file.
            if(fd == -1) {
                debug("main(): Error opening file: %s", optarg);
                exit(EXIT_FAILURE);
            }

            // Point standard error file descriptor to the file's descriptor.
            if(dup2(fd, fileno(stderr)) == -1) {
                debug("main(): Error copying file descriptor.");
                close(fd);
                exit(EXIT_FAILURE);
            }

            if(close(fd) == -1) {
                debug("main(): Error closing file descriptor.");
                exit(EXIT_FAILURE);
            }

        } else {
            exit(EXIT_FAILURE);
        }
    }
}

static void check_program(int end) {
    char program[128] = "command -v ";
    strcat(program, cmd[0]);

    char buffer[128];
    FILE *fp;

    if((fp = popen(program, "r")) == NULL ) {
        finalize(0);
    }

    if(fgets(buffer, 128, fp) == NULL) {
        fprintf(stderr, "%s: command not found\n", cmd[0]);
        pclose(fp);
        finalize(0);
    }

    pclose(fp);
}

/*
 * Initialize the program and launch a single session to run the
 * default shell.
 */
static void initialize(void) {
    curses_init();
    char *path = getenv("SHELL");
    if(path == NULL) {
        path = "/bin/bash";
    }
    char *argv[2] = { " (ecran session)", NULL };

    if(cmd != NULL) {
        session_init(cmd[0], cmd);
    } else {
        // Session could be null so we must check and exit appropriately.
        if(session_init(path, argv) == NULL) {
            debug("initialize(): session_init returned NULL.");
            curses_fini();
            exit(EXIT_FAILURE);
        }
    }

}

/*
 * Cleanly terminate the program.  All existing sessions should be killed,
 * all pty file descriptors should be closed, all memory should be deallocated,
 * and the original screen contents should be restored before terminating
 * normally.  Note that the current implementation only handles restoring
 * the original screen contents and terminating normally; the rest is left
 * to be done.
 */
void finalize(int success) {
    // REST TO BE FILLED IN
    cleanup();
    curses_fini();
    exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * Helper method to initialize the screen for use with curses processing.
 * You can find documentation of the "ncurses" package at:
 * https://invisible-island.net/ncurses/man/ncurses.3x.html
 */
static void curses_init(void) {
    initscr();

    if(raw() == ERR) { // Don't generate signals, and make typein immediately available.
        debug("curses_init(): curses_fini() returned ERR.");
        curses_fini();
        exit(EXIT_FAILURE);
    }

    if(noecho() == ERR) {                    // Don't echo -- let the pty handle it.
        debug("curses_init(): noecho() returned ERR.");
        curses_fini();
        exit(EXIT_FAILURE);
    }

    main_screen = newwin(LINES - 1, COLS, 0, 0);
    status_line = newwin(1, COLS, LINES - 1, 0);

    if(main_screen == NULL || status_line == NULL) {
        debug("curses_init(): newwin() returned ERR.");
        curses_fini();
        exit(EXIT_FAILURE);
    }

    if(nodelay(main_screen, TRUE) == ERR  // Set non-blocking I/O on input.
    || nodelay(status_line, TRUE) == ERR) {
        debug("curses_init(): nodelay() returned ERR.");
        curses_fini();
        exit(EXIT_FAILURE);
    }

    if(wclear(main_screen) == ERR         // Clear the screen.
    || wclear(status_line) == ERR) {
        debug("curses_init(): wclear() returned ERR.");
        curses_fini();
        exit(EXIT_FAILURE);
    }

    //refresh();                   // Make changes visible.
    if(wrefresh(main_screen) == ERR
    || wrefresh(status_line) == ERR) {
        debug("curses_init(): wrefresh() returned ERR.");
        curses_fini();
        exit(EXIT_FAILURE);
    }
}

/*
 * Helper method to finalize curses processing and restore the original
 * screen contents.
 */
void curses_fini(void) {
    delwin(main_screen);
    delwin(status_line);

    if(endwin() == ERR) {
        exit(EXIT_FAILURE);
    }
}

/*
 * Function to read and process a command from the terminal.
 * This function is called from mainloop(), which arranges for non-blocking
 * I/O to be disabled before calling and restored upon return.
 * So within this function terminal input can be collected one character
 * at a time by calling getch(), which will block until input is available.
 * Note that while blocked in getch(), no updates to virtual screens can
 * occur.
 */
void do_command(void) {
    // Quit command: terminates the program cleanly
    int key = wgetch(main_screen);

    if(key == ERR) {
        finalize(0);
    }

    else if(key == 'q') {
    	finalize(1);
    }

    else if(key >= '0' && key <= '9' && cmd == NULL) {
        if(sessions[key - '0'] != NULL) {
            session_setfg(sessions[key - '0']);
        } else {
            flash();
        }
    }

    else if(key == 'n' && cmd == NULL) {
        char *path = getenv("SHELL");
        if(path == NULL) {
            path = "/bin/bash";
        }
        char *argv[2] = { " (ecran session)", NULL };

        if(session_init(path, argv) == NULL) {
            debug("do_command(): session_init returned NULL.");
            set_status("The maximum number of sessions (10) has been reached.");
        }
    }

    else if(key == 'k' && cmd == NULL) {
        int sid = wgetch(main_screen);

        if(sid >= '0' && sid <= '9') {
            SESSION *session = sessions[sid - '0'];

            if(session != NULL) {
                session_kill(session);

                SESSION *next = session_next();
                if(next == NULL) {
                    finalize(1);
                } else {
                    session_setfg(next);
                }

            } else {
                flash();
            }
        }
    }
    else {
    	// OTHER COMMANDS TO BE IMPLEMENTED
    	flash();
    }
}

/*
 * Function called from mainloop(), whose purpose is do any other processing
 * that has to be taken care of.  An example of such processing would be
 * to deal with sessions that have terminated.
 */
void do_other_processing(void) {
    // TO BE FILLED IN

}
