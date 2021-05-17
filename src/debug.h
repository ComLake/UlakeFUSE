//
// Created by hoangdm on 20/04/2021.
//

#ifndef ULAKEFS_FUSE_DEBUG_H
#define ULAKEFS_FUSE_DEBUG_H

#include <syslog.h>
#include <stdbool.h>
#include <errno.h>
#include "options.h"

#define MAX_SYSLOG_MESSAGES 32	// max number of buffered syslog messages
#define MAX_MSG_SIZE 256	// max string length for syslog messages
#define DBG_IN() DBG("\n");

#define DBG(format, ...) \
	do { \
		if (!uopt.debug) break; \
		int _errno = errno; \
		FILE* dbgfile = get_dbgfile(); \
		fprintf(stderr, "%s(): %d: ", __func__, __LINE__); \
		fprintf(dbgfile, "%s(): %d: ", __func__, __LINE__); \
		fprintf(stderr, format, ##__VA_ARGS__); \
		fprintf(dbgfile, format, ##__VA_ARGS__); \
		fflush(stderr); \
		fflush(stdout); \
		put_dbgfile(); \
		errno = _errno; \
	} while (0)

#define RETURN(returncode) \
	do { \
		if (uopt.debug) DBG("return %d\n", returncode); \
		return returncode; \
	} while (0)


/* In order to prevent useless function calls and to make the compiler
 * to optimize those out, debug.c will only have definitions if DEBUG
 * is defined. So if DEBUG is NOT defined, we define empty functions here */
int debug_init();
void dbg_in(const char *function);

FILE* get_dbgfile(void);
void put_dbgfile(void);

/*
 * Syslog
 *  chained buffer list of syslog entries
 */

typedef struct ulogs {
    int priority; // first argument for syslog()
    char message[MAX_MSG_SIZE]; // 2nd argument for syslog()
    bool used;		// is this entry used?
    pthread_mutex_t lock;	// lock a single entry
    struct ulogs *next;	// pointer to the next entry
} ulogs_t;


void init_syslog(void);
void usyslog(int priority, const char *format, ...);


#define USYSLOG(priority, format, ...)  			\
	do {							\
		DBG(format, ##__VA_ARGS__);			\
		usyslog(priority, format, ##__VA_ARGS__);	\
	} while (0);

#endif //ULAKEFS_FUSE_DEBUG_H
