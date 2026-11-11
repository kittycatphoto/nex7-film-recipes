/* Focused host tests for the 0x400c WB template/rewrite/allocation path. */
#define NEXHOOK_HOST_TEST 1
/* Keep the host mock out of the sanitizer runtime's dlsym interposition. */
#define dlsym test_dlsym
#include "recipe_hook.c"
#include <stdio.h>
#include <string.h>

static unsigned char state_style = 6;
static unsigned char state_reads;
static unsigned char allocated[64];

long test_sys3(long nr, long a, long b, long c)
{
    (void)c;
    if (nr == SYS_getpid)
        return 123;
    if (nr == SYS_open) {
        const char *path = (const char *)a;
        if (!strcmp(path, STATE_PATH))
            return 10;
        if (!strcmp(path, LOG_PATH))
            return 11;
        return -ENOENT;
    }
    if (nr == SYS_read) {
        if (a == 10 && !state_reads++) {
            *(unsigned char *)b = state_style;
            return 1;
        }
        return 0;
    }
    if (nr == SYS_lseek)
        return 0;
    if (nr == SYS_write || nr == SYS_close)
        return 0;
    return 0;
}

void *dlsym(void *handle, const char *symbol)
{
    (void)handle;
    (void)symbol;
    return 0;
}
#undef dlsym

static int fake_valloc(unsigned long mailbox, void **msg, unsigned long size, unsigned long wait)
{
    (void)mailbox;
    (void)wait;
    if (size > sizeof(allocated) - 12)
        return -1;
    *msg = allocated + 12;
    return 0;
}

static long fake_backup_read(unsigned long id, void *output)
{
    unsigned char *p = output;
    switch (id) {
    case WB_STORE_MODE: *p = 2; break;
    case WB_STORE_ADJUST_LB: *p = (unsigned char)-4; break;
    case WB_STORE_ADJUST_CC: *p = 6; break;
    case WB_STORE_COLOR_TEMP: *p = 55; break;
    case WB_STORE_DAYLIGHT_LEVEL: *p = 1; break;
    default: return -1;
    }
    return 0;
}

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; \
} } while (0)

static void fill_stock(unsigned char *m)
{
    int i;
    for (i = 0; i < WB_MESSAGE_SIZE; i++)
        m[i] = 0;
    m[0] = 5;
    m[2] = 0x0c;
    m[3] = 0x40;
    m[4] = 0x77;
    m[8] = 4;
    m[9] = (unsigned char)-2;
    m[10] = 3;
    m[11] = 55;
    m[12] = 1;
}

int main(void)
{
    unsigned char stock[WB_MESSAGE_SIZE];
    unsigned char aggregate[64];
    unsigned char *prepared = 0;
    const unsigned char *rec;

    memset(allocated, 0, sizeof(allocated));
    real_backup_read = fake_backup_read;
    wb_template_valid = 0;
    CHECK(read_wb_template() == 1);
    CHECK(wb_template[8] == 2 && wb_template[9] == (unsigned char)-4);
    CHECK(wb_template[10] == 6 && wb_template[11] == 55 && wb_template[12] == 1);

    fill_stock(stock);
    capture_wb_template(stock);
    real_valloc_msg_wait = fake_valloc;
    recipes[6].wb_mode = 1;
    recipes[6].wb_temp = 58;
    recipes[6].wb_lb = 3;
    recipes[6].wb_cc = 1;
    CHECK(prepare_wb(&recipes[6], &prepared) == 0);
    CHECK(prepared == allocated + 12);
    CHECK(prepared[8] == 1);
    CHECK(prepared[9] == 3 && prepared[10] == 1);
    CHECK(prepared[11] == 58 && prepared[12] == 1);
    CHECK(prepared[4] == 0x77);

    recipes[1].wb_mode = 10;
    recipes[1].wb_temp = 53;
    recipes[1].wb_lb = -1;
    recipes[1].wb_cc = 0;
    CHECK(prepare_wb(&recipes[1], &prepared) == 0);
    CHECK(prepared[8] == 10 && prepared[11] == 53);
    CHECK(prepared[9] == (unsigned char)-1 && prepared[10] == 0);

    /* Standalone rewrite applies mode, temperature, and adjustments. */
    state_style = 6;
    state_reads = 0;
    fill_stock(stock);
    handle_outgoing_wb(stock, MSG_WB_MODE_CMD);
    CHECK(stock[8] == 1 && stock[9] == 3 && stock[10] == 1);
    CHECK(stock[11] == 58 && stock[12] == 1);

    /* Aggregate record at +0x2c follows the same rule. */
    memset(aggregate, 0, sizeof(aggregate));
    rec = aggregate + WB_INIT_OFFSET;
    memcpy((void *)rec, stock, WB_MESSAGE_SIZE);
    ((unsigned char *)rec)[8] = 4;
    ((unsigned char *)rec)[11] = 55;
    state_reads = 0;
    handle_outgoing_wb(aggregate, MSG_INIT_SETTINGS);
    CHECK(rec[8] == 1 && rec[9] == 3 && rec[10] == 1);
    CHECK(rec[11] == 58 && rec[12] == 1);

    /* Unmapped style keeps the user's WB mode untouched. */
    state_style = 0;
    state_reads = 0;
    recipes[0].wb_mode = 0xff;
    ((unsigned char *)rec)[8] = 4;
    handle_outgoing_wb(aggregate, MSG_INIT_SETTINGS);
    CHECK(rec[8] == 4);

    /* C6 applies mode, Kelvin, and WB adjustments. */
    state_style = 1;
    state_reads = 0;
    handle_outgoing_wb(aggregate, MSG_INIT_SETTINGS);
    CHECK(rec[8] == 10 && rec[11] == 53);
    CHECK(rec[9] == (unsigned char)-1 && rec[10] == 0 && rec[12] == 1);

    puts("WB message tests passed");
    return 0;
}
