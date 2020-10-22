#include "./include/mrporting.h"
typedef struct {
    uint32 next; 
    uint32 len;
} LG_mem_free_t;

//*****************************
#ifdef MYTHROAD_DEBUG
static uint32 LG_mem_min;
static uint32 LG_mem_top;
#endif

static LG_mem_free_t LG_mem_free;
static char* LG_mem_base;
static uint32 LG_mem_len;
static char* Origin_LG_mem_base;
static uint32 Origin_LG_mem_len;
static char* LG_mem_end;
static uint32 LG_mem_left;

//*****************************
#define realLGmemSize(x) (((x)+7)&(0xfffffff8))
#define MRDBGPRINTF mr_printf

static int32 _mr_mem_init(int32 ram) {
    MRDBGPRINTF("ask Origin_LG_mem_len:%d", Origin_LG_mem_len);

    if (mr_mem_get(&Origin_LG_mem_base, &Origin_LG_mem_len) != MR_SUCCESS) {
        MRDBGPRINTF("mr_mem_get failed!");
        return MR_FAILED;
    }

    MRDBGPRINTF("got Origin_LG_mem_len:%d", Origin_LG_mem_len);

    LG_mem_base = (char*)((uint32)(Origin_LG_mem_base + 3) & (~3));
    LG_mem_len = (Origin_LG_mem_len - (LG_mem_base - Origin_LG_mem_base)) & (~3);
    LG_mem_end = LG_mem_base + LG_mem_len;
    LG_mem_free.next = 0;
    LG_mem_free.len = 0;
    ((LG_mem_free_t*)LG_mem_base)->next = LG_mem_len;
    ((LG_mem_free_t*)LG_mem_base)->len = LG_mem_len;
    LG_mem_left = LG_mem_len;
#ifdef MYTHROAD_DEBUG
    LG_mem_min = LG_mem_len;
    LG_mem_top = 0;
#endif
    return MR_SUCCESS;
}

void* mr_malloc(uint32 len) {
    LG_mem_free_t *previous, *nextfree, *l;

    len = (uint32)realLGmemSize(len);
    if (len >= LG_mem_left) {
        MRDBGPRINTF("mr_malloc no memory");
        goto err;
    }

    if (!len) {
        MRDBGPRINTF("mr_malloc invalid memory request");
        goto err;
    }
    if (LG_mem_base + LG_mem_free.next > LG_mem_end) {
        MRDBGPRINTF("mr_malloc corrupted memory");
        goto err;
    }
    MRDBGPRINTF("R9:%X malloc(0x%X[%d]) base:0x%X free.next:0x%X end:0x%X", getR9(), len, len, LG_mem_base, LG_mem_free.next, LG_mem_end);

    previous = &LG_mem_free;
    nextfree = (LG_mem_free_t*)(LG_mem_base + previous->next);
    while ((char*)nextfree < LG_mem_end) {
        if (nextfree->len == len) {
            previous->next = nextfree->next;
            LG_mem_left -= len;
#ifdef MYTHROAD_DEBUG
            if (LG_mem_left < LG_mem_min)
                LG_mem_min = LG_mem_left;
            if (LG_mem_top < previous->next)
                LG_mem_top = previous->next;
#endif
            return (void*)nextfree;
        }
        if (nextfree->len > len) {
            l = (LG_mem_free_t*)((char*)nextfree + len);
            l->next = nextfree->next;
            l->len = (uint32)(nextfree->len - len);
            previous->next += len;
            LG_mem_left -= len;
#ifdef MYTHROAD_DEBUG
            if (LG_mem_left < LG_mem_min)
                LG_mem_min = LG_mem_left;
            if (LG_mem_top < previous->next)
                LG_mem_top = previous->next;
#endif
            return (void*)nextfree;
        }
        previous = nextfree;
        nextfree = (LG_mem_free_t*)(LG_mem_base + nextfree->next);
    }
    MRDBGPRINTF("mr_malloc no memory");
err:
    return 0;
}

void mr_free(void* p, uint32 len) {
    LG_mem_free_t *free, *n;

    MRDBGPRINTF("R9:%X free(0x%X, %u)", getR9(), p, len);
    len = (uint32)realLGmemSize(len);
#ifdef MYTHROAD_DEBUG
    if (!len || !p || (char*)p < LG_mem_base || (char*)p >= LG_mem_end || (char*)p + len > LG_mem_end || (char*)p + len <= LG_mem_base) {
        MRDBGPRINTF("mr_free invalid");
        MRDBGPRINTF("p=%d,l=%d,base=%d,LG_mem_end=%d", (int32)p, len, (int32)LG_mem_base, (int32)LG_mem_end);
        return;
    }
#endif

    free = &LG_mem_free;
    n = (LG_mem_free_t*)(LG_mem_base + free->next);
    while (((char*)n < LG_mem_end) && ((void*)n < p)) {
        free = n;
        n = (LG_mem_free_t*)(LG_mem_base + n->next);
    }
#ifdef MYTHROAD_DEBUG
    if (p == (void*)free || p == (void*)n) {
        MRDBGPRINTF("mr_free:already free");
        return;
    }
#endif
    if ((free != &LG_mem_free) && ((char*)free + free->len == p)) {
        free->len += len;
    } else {
        free->next = (uint32)((char*)p - LG_mem_base);
        free = (LG_mem_free_t*)p;
        free->next = (uint32)((char*)n - LG_mem_base);
        free->len = len;
    }
    if (((char*)n < LG_mem_end) && ((char*)p + len == (char*)n)) {
        free->next = n->next;
        free->len += n->len;
    }
    LG_mem_left += len;
}

void* mr_realloc(void* p, uint32 oldlen, uint32 len) {
    unsigned long minsize = (oldlen > len) ? len : oldlen;
    void* newblock;
    if (p == NULL) {
        return mr_malloc(len);
    }

    if (len == 0) {
        mr_free(p, oldlen);
        return NULL;
    }
    newblock = mr_malloc(len);
    if (newblock == NULL) {
        return newblock;
    }

    MEMMOVE(newblock, p, minsize);
    mr_free(p, oldlen);
    return newblock;
}