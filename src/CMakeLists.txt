set(HASHTABLE_SRCS hashtable.c hashtable_itr.c)
set(ULAKEFS_SRCS Ulakefs.c options.c debug.c 
    general.c
    fuse_ops.c)

add_executable(unionfs ${ULAKEFS_SRCS} ${HASHTABLE_SRCS})

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ulakefs DESTINATION bin)
