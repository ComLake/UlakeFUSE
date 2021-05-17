//
// Created by hoangdm on 20/04/2021.
//
/*
 * https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201001/homework/fuse/fuse_doc.html based on this
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>
#include "Ulakefs.h
#include "options.h"
#include "debug.h
#include "general.h"

#ifndef S_ISTXT
#define S_ISTXT S_ISVTX
#endif

/**
 * Check if a file or directory with the hidden flag exists.
 */
static int filedir_hidden(const char *path) {
    // cow mode disabled, no need for hidden files
    if (!uopt.cow_enabled) RETURN(false);

    char p[PATHLEN_MAX];
    if (strlen(path) + strlen(HIDETAG) > PATHLEN_MAX) RETURN(-ENAMETOOLONG);
    snprintf(p, PATHLEN_MAX, "%s%s", path, HIDETAG);
    DBG("%s\n", p);

    struct stat stbuf;
    int res = lstat(p, &stbuf);
    if (res == 0) RETURN(1);

    RETURN(0);
}

/**
 * check if any dir or file within path is hidden
 */
int path_hidden(const char *path, int branch) {
    DBG("%s\n", path);

    if (!uopt.cow_enabled) RETURN(false);

    char whiteoutpath[PATHLEN_MAX];
    if (BUILD_PATH(whiteoutpath, uopt.branches[branch].path, METADIR, path)) RETURN(false);

    // -1 as we MUST not end on the next path element
    char *walk = whiteoutpath + uopt.branches[branch].path_len + strlen(METADIR) - 1;

    // first slashes, e.g. we have path = /dir1/dir2/, will set walk = dir1/dir2/
    while (*walk == '/') walk++;

    do {
        // walk over the directory name, walk will now be /dir2
        while (*walk != '\0' && *walk != '/') walk++;

        // +1 due to \0, which gets added automatically
        char p[PATHLEN_MAX];
        // walk - path = strlen(/dir1)
        snprintf(p, (walk - whiteoutpath) + 1, "%s", whiteoutpath);
        int res = filedir_hidden(p);
        if (res) RETURN(res); // path is hidden or error

        // as above the do loop, walk over the next slashes, walk = dir2/
        while (*walk == '/') walk++;
    } while (*walk != '\0');

    RETURN(0);
}

/**
 * Remove a hide-file in all branches up to maxbranch
 * If maxbranch == -1, try to delete it in all branches.
 */
int remove_hidden(const char *path, int maxbranch) {
    DBG("%s\n", path);

    if (!uopt.cow_enabled) RETURN(0);

    if (maxbranch == -1) maxbranch = uopt.nbranches;

    int i;
    for (i = 0; i <= maxbranch; i++) {
        char p[PATHLEN_MAX];
        if (BUILD_PATH(p, uopt.branches[i].path, METADIR, path)) RETURN(-ENAMETOOLONG);
        if (strlen(p) + strlen(HIDETAG) > PATHLEN_MAX) RETURN(-ENAMETOOLONG);
        strcat(p, HIDETAG); // TODO check length

        switch (path_is_dir(p)) {
            case IS_FILE: unlink(p); break;
            case IS_DIR: rmdir(p); break;
            case NOT_EXISTING: continue;
        }
    }

    RETURN(0);
}

/**
 * check if path is a directory
 *
 * return proper types given by filetype_t
 */
filetype_t path_is_dir(const char *path) {
    DBG("%s\n", path);

    struct stat buf;

    if (lstat(path, &buf) == -1) RETURN(NOT_EXISTING);

    if (S_ISDIR(buf.st_mode)) RETURN(IS_DIR);

    RETURN(IS_FILE);
}

/**
 * Create a file or directory that hides path below branch_rw
 */
