#ifndef __SBUF_H__
#define __SBUF_H__
#include <sys/types.h>
#include <stdint.h>

typedef struct {
    uint8_t *buf;
    size_t size;
    int  head;
    int  tail;
} sbuf_t;

void sbuf_init(sbuf_t *sbuf, void *buf, size_t size);
size_t sbuf_count(const sbuf_t *);
int sbuf_read_in(sbuf_t *, int fd);
int sbuf_write_out(sbuf_t *, int fd, int len);
void sbuf_discard(sbuf_t *, int len);
int sbuf_find(sbuf_t *, const uint8_t *pattern, int len);
void sbuf_seek(sbuf_t *, int pos);
#endif
