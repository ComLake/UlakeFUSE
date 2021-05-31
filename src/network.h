//
// Created by hoangdm on 18/05/2021.
//

#ifndef ULAKEFS_FUSE_NETWORK_H
#define ULAKEFS_FUSE_NETWORK_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <curl/curl.h>

/**
 * \brief the maximum length of a path and a URL.
 * \details This corresponds the maximum path length under Ext4.
 */
#define MAX_PATH_LEN        4096

/**
 * \brief the maximum length of a filename.
 * \details This corresponds the filename length under Ext4.
 */
#define MAX_FILENAME_LEN    255

/**
 * \brief the default user agent string
 */
#define DEFAULT_USER_AGENT "HTTPDirFS-" VERSION






/**
 * \brief configuration data structure
 * \note The opening curly bracket should be at line 39, so the code belong
 * lines up with the initialisation code in util.c
 */
typedef struct {
    /** \brief HTTP username */
    char *http_username;
    /** \brief HTTP password */
    char *http_password;
    /** \brief HTTP proxy URL */
    char *proxy;
    /** \brief HTTP proxy username */
    char *proxy_username;
    /** \brief HTTP proxy password */
    char *proxy_password;
    /** \brief HTTP maximum connection count */
    long max_conns;
    /** \brief HTTP user agent*/
    char *user_agent;
    /** \brief The waiting time after getting HTTP 429 (too many requests) */
    int http_wait_sec;
    /** \brief Disable check for the server's support of HTTP range request */
    int no_range_check;
    /** \brief Disable TLS certificate verification */
    int insecure_tls;

    /** \brief The Ulake username */
    char *ulakeuser_username;
    /** \brief The Ulake password */
    char *ulakeuser_password;
} ConfigStruct;

/**
 * \brief The Configuration data structure
 */
extern ConfigStruct CONFIG;

/**
 * \brief append a path
 * \details This function appends a path with the next level, while taking the
 * trailing slash of the upper level into account.
 * \note You need to free the char * after use.
 */
char *path_append(const char *path, const char *filename);

/**
 * \brief division, but rounded to the nearest integer rather than truncating
 */
int64_t round_div(int64_t a, int64_t b);

/**
 * \brief wrapper for pthread_mutex_lock(), with error handling
 */
void PTHREAD_MUTEX_LOCK(pthread_mutex_t *x);

/**
 * \brief wrapper for pthread_mutex_unlock(), with error handling
 */
void PTHREAD_MUTEX_UNLOCK(pthread_mutex_t *x);

/**
 * \brief wrapper for exit(EXIT_FAILURE), with error handling
 */
void exit_failure(void);

/**
 * \brief erase a string from the terminal
 */
void erase_string(FILE *file, size_t max_len, char *s);

/**
 * \brief generate the salt for authentication string
 * \details this effectively generates a UUID string, which we use as the salt
 * \return a pointer to a 37-char array with the salt.
 */
char *generate_salt(void);

/**
 * \brief generate the md5sum of a string
 * \param[in] str a character array for the input string
 * \return a pointer to a 33-char array with the salt
 */
char *generate_md5sum(const char *str);

/**
 * \brief wrapper for calloc(), with error handling
 */
void *CALLOC(size_t nmemb, size_t size);

/**
 * \brief Convert a string to hex
 */
char *str_to_hex(char *s);

/**
 * \brief initialise the configuration data structure
 */
void Config_init(void);


/** \brief HTTP response codes */
typedef enum {
    HTTP_OK                         = 200,
    HTTP_PARTIAL_CONTENT            = 206,
    HTTP_RANGE_NOT_SATISFIABLE      = 416,
    HTTP_TOO_MANY_REQUESTS          = 429,
    HTTP_CLOUDFLARE_UNKNOWN_ERROR   = 520,
    HTTP_CLOUDFLARE_TIMEOUT         = 524
} HTTPResponseCode;

/** \brief curl shared interface */
extern CURLSH *CURL_SHARE;

/** \brief perform one transfer cycle */
int curl_multi_perform_once(void);

/** \brief initialise the network module */
void NetworkSystem_init(void);

