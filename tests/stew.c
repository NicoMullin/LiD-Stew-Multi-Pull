/* The random stew draw, end to end, without the game.
 *
 * fake_pull below is the server's stew pull in the game's own byte shapes:
 * the box stew's draw call, then the normal stew's, each given the stew's odds
 * list name from [rsi+0x18]. fake_box and fake_normal carry the bytes the
 * plugin checks inside the two draws (the odds query, the rows' clean-up and
 * the memory call). The plugin finds them, puts its real stub in, and the
 * pulls below go through it. Built with lid_stewpull.c included, so it
 * tests the plugin's own code, not a copy. */
#define LID_SP_NO_DLLMAIN
#include "../dll/lid_stewpull.c"

#include <stdio.h>
#include <stdlib.h>

void *fake_query(const wchar_t *odds_id);
void fake_free_rows(struct rows *rows);
void *fake_realloc(void *p, DWORD bytes, DWORD align);
void *game_normal_draw(struct fstring *out, const wchar_t *odds_id);
void fake_take_c(void);
/* the normal stews' queue: its decal ids at +8, how many at +0x10, taken from the end */
struct fake_queue { void *unused; struct fstring *ids; int num, max; } g_queue;
void fake_pull(const void *gacha_row);
int fake_handler(const void *gacha_row);
extern unsigned char fake_normal[], fake_pay_one[], fake_draw_all[];
int fake_purchase(void *user, const wchar_t *product, int count, void *extra, void *none);
const wchar_t *fake_text_of(void *text);
void fake_empty(void *decals, int zero);
void *draw_all_standin(void *decals, const void *gacha_row, int count);
char fake_check_c(struct rows *ids);
unsigned char g_reply[0x700], g_user[16];
void *g_handler_vtable[16], *g_handler[1] = {g_handler_vtable};
const wchar_t empty_text[] = L"";
struct fstring g_out;

/* the shop's stew list: rows of 0x30 bytes (the id at +8, the type at +0x28) */
struct shop_row { void *unused; struct fstring id; void *pad[2]; const wchar_t *type; };
struct id_list { const wchar_t *ids[8]; int num, max; };
void fake_current(const struct shop_row *rows, int count, struct id_list *out);
int (*g_wcscmp)(const wchar_t *, const wchar_t *) = wcscmp;
const wchar_t box_text[] = L"BOX";
void fake_add_id(struct id_list *out, const struct fstring *id)
{
    if (out->num < 8)
        out->ids[out->num] = id->data;
    out->num++;
}

