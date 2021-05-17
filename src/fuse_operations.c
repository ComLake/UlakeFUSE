//
// Created by hoangdm on 20/04/2021.
//

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>

#include "Ulakefs.h"
#include "options.h"
#include "debug.h"

static int ulakefs_chmod(const char *path,mode_t mode){
    DBG("%s\n", path);

    int i = find_rw_branch_cow(path);
    if (i == -1) RETURN(-errno);

    char p[PATHLEN_MAX];
    if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

    int res = chmod(p, mode);
    if (res == -1) RETURN(-errno);

    RETURN(0);
}
