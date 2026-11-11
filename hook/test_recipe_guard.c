/* Host-only tests for recipe_hook.c. All syscalls and files are faked in memory. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEXHOOK_HOST_TEST 1
#include "recipe_hook.c"

#define FAKE_FILES 96
#define FAKE_FDS 32
#define FAKE_FILE_SIZE 32768

struct fake_file {
    char path[160];
    unsigned char data[FAKE_FILE_SIZE];
    long size;
    int used;
};

struct fake_fd {
    int file;
    long offset;
    int append;
    int used;
    int eof;
};

static struct fake_file files[FAKE_FILES];
static struct fake_fd fds[FAKE_FDS];
static long fake_pid;
static long fake_now;
static int fake_clock_error;
static int fake_lock_error;
static int fake_open_error_once;
static int fake_read_error_once;
static char fake_error_path[160];
static long boot_log_bytes;
static long boot_log_reads;
static long fake_reads;

static int find_file(const char *path)
{
    int i;
    for (i = 0; i < FAKE_FILES; i++) {
        if (files[i].used && !strcmp(files[i].path, path))
            return i;
    }
    return -1;
}

static int add_file(const char *path, const void *data, long size)
{
    int i = find_file(path);
    if (i < 0) {
        for (i = 0; i < FAKE_FILES && files[i].used; i++)
            ;
    }
    if (i == FAKE_FILES || size < 0 || size > FAKE_FILE_SIZE) {
        fprintf(stderr, "fake file capacity exceeded: %s\n", path);
        exit(2);
    }
    files[i].used = 1;
    strcpy(files[i].path, path);
    files[i].size = size;
    if (size)
        memcpy(files[i].data, data, size);
    return i;
}

static void add_text(const char *path, const char *text)
{
    add_file(path, text, (long)strlen(text));
}

static void append_text(const char *path, const char *text)
{
    int i = find_file(path);
    long n = (long)strlen(text);
    if (i < 0 || files[i].size + n > FAKE_FILE_SIZE) {
        fprintf(stderr, "cannot append fake file: %s\n", path);
        exit(2);
    }
    memcpy(files[i].data + files[i].size, text, n);
    files[i].size += n;
}

static long fake_open(const char *path, long flags)
{
    int i = find_file(path);
    int fd;
    if (fake_open_error_once && !strcmp(fake_error_path, path)) {
        fake_open_error_once = 0;
        return -5;
    }
    if (i >= 0 && (flags & O_EXCL))
        return -EEXIST;
    if (i < 0) {
        if (!(flags & O_CREAT))
            return -ENOENT;
        i = add_file(path, "", 0);
    }
    for (fd = 0; fd < FAKE_FDS && fds[fd].used; fd++)
        ;
    if (fd == FAKE_FDS)
        return -24;
    fds[fd].used = 1;
    fds[fd].file = i;
    fds[fd].offset = 0;
    fds[fd].append = !!(flags & O_APPEND);
    fds[fd].eof = 0;
    return fd + 3;
}

long test_sys3(long nr, long a, long b, long c)
{
    (void)c;
    if (nr == SYS_getpid)
        return fake_pid;
    if (nr == SYS_open)
        return fake_open((const char *)a, b);
    if (nr == SYS_close) {
        long fd = a - 3;
        if (fd < 0 || fd >= FAKE_FDS || !fds[fd].used)
            return -9;
        fds[fd].used = 0;
        return 0;
    }
    if (nr == SYS_read) {
        long fd = a - 3;
        struct fake_fd *f;
        struct fake_file *file;
        long n;
        fake_reads++;
        if (fd < 0 || fd >= FAKE_FDS || !fds[fd].used)
            return -9;
        f = &fds[fd];
        file = &files[f->file];
        if (fake_read_error_once && !strcmp(fake_error_path, file->path)) {
            fake_read_error_once = 0;
            return -5;
        }
        if (f->eof)
            return 0;
        if (f->offset >= file->size) {
            f->eof = 1;
            return 0;
        }
        n = file->size - f->offset;
        if (n > c)
            n = c;
        memcpy((void *)b, file->data + f->offset, n);
        f->offset += n;
        if (!strcmp(file->path, BOOT_LOG_PATH)) {
            boot_log_bytes += n;
            boot_log_reads++;
        }
        return n;
    }
    if (nr == SYS_write) {
        long fd = a - 3;
        struct fake_fd *f;
        struct fake_file *file;
        if (fd < 0 || fd >= FAKE_FDS || !fds[fd].used)
            return -9;
        f = &fds[fd];
        file = &files[f->file];
        if (f->append)
            f->offset = file->size;
        if (f->offset + c > FAKE_FILE_SIZE)
            return -28;
        memcpy(file->data + f->offset, (const void *)b, c);
        f->offset += c;
        if (file->size < f->offset)
            file->size = f->offset;
        return c;
    }
    if (nr == SYS_lseek) {
        long fd = a - 3;
        if (fd < 0 || fd >= FAKE_FDS || !fds[fd].used)
            return -9;
        if (c == SEEK_SET) {
            if (b < 0 || b > files[fds[fd].file].size)
                return -22;
            fds[fd].offset = b;
        } else if (c == SEEK_END) {
            if (b != 0)
                return -22;
            fds[fd].offset = files[fds[fd].file].size;
        } else {
            return -22;
        }
        fds[fd].eof = 0;
        return fds[fd].offset;
    }
    if (nr == SYS_clock_gettime) {
        struct { long sec; long nsec; } *ts = (void *)b;
        if (fake_clock_error)
            return -5;
        ts->sec = fake_now;
        ts->nsec = 0;
        return 0;
    }
    if (nr == SYS_flock)
        return fake_lock_error ? -5 : 0;
    if (nr == SYS_unlink) {
        int i = find_file((const char *)a);
        if (i < 0)
            return -ENOENT;
        files[i].used = 0;
        return 0;
    }
    if (nr == SYS_rename) {
        int src = find_file((const char *)a);
        int dst = find_file((const char *)b);
        if (src < 0)
            return -ENOENT;
        if (dst >= 0 && dst != src)
            files[dst].used = 0;
        strcpy(files[src].path, (const char *)b);
        return 0;
    }
    fprintf(stderr, "unexpected fake syscall %ld\n", nr);
    exit(2);
}

static const char boot_a[] = "00112233-4455-6677-8899-aabbccddeeff";
static const char boot_b[] = "ffeeddcc-bbaa-9988-7766-554433221100";

static void reset_model(void)
{
    memset(files, 0, sizeof(files));
    memset(fds, 0, sizeof(fds));
    fake_pid = 100;
    fake_now = 10;
    fake_clock_error = 0;
    fake_lock_error = 0;
    fake_open_error_once = 0;
    fake_read_error_once = 0;
    fake_error_path[0] = 0;
    boot_log_bytes = 0;
    boot_log_reads = 0;
    fake_reads = 0;

    process_is_im = 1;
    recipe_enabled = 1;
    guard_failed = 0;
    guard_pending = 0;
    guard_owner_pid = 100;
    guard_generation_known = 0;
    guard_poll_valid = 0;
    guard_poll_ok = 0;
    generation_gate = 0;
    generation_gate_pid = fake_pid;
    guard_clear_after = 0;
    generation_wait_logged = 0;
    temp_serial = 0;
    log_reader.fd = -1;
    log_reader.pid = -1;
    log_reader.have_boot_id = 0;
    log_reader.malformed = 0;
    warm_parser_init(&log_reader.parser);
    add_text(BOOT_ID_PATH, "00112233-4455-6677-8899-aabbccddeeff\n");
    add_text(BOOT_LOG_PATH, "L: BOOT (test)\n");
    add_text(GUARD_PATH, "0\n");
}

static struct generation expected_generation(const char *boot, unsigned int warm)
{
    struct generation g;
    memcpy(g.boot_id, boot, BOOT_ID_SIZE);
    g.warm_count = warm;
    return g;
}

static int guard_count_is(unsigned long expected)
{
    unsigned long count = 999;
    return guard_peek(&count) && count == expected;
}

static int publication_is(const char *boot, unsigned int warm)
{
    struct generation actual, expected = expected_generation(boot, warm);
    int i = find_file(GUARD_GEN_PATH);
    return i >= 0 && files[i].size == GENERATION_SIZE &&
           read_generation(&actual) && generation_equal(&actual, &expected);
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __func__, __LINE__, #condition); \
        return 0; \
    } \
} while (0)

static int test_generation_parser(void)
{
    static const char log[] = "x WARM(warm_count=2) y\nWARM(warm_count=258) z\n";
    struct warm_parser parser;
    struct generation g;
    unsigned long split;
    reset_model();

    CHECK(scan_generation(&log_reader, &g));
    CHECK(g.warm_count == 0); /* complete cold scan: no WARM means zero */
    CHECK(!memcmp(g.boot_id, boot_a, BOOT_ID_SIZE));

    for (split = 0; split <= sizeof(log) - 1; split++) {
        warm_parser_init(&parser);
        CHECK(warm_parser_feed(&parser, (const unsigned char *)log, split));
        CHECK(warm_parser_feed(&parser, (const unsigned char *)log + split,
                               (long)sizeof(log) - 1 - split));
        CHECK(warm_parser_finish(&parser));
        CHECK(parser.seen && parser.latest == 258); /* latest record wins */
    }
    warm_parser_init(&parser);
    CHECK(!warm_parser_feed(&parser, (const unsigned char *)"WARM(warm_count=x)", 18));
    warm_parser_init(&parser);
    CHECK(!warm_parser_feed(&parser, (const unsigned char *)"WARM(warm_count=4294967296)", 27));

    generation_reader_reset(&log_reader, fake_pid);
    add_text(BOOT_LOG_PATH, log);
    CHECK(scan_generation(&log_reader, &g));
    CHECK(g.warm_count == 258);
    CHECK(write_generation(&g));
    CHECK(publication_is(boot_a, 258));
    {
        int i = find_file(GUARD_GEN_PATH);
        CHECK(files[i].data[36] == 0x02 && files[i].data[37] == 0x01 &&
              files[i].data[38] == 0x00 && files[i].data[39] == 0x00);
    }
    return 1;
}

