/*
 * Virtual screen sessions.
 *
 * A session has a pseudoterminal (pty), a virtual screen,
 * and a process that is the session leader.  Output from the
 * pty goes to the virtual screen, which can be one of several
 * virtual screens multiplexed onto the physical screen.
 * At any given time there is a particular session that is
 * designated the "foreground" session.  The contents of the
 * virtual screen associated with the foreground session is what
 * is shown on the physical screen.  Input from the physical
 * keyboard is directed to the pty for the foreground session.
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "session.h"
#include "debug.h"
#include "ecran.h"

SESSION *sessions[MAX_SESSIONS];  // Table of existing sessions
SESSION *fg_session;              // Current foreground session

/*
 * Initialize a new session whose session leader runs a specified command.
 * If the command is NULL, then the session leader runs a shell.
 * The new session becomes the foreground session.
 */
SESSION *session_init(char *path, char *argv[]) {
    for(int i = 0; i < MAX_SESSIONS; i++) {
    	if(sessions[i] == NULL) {
    	    int mfd = posix_openpt(O_RDWR | O_NOCTTY);

            // posix_openpt might return -1 on error.
    	    if(mfd == -1) {
                debug("session_init(): posix_openpt() returned -1.");
                finalize(0);
            }

            // unlockpt might return -1 on error.
    	    if(unlockpt(mfd) == -1) {
                debug("session_init(): unlockpt() returned -1.");
                close(mfd);
                finalize(0);
            }

            // ptsname might return NULL on error.
    	    char *sname = ptsname(mfd);
            if(sname == NULL) {
                debug("session_init(): ptsname() returned NULL.");
                close(mfd);
                finalize(0);
            }
    	    // Set nonblocking I/O on master side of pty
            // fcntl might return -1 on error.
    	    if(fcntl(mfd, F_SETFL, O_NONBLOCK) == -1) {
                debug("session_init(): fcntl() returned -1.");
                close(mfd);
                finalize(0);
            }

    	    SESSION *session = calloc(sizeof(SESSION), 1);
    	    sessions[i] = session;
    	    session->sid = i;
    	    session->vscreen = vscreen_init();
    	    session->ptyfd = mfd;

            // Automatically terminates processes that are killed instead of
            // converting them into zombies.

            if(signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
                debug("session_init(): signal() returned SIG_ERROR.");
                close(mfd);
                finalize(0);
            }

    	    // Fork process to be leader of new session.
    	    if((session->pid = fork()) == 0) {
        		// Open slave side of pty, create new session,
        		// and set pty as controlling terminal.
        		int sfd = open(sname, O_RDWR);

                // open might return -1 on error.
                // We have to deallocate the vscreen and free the session.
                if(sfd == -1) {
                    debug("session_init(): open() returned -1.");
                    close(mfd);
                    finalize(0);
                }

        		if(setsid() == (pid_t) -1) {
                    debug("session_init(): setsid() returned -1.");
                    close(sfd); close(mfd);
                    finalize(0);
                }

        		if(ioctl(sfd, TIOCSCTTY, 0) == -1) {
                    debug("session_init(): ioctl() returned -1.");
                    close(sfd); close(mfd);
                    finalize(0);
                }

        		if(dup2(sfd, 2) == -1) {
                    debug("session_init(): dup2(sfd, 2) returned -1.");
                    close(sfd); close(mfd);
                    finalize(0);
                }

        		// Set TERM environment variable to match vscreen terminal
        		// emulation capabilities (which currently aren't that great).
        		if(putenv("TERM=dumb") != 0) {
                    debug("session_init(): putenv() returned non-zero (error).");
                    close(sfd); close(mfd);
                    finalize(0);
                }

        		// Set up stdin/stdout and do exec.
        		// TO BE FILLED IN
                if(dup2(sfd, 0) == -1 || dup2(sfd, 1) == -1) {
                    debug("session_init(): dup2(sfd, 0) or dup2(sfd, 1) returned -1.");
                    close(sfd); close(mfd);
                    finalize(0);
                }

                if(close(sfd) == -1) {
                    debug("session_init(): close(sfd) returned -1.");
                    close(mfd);
                    finalize(0);
                }

                if(close(mfd) == -1) {
                    debug("session_init(): close(mfd) returned -1.");
                    finalize(0);
                }

                if(execvp(path, argv) == -1) {
                    debug("session_init(): execvp() returned -1.");
                }

        		fprintf(stderr, "Command not found. Please quit the program.\n");
        		exit(1);
    	    } else {
                if(session->pid == -1) {
                    debug("session_init(): fork() returned -1.");
                    close(mfd);
                    finalize(0);
                }
            }

    	    // Parent drops through
    	    session_setfg(session);
    	    return session;
    	}
    }
    return NULL;  // Session table full.
}
/*
 * Returns the lowest indexed session in the session table.
 * If none exists (empty table), return NULL.
 */
SESSION *session_next() {
    if(fg_session != NULL) {
        return fg_session;
    }

    for(int i = 0; i < MAX_SESSIONS; i++) {
        if(sessions[i] != NULL) {
            return sessions[i];
        }
    }
    return NULL;
}

/*
 * Set a specified session as the foreground session.
 * The current contents of the virtual screen of the new foreground session
 * are displayed in the physical screen, and subsequent input from the
 * physical terminal is directed at the new foreground session.
 */
void session_setfg(SESSION *session) {
    // Display the contents of 'session'.
    vscreen_show(session->vscreen);

    // Set the foreground session.
    fg_session = session;

    // Set the status line to display the session id.
    char session_num[12]; // Big enough to hold a 32-bit integer.
    char session_text[10] = "Session #";

    sprintf(session_num, "%d", session->sid); // Convert integer to string.
    set_status(cmd == NULL ? strncat(session_text, session_num, 11) : "TYPE CTRL-A + Q TO QUIT."); // Concat "Session" with the session id.
}

/*
 * Read up to bufsize bytes of available output from the session pty.
 * Returns the number of bytes read, or EOF on error.
 */
int session_read(SESSION *session, char *buf, int bufsize) {
    int r = read(session->ptyfd, buf, bufsize);
    if(r == -1) {
        //finalize(0);
    }

    return r;
}

/*
 * Write a single byte to the session pty, which will treat it as if
 * typed on the terminal.  The number of bytes written is returned,
 * or EOF in case of an error.
 */
int session_putc(SESSION *session, char c) {
    // TODO: Probably should use non-blocking I/O to avoid the potential
    // for hanging here, but this is ignored for now.
    int w = write(session->ptyfd, &c, 1);
    if(w == -1) {
        finalize(0);
    }

    return w;
}

/*
 * Forcibly terminate a session by sending SIGKILL to its process group.
 */
void session_kill(SESSION *session) {
    // TO BE FILLED IN
    if(fg_session == session) {
        fg_session = NULL;
    }

    kill(session->pid * (-1), SIGKILL);
    close(session->ptyfd);

    session_fini(session);
}

/*
 * Deallocate a session that has terminated.  This function does not
 * actually deal with marking a session as terminated; that would be done
 * by a signal handler.  Note that when a session does terminate,
 * the session leader must be reaped.  In addition, if the terminating
 * session was the foreground session, then the foreground session must
 * be set to some other session, or to NULL if there is none.
 */
void session_fini(SESSION *session) {
    // TO BE FILLED IN
    sessions[session->sid] = NULL;
    vscreen_fini(session->vscreen);
    free(session);
}
