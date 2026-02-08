#if defined(__PRX__)
#include "lv2_stdio.h"
#else
#include <stdint.h>
#include <string.h>
#endif
#include "../shared/macros.h"

static bool is_comment_or_empty(const char* line)
{
    const char* p = line;
    while (*p && isspace(*p))
    {
        p++;
    }
    return (*p == '#' || *p == '\0');
}

static size_t get_indent_level(const char* line)
{
    size_t count = 0;
    while (line[count] == ' ')
    {
        count++;
    }
    return count;
}

static char* trim(char* str)
{
    char* end;
    while (isspace((unsigned char)*str))
    {
        str++;
    }
    if (*str == 0)
    {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
    {
        end--;
    }
    end[1] = '\0';
    return str;
}

static char* str_dup(const char* str)
{
#if !defined(__PRX__)
    return strdup(str);
#else
    if (!str)
    {
        return NULL;
    }
    char* dup = malloc(strlen(str) + 1);
    if (dup)
    {
        strcpy(dup, str);
    }
    return dup;
#endif
}

static int parse_hex_digit(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

static int parse_octal_digit(char c)
{
    if (c >= '0' && c <= '7')
    {
        return c - '0';
    }
    return -1;
}

static size_t unescape_char(const char* src, char* dest)
{
    if (src[0] != '\\')
    {
        *dest = src[0];
        return 1;
    }

    switch (src[1])
    {
        case 'n':
            *dest = '\n';
            return 2;
        case 't':
            *dest = '\t';
            return 2;
        case 'r':
            *dest = '\r';
            return 2;
        case 'b':
            *dest = '\b';
            return 2;
        case 'f':
            *dest = '\f';
            return 2;
        case 'v':
            *dest = '\v';
            return 2;
        case 'a':
            *dest = '\a';
            return 2;
        case '\\':
            *dest = '\\';
            return 2;
        case '\'':
            *dest = '\'';
            return 2;
        case '"':
            *dest = '"';
            return 2;
        case '?':
            *dest = '?';
            return 2;
        case '0':
            if (src[2] == '\0' || !isdigit(src[2]))
            {
                *dest = '\0';
                return 2;
            }
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        {
            int value = 0;
            size_t consumed = 1;
            for (size_t i = 0; i < 3 && src[consumed] != '\0'; i++)
            {
                int digit = parse_octal_digit(src[consumed]);
                if (digit < 0)
                {
                    break;
                }
                value = value * 8 + digit;
                consumed++;
            }
            *dest = (char)value;
            return consumed;
        }
        case 'x':
        {
            int value = 0;
            size_t consumed = 2;
            size_t digits = 0;
            for (size_t i = 0; i < 2 && src[consumed] != '\0'; i++)
            {
                int digit = parse_hex_digit(src[consumed]);
                if (digit < 0)
                {
                    break;
                }
                value = value * 16 + digit;
                consumed++;
                digits++;
            }
            if (digits == 0)
            {
                *dest = 'x';
                return 2;
            }
            *dest = (char)value;
            return consumed;
        }
        default:
            *dest = src[1];
            return 2;
    }
}

static char* unescape_string(const char* str)
{
    if (!str)
    {
        return NULL;
    }

    size_t len = strlen(str);
    char* result = malloc(len + 1);
    if (!result)
    {
        return NULL;
    }

    size_t src_pos = 0;
    size_t dest_pos = 0;

    while (src_pos < len)
    {
        size_t consumed = unescape_char(str + src_pos, result + dest_pos);
        src_pos += consumed;
        dest_pos++;
    }

    result[dest_pos] = '\0';
    return result;
}

static char* parse_quoted_string(const char* str)
{
    const char* start = strchr(str, '"');
    if (!start)
    {
        return NULL;
    }
    start++;

    const char* end = start;
    while (*end)
    {
        if (*end == '\\' && *(end + 1))
        {
            end += 2;
            continue;
        }
        if (*end == '"')
        {
            break;
        }
        end++;
    }

    if (*end != '"')
    {
        return NULL;
    }

    size_t len = end - start;
    char* escaped = malloc(len + 1);
    if (!escaped)
    {
        return NULL;
    }

    strncpy(escaped, start, len);
    escaped[len] = '\0';

    char* result = unescape_string(escaped);
    free(escaped);

    return result;
}

static bool is_list_value(const char* str)
{
    const char* p = strchr(str, ':');
    if (!p)
    {
        return false;
    }
    p++;
    while (*p && isspace(*p))
    {
        p++;
    }
    return (*p == '[');
}

static size_t parse_string_list(const char* str, char** output, size_t max_items)
{
    char buffer[MAX_LINE_LENGTH + 1] = {0};
    strncpy(buffer, str, _countof_1(buffer));

    char* start = strchr(buffer, '[');
    if (!start)
    {
        return 0;
    }
    start++;

    char* end = start;
    bool in_quote = false;
    while (*end)
    {
        if (*end == '\\' && *(end + 1))
        {
            end += 2;
            continue;
        }
        if (*end == '"')
        {
            in_quote = !in_quote;
        }
        if (!in_quote && *end == ']')
        {
            break;
        }
        end++;
    }

    if (*end != ']')
    {
        return 0;
    }
    *end = '\0';

    size_t count = 0;
    char* pos = start;

    while (*pos && count < max_items)
    {
        while (*pos && isspace(*pos))
        {
            pos++;
        }
        if (!*pos)
        {
            break;
        }

        if (*pos == '"')
        {
            char* quote_start = pos;
            pos++;

            while (*pos && !(*pos == '"' && *(pos - 1) != '\\'))
            {
                if (*pos == '\\' && *(pos + 1))
                {
                    pos += 2;
                }
                else
                {
                    pos++;
                }
            }

            if (*pos == '"')
            {
                pos++;
                size_t quote_len = pos - quote_start;
                char temp[MAX_LINE_LENGTH + 1] = {0};
                strncpy(temp, quote_start, quote_len);
                temp[quote_len] = '\0';

                output[count] = parse_quoted_string(temp);
                if (output[count])
                {
                    count++;
                }
            }
        }

        while (*pos && *pos != ',')
        {
            pos++;
        }
        if (*pos == ',')
        {
            pos++;
        }
    }

    return count;
}
