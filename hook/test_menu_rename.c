/* Host-only path/fallback tests for menu_rename.c. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define O_CREAT 0100
#define STOCK_BANK "/setting/string_english.uxc"

static int active;
static int stock_result = -1;
static int calls;
static int last_flags;
static unsigned long last_mode;
static char last_path[160];

int nexhook_is_im(void)
{
    return active;
}

static int mock_open(const char *path, int flags, ...)
{
    unsigned long mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, unsigned long);
        va_end(ap);
    }
    calls++;
    last_flags = flags;
    last_mode = mode;
    snprintf(last_path, sizeof(last_path), "%s", path);
    return !strcmp(path, STOCK_BANK) ? stock_result : 100;
}

static void *mock_fopen(const char *path, const char *mode)
{
    (void)mode;
    calls++;
    snprintf(last_path, sizeof(last_path), "%s", path);
    return !strcmp(path, STOCK_BANK) ? (void *)0 : (void *)0x1234;
}

long test_sys3(long nr, long a, long b, long c)
{
    (void)a;
    (void)b;
    (void)c;
    return nr == 20 ? 1 : 0;
}

/* Keep host mocks out of the sanitizer runtime's libc interposition. */
#define dlsym test_dlsym
void *dlsym(void *handle, const char *symbol)
{
    (void)handle;
    if (!strcmp(symbol, "open") || !strcmp(symbol, "open64"))
        return (void *)mock_open;
    if (!strcmp(symbol, "fopen") || !strcmp(symbol, "fopen64"))
        return (void *)mock_fopen;
    return 0;
}

#define NEXHOOK_HOST_TEST 1
#define open test_open
#define open64 test_open64
#define openat test_openat
#define fopen test_fopen
#define fopen64 test_fopen64
#include "menu_rename.c"
#undef open
#undef open64
#undef openat
#undef fopen
#undef fopen64
#undef dlsym

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s (path=%s)\n", __LINE__, #x, last_path); \
    return 1; \
} } while (0)

static void reset(void)
{
    active = 1;
    stock_result = -1;
    calls = 0;
    last_flags = last_mode = 0;
    last_path[0] = 0;
}

int main(void)
{
    reset();
    CHECK(test_open("/tmp/other", 0) == 100);
    CHECK(!strcmp(last_path, "/tmp/other"));

    reset();
    stock_result = 42;
    CHECK(test_open("/usr/share/app/string_english.uxc", 0) == 42);
    CHECK(!strcmp(last_path, STOCK_BANK));

    reset();
    stock_result = 43;
    CHECK(test_open64("share/app/string_english.uxc", 0) == 43);
    CHECK(!strcmp(last_path, STOCK_BANK));

    reset();
    stock_result = 44;
    CHECK(test_open("./string_english.uxc", 0) == 44);
    CHECK(!strcmp(last_path, STOCK_BANK));

    reset();
    active = 0;
    stock_result = 44;
    CHECK(test_open("/usr/share/app/string_english.uxc", 0) == 100);
    CHECK(!strcmp(last_path, "/usr/share/app/string_english.uxc"));

    reset();
    CHECK(test_open("/usr/share/app/string_english.uxc", O_CREAT, 0600) == 100);
    CHECK(!strcmp(last_path, "/usr/share/app/string_english.uxc"));
    CHECK(last_flags == O_CREAT && last_mode == 0600);

    reset();
    stock_result = 45;
    CHECK(test_open("/usr/share/app/string_english.uxc", O_CREAT, 0640) == 45);
    CHECK(!strcmp(last_path, STOCK_BANK) && last_mode == 0640);

    reset();
    CHECK(test_fopen("/usr/share/app/string_english.uxc", "r") == (void *)0x1234);
    CHECK(!strcmp(last_path, "/usr/share/app/string_english.uxc"));

    reset();
    CHECK(test_fopen64("/usr/share/app/string_english.uxc", "r") == (void *)0x1234);
    CHECK(!strcmp(last_path, "/usr/share/app/string_english.uxc"));

    reset();
    CHECK(test_fopen("/tmp/other", "r") == (void *)0x1234);
    CHECK(!strcmp(last_path, "/tmp/other"));

    puts("menu rename path tests passed");
    return 0;
}
