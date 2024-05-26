#ifndef HTTP_H
# define HTTP_H

# include <stddef.h>

# define CONTENT_TYPE "Content-Type"
# define CONTENT_LENGTH "Content-Length"
# define USER_AGENT "User-Agent"
# define TEXT_PLAIN "text/plain"
# define APPLICATION_OCTET_STREAM "application/octet-stream"

typedef struct header_s
{
    char *key;
    char *value;
    struct header_s *next;
} header_t;

typedef enum
{
    GET,
    POST,
    method_t_size
} method_t;

typedef enum
{
    OK,
    CREATED,
    NOT_FOUND,
    status_t_size
} status_t;

typedef struct
{
    method_t method;
    char *path;
    header_t *headers;
    unsigned char *body;
    size_t body_length;
} request_t;

typedef struct
{
    status_t status;
    header_t *headers;
    unsigned char *body;
    size_t body_length;
} response_t;

extern const int STATUS_TO_CODE[status_t_size];
extern const char *STATUS_TO_PHRASE[status_t_size];

method_t method_valueof(const char *input);

header_t *headers_add(header_t *previous, const char *key, const char *value);
header_t *headers_add_number(header_t *previous, const char *key, size_t value);
void headers_clear(header_t *header);
const char *headers_get(header_t *first, const char *key);

#endif
