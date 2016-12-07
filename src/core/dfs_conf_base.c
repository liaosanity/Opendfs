#include "dfs_conf_base.h"
#include "dfs_array.h"
#include "dfs_list.h"
#include "dfs_memory.h"

#define CONF_BIND_TYPE_ERROR 0
#define CONF_BIND_TYPE_PORT  1
#define CONF_BIND_TYPE_IP    2
#define CONF_BIND_TYPE_ADDR  3

#define CONF_SERVER_DEF_PORT 80
#define CONF_SERVER_DEF_IP   "0.0.0.0"

#define CONF_INVALID_DIGITAL_LEN(len, digtal_type) \
    (len) > CONF_MAX_LEN_##digtal_type ? DFS_TRUE : DFS_FALSE
    
int conf_parse_int_array(conf_variable_t *v, uint32_t offset, int type,
                                string_t *args, int args_n)
{
    int      i = 0;
    int     *code = NULL;
    array_t *codes = NULL;

    if (type != OPE_EQUAL)
	{
        return DFS_ERROR;
    }

    if (args_n > CONF_TAKE12) 
	{
        return DFS_ERROR;
    }

    codes = (array_t *)((uchar_t *)v->conf + offset);

    for (i = CONF_TAKE2; i < args_n; i++) 
	{
        code = (int *)array_push(codes);
        *code = string_xxstrtoui(args[i].data, args[i].len);
    }

    return DFS_OK;
}

int conf_parse_short(conf_variable_t *v, uint32_t offset, int type,
                           string_t *args, int args_n)
{
    short *port = 0;
    int    vi = 0;

    if (type != OPE_EQUAL) 
	{
        return DFS_ERROR;
    }

    if (args_n != CONF_TAKE3) 
	{
        return DFS_ERROR;
    }

    vi = args_n - 1;
    if (CONF_INVALID_DIGITAL_LEN(args[vi].len, SHORT)) 
	{
        return DFS_ERROR;
    }
	
    port = (short *)((uchar_t *)v->conf + offset);
    *port = (short)string_xxstrtoui(args[vi].data, args[vi].len);
	
    if (*port <= 0) 
	{
        return DFS_ERROR;
    }

    return DFS_OK;
}

int conf_parse_bytes_size(conf_variable_t *v, uint32_t offset, int type,
                                  string_t *args, int args_n)
{
    size_t    i = 0;
    int       vi = 0;
    char      unit[20] = {0};
    char     *p_unit = NULL;
    uint64_t *size = NULL;
    uint64_t  b_size = 0;

    if ( (type != OPE_EQUAL) || !v ) 
	{
        return DFS_ERROR;
    }

    if (args_n != CONF_TAKE3) 
	{
        return DFS_ERROR;
    }

    vi = args_n - 1;

    p_unit = unit;
    size = (uint64_t *)((uchar_t *)v->conf + offset);
	
    for (i = 0; i < args[vi].len; i++) 
	{
        if ((args[vi].data[i] < '0') || (args[vi].data[i] > '9')) 
		{
            break;
        }
		
        b_size = b_size * 10 + (args[vi].data[i] - '0');
    }

    for (; i < args[vi].len; i++) 
	{
        *p_unit++ = toupper(args[vi].data[i]);
    }

    *p_unit = 0;

    if (string_strncasecmp(unit, "KB", 2) == 0) 
	{
        b_size = b_size << 10;
    } 
	else if (string_strncasecmp(unit, "MB", 2) == 0) 
    {
        b_size = b_size << 20;
    } 
	else if (string_strncasecmp(unit, "GB", 2) == 0) 
	{
        b_size = b_size << 30;
    } 
	else if (string_strncasecmp(unit, "TB", 2) == 0) 
	{
        b_size = b_size << 40;
    } 
	else if (string_strncasecmp(unit, "PB", 2) == 0) 
	{
        b_size = b_size << 50;
    } 
	else if (string_strncasecmp(unit, "B", 1) == 0) 
	{   
    } 
	else 
	{
        return DFS_ERROR;
    }
	
    *size = b_size;

    return DFS_OK;
}

