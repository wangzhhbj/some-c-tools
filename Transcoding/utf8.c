

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

 
#define WIND_ERR_OVERRUN -1 
#define WIND_ERR_INVALID_UTF8 -2
#define WIND_ERR_INVALID_UTF16 -3
#define WIND_ERR_INVALID_UTF32 -4
#define WIND_ERR_LENGTH_NOT_MOD2 -5

#define WIND_ERR_NOT_UTF16 -6

#define WIND_ERR_NO_BOM -7

/* flags to wind_ucs2read/wind_ucs2write */
#define WIND_RW_LE    1
#define WIND_RW_BE    2
#define WIND_RW_BOM   4

/*
 *---------------------------------------------------
 * ucs2              : UTF8
 * utf32             1 Bytes 0xxxxxxx 
 * utf32             2 Bytes 110xxxxx 10xxxxxx 
 * utf32             3 Bytes 1110xxxx 10xxxxxx 10xxxxxx 
 * utf32             4 Bytes 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx 
 */
 
static int
utf8toutf32(const unsigned char **pp, uint32_t *out)
{
    const unsigned char *p = *pp;
    unsigned c = *p;

    if (c & 0x80)  // c & 0b10000000,返回值非0表明是超过1个字节的编码
    {
        if ((c & 0xE0) == 0xC0)   // 2字节编码场景,(c & 0b11100000)==0b11000000,符合 110xxxxx 
        {
            const unsigned c2 = *++p;  // 判断下一个字节,符合10xxxxxx 
            if ((c2 & 0xC0) == 0x80)   //  (c2 & 0b11000000)==0b10000000 
            {
                *out =  ((c  & 0x1F) << 6) | (c2 & 0x3F);
            } 
            else // 不符合110xxxxx 10xxxxxx 
            {
                return WIND_ERR_INVALID_UTF8;
            }
        } 
        else if ((c & 0xF0) == 0xE0)    //3字节场景 (c & 0b11110000)==0b11100000,满足1110xxxx
        {
            const unsigned c2 = *++p;
            if ((c2 & 0xC0) == 0x80)    //判断下一字节 (c2 & 0b11000000)==0b10000000,满足10xxxxxx
            {
                const unsigned c3 = *++p;
                if ((c3 & 0xC0) == 0x80)    //判断下一字节 (c3 & 0b11000000)==0b10000000,满足10xxxxxx
                {
                    *out =   ((c  & 0x0F) << 12)
                    | ((c2 & 0x3F) << 6)
                    |  (c3 & 0x3F);
                } 
                else 
                {
                    return WIND_ERR_INVALID_UTF8;
                }
            } 
            else 
            {
                return WIND_ERR_INVALID_UTF8;
            }
        } 
        else if ((c & 0xF8) == 0xF0)     // (c & 0b11111000)==0b11110000  4字节场景
        {
            const unsigned c2 = *++p;
            if ((c2 & 0xC0) == 0x80)      // 3字节, (c2 & 0b11000000)==0b10000000,满足10xxxxxx
            {
                const unsigned c3 = *++p;
                if ((c3 & 0xC0) == 0x80)       // 2字节, (c2 & 0b11000000)==0b10000000,满足10xxxxxx
                {
                    const unsigned c4 = *++p;
                    if ((c4 & 0xC0) == 0x80)   // 1字节, (c2 & 0b11000000)==0b10000000,满足10xxxxxx
                    {
                        *out =   ((c  & 0x07) << 18)
                            | ((c2 & 0x3F) << 12)
                            | ((c3 & 0x3F) <<  6)
                            |  (c4 & 0x3F);
                    } 
                    else 
                    {
                        return WIND_ERR_INVALID_UTF8;
                    }
                } 
                else 
                {
                    return WIND_ERR_INVALID_UTF8;
                }
            } 
            else 
            {
                return WIND_ERR_INVALID_UTF8;
            }
        } 
        else 
        {
            return WIND_ERR_INVALID_UTF8;
        }
    } 
    else 
    {
        *out = c;  //单个字节场景
    }

    *pp = p;

    return 0;
}