static int test_incremental_split_and_failures(void)
{
    static const char first[] = "0: WARM(warm_co";
    static const char second[] = "unt=9)\n";
    struct generation g;
    long before;
    reset_model();

    CHECK(scan_generation(&log_reader, &g) && g.warm_count == 0);
    before = boot_log_bytes;
    append_text(BOOT_LOG_PATH, first);
    CHECK(!scan_generation(&log_reader, &g)); /* incomplete marker is temporary stock */
    append_text(BOOT_LOG_PATH, second);
    CHECK(scan_generation(&log_reader, &g) && g.warm_count == 9);
    CHECK(boot_log_bytes - before == (long)(sizeof(first) + sizeof(second) - 2));
    CHECK(log_reader.pid == fake_pid);

    add_text(BOOT_LOG_PATH, "WARM(warm_count=bad)\n");
    generation_reader_reset(&log_reader, fake_pid);
    CHECK(!scan_generation(&log_reader, &g));
    generation_reader_reset(&log_reader, fake_pid);
    add_text(BOOT_LOG_PATH, "WARM(warm_count=1)\n");
    strcpy(fake_error_path, BOOT_LOG_PATH);
    fake_read_error_once = 1;
    CHECK(!scan_generation(&log_reader, &g));
    CHECK(scan_generation(&log_reader, &g) && g.warm_count == 1); /* retry same fd */
    add_text(BOOT_ID_PATH, "short\n");
    CHECK(!scan_generation(&log_reader, &g));
    return 1;
}

