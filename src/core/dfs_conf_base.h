#ifndef DFS_CONF_BASE_H
#define DFS_CONF_BASE_H

#include "dfs_types.h"
#include "dfs_string.h"

#define CONF_MAX_LEN_INT    (10UL)
#define CONF_MAX_LEN_UINT32 (10UL)
#define CONF_MAX_LEN_SIZE   (20UL)
#define CONF_MAX_LEN_TIME_T (20UL)
#define CONF_MAX_LEN_SHORT  (5UL)

#define CONF_SIZE_NOT_SET  0
#define CONF_INT_NOT_SET  -1

#define CONF_TIME_T_NOT_SET ((time_t)-1)
#define CONF_TIME_T_DEFAULT 0

enum 
{
    CONF_TAKE1 = 1,
    CONF_TAKE2,
    CONF_TAKE3,
    CONF_TAKE4,
    CONF_TAKE5,
    CONF_TAKE6,
    CONF_TAKE7,
    CONF_TAKE8,
    CONF_TAKE9,
    CONF_TAKE10,
    CONF_TAKE11,
    CONF_TAKE12
};

enum 
{
    OPE_REGEX = 1,
    OPE_EQUAL,
    OPE_OR,
    OPE_NOT_REGEX,
    OPE_NOT_EQUAL,
    OPE_UNKNOW
};

typedef struct conf_variable_s conf_variable_t;
typedef struct conf_macro_s    conf_macro_t;
typedef struct conf_option_s   conf_option_t;
typedef struct server_bind_s   server_bind_t;

struct conf_variable_s 
{
    string_t       name;
    string_t       obj_name;
    void          *conf;
    conf_option_t *option;
    int           (*make_default)(void *);
};

struct conf_macro_s 
{
    string_t name;
    uint32_t value;
};

typedef int (*conf_parse_opt_pt)(conf_variable_t *v, uint32_t offset, int type, string_t *value, int n);

struct conf_option_s 
{
    string_t          option;
    conf_parse_opt_pt handler;
    int               type;
    uint32_t          offset;
};

struct server_bind_s
{
    string_t    addr;
    uint16_t    port;
};

int conf_parse_short(conf_variable_t *v, uint32_t offset, int type,
    string_t *args, int args_n);
int conf_parse_string(conf_variable_t *v, uint32_t offset, int type,
    string_t *args, int args_n);
int conf_parse_time_t(conf_variable_t *v, uint32_t offset, int type,
    string_t *args, int args_n);
int conf_parse_int(conf_variable_t *v, uint32_t offset, int type,
    string_t *args, int args_n);
int conf_parse_macro(conf_variable_t *v, uint32_t offset, int type,
    string_t *args, int args_n, conf_macro_t *conf_macro);
int conf_parse_list_string(conf_variable_t *v, uint32_t offset,
    int type, string_t *args, int args_n);
int conf_parse_bytes_size(conf_variable_t *v, uint32_t offset,
    int type, string_t *args, int args_n);
int conf_parse_percent(conf_variable_t *v, uint32_t offset, int type,
    string_t *args, int args_n);
int conf_parse_deftime_t(conf_variable_t *v, uint32_t offset, int type,
    string_t *args, int args_n);
int conf_parse_bind(conf_variable_t *v, uint32_t offset, int type,
    string_t *args, int args_n);

#endif