/**
 * Convert an UTF-8 string to an UCS4 string.
 *
 * @param in an UTF-8 string to convert.
 * @param out the resulting UCS4 strint, must be at least
 * wind_utf8ucs4_length() long.  If out is NULL, the function will
 * calculate the needed space for the out variable (just like
 * wind_utf8ucs4_length()).
 * @param out_len before processing out_len should be the length of
 * the out variable, after processing it will be the length of the out
 * string.
 *
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_utf8ucs4(const char *in, uint32_t *out, size_t *out_len)
{
    const unsigned char *p;
    size_t o = 0;
    int ret;

    for (p = (const unsigned char *)in; *p != '\0'; ++p) 
    {
        uint32_t u;

        ret = utf8toutf32(&p, &u);
        if (ret)
        {
            return ret;
        }

        if (out)
        {
            if (o >= *out_len)
            {
                return WIND_ERR_OVERRUN;
            }
            out[o] = u;
        }
        o++;
    }
    *out_len = o;
    return 0;
}

/**
 * Calculate the length of from converting a UTF-8 string to a UCS4
 * string.
 *
 * @param in an UTF-8 string to convert.
 * @param out_len the length of the resulting UCS4 string.
 *
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_utf8ucs4_length(const char *in, size_t *out_len)
{
    return wind_utf8ucs4(in, NULL, out_len);
}

/*  0b00000000  0b11000000  0b11100000  0b11110000 */
static const char first_char[4] = { 0x00, 0xC0, 0xE0, 0xF0 };

/**
 * Convert an UCS4 string to a UTF-8 string.
 *
 * @param in an UCS4 string to convert.
 * @param in_len the length input array.

 * @param out the resulting UTF-8 strint, must be at least
 * wind_ucs4utf8_length() + 1 long (the extra char for the NUL).  If
 * out is NULL, the function will calculate the needed space for the
 * out variable (just like wind_ucs4utf8_length()).

 * @param out_len before processing out_len should be the length of
 * the out variable, after processing it will be the length of the out
 * string.
 *
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_ucs4utf8(const uint32_t *in, size_t in_len, char *out, size_t *out_len)
{
    uint32_t ch;
    size_t i, len, o;

    for (o = 0, i = 0; i < in_len; i++)
    {
        ch = in[i];

        if (ch < 0x80)
        {
            len = 1;
        } 
        else if (ch < 0x800) 
        {
            len = 2;
        } 
        else if (ch < 0x10000) 
        {
            len = 3;
        } 
        else if (ch <= 0x10FFFF) 
        {
            len = 4;
        } 
        else
        {
            return WIND_ERR_INVALID_UTF32;
        }

        o += len;

        if (out) 
        {
            if (o >= *out_len)
            {
                return WIND_ERR_OVERRUN;
            }

            switch(len) 
            {
            case 4:
                out[3] = (ch | 0x80) & 0xbf;
                ch = ch >> 6;
            case 3:
                out[2] = (ch | 0x80) & 0xbf;
                ch = ch >> 6;
            case 2:
                out[1] = (ch | 0x80) & 0xbf;
                ch = ch >> 6;
            case 1:
                out[0] = ch | first_char[len - 1];
            }
        }
        out += len;
    }
    if (out) 
    {
        if (o + 1 >= *out_len)
        {
            return WIND_ERR_OVERRUN;
        }
        *out = '\0';
    }
    *out_len = o;
    return 0;
}

/**
 * Calculate the length of from converting a UCS4 string to an UTF-8 string.
 *
 * @param in an UCS4 string to convert.
 * @param in_len the length of UCS4 string to convert.
 * @param out_len the length of the resulting UTF-8 string.
 *
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_ucs4utf8_length(const uint32_t *in, size_t in_len, size_t *out_len)
{
    return wind_ucs4utf8(in, in_len, NULL, out_len);
}

/**
 * Read in an UCS2 from a buffer.
 *
 * @param ptr The input buffer to read from.
 * @param len the length of the input buffer.
 * @param flags Flags to control the behavior of the function.
 * @param out the output UCS2, the array must be at least out/2 long.
 * @param out_len the output length
 *
 * @return returns 0 on success, an wind error code otherwise.
 * @ingroup wind
 */