__asm__(
    ".text\n"
    /* the normal draw: a jump to the stand-in, then the bytes checked at +0x4a */
    ".globl fake_normal\n"
    "fake_normal:\n"
    "    jmp game_normal_draw\n"
    "    .fill 0x2E - (. - fake_normal), 1, 0xCC\n"
    "    .byte 0x48,0x8D,0x0D\n"
    "    .long g_queue - (. + 4)\n"
    "    .byte 0xE8\n"
    "    .long fake_take - (. + 4)\n"
    "    .fill 0x4A - (. - fake_normal), 1, 0xCC\n"
    "    .byte 0x48,0x89,0x33,0x89,0x53,0x08,0x89,0x53,0x0C,0x03,0xD2\n"
    "    .byte 0x44,0x8D,0x46,0x08,0x48,0x8B,0x0B,0xE8\n"
    "    .long fake_realloc - (. + 4)\n"
    "    int3\n"
    /* the queue's take: never run here, only read */
    ".globl fake_take\n"
    "fake_take:\n"
    "    jmp fake_take_c\n"
    "    .fill 0x42 - (. - fake_take), 1, 0xCC\n"
    "    .byte 0x8B,0x41,0x10,0xFF,0xC8,0x4C,0x63,0xF0,0x49,0xC1,0xE6,0x04,0x4C,0x03,0x71,0x08\n"
    "    int3\n"
    /* the box draw: never run here, only read */
    ".globl fake_box\n"
    "fake_box:\n"
    "    .fill 0x3B, 1, 0xCC\n"
    "    .byte 0x48,0x8B,0xCA,0xE8\n"
    "    .long fake_query - (. + 4)\n"
    "    .byte 0x4C,0x8B,0x10,0x45,0x33,0xC9,0x45,0x33,0xC0,0x48,0x8D,0x55,0xB0,0x48,0x8B,0xC8\n"
    "    .byte 0x41,0xFF,0x52,0x10\n"
    "    .fill 0x325 - (. - fake_box), 1, 0xCC\n"
    "    .byte 0x48,0x8D,0x4D,0xB0,0xE8\n"
    "    .long fake_free_rows - (. + 4)\n"
    "    .byte 0x48,0x8B,0xC6\n"
    "    int3\n"
    /* the game's draw of `count` decals: the normal draw call below is in it */
    ".globl fake_draw_all\n"
    "fake_draw_all:\n"
    "    jmp draw_all_standin\n"
    /* the pull: rcx = the stew's row (its odds list name at +0x18) */
    ".globl fake_pull\n"
    "fake_pull:\n"
    "    pushq %rbp\n"
    "    pushq %rsi\n"
    "    pushq %rbx\n"
    "    movq %rsp, %rbp\n"
    "    subq $0x60, %rsp\n"
    "    movq %rcx, %rsi\n"
    "    jmp 1f\n"
    "    .byte 0x48,0x8B,0x56,0x18,0xEB,0x07,0x48,0x8D,0x15\n"
    "    .long empty_text - (. + 4)\n"
    "    .byte 0x4C,0x8D,0x45,0x50,0x48,0x8D,0x4D,0xD8,0xE8\n"
    "    .long fake_box - (. + 4)\n"
    "    .byte 0x48,0x8B,0xD8\n"
    "1:\n"
    "    .byte 0x48,0x8B,0x56,0x18,0xEB,0x07,0x48,0x8D,0x15\n"
    "    .long empty_text - (. + 4)\n"
    "    .byte 0x48,0x8D,0x4D,0xE8,0xE8\n"
    "    .long fake_normal - (. + 4)\n"
    "    .byte 0x48,0x8B,0xD8,0x48,0x8D,0x45,0xC8,0x48,0x3B,0xC3,0x74,0x00\n"
    "    movq (%rbx), %rax\n"
    "    movq %rax, g_out(%rip)\n"
    "    movq 8(%rbx), %rax\n"
    "    movq %rax, g_out+8(%rip)\n"
    "    movq %rbp, %rsp\n"
    "    popq %rbx\n"
    "    popq %rsi\n"
    "    popq %rbp\n"
    "    ret\n"
    /* the game's purchase of one: a count of 1 passed on */
    ".globl fake_pay_one\n"
    "fake_pay_one:\n"
    "    .byte 0x48,0x83,0xEC,0x38,0x4D,0x8B,0xC8,0x41,0xB8,0x01,0x00,0x00,0x00\n"
    "    .byte 0x48,0xC7,0x44,0x24,0x20,0x00,0x00,0x00,0x00,0xE8\n"
    "    .long fake_purchase - (. + 4)\n"
    "    addq $0x38, %rsp\n"
    "    ret\n"
    /* the pull's check of its list of stews: only read, never run */
    "    .byte 0x83,0xBD,0x44,0x04,0x00,0x00,0x00,0x7E,0x00,0x48,0x8D,0x8D,0x3C,0x04,0x00,0x00,0xE8\n"
    "    .long fake_check_c - (. + 4)\n"
    "    .byte 0x84,0xC0,0x74,0x00,0x49,0x8B,0x06,0x45,0x33,0xC9,0x41,0xB8,0xEB,0x03,0x00,0x00\n"
    "    int3\n"
    /* the stew pull handler, from the payment to the draw: rcx = the stew's
     * row, copied to [rbp-0x80]; r13 = the reply; r14 = the handler, whose
     * method +0x68 is the error reply. Returns 0, or -1 when not paid. */
    ".globl fake_handler\n"
    "fake_handler:\n"
    "    pushq %rbp\n"
    "    pushq %rsi\n"
    "    pushq %rbx\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    movq %rsp, %rbp\n"
    "    subq $0xA0, %rsp\n"
    "    movq (%rcx), %rax\n    movq %rax, -0x80(%rbp)\n"
    "    movq 8(%rcx), %rax\n    movq %rax, -0x78(%rbp)\n"
    "    movq 16(%rcx), %rax\n    movq %rax, -0x70(%rbp)\n"
    "    movq 24(%rcx), %rax\n    movq %rax, -0x68(%rbp)\n"
    "    leaq g_reply(%rip), %r13\n"
    "    leaq g_handler(%rip), %r14\n"
    "    xorl %esi, %esi\n"
    "    .byte 0x48,0x8D,0x4D,0xE0,0xE8\n"
    "    .long fake_text_of - (. + 4)\n"
    "    .byte 0x48,0x8B,0xD0,0x4C,0x8B,0xC6,0x48,0x8D,0x0D\n"
    "    .long g_user - (. + 4)\n"
    "    .byte 0xE8\n"
    "    .long fake_pay_one - (. + 4)\n"
    "    .byte 0x85,0xC0,0x79,0x11\n"
    "    .byte 0x49,0x8B,0x06,0x49,0x8B,0xD5,0x49,0x8B,0xCE,0xFF,0x50,0x68\n"
    "    .byte 0xE9\n"
    "    .long 5f - (. + 4)\n"
    "    .byte 0x49,0x8D,0x9D,0xE8,0x05,0x00,0x00,0x33,0xD2,0x48,0x8B,0xCB,0xE8\n"
    "    .long fake_empty - (. + 4)\n"
    "    .byte 0x41,0xB8,0x01,0x00,0x00,0x00,0x48,0x8D,0x55,0x80,0x48,0x8B,0xCB,0xE8\n"
    "    .long fake_draw_all - (. + 4)\n"
    "    .byte 0x41,0xB0,0x01\n"
    "    xorl %eax, %eax\n"
    "    jmp 6f\n"
    "5:\n"
    "    movl $-1, %eax\n"
    "6:\n"
    "    movq %rbp, %rsp\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %rbx\n"
    "    popq %rsi\n"
    "    popq %rbp\n"
    "    ret\n"
    /* the shop's stew list: rcx = rows, edx = how many, r8 = the id list.
     * Walks the rows, skips box stews, adds the first other one and stops. */
    ".globl fake_current\n"
    "fake_current:\n"
    "    pushq %rbx\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %r14\n"
    "    subq $0x38, %rsp\n"
    "    movl %edx, 0x30(%rsp)\n"
    "    movq %rcx, 0x28(%rsp)\n"
    "    movq %r8, %r14\n"
    "    leaq box_text(%rip), %rsi\n"
    "    xorl %edi, %edi\n"
    "2:\n"
    "    movl 0x30(%rsp), %eax\n"
    "    cmpl %eax, %edi\n"
    "    jge 3f\n"
    "    movslq %edi, %rbx\n"
    "    imulq $0x30, %rbx, %rbx\n"
    "    addq 0x28(%rsp), %rbx\n"
    "    movq 0x28(%rbx), %rcx\n"
    "    .byte 0x48,0x8B,0xD6,0xFF,0x15\n"
    "    .long g_wcscmp - (. + 4)\n"
    "    .byte 0x85,0xC0,0x75,0x04,0xFF,0xC7,0xEB\n"
    "    .byte 2b - (. + 1)\n"
    "    .byte 0x48,0x8D,0x53,0x08,0x49,0x8B,0xCE,0xE8\n"
    "    .long fake_add_id - (. + 4)\n"
    "    .byte 0x8B,0x44,0x24,0x30,0x41,0x83,0x7E,0x08,0x00\n"
    "3:\n"
    "    addq $0x38, %rsp\n"
    "    popq %r14\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rbx\n"
    "    ret\n"
);