int conf_parse_percent(conf_variable_t *v, uint32_t offset, int type,
                              string_t *args, int args_n)
{
    uint32_t  i = 0;
    int       vi = 0;
    double   *perc = NULL;
    uint32_t  b_size = 0;

    if (type != OPE_EQUAL || args_n != CONF_TAKE3) 
    {
        return DFS_ERROR;
	}

    vi = args_n - 1;

    perc = (double *)((uchar_t *)v->conf + offset);
	
    for (i = 0; i < args[vi].len; i++) 
	{
        if ((args[vi].data[i] < '0') || (args[vi].data[i] > '9')) 
		{
            if (args[vi].data[i] == '%' &&
                ((i < 3 && b_size < 100) || (i == 3 && b_size == 100)))
            {
                break;
            }
			else 
			{
                return DFS_ERROR;
            }
        }
		
        b_size = b_size * 10 + (args[vi].data[i] - '0');
    }

    *perc = (double )b_size / 100;

    return DFS_OK;
}

int conf_parse_string(conf_variable_t *v, uint32_t offset, int type,
                            string_t *args, int args_n)
{
    string_t  *str = NULL;

    if (type != OPE_EQUAL) 
	{
        return DFS_ERROR;
    }

    if (args_n != CONF_TAKE3) 
	{
        return DFS_ERROR;
    }

    str = (string_t *)((uchar_t *)v->conf + offset);
    if (str->len > 0) 
	{
        return DFS_ERROR;
    }
	
    str->data = string_xxdup(&args[args_n -1]);
    str->len = args[args_n -1].len;
	
    return DFS_OK;
}

int conf_parse_int(conf_variable_t *v, uint32_t offset, 
	                    int type, string_t *args, int args_n)
{
    int *p = NULL;
    int  vi = 0;

    if (type != OPE_EQUAL) 
	{
        return DFS_ERROR;
    }

    if (args_n != CONF_TAKE3) 
	{
        return DFS_ERROR;
    }

    vi = args_n - 1;

    if (CONF_INVALID_DIGITAL_LEN(args[args_n - 1].len, INT))
	{
        return DFS_ERROR;
    }
	
    p = (int *)((uchar_t *)v->conf + offset);
    *p = (int)string_xxstrtoui(args[vi].data, args[vi].len);
	
    if (*p == DFS_ERROR) 
	{
        return DFS_ERROR;
    }

    return DFS_OK;
}

int conf_parse_deftime_t(conf_variable_t *v, uint32_t offset, int type,
                                string_t *args, int args_n)
{
    time_t *p = NULL;
    int     rc = 0;
	
    p = (time_t *)((uchar_t *)v->conf + offset);
    if (*p != CONF_TIME_T_NOT_SET) 
	{
        return DFS_ERROR;
    }
	
    rc = conf_parse_time_t(v, offset, type, args, args_n);
	
    return rc;
}

int conf_parse_time_t(conf_variable_t *v, uint32_t offset, int type,
                            string_t *args, int args_n)
{
    time_t *p = NULL;
    int     vi = 0;

    if (type != OPE_EQUAL) 
	{
        return DFS_ERROR;
    }
	
    if (args_n != CONF_TAKE3) 
	{
        return DFS_ERROR;
    }

    vi = args_n - 1;

    if (CONF_INVALID_DIGITAL_LEN(args[vi].len, TIME_T)) 
	{
        return DFS_ERROR;
    }
	
    p = (time_t *)((uchar_t *)v->conf + offset);
    *p = string_xxstrtotime(args[vi].data, args[vi].len);
	
    if (*p == DFS_ERROR) 
	{
        return DFS_ERROR;
    }

    return DFS_OK;
}