static int test_initial_single_arm_and_repair(void)
{
    long reads;
    reset_model();
    CHECK(guard_begin() == 1);
    CHECK(guard_count_is(1));
    CHECK(invalidate_generation());
    CHECK(guard_runtime_check() == 1);
    CHECK(guard_count_is(1)); /* first publication reuses init arm */
    CHECK(publication_is(boot_a, 0));

    reads = boot_log_reads;
    fake_now++; /* same second: no second generation scan */
    fake_now--;
    CHECK(guard_runtime_check() == 1);
    CHECK(boot_log_reads == reads);

    add_text(GUARD_GEN_PATH, "x"); /* stale/short known publication is repaired */
    fake_now++;
    CHECK(guard_runtime_check() == 1);
    CHECK(publication_is(boot_a, 0));
    CHECK(guard_count_is(1));

    append_text(BOOT_LOG_PATH, "WARM(warm_count=1)\n");
    fake_now++;
    CHECK(guard_runtime_check() == 1);
    CHECK(publication_is(boot_a, 1));
    CHECK(guard_count_is(2)); /* only replacement of a known generation arms */

    add_text(GUARD_GEN_PATH, "bad");
    fake_now++;
    CHECK(guard_runtime_check() == 1); /* repair does not increment again */
    CHECK(publication_is(boot_a, 1));
    CHECK(guard_count_is(2));
    return 1;
}