static int do_create_whiteout(const char *path, int branch_rw, enum whiteout mode) {
    DBG("%s\n", path);

    char metapath[PATHLEN_MAX];

    if (BUILD_PATH(metapath, METADIR, path)) RETURN(-1);

    // p MUST be without path to branch prefix here! 2 x branch_rw is correct here!
    // this creates e.g. branch/.ulakefs/some_directory
    path_create_cutlast(metapath, branch_rw, branch_rw);

    char p[PATHLEN_MAX];
    if (BUILD_PATH(p, uopt.branches[branch_rw].path, metapath)) RETURN(-1);
    strcat(p, HIDETAG); // TODO check length

    int res;
    if (mode == WHITEOUT_FILE) {
        res = open(p, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        if (res == -1) RETURN(-1);
        res = close(res);
    } else {
        res = mkdir(p, S_IRWXU);
        if (res)
            USYSLOG(LOG_ERR, "Creating %s failed: %s\n", p, strerror(errno));
    }

    RETURN(res);
}

/**
 * Create a file that hides path below branch_rw
 */
int hide_file(const char *path, int branch_rw) {
    DBG("%s\n", path);
    int res = do_create_whiteout(path, branch_rw, WHITEOUT_FILE);
    RETURN(res);
}

/**
 * Create a directory that hides path below branch_rw
 */
int hide_dir(const char *path, int branch_rw) {
    DBG("%s\n", path);
    int res = do_create_whiteout(path, branch_rw, WHITEOUT_DIR);
    RETURN(res);
}

/**
 * This is called *after* unlink() or rmdir(), create a whiteout file
 * if the same file/dir does exist in a lower branch
 */
int maybe_whiteout(const char *path, int branch_rw, enum whiteout mode) {
    DBG("%s\n", path);

    // we are not interested in the branch itself, only if it exists at all
    if (find_rorw_branch(path) != -1) {
        int res = do_create_whiteout(path, branch_rw, mode);
        RETURN(res);
    }

    RETURN(0);
}

/**
 * Set file owner of after an operation, which created a file.
 */
int set_owner(const char *path) {
    struct fuse_context *ctx = fuse_get_context();
    if (ctx->uid != 0 && ctx->gid != 0) {
        int res = lchown(path, ctx->uid, ctx->gid);
        if (res) {
            USYSLOG(LOG_WARNING,
                    ":%s: Setting the correct file owner failed: %s !\n",
                    __func__, strerror(errno));
            RETURN(-errno);
        }
    }
    RETURN(0);
}

/** Branches
 *  Find a branch that has "path". Return the branch number.
 */
static int find_branch(const char *path, searchflag_t flag) {
    DBG("%s\n", path);

    int i = 0;
    for (i = 0; i < uopt.nbranches; i++) {
        char p[PATHLEN_MAX];
        if (BUILD_PATH(p, uopt.branches[i].path, path)) {
            errno = ENAMETOOLONG;
            RETURN(-1);
        }

        struct stat stbuf;
        int res = lstat(p, &stbuf);

        DBG("%s: res = %d\n", p, res);

        if (res == 0) { // path was found
            switch (flag) {
                case RWRO:
                    // any path we found is fine
                    RETURN(i);
                case RWONLY:
                    // we need a rw-branch
                    if (uopt.branches[i].rw) RETURN(i);
                    break;
                default:
                    USYSLOG(LOG_ERR, "%s: Unknown flag %d\n", __func__, flag);
            }
        }

        // check check for a hide file, checking first here is the magic to hide files *below* this level
        res = path_hidden(path, i);
        if (res > 0) {
            // So no path, but whiteout found. No need to search in further branches
            errno = ENOENT;
            RETURN(-1);
        } else if (res < 0) {
            errno = res; // error
            RETURN(-1);
        }
    }

    errno = ENOENT;
    RETURN(-1);
}

/**
 * Find a ro or rw branch.
 */
int find_rorw_branch(const char *path) {
    DBG("%s\n", path);
    int res = find_branch(path, RWRO);
    RETURN(res);
}

/**
 * Find a writable branch. If file does not exist, we check for
 * the parent directory.
 * @path 	- the path to find or to copy (with last element cut off)
 * @ rw_hint	- the rw branch to copy to, set to -1 to autodetect it
 */
int __find_rw_branch_cutlast(const char *path, int rw_hint) {
    int branch = find_rw_branch_cow(path);
    DBG("branch = %d\n", branch);

    if (branch >= 0 || (branch < 0 && errno != ENOENT)) RETURN(branch);

    DBG("Check for parent directory\n");

    // So path does not exist, now again, but with dirname only.
    // We MUST NOT call find_rw_branch_cow() // since this function
    // doesn't work properly for directories.
    char *dname = u_dirname(path);
    if (dname == NULL) {
        errno = ENOMEM;
        RETURN(-1);
    }

    branch = find_rorw_branch(dname);
    DBG("branch = %d\n", branch);

    // No branch found, so path does nowhere exist, error
    if (branch < 0) goto out;

    // Reminder rw_hint == -1 -> autodetect, we do not care which branch it is
    if (uopt.branches[branch].rw
        && (rw_hint == -1 || branch == rw_hint)) goto out;

    if (!uopt.cow_enabled) {
        // So path exists, but is not writable.
        branch = -1;
        errno = EACCES;
        goto out;
    }

    int branch_rw;
    // since it is a directory, any rw-branch is fine
    if (rw_hint == -1)
        branch_rw = find_lowest_rw_branch(uopt.nbranches);
    else
        branch_rw = rw_hint;

    DBG("branch_rw = %d\n", branch_rw);

    // no writable branch found, we must return an error
    if (branch_rw < 0) {
        branch = -1;
        errno = EACCES;
        goto out;
    }

    if (path_create(dname, branch, branch_rw) == 0) branch = branch_rw; // path successfully copied

    out:
    free(dname);

    RETURN(branch);
}

/**
 * Call __find_rw_branch_cutlast()
 */
int find_rw_branch_cutlast(const char *path) {
    int rw_hint = -1; // autodetect rw_branch
    int res = __find_rw_branch_cutlast(path, rw_hint);
    RETURN(res);
}

int find_rw_branch_cow(const char *path) {
    return find_rw_branch_cow_common(path, false);
}

/**
 * copy-on-write
 * Find path in a union branch and if this branch is read-only,
 * copy the file to a read-write branch.
 * NOTE: Don't call this to copy directories. Use path_create() for that!
 *       It will definitely fail, when a ro-branch is on top of a rw-branch
 *       and a directory is to be copied from ro- to rw-branch.
 */
int find_rw_branch_cow_common(const char *path, bool copy_dir) {
    DBG("%s\n", path);

    int branch_rorw = find_rorw_branch(path);

    // not found anywhere
    if (branch_rorw < 0) RETURN(-1);

    // the found branch is writable, good!
    if (uopt.branches[branch_rorw].rw) RETURN(branch_rorw);

    // cow is disabled and branch is not writable, so deny write permission
    if (!uopt.cow_enabled) {
        errno = EACCES;
        RETURN(-1);
    }

    int branch_rw = find_lowest_rw_branch(branch_rorw);
    if (branch_rw < 0) {
        // no writable branch found
        errno = EACCES;
        RETURN(-1);
    }

    if (cow_cp(path, branch_rorw, branch_rw, copy_dir)) RETURN(-1);

    // remove a file that might hide the copied file
    remove_hidden(path, branch_rw);

    RETURN(branch_rw);
}

/**
 * Find lowest possible writable branch but only lower than branch_ro.
 */
int find_lowest_rw_branch(int branch_ro) {
    DBG_IN();

    int i = 0;
    for (i = 0; i < branch_ro; i++) {
        if (uopt.branches[i].rw) RETURN(i); // found it it.
    }

    RETURN(-1);
}

/** Copy on write
 * Actually create the directory here.
 */
static int do_create(const char *path, int nbranch_ro, int nbranch_rw) {
    DBG("%s\n", path);

    char dirp[PATHLEN_MAX]; // dir path to create
    sprintf(dirp, "%s%s", uopt.branches[nbranch_rw].path, path);

    struct stat buf;
    int res = stat(dirp, &buf);
    if (res != -1) RETURN(0); // already exists

    if (nbranch_ro == nbranch_rw) {
        // special case nbranch_ro = nbranch_rw, this is if we a create
        // ulakefs meta directories, so not directly on cow operations
        buf.st_mode = S_IRWXU | S_IRWXG;
    } else {
        // data from the ro-branch
        char o_dirp[PATHLEN_MAX]; // the pathname we want to copy
        sprintf(o_dirp, "%s%s", uopt.branches[nbranch_ro].path, path);
        res = stat(o_dirp, &buf);
        if (res == -1) RETURN(1); // lower level branch removed in the mean time?
    }

    res = mkdir(dirp, buf.st_mode);
    if (res == -1) {
        USYSLOG(LOG_DAEMON, "Creating %s failed: \n", dirp);
        RETURN(1);
    }

    if (nbranch_ro == nbranch_rw) RETURN(0); // the special case again

    if (setfile(dirp, &buf)) RETURN(1); // directory already removed by another process?

    // TODO: time, but its values are modified by the next dir/file creation steps?

    RETURN(0);
}

/**
 * l_nbranch (lower nbranch than nbranch) is write protected, create the dir path on
 * nbranch for an other COW operation.
 */
int path_create(const char *path, int nbranch_ro, int nbranch_rw) {
    DBG("%s\n", path);

    if (!uopt.cow_enabled) RETURN(0);

    char p[PATHLEN_MAX];
    if (BUILD_PATH(p, uopt.branches[nbranch_rw].path, path)) RETURN(-ENAMETOOLONG);

    struct stat st;
    if (!stat(p, &st)) {
        // path does already exists, no need to create it
        RETURN(0);
    }

    char *walk = (char *)path;

    // first slashes, e.g. we have path = /dir1/dir2/, will set walk = dir1/dir2/
    while (*walk == '/') walk++;

    do {
        // walk over the directory name, walk will now be /dir2
        while (*walk != '\0' && *walk != '/') walk++;

        // +1 due to \0, which gets added automatically
        snprintf(p, (walk - path) + 1, "%s", path); // walk - path = strlen(/dir1)
        int res = do_create(p, nbranch_ro, nbranch_rw);
        if (res) RETURN(res); // creating the directory failed

        // as above the do loop, walk over the next slashes, walk = dir2/
        while (*walk == '/') walk++;
    } while (*walk != '\0');

    RETURN(0);
}

/**
 * Same as  path_create(), but ignore the last segment in path,
 * i.e. it might be a filename.
 */
int path_create_cutlast(const char *path, int nbranch_ro, int nbranch_rw) {
    DBG("%s\n", path);

    char *dname = u_dirname(path);
    if (dname == NULL)
        RETURN(-ENOMEM);
    int ret = path_create(dname, nbranch_ro, nbranch_rw);
    free(dname);

    RETURN(ret);
}

/**
 * initiate the cow-copy action
 */
int cow_cp(const char *path, int branch_ro, int branch_rw, bool copy_dir) {
    DBG("%s\n", path);

    // create the path to the file
    path_create_cutlast(path, branch_ro, branch_rw);

    char from[PATHLEN_MAX], to[PATHLEN_MAX];
    if (BUILD_PATH(from, uopt.branches[branch_ro].path, path))
        RETURN(-ENAMETOOLONG);
    if (BUILD_PATH(to, uopt.branches[branch_rw].path, path))
        RETURN(-ENAMETOOLONG);

    setlocale(LC_ALL, "");

    struct cow cow;

    cow.uid = getuid();

    // Copy the umask for explicit mode setting.
    cow.umask = umask(0);
    umask(cow.umask);

    cow.from_path = from;
    cow.to_path = to;

    struct stat buf;
    lstat(cow.from_path, &buf);
    cow.stat = &buf;

    int res;
    switch (buf.st_mode & S_IFMT) {
        case S_IFLNK:
            res = copy_link(&cow);
            break;
        case S_IFDIR:
            if (copy_dir) {
                res = copy_directory(path, branch_ro, branch_rw);
            } else {
                res = path_create(path, branch_ro, branch_rw);
            }
            break;
        case S_IFBLK:
        case S_IFCHR:
            res = copy_special(&cow);
            break;
        case S_IFIFO:
            res = copy_fifo(&cow);
            break;
        case S_IFSOCK:
            USYSLOG(LOG_WARNING, "COW of sockets not supported: %s\n", cow.from_path);
            RETURN(1);
        default:
            res = copy_file(&cow);
    }

    RETURN(res);
}

/**
 * copy a directory between branches (includes all contents of the directory)
 */
int copy_directory(const char *path, int branch_ro, int branch_rw) {
    DBG("%s\n", path);

    /* create the directory on the destination branch */
    int res = path_create(path, branch_ro, branch_rw);
    if (res != 0) {
        RETURN(res);
    }

    /* determine path to source directory on read-only branch */
    char from[PATHLEN_MAX];
    if (BUILD_PATH(from, uopt.branches[branch_ro].path, path)) RETURN(1);

    DIR *dp = opendir(from);
    if (dp == NULL) RETURN(1);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

        char member[PATHLEN_MAX];
        if (BUILD_PATH(member, path, "/", de->d_name)) {
            res = 1;
            break;
        }
        res = cow_cp(member, branch_ro, branch_rw, true);
        if (res != 0) break;
    }

    closedir(dp);
    RETURN(res);
}