/* ------------------------------------------------- the game stand-ins */

struct odds { const wchar_t *sklid; int odds; };
static const struct odds DM_LIST[] = {
    {L"SKL_A", 1}, {L"SKL_ZERO", 0}, {L"SKL_C", 3}, {L"", 5}, {L"SKL_NEG", -4},
};
static const struct odds EMPTY_LIST[] = {{L"SKL_ZERO", 0}, {L"", 5}, {L"SKL_NEG", -4}};
static const struct odds RUN_LIST[] = {{L"SKL_A", 1}, {L"SKL_B", 1}, {L"SKL_ZERO", 0}, {L"SKL_C", 5}};

/* the pull's check of its list of stews: the game's always answers "fine" */
static int g_checks;
static char fake_check(struct rows *ids) { (void)ids; g_checks++; return 0; }
char fake_check_c(struct rows *ids) { return fake_check(ids); }

/* the payment and draw stand-ins of the handler above */
static int g_money, g_purchases, g_paid_count, g_draw_calls, g_draw_count, g_errors, g_decal_n;
static wchar_t g_decals[32][64];
const wchar_t *fake_text_of(void *text) { (void)text; return L"PRD_SKILL_GACHA_MEDAL"; }
int fake_purchase(void *user, const wchar_t *product, int count, void *extra, void *none)
{
    (void)user; (void)product; (void)extra; (void)none;
    g_purchases++;
    g_paid_count = count;
    if (g_money < count * 10)
        return -1;
    g_money -= count * 10;
    return 0;
}
void fake_empty(void *decals, int zero) { (void)decals; (void)zero; g_decal_n = 0; }
static void fake_error(void *self, void *reply) { (void)self; (void)reply; g_errors++; }

