//
// Created by hoangdm on 18/05/2021.
//

#ifndef ULAKEFS_FUSE_READRMDIR_H
#define ULAKEFS_FUSE_READRMDIR_H

#include <fuse.h>

int ulakefs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int ulakefs_rmdir(const char *path);
int ulakefs_unlink(const char *path);
int dir_not_empty(const char *path);

#endif //ULAKEFS_FUSE_READRMDIR_H