/** \brief blocking file transfer */
void transfer_blocking(CURL *curl);

/** \brief non blocking file transfer */
void transfer_nonblocking(CURL *curl);

/** \brief callback function for file transfer */
size_t
write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp);

/**
 * \brief check if a HTTP response code corresponds to a temporary failure
 */
int HTTP_temp_failure(HTTPResponseCode http_resp);

/** \brief Link type */
typedef struct Link Link;


/** \brief the link type */
typedef enum {
    LINK_HEAD = 'H',
    LINK_DIR = 'D',
    LINK_FILE = 'F',
    LINK_INVALID = 'I',
    LINK_UNINITIALISED_FILE = 'U'
} LinkType;

/** \brief for storing downloaded data in memory */
typedef struct {
    char *data;
    size_t size;
} DataStruct;

/** \brief specify the type of data transfer */
typedef enum {
    FILESTAT = 's',
    DATA = 'd'
} TransferType;

/** \brief for storing the link being transferred, and metadata */
typedef struct {
    TransferType type;
    int transferring;
    Link *link;
} TransferStruct;

/**
 * \brief link table type
 * \details index 0 contains the Link for the base URL
 */
typedef struct LinkTable LinkTable;

/**
 * \brief Link type data structure
 */
struct Link {
    /** \brief The link name in the last level of the URL */
    char linkname[MAX_FILENAME_LEN+1];
    /** \brief The full URL of the file */
    char f_url[MAX_PATH_LEN+1];
    /** \brief The type of the link */
    LinkType type;
    /** \brief CURLINFO_CONTENT_LENGTH_DOWNLOAD of the file */
    size_t content_length;
    /** \brief The next LinkTable level, if it is a LINK_DIR */
    LinkTable *next_table;
    /** \brief CURLINFO_FILETIME obtained from the server */
    long time;
    /**
     * \brief Ulake id field
     * \details This is used to store the followings:
     *  Discussed with Thao , working atm
     */
    char *ulakeuser_id;
};

struct LinkTable {
    int num;
    Link **links;
};

/**
 * \brief root link table
 */
extern LinkTable *ROOT_LINK_TBL;

/**
 * \brief the offset for calculating partial URL
 */
extern int ROOT_LINK_OFFSET;

/**
 * \brief initialise link sub-system.
 */
LinkTable *LinkSystem_init(const char *raw_url);

/**
 * \brief Add a link to the curl multi bundle for querying stats
 */
void Link_req_file_stat(Link *this_link);

/**
 * \brief Set the stats of a link, after curl multi handle finished querying
 */
void Link_set_file_stat(Link* this_link, CURL *curl);

/**
 * \brief create a new LinkTable
 */
LinkTable *LinkTable_new(const char *url);

/**
 * \brief download a link
 * \return the number of bytes downloaded
 */
long path_download(const char *path, char *output_buf, size_t size,
                   off_t offset);

/**
 * \brief find the link associated with a path
 */
Link *path_to_Link(const char *path);

/**
 * \brief return the link table for the associated path
 */
LinkTable *path_to_Link_LinkTable_new(const char *path);

/**
 * \brief dump a link table to the disk.
 */
int LinkTable_disk_save(LinkTable *linktbl, const char *dirn);

/**
 * \brief load a link table from the disk.
 */
LinkTable *LinkTable_disk_open(const char *dirn);

/**
 * \brief Download a link's content to the memory
 * \warning You MUST free the memory field in DataStruct after use!
 */
DataStruct Link_to_DataStruct(Link *head_link);

/**
 * \brief Allocate a LinkTable
 * \note This does not fill in the LinkTable.
 */
LinkTable *LinkTable_alloc(const char *url);

/**
 * \brief free a LinkTable
 */
void LinkTable_free(LinkTable *linktbl);

/**
 * \brief print a LinkTable
 */
void LinkTable_print(LinkTable *linktbl);

/**
 * \brief add a Link to a LinkTable
 */
void LinkTable_add(LinkTable *linktbl, Link *link);

#endif //ULAKEFS_FUSE_NETWORK_H