static int test_descendant_handshake_and_pid_reset(void)
{
    struct generation published;
    reset_model();
    CHECK(guard_begin() == 1);
    CHECK(invalidate_generation());
    CHECK(guard_runtime_check() == 1);
    CHECK(read_generation(&published));

    fake_pid = 200;
    add_text(GUARD_GEN_PATH, "");
    CHECK(guard_target_check() == 0); /* unpublished descendant remains stock */
    CHECK(guard_count_is(1));
    CHECK(write_generation(&published));
    CHECK(guard_target_check() == 1);
    CHECK(log_reader.pid == 200);

    append_text(BOOT_LOG_PATH, "WARM(warm_count=4)\n");
    CHECK(guard_target_check() == 0); /* stale publication cannot authorize new WARM */
    CHECK(guard_count_is(1));

    fake_pid = 201; /* inherited reader state/fd must not be reused */
    CHECK(guard_target_check() == 0);
    CHECK(log_reader.pid == 201);
    CHECK(guard_count_is(1));
    return 1;
}

static int test_cold_uuid_change_and_generation_changes(void)
{
    reset_model();
    CHECK(guard_begin() == 1 && invalidate_generation());
    CHECK(guard_runtime_check() == 1 && guard_count_is(1));
    append_text(BOOT_LOG_PATH, "WARM(warm_count=2)\nWARM(warm_count=7)\n");
    fake_now++;
    CHECK(guard_runtime_check() == 1);
    CHECK(publication_is(boot_a, 7) && guard_count_is(2));

    add_text(BOOT_ID_PATH, "ffeeddcc-bbaa-9988-7766-554433221100\n");
    fake_now++;
    CHECK(guard_runtime_check() == 1);
    CHECK(publication_is(boot_b, 7) && guard_count_is(3));
    return 1;
}

static int test_transient_stock_vs_explicit_disarm(void)
{
    reset_model();
    CHECK(guard_begin() == 1 && invalidate_generation());
    CHECK(guard_runtime_check() == 1);
    fake_pid = 200;

    add_text(GUARD_PATH, ""); /* short guard file is not a disarm */
    CHECK(guard_target_check() == 0);
    CHECK(!guard_failed && recipe_enabled);
    add_text(GUARD_PATH, "garbage\n");
    CHECK(guard_target_check() == 0);
    CHECK(!guard_failed && recipe_enabled);

    add_text(GUARD_PATH, "1\n");
    add_text(GUARD_GEN_PATH, "x"); /* short generation is also temporary stock */
    CHECK(guard_target_check() == 0);
    CHECK(!guard_failed && guard_count_is(1));

    add_text(GUARD_PATH, "5\n");
    CHECK(guard_target_check() == -1);
    disarm_all();
    CHECK(guard_failed && !recipe_enabled);
    CHECK(guard_count_is(GUARD_DISARM));
    CHECK(read_state() == STATE_DISARMED);
    return 1;
}

static int test_clock_and_lock_errors_are_temporary(void)
{
    reset_model();
    fake_clock_error = 1;
    CHECK(guard_begin() == 0);
    CHECK(guard_count_is(0) && !guard_failed);
    fake_clock_error = 0;
    fake_lock_error = 1;
    CHECK(guard_begin() == 0);
    CHECK(guard_count_is(0) && !guard_failed);
    return 1;
}