/* the request's list of stews, with the run's hints: `left` > 0 the pulls
 * left, `all` > 0 the whole run at once */
static char check_ids(int left, int all)
{
    static wchar_t hint[32], whole[32];
    struct fstring items[4] = {
        {(wchar_t *)L"SKLGACH_NORMAL_OFFLINE", 23, 23}, {(wchar_t *)L"SKLGACH_RANDOM_DM", 18, 18},
        {hint, 0, 0}, {whole, 0, 0}};
    struct rows ids = {(unsigned char *)items, 2, 4};
    swprintf(hint, 32, L"LID_MULTI_LEFT=%d", left);
    swprintf(whole, 32, L"LID_MULTI_ALL=%d", all);
    items[2].num = items[2].max = (int)wcslen(hint) + 1;
    items[3].num = items[3].max = (int)wcslen(whole) + 1;
    if (left > 0)
        ids.num = 3;
    if (all > 0)
        items[ids.num++] = items[3];
    return lid_sp_pull_ids(&ids);
}

/* the request's list of stews, with the multi pull's count when `left` > 0 */
static char check_pull(int left) { return check_ids(left, 0); }

static void pull(const wchar_t *odds_id);

/* one pull of a random stew; the decal it gave, as a short name */
static const wchar_t *pull_one(int left)
{
    static wchar_t got[64];
    check_pull(left);
    pull(L"SKLGACH_RANDOM_DM");
    wcscpy(got, g_out.data ? g_out.data : L"?");
    free(g_out.data);
    return got;
}
static const struct odds *g_list;
static int g_list_n, g_queries, g_frees, g_normal_calls, g_allocs;
static const wchar_t *g_query_id;

static void get_rows(void *query, struct rows *out, void *a, void *b)
{
    int i;
    (void)query; (void)a; (void)b;
    out->num = out->max = g_list_n;
    out->data = calloc(g_list_n ? g_list_n : 1, ODDS_ROW_SIZE);
    for (i = 0; i < g_list_n; i++) {
        unsigned char *row = out->data + (size_t)i * ODDS_ROW_SIZE;
        struct fstring id = {(wchar_t *)g_list[i].sklid, (int)wcslen(g_list[i].sklid) + 1, 0};
        if (!g_list[i].sklid[0])
            id.num = 0, id.data = NULL;
        memcpy(row + ODDS_ROW_SKLID, &id, sizeof id);
        memcpy(row + ODDS_ROW_ODDS, &g_list[i].odds, sizeof(int));
    }
}

static void *query_vtable[4] = {NULL, NULL, (void *)get_rows, NULL};
static void *query_object = query_vtable;
static int g_query_fails;

void *fake_query(const wchar_t *odds_id)
{
    g_queries++;
    g_query_id = odds_id;
    return g_query_fails ? NULL : &query_object;
}