int conf_parse_macro(conf_variable_t *v, uint32_t offset, int type,
                            string_t *args, int args_n, conf_macro_t *conf_macro)
{
    int       i = 0;
    int       vi = 0;
    size_t    len = 0;
    uint32_t *p = 0;
    uchar_t  *start = NULL;
    uchar_t  *end = NULL;
    uchar_t  *pos = NULL;
    int       matched;

    if (type != OPE_EQUAL) 
	{
        return DFS_ERROR;
    }

    if (args_n != CONF_TAKE3) 
	{
        return DFS_ERROR;
    }

    vi = args_n - 1;
	
    if (args[vi].len == 0 || !args[vi].data) 
	{
        return DFS_ERROR;
    }

    p = (uint32_t *)((uchar_t *)v->conf + offset);
    start = pos = args[vi].data;
    end = start + args[vi].len;
    len = end - start;
	
    // single value type, if the init value is -1, *p |= is invalid
    if (strchr((char *)start, '|') == NULL) 
	{
        for (i = 0; conf_macro[i].name.len > 0; i++) 
		{
            if (conf_macro[i].name.len == len
                && (string_xxstrncasecmp(start,
                conf_macro[i].name.data, len) == 0)) 
            {
                *p = conf_macro[i].value;
				
                return DFS_OK;
            }
        }
		
        return DFS_ERROR;
    }
	
    // multi value type, the init value should be uint32_t
    while ((pos = (uchar_t *)strchr((char *)start, '|')) != NULL) 
	{
        len = pos - start;
        i = 1;
        matched = 0;
		
        while (*(pos - i) == ' ') 
		{
            i++;
            len--;
        }
		
        for (i = 0; conf_macro[i].name.len > 0; i++) 
		{
            if (conf_macro[i].name.len == len &&
                (string_xxstrncasecmp(start, conf_macro[i].name.data, len) == 0)) 
            {
                *p |= conf_macro[i].value;
                matched = 1;
				
                break;
            }
        }
		
        if (!matched) 
		{
            return DFS_ERROR;
        }
		
        start = pos + 1;
    }
	
    len = end - start;
    i = 1;
	
    while (*(end - i) == ' ') 
	{
        i++;
        len--;
    }
	
    for (i = 0; conf_macro[i].name.len > 0; i++) 
	{
        if (conf_macro[i].name.len == len
            && (string_xxstrncasecmp(start, conf_macro[i].name.data, len) == 0)) 
        {
            *p |= conf_macro[i].value;
			
            return DFS_OK;
        }
    }

    return DFS_ERROR;
}

int conf_parse_list_string(conf_variable_t *v, uint32_t offset, int type,
                                 string_t *args, int args_n)
{
    list_t   *l = NULL;
    string_t *p = NULL;

    if (type != OPE_EQUAL || args_n != CONF_TAKE3) 
	{
        return DFS_ERROR;
    }

    l = (list_t *)((uchar_t *)v->conf + offset);
    p = (string_t *)list_push(l);
    if (p == NULL) 
	{
        return DFS_ERROR;
    }
	
    p->data = string_xxxpdup(l->pool, args[args_n -1].data, args[args_n -1].len);
    p->len = args[args_n -1].len;
	
    return DFS_OK;
}

static int conf_parse_valid_bind_ipv4(string_t *bind_str)
{
    size_t  i = 0;
    char   *port = NULL;
    char   *ip = NULL;
    int     type = CONF_BIND_TYPE_ERROR;

    if (!bind_str || !bind_str->len) 
	{
        return CONF_BIND_TYPE_ERROR;
    }

    if (bind_str->len >= sizeof("255.255.255.255:65535")) 
	{
        return CONF_BIND_TYPE_ERROR;
    }

    if (!string_isalnum(bind_str->data[bind_str->len - 1])) 
	{
        return CONF_BIND_TYPE_ERROR;
    }

    if (!string_isalnum(bind_str->data[0])) 
	{
        return CONF_BIND_TYPE_ERROR;
    }

    for (i = 0; i < bind_str->len; i++)
	{
        if (bind_str->data[i] != '.' && bind_str->data[i] != ':'
            && !string_isalnum(bind_str->data[i])) 
        {
            return CONF_BIND_TYPE_ERROR;
        }
    }

    ip = string_strchr(bind_str->data, '.');
    port = string_strchr(bind_str->data, ':');
	
    if (ip && port) 
	{
        type = CONF_BIND_TYPE_ADDR;
    } 
	else if(ip && !port) 
	{
        type = CONF_BIND_TYPE_IP;
    } 
	else 
	{
        type = CONF_BIND_TYPE_PORT;
    }

    return type;
}

