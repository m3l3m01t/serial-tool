#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "sbuf.h"

#ifndef MIN
#define MIN(a, b) ((a)>(b) ? (b) : (a))
#endif

void sbuf_init(sbuf_t *sbuf, void *buf, size_t size)
{
    sbuf->buf = buf;
    sbuf->size = size;
    sbuf->head = sbuf->tail = 0;
}

inline size_t sbuf_count(const sbuf_t *sbuf)
{
    return sbuf->tail - sbuf->head;
}

int sbuf_read_in(sbuf_t *sbuf, int fd)
{
    int count;

    if (sbuf->tail == sbuf->size) {
        if (!sbuf->head)
            return 0;
        else {
            count = sbuf_count(sbuf);
            if (count)
                memmove(sbuf->buf, sbuf->buf + sbuf->head, count);
            sbuf->tail = count;
            sbuf->head = 0;
        }
    }

    count = read(fd, sbuf->buf + sbuf->tail, sbuf->size - sbuf->tail);
    if (count > 0) {
        sbuf->tail += count;
        return count;
    } if (count < 0) {
        if (errno == EAGAIN)
            return 0;
        else if (errno == EINTR)
            return 0;
        else
            return -1;
    } else {
        return -1;
    }
}

int sbuf_write_out(sbuf_t *sbuf, int fd, int len)
{
    int count, written = 0;

    count = sbuf_count(sbuf);
    if (len == -1)
        len = count;
    else
        len = MIN(len, count);

    while (written < len) {
        count = write(fd, sbuf->buf + sbuf->head, len - written);
        if (count > 0) {
            sbuf->head += count;
            written += count;
        } else if (count == -1) {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN) {
                break;
            }
        }
    }

    return written;
}

int sbuf_find(sbuf_t *sbuf, const uint8_t *pat, int len)
{
    int pos;
    for (pos = sbuf->head; pos <= sbuf->tail - len; pos++) {
        if (memcmp(sbuf->buf + pos, pat, len) == 0)
            return pos;
    }
    return -1;
}

void sbuf_discard(sbuf_t *sbuf, int len)
{
    if (len > 0) {
        sbuf->head += len;
    } else if (len < 0) {
        sbuf->head = sbuf->tail + len;
        if (sbuf->head < 0) {
            sbuf->head = 0;
        }
    }

    if (sbuf->head >= sbuf->tail) {
        sbuf->head = sbuf->tail = 0;
    }
}

void sbuf_seek(sbuf_t *sbuf, int pos)
{
    sbuf->head = pos;
    if (sbuf->head >= sbuf->tail) {
        sbuf->head = sbuf->tail = 0;
    }
}