void fake_free_rows(struct rows *rows) { g_frees++; free(rows->data); rows->data = NULL; }
void *fake_realloc(void *p, DWORD bytes, DWORD align) { (void)align; g_allocs++; return realloc(p, bytes); }

/* the game's normal draw: the queue's last decal, taken off it; an empty
 * queue is rolled again first (here: one SKL_FROM_QUEUE) */
static struct fstring g_queue_store[64];
static int g_refills;
void fake_take_c(void) {}
void *game_normal_draw(struct fstring *out, const wchar_t *odds_id)
{
    static wchar_t text[] = L"SKL_FROM_QUEUE";
    (void)odds_id;
    g_normal_calls++;
    if (g_queue.num <= 0) {
        g_refills++;
        g_queue.ids = g_queue_store;
        g_queue_store[0].data = text;
        g_queue_store[0].num = g_queue_store[0].max = (int)wcslen(text) + 1;
        g_queue.num = 1;
    }
    *out = g_queue.ids[--g_queue.num];
    return out;
}

/* the queue, from its first decal to its last (the next to come) */
static void set_queue(int n, const wchar_t *const *ids)
{
    int i;
    for (i = 0; i < n; i++) {
        g_queue_store[i].data = (wchar_t *)ids[i];
        g_queue_store[i].num = g_queue_store[i].max = (int)wcslen(ids[i]) + 1;
    }
    g_queue.ids = g_queue_store;
    g_queue.num = n;
}

/* ----------------------------------------------------------- the tests */

static int fails;
static void expect(const char *what, long long got, long long want)
{
    printf("%-58s got %lld want %lld  %s\n", what, got, want, got == want ? "ok" : "FAIL");
    if (got != want) fails++;
}

static void pull(const wchar_t *odds_id)
{
    unsigned char row[0x20] = {0};
    memcpy(row + 0x18, &odds_id, sizeof odds_id);
    memset(&g_out, 0, sizeof g_out);
    fake_pull(row);
}

/* the game's draw loop: `count` pulls into the reply */
void *draw_all_standin(void *decals, const void *gacha_row, int count)
{
    int i;
    (void)decals;
    g_draw_calls++;
    g_draw_count = count;
    for (i = 0; i < count; i++) {
        memset(&g_out, 0, sizeof g_out);
        fake_pull(gacha_row);
        if (g_decal_n < 32)
            wcscpy(g_decals[g_decal_n++], g_out.data ? g_out.data : L"?");
        if (g_out.data && is_random_stew(*(const wchar_t *const *)((const unsigned char *)gacha_row + 0x18)))
            free(g_out.data);
    }
    return decals;
}

/* one request of the handler for the stew whose odds list is `odds_id` */
static int request(const wchar_t *odds_id, int left, int all)
{
    unsigned char row[0x20] = {0};
    memcpy(row + 0x18, &odds_id, sizeof odds_id);
    check_ids(left, all);
    g_decal_n = 0;
    return fake_handler(row);
}