static int test_contended_transition_is_stock(void)
{
    long reads;
    unsigned long serial;
    reset_model();
    CHECK(guard_begin() == 1 && invalidate_generation());
    reads = fake_reads;
    serial = temp_serial;
    generation_gate = 1; /* simulate another thread owning the complete transition */

    CHECK(guard_runtime_check() == 0);
    CHECK(guard_target_check() == 0);
    CHECK(fake_reads == reads); /* no guard, UUID, publication, or log reads */
    CHECK(temp_serial == serial); /* no atomic publication/arm write was attempted */
    CHECK(guard_count_is(1));

    generation_gate = 0;
    CHECK(guard_runtime_check() == 1);
    CHECK(guard_count_is(1) && publication_is(boot_a, 0));
    return 1;
}

static int test_inherited_locked_gate_recovers(void)
{
    reset_model();
    CHECK(guard_begin() == 1 && invalidate_generation());
    CHECK(guard_runtime_check() == 1 && publication_is(boot_a, 0));

    generation_gate = 1; /* fork while another parent thread owns the gate */
    generation_gate_pid = 100;
    fake_pid = 200; /* child: guard_owner_pid deliberately remains the parent's PID */
    CHECK(guard_target_check() == 1);
    CHECK(generation_gate_pid == 200 && generation_gate == 0);
    CHECK(log_reader.pid == 200); /* the recovered child then resets its inherited reader */
    CHECK(guard_count_is(1) && publication_is(boot_a, 0));

    generation_gate = 1; /* same-child contention must still fail immediately */
    CHECK(guard_target_check() == 0);
    CHECK(generation_gate_pid == 200 && generation_gate == 1);
    generation_gate = 0;
    return 1;
}

#define OWNER_THREADS 16
static void *run_owner_transition(void *argument)
{
    int *status = argument;
    *status = guard_runtime_check();
    return 0;
}

static int test_threaded_owner_single_increment(void)
{
    pthread_t threads[OWNER_THREADS];
    int status[OWNER_THREADS];
    int created = 0;
    int create_ok = 1;
    int i;
    long reads;
    reset_model();
    CHECK(guard_begin() == 1 && invalidate_generation());
    CHECK(guard_runtime_check() == 1 && guard_count_is(1));
    append_text(BOOT_LOG_PATH, "WARM(warm_count=1)\n");
    fake_now++;
    reads = boot_log_reads;

    for (i = 0; i < OWNER_THREADS; i++) {
        status[i] = -99;
        if (pthread_create(&threads[i], 0, run_owner_transition, &status[i]) != 0) {
            create_ok = 0;
            break;
        }
        created++;
    }
    for (i = 0; i < created; i++)
        CHECK(pthread_join(threads[i], 0) == 0);
    CHECK(create_ok);
    for (i = 0; i < created; i++)
        CHECK(status[i] == 0 || status[i] == 1); /* contention is stock, never disarm */

    CHECK(guard_count_is(2)); /* exactly one arm for this one WARM transition */
    CHECK(publication_is(boot_a, 1));
    CHECK(boot_log_reads == reads + 1); /* only the winning transition read appended data */
    return 1;
}

int main(void)
{
    static const struct { const char *name; int (*run)(void); } tests[] = {
        { "generation parser and exact encoding", test_generation_parser },
        { "incremental split markers and failures", test_incremental_split_and_failures },
        { "initial single arm and publication repair", test_initial_single_arm_and_repair },
        { "descendant handshake and PID reset", test_descendant_handshake_and_pid_reset },
        { "cold UUID and warm generation changes", test_cold_uuid_change_and_generation_changes },
        { "transient stock versus explicit disarm", test_transient_stock_vs_explicit_disarm },
        { "clock and lock failures", test_clock_and_lock_errors_are_temporary },
        { "contended transition returns stock without work", test_contended_transition_is_stock },
        { "inherited locked gate recovers in child", test_inherited_locked_gate_recovers },
        { "threaded owner transition increments once", test_threaded_owner_single_increment },
    };
    unsigned int i;
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (!tests[i].run())
            return 1;
        printf("ok - %s\n", tests[i].name);
    }
    printf("%u tests passed\n", i);
    return 0;
}