static int bind_ipv4_str_cmp(void *b1, void *b2)
{
	server_bind_t *c1 = (server_bind_t *)b1;
	string_t      *str = (string_t *)b2;
	uchar_t       *sp = NULL;
	uint16_t       port = 0;

    sp = (uchar_t *)string_strchr(str->data, ':');
    port = sp ? (uint16_t)string_xxstrtoi(sp + 1, string_strlen(sp + 1)) :
        CONF_SERVER_DEF_PORT;
		
    if (!string_strcmp(c1->addr.data, "0.0.0.0")
    	|| string_strstr(str->data, "0.0.0.0")) 
    {
    	if (port == c1->port) 
		{
            return DFS_TRUE;
        }
    }

    if (port == c1->port
    	&& !string_strncmp(c1->addr.data, str->data, c1->addr.len)) 
    {
    	return DFS_TRUE;
    }
	
    return DFS_FALSE;
}

int conf_parse_bind(conf_variable_t *v, uint32_t offset, int type,
                          string_t *args, int args_n)
{
    array_t       *l = NULL;
    server_bind_t *bind = NULL;
    uchar_t       *sp = NULL;
    size_t         len = 0;
    int            vi = 0;
    uint32_t       port = 0;
    int            bind_type = -1;
    string_t       tmp_bind_str = string_null;
    pool_t        *pool = NULL;

    if (type != OPE_EQUAL || args_n != CONF_TAKE3) 
	{
        return DFS_ERROR;
    }

    vi = args_n - 1;

    bind_type = conf_parse_valid_bind_ipv4(&args[vi]);
    if (bind_type == CONF_BIND_TYPE_ERROR) 
	{
        return DFS_ERROR;
    }
    
    l = (array_t *)((uchar_t *)v->conf + offset);
    pool = l->pool;
	
    if (bind_type == CONF_BIND_TYPE_PORT) 
	{
        len = sizeof("255.255.255.255:65535");
        tmp_bind_str.data = (uchar_t *)memory_calloc(len);
        len = sizeof(CONF_SERVER_DEF_IP) - 1;

        *(memory_cpymem(tmp_bind_str.data, CONF_SERVER_DEF_IP, len)) = ':';

        tmp_bind_str.len = len + 1;

        memory_memcpy(&tmp_bind_str.data[tmp_bind_str.len], args[vi].data,
            args[vi].len);

        tmp_bind_str.len += args[vi].len;

        string_swap(&tmp_bind_str, &args[vi]);
    }

    if (array_find(l, &args[vi], bind_ipv4_str_cmp)) 
	{
        return DFS_ERROR;
    }

    sp = (uchar_t *)string_strchr(args[vi].data, ':');
    if (sp) 
	{
        *sp = 0;
        port = (uint32_t)string_xxstrtoi(sp + 1,
            string_strlen(sp + 1));
        len = string_strlen(args[vi].data);
    } 
	else 
	{
        port = CONF_SERVER_DEF_PORT;
        len = args[vi].len;
    }

    if (!port || port > 65535) 
	{
        return DFS_ERROR;
    }

    bind = (server_bind_t *)array_push(l);
    bind->port = (uint16_t)port;
    bind->addr.data = string_xxxpdup(pool, args[vi].data, len);
	
    if (!bind->addr.data) 
	{
        return DFS_ERROR;
    }

    bind->addr.len = len;
    if (sp) 
	{
        *sp = ':';
    }
    
    if (tmp_bind_str.data && tmp_bind_str.len) 
	{
        string_swap(&args[vi], &tmp_bind_str);
        memory_free(tmp_bind_str.data, tmp_bind_str.len);
    }

    return DFS_OK;
}