int
wind_ucs2read(const void *ptr, size_t len, unsigned int *flags,
          uint16_t *out, size_t *out_len)
{
    const unsigned char *p = ptr;
    int little = ((*flags) & WIND_RW_LE);
    size_t olen = *out_len;

    /** if len is zero, flags are unchanged */
    if (len == 0)
    {
        *out_len = 0;
        return 0;
    }

    /** if len is odd, WIND_ERR_LENGTH_NOT_MOD2 is returned */
    if (len & 1)
    {
        return WIND_ERR_LENGTH_NOT_MOD2;
    }

    /**
     * If the flags WIND_RW_BOM is set, check for BOM. If not BOM is
     * found, check is LE/BE flag is already and use that otherwise
     * fail with WIND_ERR_NO_BOM. When done, clear WIND_RW_BOM and
     * the LE/BE flag and set the resulting LE/BE flag.
     */
    if ((*flags) & WIND_RW_BOM)
    {
        uint16_t bom = (p[0] << 8) + p[1];
        if (bom == 0xfffe || bom == 0xfeff)
        {
            little = (bom == 0xfffe);
            p += 2;
            len -= 2;
        }
        else if (((*flags) & (WIND_RW_LE|WIND_RW_BE)) != 0)
        {
            /* little already set */
        }
        else
        {
            return WIND_ERR_NO_BOM;
        }
        *flags = ((*flags) & ~(WIND_RW_BOM|WIND_RW_LE|WIND_RW_BE));
        *flags |= little ? WIND_RW_LE : WIND_RW_BE;
    }

    while (len)
    {
        if (olen < 1)
        {
            return WIND_ERR_OVERRUN;
        }
        if (little)
        {
            *out = (p[1] << 8) + p[0];
        }
        else
        {
            *out = (p[0] << 8) + p[1];
        }
        out++;
        p += 2;
        len -= 2;
        olen--;
    }
    *out_len -= olen;
    return 0;
}

/**
 * Write an UCS2 string to a buffer.
 *
 * @param in The input UCS2 string.
 * @param in_len the length of the input buffer.
 * @param flags Flags to control the behavior of the function.
 * @param ptr The input buffer to write to, the array must be at least
 * (in + 1) * 2 bytes long.
 * @param out_len the output length
 *
 * @return returns 0 on success, an wind error code otherwise.
 * @ingroup wind
 */

int
wind_ucs2write(const uint16_t *in, size_t in_len, unsigned int *flags,
           void *ptr, size_t *out_len)
{
    unsigned char *p = ptr;
    size_t len = *out_len;

    /** If in buffer is not of length be mod 2, WIND_ERR_LENGTH_NOT_MOD2 is returned*/
    if (len & 1)
    {
        return WIND_ERR_LENGTH_NOT_MOD2;
    }

    /** On zero input length, flags are preserved */
    if (in_len == 0)
    {
        *out_len = 0;
        return 0;
    }
    /** If flags have WIND_RW_BOM set, the byte order mark is written
     * first to the output data */
    if ((*flags) & WIND_RW_BOM)
    {
        uint16_t bom = 0xfffe;

        if (len < 2)
        {
            return WIND_ERR_OVERRUN;
        }

        if ((*flags) & WIND_RW_LE)
        {
            p[0] = (bom     ) & 0xff;
            p[1] = (bom >> 8) & 0xff;
        }
        else
        {
            p[1] = (bom     ) & 0xff;
            p[0] = (bom >> 8) & 0xff;
        }
        len -= 2;
    }

    while (in_len)
    {
        /** If the output wont fit into out_len, WIND_ERR_OVERRUN is returned */
        if (len < 2)
        {
            return WIND_ERR_OVERRUN;
        }
        if ((*flags) & WIND_RW_LE)
        {
            p[0] = (in[0]     ) & 0xff;
            p[1] = (in[0] >> 8) & 0xff;
        }
        else
        {
            p[1] = (in[0]     ) & 0xff;
            p[0] = (in[0] >> 8) & 0xff;
        }
        len -= 2;
        in_len--;
        p += 2;
        in++;
    }
    *out_len -= len;
    return 0;
}


