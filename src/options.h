//
// Created by hoangdm on 20/04/2021.
//

#ifndef ULAKEFS_FUSE_OPTIONS_H
#define ULAKEFS_FUSE_OPTIONS_H

#include <fuse.h>
#include <stdbool.h>
#include "Ulakefs.h"

#define ROOT_SEP ":"
typedef struct {
    int nbranches;
    branch_entry_t *branches;

    bool cow_enabled;
    bool statfs_omit_ro;
    int doexit;
    int retval;
    char *chroot; 		// chroot we might go into
    bool debug;		// enable debugging
    char *dbgpath;		// debug file we write debug information into
    pthread_rwlock_t dbgpath_lock; // locks dbgpath
    bool hide_meta_files;
    bool relaxed_permissions;

} uoptions_t;

enum {
    KEY_CHROOT,
    KEY_COW,
    KEY_DEBUG_FILE,
    KEY_DIRS,
    KEY_HELP,
    KEY_HIDE_META_FILES,
    KEY_HIDE_METADIR,
    KEY_MAX_FILES,
    KEY_NOINITGROUPS,
    KEY_RELAXED_PERMISSIONS,
    KEY_STATFS_OMIT_RO,
    KEY_VERSION
};

extern uoptions_t uopt;
void set_debug_path(char *new_path, int len);
bool set_debug_onoff(int value);
void uopt_init();
int ulakefs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs);
void ulakefs_post_opts();
void add_branch(char *branch);
int parse_branches(const char *arg);

char *whiteout_tag(const char *fname);
int build_path(char *dest, int max_len, const char *callfunc, int line, ...);
char *u_dirname(const char *path);
unsigned int string_hash(void *s);

/**
 * A wrapper for build_path(). In build_path() we test if the given number of strings does exceed
 * a maximum string length. Since there is no way in C to determine the given number of arguments, we
 * simply add NULL here.
 */
#define BUILD_PATH(dest, ...) build_path(dest, PATHLEN_MAX, __func__, __LINE__, __VA_ARGS__, NULL)

/**
  * Test if two strings are eqal.
  * Return 1 if the strings are equal and 0 if they are different.
  */
// This is left in the header file bacause gcc is too stupid to inline across object files
static inline int string_equal(void *s1, void *s2) {
    if (strcmp(s1, s2) == 0) return 1;
    return 0;
}

#endif //ULAKEFS_FUSE_OPTIONS_H
