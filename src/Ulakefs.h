//
// Created by hoangdm on 20/04/2021.
//

#ifndef ULAKEFS_FUSE_ULAKEFS_H
#define ULAKEFS_FUSE_ULAKEFS_H

#define PATHLEN_MAX 1024
#define HIDETAG "_HIDDEN~"

#define METANAME ".ulakefs"
#define METADIR (METANAME  "/") // string

// fuse meta files, we might want to hide those
#define FUSE_META_FILE ".fuse_hidden"
#define FUSE_META_LENGTH 12

#ifndef IOCPARM_MASK
#define IOCPARM_MASK      0x1FFF
#endif
#ifndef IOCPARM_LEN
#define IOCPARM_LEN(a)    (((a) >> 16) & IOCPARM_MASK)
#endif

#ifndef _IOC_SIZE
#ifdef IOCPARM_LEN
#define _IOC_SIZE(x) IOCPARM_LEN(x)
#else
#error "No mechanism for determining ioctl length found."
#endif
#endif

// file access protection mask
#define S_PROT_MASK (S_ISUID| S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)

typedef struct {
    char *path;
    int path_len;		// strlen(path)
    int fd;			 // used to prevent accidental umounts of path
    unsigned char rw;	 // the writable flag
} branch_entry_t;

extern struct fuse_operations ulakefs_oper;


#endif //ULAKEFS_FUSE_ULAKEFS_H