/**
 * Convert an UTF-8 string to an UCS2 string.
 *
 * @param in an UTF-8 string to convert.
 * @param out the resulting UCS2 strint, must be at least
 * wind_utf8ucs2_length() long.  If out is NULL, the function will
 * calculate the needed space for the out variable (just like
 * wind_utf8ucs2_length()).
 * @param out_len before processing out_len should be the length of
 * the out variable, after processing it will be the length of the out
 * string.
 *
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_utf8ucs2(const char *in, uint16_t *out, size_t *out_len)
{
    const unsigned char *p;
    size_t o = 0;
    int ret;

    for (p = (const unsigned char *)in; *p != '\0'; ++p)
    {
        uint32_t u;

        ret = utf8toutf32(&p, &u);
        if (ret)
        {
            return ret;
        }

        if (u & 0xffff0000)
        {
            return WIND_ERR_NOT_UTF16;
        }

        if (out)
        {
            if (o >= *out_len)
            {
                return WIND_ERR_OVERRUN;
            }
            out[o] = u;
        }
        o++;
    }
    *out_len = o;
    return 0;
}

/**
 * Calculate the length of from converting a UTF-8 string to a UCS2
 * string.
 *
 * @param in an UTF-8 string to convert.
 * @param out_len the length of the resulting UCS4 string.
 *
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_utf8ucs2_length(const char *in, size_t *out_len)
{
    return wind_utf8ucs2(in, NULL, out_len);
}

/**
 * Convert an UCS2 string to a UTF-8 string.
 *
 * @param in an UCS2 string to convert.
 * @param in_len the length of the in UCS2 string.
 * @param out the resulting UTF-8 strint, must be at least
 * wind_ucs2utf8_length() long.  If out is NULL, the function will
 * calculate the needed space for the out variable (just like
 * wind_ucs2utf8_length()).
 * @param out_len before processing out_len should be the length of
 * the out variable, after processing it will be the length of the out
 * string.
 *
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_ucs2utf8(const uint16_t *in, size_t in_len, char *out, size_t *out_len)
{
    uint16_t ch;
    size_t i, len, o;

    for (o = 0, i = 0; i < in_len; i++)
    {
        ch = in[i];
        if (ch < 0x80)
        {
            len = 1;
        }
        else if (ch < 0x800)
        {
            len = 2;
        }
        else
        {
            len = 3;
        }

        o += len;

        if (out)
        {
            if (o >= *out_len)
            {
                eturn WIND_ERR_OVERRUN;
            }

            switch(len)
            {
                case 3:
                    out[2] = (ch | 0x80) & 0xbf;
                    ch = ch >> 6;
                case 2:
                    out[1] = (ch | 0x80) & 0xbf;
                    ch = ch >> 6;
                case 1:
                    out[0] = ch | first_char[len - 1];
            }
            out += len;
        }
    }
    if (out)
    {
        if (o >= *out_len)
        {
            return WIND_ERR_OVERRUN;
        }
        *out = '\0';
    }
    *out_len = o;
    return 0;
}

/**
 * Calculate the length of from converting a UCS2 string to an UTF-8 string.
 *
 * @param in an UCS2 string to convert.
 * @param in_len an UCS2 string length to convert.
 * @param out_len the length of the resulting UTF-8 string.
 *
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */
int
wind_ucs2utf8_length(const uint16_t *in, size_t in_len, size_t *out_len)
{
    return wind_ucs2utf8(in, in_len, NULL, out_len);
}