**
* set the stat() data of a file
**/
int setfile(const char *path, struct stat *fs)
{
    DBG("%s\n", path);

    struct utimbuf ut;
    int rval = 0;

    fs->st_mode &= S_ISUID | S_ISGID | S_ISTXT | S_IRWXU | S_IRWXG | S_IRWXO;

    ut.actime  = fs->st_atime;
    ut.modtime = fs->st_mtime;
    if (utime(path, &ut)) {
        USYSLOG(LOG_WARNING, "utimes: %s", path);
        rval = 1;
    }
    /*
    * Changing the ownership probably won't succeed, unless we're root
    * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
    * the mode; current BSD behavior is to remove all setuid bits on
    * chown.  If chown fails, lose setuid/setgid bits.
    */
    if (chown(path, fs->st_uid, fs->st_gid)) {
        if (errno != EPERM) {
            USYSLOG(LOG_WARNING, "chown: %s", path);
            rval = 1;
        }
        fs->st_mode &= ~(S_ISTXT | S_ISUID | S_ISGID);
    }

    if (chmod(path, fs->st_mode)) {
        USYSLOG(LOG_WARNING, "chown: %s", path);
        rval = 1;
    }

#ifdef HAVE_CHFLAGS
        /*
		 * XXX
		 * NFS doesn't support chflags; ignore errors unless there's reason
		 * to believe we're losing bits.  (Note, this still won't be right
		 * if the server supports flags and we were trying to *remove* flags
		 * on a file that we copied, i.e., that we didn't create.)
		 */
		errno = 0;
		if (chflags(path, fs->st_flags)) {
			if (errno != EOPNOTSUPP || fs->st_flags != 0) {
				USYSLOG(LOG_WARNING, "chflags: %s", path);
				rval = 1;
			}
			RETURN(rval);
		}
#endif
    RETURN(rval);
}