int main(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleW(NULL);
    int i, a = 0, c = 0, other = 0, n = 8000;
    char log[8192] = {0};
    FILE *f;

    InitializeCriticalSection(&g_log_lock);
    wcscpy(g_log_path, L"stew-test.log");
    DeleteFileW(g_log_path);

    find_pieces(base, &g_pc);
    expect("the stew pull is found", g_pc.stew_ok, 1);
    expect("  the normal draw", g_pc.normal_draw == (void *)fake_normal, 1);
    expect("  the odds query", g_pc.odds_query == (void *)fake_query, 1);
    expect("  the rows' clean-up", g_pc.odds_free == (void *)fake_free_rows, 1);
    expect("  the memory function", g_pc.game_realloc == (void *)fake_realloc, 1);
    expect("the payment is found", g_pc.pay != NULL && g_pc.pay_one == (void *)fake_pay_one, 1);
    expect("  the purchase behind it", g_pc.purchase == (void *)fake_purchase, 1);
    expect("the draw of the reply's decals is found", g_pc.drawn != NULL && g_pc.draw_all == (void *)fake_draw_all, 1);
    expect("the pull's check is found", g_pc.pull_ids != NULL, 1);
    if (!g_pc.stew_ok) { puts("\nFAILED - nothing to test"); return 1; }

    {
        static const struct shop_row rows[] = {
            {NULL, {(wchar_t *)L"SKLGACH_NORMAL", 15, 15}, {NULL, NULL}, L"NORMAL"},
            {NULL, {(wchar_t *)L"SKLGACH_BOX", 12, 12}, {NULL, NULL}, L"BOX"},
            {NULL, {(wchar_t *)L"SKLGACH_RANDOM_DM", 18, 18}, {NULL, NULL}, L"NORMAL"},
            {NULL, {(wchar_t *)L"SKLGACH_FEST", 13, 13}, {NULL, NULL}, L"NORMAL"},
        };
        struct id_list list;

        expect("the shop's stew list is found",
               g_pc.stew_ids != NULL && g_pc.stew_ids_add == (void *)fake_add_id, 1);
        memset(&list, 0, sizeof list);
        fake_current(rows, 4, &list);
        expect("before: only the first normal stew is offered", list.num, 1);

        g_page = page_near(base);
        patch_stew(base);

        puts("-- the shop's stew list");
        memset(&list, 0, sizeof list);
        fake_current(rows, 4, &list);
        expect("every normal stew is offered", list.num, 3);
        expect("  in order, without the box stew",
               list.num == 3 && wcscmp(list.ids[0], L"SKLGACH_NORMAL") == 0 &&
               wcscmp(list.ids[1], L"SKLGACH_RANDOM_DM") == 0 && wcscmp(list.ids[2], L"SKLGACH_FEST") == 0, 1);
        memset(&list, 0, sizeof list);
        fake_current(rows + 1, 1, &list);
        expect("only a box stew: nothing offered", list.num, 0);
    }

    puts("-- a normal stew: the game's own draw, unchanged");
    pull(L"SKLGACH_NORMAL_OFFLINE");
    expect("the game's draw ran", g_normal_calls, 1);
    expect("no odds list read", g_queries, 0);
    expect("its decal comes back", g_out.data && wcscmp(g_out.data, L"SKL_FROM_QUEUE") == 0, 1);

    puts("-- a random stew: a fresh pick from its own list");
    g_list = DM_LIST;
    g_list_n = sizeof DM_LIST / sizeof DM_LIST[0];
    g_rng = 12345;
    for (i = 0; i < n; i++) {
        pull(L"SKLGACH_RANDOM_DM");
        if (g_out.data && wcscmp(g_out.data, L"SKL_A") == 0 && g_out.num == 6) a++;
        else if (g_out.data && wcscmp(g_out.data, L"SKL_C") == 0 && g_out.num == 6) c++;
        else other++;
        free(g_out.data);
    }
    expect("the game's draw never ran", g_normal_calls, 1);
    expect("the odds list read once a pull", g_queries, n);
    expect("  by its own name", wcscmp(g_query_id, L"SKLGACH_RANDOM_DM") == 0, 1);
    expect("its rows freed once a pull", g_frees, n);
    expect("never a decal with odds 0 or below, or no id", other, 0);
    printf("  SKL_A (odds 1): %d, SKL_C (odds 3): %d of %d\n", a, c, n);
    expect("SKL_A about 1 in 4 (22-28%)", a > n * 22 / 100 && a < n * 28 / 100, 1);
    expect("repeats happen (both drawn many times)", a > 1000 && c > 1000, 1);

    puts("-- multi pulls: no decal twice");
    g_pc.pull_ids_check = (void *)fake_check;
    g_list = RUN_LIST;
    g_list_n = sizeof RUN_LIST / sizeof RUN_LIST[0];
    {
        int run, bad3 = 0, bad5 = 0, single_repeat = 0, odd_c_first = 0;
        wchar_t got[5][64];
        for (run = 0; run < 2000; run++) {
            /* 3 pulls, 3 decals: all three, in some order */
            wcscpy(got[0], pull_one(3));
            wcscpy(got[1], pull_one(2));
            wcscpy(got[2], pull_one(1));
            if (!wcscmp(got[0], got[1]) || !wcscmp(got[0], got[2]) || !wcscmp(got[1], got[2]))
                bad3++;
            if (!wcscmp(got[0], L"SKL_C"))
                odd_c_first++;
            /* 5 pulls, 3 decals: three different, then two more, different again */
            for (i = 0; i < 5; i++)
                wcscpy(got[i], pull_one(5 - i));
            if (!wcscmp(got[0], got[1]) || !wcscmp(got[0], got[2]) || !wcscmp(got[1], got[2]) ||
                !wcscmp(got[3], got[4]))
                bad5++;
            /* single pulls (no count) start afresh: the same decal twice in a row
             * can happen */
            if (!wcscmp(pull_one(0), pull_one(0)))
                single_repeat++;
        }
        expect("a 3-pull run of 3 decals: never the same twice", bad3, 0);
        expect("a 5-pull run of 3 decals: all 3, then 2 different", bad5, 0);
        expect("the odds still count (SKL_C, odds 5 of 7, first most runs)",
               odd_c_first > 2000 * 60 / 100, 1);
        expect("single pulls still repeat sometimes", single_repeat > 0, 1);
        expect("the game's check still runs for every pull", g_checks, 2000 * 10);
        g_run_left = 0;
        g_run_n = 0;
    }

    puts("-- a whole run in one request");
    g_handler_vtable[0x68 / 8] = (void *)fake_error;
    {
        int run, bad = 0, paid_once = 0, drawn_once = 0;
        for (run = 0; run < 500; run++) {
            g_money = 1000; g_purchases = 0; g_draw_calls = 0;
            if (request(L"SKLGACH_RANDOM_DM", 5, 5) != 0)
                bad++;
            if (g_purchases == 1 && g_paid_count == 5 && g_money == 950)
                paid_once++;
            if (g_draw_calls == 1 && g_draw_count == 5 && g_decal_n == 5)
                drawn_once++;
            /* 5 of 3 decals: all three, then two different */
            if (g_decal_n == 5 && (!wcscmp(g_decals[0], g_decals[1]) || !wcscmp(g_decals[0], g_decals[2]) ||
                                   !wcscmp(g_decals[1], g_decals[2]) || !wcscmp(g_decals[3], g_decals[4])))
                bad++;
        }
        expect("paid for once, the price times 5", paid_once, 500);
        expect("drawn once, 5 decals into the reply", drawn_once, 500);
        expect("  no decal twice before all have come", bad, 0);
    }
    g_money = 1000; g_purchases = 0; g_draw_calls = 0;
    request(L"SKLGACH_RANDOM_DM", 4, 0);
    expect("a pull of a run without the whole-run entry: paid for 1", g_paid_count, 1);
    expect("  and 1 drawn", g_draw_count == 1 && g_decal_n == 1, 1);
    request(L"SKLGACH_RANDOM_DM", 0, 0);
    expect("a single pull: paid for 1", g_paid_count, 1);
    expect("  and 1 drawn", g_draw_count == 1 && g_decal_n == 1, 1);
    g_money = 30; g_draw_calls = 0; g_errors = 0;
    expect("a run of 10 with money for 3: refused", request(L"SKLGACH_RANDOM_DM", 10, 10), -1);
    expect("  asked for all 10 at once", g_paid_count, 10);
    expect("  nothing paid, nothing drawn, the error reply", g_money == 30 && g_draw_calls == 0 && g_errors == 1, 1);
    request(L"SKLGACH_RANDOM_DM", 0, 0);
    expect("  the next single pull: 1 paid, 1 drawn", g_paid_count == 1 && g_draw_count == 1, 1);
    g_money = 1000; g_normal_calls = 0;
    request(L"SKLGACH_NORMAL_OFFLINE", 3, 3);
    expect("a normal stew's run of 3: paid for 3", g_paid_count, 3);
    expect("  3 taken from the game's own queue", g_normal_calls == 3 && g_decal_n == 3, 1);
    puts("-- a normal stew's run: the queue in the save as it is, repeats and all");
    {
        static const wchar_t *const start[] = {L"SKL_C", L"SKL_B", L"SKL_A", L"SKL_A"};
        static const wchar_t *const two_a[] = {L"SKL_A", L"SKL_A"};
        g_refills = 0;
        set_queue(4, start);
        request(L"SKLGACH_NORMAL_OFFLINE", 3, 3);
        expect("one request of 3: A, A, B - the queue's own order",
               g_decal_n == 3 && !wcscmp(g_decals[0], L"SKL_A") && !wcscmp(g_decals[1], L"SKL_A") &&
               !wcscmp(g_decals[2], L"SKL_B"), 1);
        expect("  C left", g_queue.num == 1 && !wcscmp(g_queue.ids[0].data, L"SKL_C"), 1);
        set_queue(4, start);
        request(L"SKLGACH_NORMAL_OFFLINE", 3, 0);
        wcscpy(g_decals[8], g_decals[0]);
        request(L"SKLGACH_NORMAL_OFFLINE", 2, 0);
        wcscpy(g_decals[9], g_decals[0]);
        request(L"SKLGACH_NORMAL_OFFLINE", 1, 0);
        expect("the same run a pull at a time: A, A, B",
               !wcscmp(g_decals[8], L"SKL_A") && !wcscmp(g_decals[9], L"SKL_A") && !wcscmp(g_decals[0], L"SKL_B"), 1);
        set_queue(4, start);
        request(L"SKLGACH_NORMAL_OFFLINE", 0, 0);
        wcscpy(g_decals[8], g_decals[0]);
        request(L"SKLGACH_NORMAL_OFFLINE", 0, 0);
        expect("single pulls: the queue's order, A then A",
               !wcscmp(g_decals[8], L"SKL_A") && !wcscmp(g_decals[0], L"SKL_A"), 1);
        set_queue(2, two_a);
        request(L"SKLGACH_NORMAL_OFFLINE", 2, 2);
        expect("a run of 2 from A, A: A twice", g_decal_n == 2 && !wcscmp(g_decals[1], L"SKL_A"), 1);
        expect("  never rolled again early", g_refills, 0);
        g_queue.num = 0;
    }
    expect("a whole-run entry of 1 or over the limit is not a run",
           (check_ids(1, 1), g_all) == 0 && (check_ids(300, 300), g_all) == 0, 1);
    g_run_left = 0;
    g_run_n = 0;

    puts("-- a random stew with nothing to draw");
    g_list = EMPTY_LIST;
    g_list_n = sizeof EMPTY_LIST / sizeof EMPTY_LIST[0];
    g_frees = 0; g_allocs = 0;
    pull(L"SKLGACH_RANDOM_EMPTY");
    expect("nothing drawn", g_out.num, 0);
    expect("no memory taken", g_allocs, 0);
    expect("its rows still freed", g_frees, 1);
    g_query_fails = 1;
    g_frees = 0;
    pull(L"SKLGACH_RANDOM_DM");
    expect("an unreadable odds list: nothing drawn", g_out.num, 0);
    expect("  and nothing freed", g_frees, 0);

    f = fopen("stew-test.log", "rb");
    if (f) { fread(log, 1, 512, f); fclose(f); }
    expect("the log names the hook", strstr(log, "draw a fresh decal") != NULL, 1);
    f = fopen("stew-test.log", "rb");
    if (f) { fseek(f, -2048, SEEK_END); fread(log, 1, sizeof log - 1, f); fclose(f); }
    expect("the log names a pick", strstr(log, "SKLGACH_RANDOM_DM -> SKL_") != NULL, 1);
    expect("the log names the empty stew", strstr(log, "SKLGACH_RANDOM_EMPTY has no decal") != NULL, 1);
    expect("the log names a whole run", strstr(log, "one request for a run of 3 pulls: paid 3 x PRD_SKILL_GACHA_MEDAL") != NULL, 1);
    expect("the log names a run not paid for", strstr(log, "a run of 10 pulls could not be paid for") != NULL, 1);
    f = fopen("stew-test.log", "rb");
    if (f) { fread(log, 1, sizeof log - 1, f); fclose(f); }
    expect("the log names the one-request hook", strstr(log, "paid for and drawn in one request") != NULL, 1);


    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails != 0;
}
