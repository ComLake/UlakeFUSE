//
// Created by hoangdm on 11/05/2021.
//

#ifndef ULAKEFS_FUSE_HTTP_H
#define ULAKEFS_FUSE_HTTP_H

#include <jansson.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <curl/curl.h>

#define HTTP_FLAG_LAZY_SSL          (1U << 0)

typedef struct http http;

typedef int     (*DataHandler) (http * conn, void *data);

http         *http_create(void);

void            http_destroy(http * conn);

void            http_set_connect_flags(http * conn, unsigned int flags);

void            http_set_data_handler(http * conn,
                                      DataHandler data_handler, void *cb_data);

int             http_get_buf(http * conn, const char *url);

int             http_post_buf(http * conn, const char *url,
                              const char *post_args);

int             http_get_file(http * conn, const char *url,
                              const char *path);

json_t         *http_parse_buf_json(http * conn, size_t flags,
                                    json_error_t * error);

int             http_post_file(http * conn, const char *url, FILE * fh,
                               struct curl_slist **custom_headers,
                               uint64_t filesize);

char           *urlencode(const char *input);

#endif //ULAKEFS_FUSE_HTTP_H