/**
 * set the stat() data of a link
 **/
static int setlink(const char *path, struct stat *fs)
{
    DBG("%s\n", path);

    if (lchown(path, fs->st_uid, fs->st_gid)) {
        if (errno != EPERM) {
            USYSLOG(LOG_WARNING, "lchown: %s", path);
            RETURN(1);
        }
    }
    RETURN(0);
}


/**
 * copy an ordinary file with all of its stat() data
 **/
int copy_file(struct cow *cow)
{
    DBG("from %s to %s\n", cow->from_path, cow->to_path);

    static char buf[MAXBSIZE];
    struct stat to_stat, *fs;
    int from_fd, rcount, to_fd, wcount;
    int rval = 0;
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
    char *p;
#endif

    if ((from_fd = open(cow->from_path, O_RDONLY, 0)) == -1) {
        USYSLOG(LOG_WARNING, "%s", cow->from_path);
        RETURN(1);
    }

    fs = cow->stat;

    to_fd = open(cow->to_path, O_WRONLY | O_TRUNC | O_CREAT,
                 fs->st_mode & ~(S_ISTXT | S_ISUID | S_ISGID));

    if (to_fd == -1) {
        USYSLOG(LOG_WARNING, "%s", cow->to_path);
        (void)close(from_fd);
        RETURN(1);
    }

    /*
     * Mmap and write if less than 8M (the limit is so we don't totally
     * trash memory on big files.  This is really a minor hack, but it
     * wins some CPU back.
     */
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
    if (fs->st_size > 0 && fs->st_size <= 8 * 1048576) {
		if ((p = mmap(NULL, (size_t)fs->st_size, PROT_READ,
		    MAP_FILE|MAP_SHARED, from_fd, (off_t)0)) == MAP_FAILED) {
			USYSLOG(LOG_WARNING, "mmap: %s", cow->from_path);
			rval = 1;
		} else {
			madvise(p, fs->st_size, MADV_SEQUENTIAL);
			if (write(to_fd, p, fs->st_size) != fs->st_size) {
				USYSLOG(LOG_WARNING, "%s", cow->to_path);
				rval = 1;
			}
			/* Some systems don't unmap on close(2). */
			if (munmap(p, fs->st_size) < 0) {
				USYSLOG(LOG_WARNING, "%s", cow->from_path);
				rval = 1;
			}
		}
	} else
#endif
    {
        while ((rcount = read(from_fd, buf, MAXBSIZE)) > 0) {
            wcount = write(to_fd, buf, rcount);
            if (rcount != wcount || wcount == -1) {
                USYSLOG(LOG_WARNING, "%s", cow->to_path);
                rval = 1;
                break;
            }
        }
        if (rcount < 0) {
            USYSLOG(LOG_WARNING, "copy failed: %s", cow->from_path);
            rval = 1;
        }
    }

    if (rval == 1) {
        (void)close(from_fd);
        (void)close(to_fd);
        RETURN(1);
    }

    if (setfile(cow->to_path, cow->stat))
        rval = 1;
        /*
         * If the source was setuid or setgid, lose the bits unless the
         * copy is owned by the same user and group.
         */
#define	RETAINBITS \
	(S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)
    else if (fs->st_mode & (S_ISUID | S_ISGID) && fs->st_uid == cow->uid) {
        if (fstat(to_fd, &to_stat)) {
            USYSLOG(LOG_WARNING, "%s", cow->to_path);
            rval = 1;
        } else if (fs->st_gid == to_stat.st_gid &&
                   fchmod(to_fd, fs->st_mode & RETAINBITS & ~cow->umask)) {
            USYSLOG(LOG_WARNING, "%s", cow->to_path);
            rval = 1;
        }
    }
    (void)close(from_fd);
    if (close(to_fd)) {
        USYSLOG(LOG_WARNING, "%s", cow->to_path);
        rval = 1;
    }

    RETURN(rval);
}

