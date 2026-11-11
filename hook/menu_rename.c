/* Narrow English Creative Style string-bank redirect for the NEX-7.
 *
 * Only im.elf may redirect, and only these two exact paths:
 *   /usr/share/app/string_english.uxc
 *   share/app/string_english.uxc
 *   ./string_english.uxc
 * The replacement is tried first; any failure falls back to the stock path.
 * All other files and processes use the real libc functions unchanged.
 */

#define O_WRONLY 01
#define O_CREAT 0100
#define O_APPEND 02000
#define SEEK_END 2
#define SYS_read 3
#define SYS_write 4
#define SYS_open 5
#define SYS_close 6
#define SYS_getpid 20
#define SYS_lseek 19
#define STOCK_BANK "/setting/string_english.uxc"
#define OPEN_LOG "/setting/nexhook-open.log"
#define OPEN_LOG_MAX 65536

extern int nexhook_is_im(void);
extern void *dlsym(void *handle, const char *symbol);
#define RTLD_NEXT ((void *)-1L)

typedef int (*open_fn)(const char *path, int flags, ...);
typedef int (*openat_fn)(int dirfd, const char *path, int flags, ...);
typedef void *(*fopen_fn)(const char *path, const char *mode);

#ifdef NEXHOOK_HOST_TEST
extern long test_sys3(long nr, long a, long b, long c);
#define sys3 test_sys3
#else
static long sys3(long nr, long a, long b, long c)
{
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r7 __asm__("r7") = nr;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
#endif

static int put_text(char *p, const char *s)
{
    int n = 0;
    while (s[n]) {
        p[n] = s[n];
        n++;
    }
    return n;
}

static int put_dec(char *p, unsigned long v)
{
    char tmp[12];
    int n = 0, i = 0;
    if (!v) {
        p[0] = '0';
        return 1;
    }
    while (v && n < 11) {
        tmp[n++] = '0' + v % 10;
        v /= 10;
    }
    while (n)
        p[i++] = tmp[--n];
    return i;
}

static int contains_uxc(const char *path)
{
    int i;
    if (!path)
        return 0;
    for (i = 0; path[i]; i++) {
        if (path[i] == 'u' && path[i + 1] == 'x' && path[i + 2] == 'c')
            return 1;
    }
    return 0;
}

static void trace_open(const char *operation, const char *path)
{
    char line[320];
    long fd;
    int n;
    if (!contains_uxc(path))
        return;
    n = put_text(line, "pid=");
    n += put_dec(line + n, sys3(SYS_getpid, 0, 0, 0));
    n += put_text(line + n, " im=");
    n += put_dec(line + n, nexhook_is_im());
    line[n++] = ' ';
    n += put_text(line + n, operation);
    line[n++] = ' ';
    n += put_text(line + n, path);
    line[n++] = '\n';
    fd = sys3(SYS_open, (long)OPEN_LOG, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
        return;
    if (sys3(SYS_lseek, fd, 0, SEEK_END) < OPEN_LOG_MAX)
        sys3(SYS_write, fd, (long)line, n);
    sys3(SYS_close, fd, 0, 0);
}

static open_fn real_open;
static open_fn real_open64;
static openat_fn real_openat;
static fopen_fn real_fopen;
static fopen_fn real_fopen64;

static int same_string(const char *a, const char *b)
{
    int i = 0;
    if (!a)
        return 0;
    while (a[i] && a[i] == b[i])
        i++;
    return a[i] == 0 && b[i] == 0;
}

static int is_english_bank(const char *path)
{
    return same_string(path, "/usr/share/app/string_english.uxc") ||
           same_string(path, "share/app/string_english.uxc") ||
           same_string(path, "./string_english.uxc");
}

static int call_open(open_fn real, const char *path, int flags,
                     unsigned long mode, int have_mode)
{
    if (have_mode)
        return real(path, flags, mode);
    return real(path, flags);
}

int open(const char *path, int flags, ...)
{
    unsigned long mode = 0;
    int have_mode = flags & O_CREAT;
    if (have_mode) {
        __builtin_va_list ap;
        __builtin_va_start(ap, flags);
        mode = __builtin_va_arg(ap, unsigned long);
        __builtin_va_end(ap);
    }
    trace_open("open", path);
    if (!real_open)
        real_open = (open_fn)dlsym(RTLD_NEXT, "open");
    if (!real_open)
        return -1;
    if (nexhook_is_im() && is_english_bank(path)) {
        int fd = call_open(real_open, STOCK_BANK, flags, mode, have_mode);
        if (fd >= 0) {
            trace_open("redirect", STOCK_BANK);
            return fd;
        }
    }
    return call_open(real_open, path, flags, mode, have_mode);
}

int open64(const char *path, int flags, ...)
{
    unsigned long mode = 0;
    int have_mode = flags & O_CREAT;
    if (have_mode) {
        __builtin_va_list ap;
        __builtin_va_start(ap, flags);
        mode = __builtin_va_arg(ap, unsigned long);
        __builtin_va_end(ap);
    }
    trace_open("open64", path);
    if (!real_open64)
        real_open64 = (open_fn)dlsym(RTLD_NEXT, "open64");
    if (!real_open64)
        return -1;
    if (nexhook_is_im() && is_english_bank(path)) {
        int fd = call_open(real_open64, STOCK_BANK, flags, mode, have_mode);
        if (fd >= 0) {
            trace_open("redirect", STOCK_BANK);
            return fd;
        }
    }
    return call_open(real_open64, path, flags, mode, have_mode);
}

int openat(int dirfd, const char *path, int flags, ...)
{
    unsigned long mode = 0;
    int have_mode = flags & O_CREAT;
    if (have_mode) {
        __builtin_va_list ap;
        __builtin_va_start(ap, flags);
        mode = __builtin_va_arg(ap, unsigned long);
        __builtin_va_end(ap);
    }
    trace_open("openat", path);
    if (!real_openat)
        real_openat = (openat_fn)dlsym(RTLD_NEXT, "openat");
    if (!real_openat)
        return -1;
    if (nexhook_is_im() && is_english_bank(path)) {
        int fd = have_mode ? real_openat(dirfd, STOCK_BANK, flags, mode) :
                             real_openat(dirfd, STOCK_BANK, flags);
        if (fd >= 0) {
            trace_open("redirect", STOCK_BANK);
            return fd;
        }
    }
    if (have_mode)
        return real_openat(dirfd, path, flags, mode);
    return real_openat(dirfd, path, flags);
}

void *fopen(const char *path, const char *mode)
{
    void *fp;
    trace_open("fopen", path);
    if (!real_fopen)
        real_fopen = (fopen_fn)dlsym(RTLD_NEXT, "fopen");
    if (!real_fopen)
        return 0;
    if (nexhook_is_im() && is_english_bank(path)) {
        fp = real_fopen(STOCK_BANK, mode);
        if (fp) {
            trace_open("redirect", STOCK_BANK);
            return fp;
        }
    }
    return real_fopen(path, mode);
}

void *fopen64(const char *path, const char *mode)
{
    void *fp;
    trace_open("fopen64", path);
    if (!real_fopen64)
        real_fopen64 = (fopen_fn)dlsym(RTLD_NEXT, "fopen64");
    if (!real_fopen64)
        return 0;
    if (nexhook_is_im() && is_english_bank(path)) {
        fp = real_fopen64(STOCK_BANK, mode);
        if (fp) {
            trace_open("redirect", STOCK_BANK);
            return fp;
        }
    }
    return real_fopen64(path, mode);
}
