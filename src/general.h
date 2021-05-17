//
// Created by hoangdm on 20/04/2021.
//
/*
 * Include manything including such as Copy On Write (COW, branches, readdir, ...)
 */
#ifndef ULAKEFS_FUSE_GENERAL_H
#define ULAKEFS_FUSE_GENERAL_H

#include <stdbool.h>
#include <sys/stat.h>

enum  whiteout {
    WHITEOUT_FILE,
    WHITEOUT_DIR
};

typedef enum filetype {
    NOT_EXISTING=-1,
    IS_DIR=0,
    IS_FILE=1,
} filetype_t;

int path_hidden(const char *path, int branch);
int remove_hidden(const char *path, int maxbranch);
int hide_file(const char *path, int branch_rw);
int hide_dir(const char *path, int branch_rw);
filetype_t path_is_dir (const char *path);
int maybe_whiteout(const char *path, int branch_rw, enum whiteout mode);
int set_owner(const char *path);

/*
 * Copy on write and utils
 */
int cow_cp(const char *path, int branch_ro, int branch_rw, bool copy_dir);
int path_create(const char *path, int nbranch_ro, int nbranch_rw);
int path_create_cutlast(const char *path, int nbranch_ro, int nbranch_rw);
int copy_directory(const char *path, int branch_ro, int branch_rw);

struct cow {
    mode_t umask;
    uid_t uid;

    // source file
    char  *from_path;
    struct stat *stat;

    // destination file
    char *to_path;
};

int setfile(const char *path, struct stat *fs);
int copy_special(struct cow *cow);
int copy_fifo(struct cow *cow);
int copy_link(struct cow *cow);
int copy_file(struct cow *cow);

/*
 * Find branches
 */
typedef enum searchflag {
    RWRO,
    RWONLY
} searchflag_t;

int find_rorw_branch(const char *path);
int find_lowest_rw_branch(int branch_ro);
int find_rw_branch_cutlast(const char *path);
int __find_rw_branch_cutlast(const char *path, int rw_hint);
int find_rw_branch_cow(const char *path);
int find_rw_branch_cow_common(const char *path, bool copy_dir);

#endif //ULAKEFS_FUSE_GENERAL_H