/**
 * copy a link, actually we recreate the link and only copy its stat() data.
 */
int copy_link(struct cow *cow)
{
    DBG("from %s to %s\n", cow->from_path, cow->to_path);

    int len;
    char link[PATHLEN_MAX];

    if ((len = readlink(cow->from_path, link, sizeof(link)-1)) == -1) {
        USYSLOG(LOG_WARNING, "readlink: %s", cow->from_path);
        RETURN(1);
    }

    link[len] = '\0';

    if (symlink(link, cow->to_path)) {
        USYSLOG(LOG_WARNING, "symlink: %s", link);
        RETURN(1);
    }

    RETURN(setlink(cow->to_path, cow->stat));
}

/**
 * copy a fifo, actually we recreate the fifo and only copy
 * its stat() data
 **/
int copy_fifo(struct cow *cow)
{
    DBG("from %s to %s\n", cow->from_path, cow->to_path);

    if (mkfifo(cow->to_path, cow->stat->st_mode)) {
        USYSLOG(LOG_WARNING, "mkfifo: %s", cow->to_path);
        RETURN(1);
    }
    RETURN(setfile(cow->to_path, cow->stat));
}

/**
 * copy a special file, actually we recreate this file and only copy
 * its stat() data
 */
int copy_special(struct cow *cow)
{
    DBG("from %s to %s\n", cow->from_path, cow->to_path);

    if (mknod(cow->to_path, cow->stat->st_mode, cow->stat->st_rdev)) {
        USYSLOG(LOG_WARNING, "mknod: %s", cow->to_path);
        RETURN(1);
    }
    RETURN(setfile(cow->to_path, cow->stat));
}
