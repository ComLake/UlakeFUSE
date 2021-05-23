set(HASHTABLE_SRCS hashtable.c hashtable_itr.c)
set(ULAKEFS_SRCS Ulakefs.c options.c debug.c 
    general.c readrmdir.c
    fuse_operations.c)

add_executable(ulakefs ${ULAKEFS_SRCS} ${HASHTABLE_SRCS})

if (UNIX AND NOT APPLE)
    target_link_libraries(ulakefs fuse pthread rt)
else()
    target_link_libraries(ulakefs fuse pthread)
endif()

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ulakefs DESTINATION bin)