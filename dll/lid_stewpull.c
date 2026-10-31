/*
 * lid-stewpull.dll for Let It Die - a plugin for the LiD Plugin Loader that
 * changes the offline game's Mushroom Stew server code, in memory only. It
 * goes with the Stew Multi Pull mod, and gives other mods' stews what they
 * need too.
 *
 * The loader (dinput8.dll) loads this from Binaries\Win64\lid-plugins when the
 * game starts. Three changes, all in the game's built-in offline server:
 *
 * 1. A whole multi pull in one request. The Stew Multi Pull mod's Purchase xN
 *    marks every pull's request with how many pulls are left, and its first
 *    pull also asks for the whole run at once. That one request then pays for
 *    every pull with the game's own purchase (the price times the count, or
 *    nothing if that is more than the player has), the game's draw runs that
 *    many times into the reply, and the game saves once - so x25 takes about
 *    as long as one pull. Without this plugin the mod still works, one pull
 *    after another.
 *
 * 2. Random stews. Every normal stew takes its decals from ONE queue, rolled
 *    ahead and kept in the save, so a second stew could not have decals of its
 *    own. The server's call to that draw goes through lid_sp_draw(): a stew
 *    whose odds list (master_skillgacha.odds_id) starts with SKLGACH_RANDOM_
 *    gets a fresh pick from its own list, weighted by the odds, every pull,
 *    using the game's own masters.db query and memory functions - and in a
 *    multi pull, no decal twice, unless the list's name ends in _REPEATS.
 *    Every other stew is passed to the game's draw unchanged, and the queue in
 *    the save is not touched. (Used by the Death Metals mod's Death Metal
 *    stew, the Events and Seasons festival stew, and the Stew Multi Pull mod's
 *    "a fresh random pick" for the regular stew.)
 *
 * 3. Every stew offered. The offline server lists only the first stew in
 *    masters.db that is not a box stew. Where it adds that one and stops, it
 *    goes on to the next instead, so every stew is offered. The list is
 *    written to the log on each visit to the shop.
 *
 * 4. Pity. After a number of pulls of a stew without the decal the player
 *    picked for it (in the stew's Check Lineup, sent along with the next
 *    pull), the next pull gives that decal. Each stew counts on its own, the
 *    number of pulls is set by the Stew Multi Pull mod, and the picks and
 *    counts are kept in the player's save. The shop's reply and every pull's
 *    reply also tell the mod's menu how far each stew's pity is, so it can
 *    show it.
 *
 * These were part of lid-deathmetals.dll up to 0.5.x; they moved here so the
 * multi pull does not need the Death Metals mod.
 *
 * Safety rules:
 *   - Every piece is found by its exact bytes and must be found exactly once.
 *     Anything not found is left alone and named in the log. After a game
 *     update that moves the code, the plugin does nothing at all.
 *   - No network, no registry. The only file it writes is lid-stewpull.log
 *     next to itself.
 *   - Delete lid-stewpull.dll and the game is exactly stock again.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "version.h"

/* ------------------------------------------------------------------ log */

static wchar_t g_log_path[MAX_PATH];
static int g_log_started;
static CRITICAL_SECTION g_log_lock;

static void log_line(const char *fmt, ...)
{
    char msg[512];
    SYSTEMTIME t;
    va_list ap;
    int n;
    HANDLE f;
    DWORD written;

    if (!g_log_path[0])
        return;
    GetLocalTime(&t);
    n = snprintf(msg, sizeof msg, "%02u:%02u:%02u  ", t.wHour, t.wMinute, t.wSecond);
    va_start(ap, fmt);
    n += vsnprintf(msg + n, sizeof msg - n - 2, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof msg - 3)
        n = (int)sizeof msg - 3;
    msg[n++] = '\r';
    msg[n++] = '\n';

    /* the stew pull runs on the game's server thread */
    EnterCriticalSection(&g_log_lock);
    /* a fresh log each time the game starts */
    f = CreateFileW(g_log_path, g_log_started ? FILE_APPEND_DATA : GENERIC_WRITE,
                    FILE_SHARE_READ, NULL, g_log_started ? OPEN_ALWAYS : CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        g_log_started = 1;
        WriteFile(f, msg, (DWORD)n, &written, NULL);
        CloseHandle(f);
    }
    LeaveCriticalSection(&g_log_lock);
}

/* ------------------------------------------------------------ helpers */

struct span {
    unsigned char *start;
    size_t size;
};

static int section(unsigned char *base, const char *name, struct span *out)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt;
    IMAGE_SECTION_HEADER *sec;
    WORD i;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (strncmp((const char *)sec->Name, name, IMAGE_SIZEOF_SHORT_NAME) == 0) {
            out->start = base + sec->VirtualAddress;
            out->size = sec->Misc.VirtualSize;
            return 1;
        }
    }
    return 0;
}

/* pattern with a mask: 'x' = must match, '?' = any byte */
static int matches(const unsigned char *p, const unsigned char *pat, const char *mask)
{
    size_t i;
    for (i = 0; mask[i]; i++)
        if (mask[i] == 'x' && p[i] != pat[i])
            return 0;
    return 1;
}

static int find_all(struct span s, const unsigned char *pat, const char *mask, unsigned char **last)
{
    size_t i, n = strlen(mask);
    int hits = 0;
    for (i = 0; i + n <= s.size; i++) {
        if (s.start[i] == pat[0] && matches(s.start + i, pat, mask)) {
            hits++;
            *last = s.start + i;
        }
    }
    return hits;
}

/* the same, for a pattern with no wildcards */
static int find_exact(struct span s, const unsigned char *pat, size_t n, unsigned char **last)
{
    size_t i;
    int hits = 0;
    for (i = 0; i + n <= s.size; i++) {
        if (s.start[i] == pat[0] && memcmp(s.start + i, pat, n) == 0) {
            hits++;
            *last = s.start + i;
        }
    }
    return hits;
}

static int32_t read32(const unsigned char *p)
{
    int32_t v;
    memcpy(&v, p, 4);
    return v;
}

/* where a rel32 field at `at` points, for an instruction ending at `end` */
static unsigned char *rel_target(const unsigned char *at, const unsigned char *end)
{
    return (unsigned char *)end + read32(at);
}

static int rel32(const unsigned char *next, const unsigned char *target, int32_t *out)
{
    intptr_t d = target - next;
    if (d != (int32_t)d)
        return 0;
    *out = (int32_t)d;
    return 1;
}

static int is_wstr(const unsigned char *p, const wchar_t *s)
{
    size_t n = (wcslen(s) + 1) * sizeof(wchar_t);
    return p && memcmp(p, s, n) == 0;
}

static int write_code(unsigned char *at, const void *bytes, size_t n)
{
    DWORD old;
    if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
    return 1;
}

/* One page within 2 GB of the game code, for the stubs. */
static unsigned char *g_page;
static size_t g_page_used;

static unsigned char *page_near(unsigned char *code)
{
    const ULONG_PTR step = 0x10000;
    ULONG_PTR origin = (ULONG_PTR)code & ~(step - 1);
    ULONG_PTR d;

    for (d = step; d < 0x70000000; d += step) {
        void *p = VirtualAlloc((void *)(origin - d), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!p)
            p = VirtualAlloc((void *)(origin + d), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (p)
            return p;
    }
    return NULL;
}

static unsigned char *emit(const unsigned char *code, size_t n)
{
    unsigned char *at;
    if (!g_page || g_page_used + n > 4096)
        return NULL;
    at = g_page + g_page_used;
    memcpy(at, code, n);
    FlushInstructionCache(GetCurrentProcess(), at, n);
    g_page_used = (g_page_used + n + 15) & ~(size_t)15;
    return at;
}

/* call or jmp rel32 from `site` to `to`, padded with no-ops to `len` bytes */
static int branch_to(unsigned char *site, size_t len, unsigned char op, unsigned char *to)
{
    unsigned char code[16];
    int32_t r;
    size_t i;
    if (len < 5 || len > sizeof code || !rel32(site + 5, to, &r))
        return 0;
    code[0] = op;
    memcpy(code + 1, &r, 4);
    for (i = 5; i < len; i++)
        code[i] = 0x90;
    return write_code(site, code, len);
}

/* ------------------------------------------------ the random stew draw */

/* The server's stew pull, choosing the draw by the stew's type: a box stew's
 * draw, then the normal stew's, each given the stew's odds list name:
 *   mov rdx,[rsi+0x18] / jmp +7 / lea rdx,[L""] / lea r8,[rbp+0x50] /
 *   lea rcx,[rbp-0x28] / call box_draw / mov rbx,rax */
static const unsigned char STEW_BOX_PATTERN[] = {
    0x48, 0x8B, 0x56, 0x18, 0xEB, 0x07, 0x48, 0x8D, 0x15, 0x00, 0x00, 0x00, 0x00,
    0x4C, 0x8D, 0x45, 0x50, 0x48, 0x8D, 0x4D, 0xD8, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xD8,
};
static const char STEW_BOX_MASK[] = "xxxxxxxxx????xxxxxxxxx????xxx";
#define STEW_BOX_CALL 21
/*   mov rdx,[rsi+0x18] / jmp +7 / lea rdx,[L""] / lea rcx,[rbp-0x18] /
 *   call normal_draw / mov rbx,rax / lea rax,[rbp-0x38] / cmp rax,rbx / je */
static const unsigned char STEW_PATTERN[] = {
    0x48, 0x8B, 0x56, 0x18, 0xEB, 0x07, 0x48, 0x8D, 0x15, 0x00, 0x00, 0x00, 0x00,
    0x48, 0x8D, 0x4D, 0xE8, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x48, 0x8B, 0xD8, 0x48, 0x8D, 0x45, 0xC8, 0x48, 0x3B, 0xC3, 0x74,
};
static const char STEW_MASK[] = "xxxxxxxxx????xxxxx????xxxxxxxxxxx";
#define STEW_CALL 17    /* call normal_draw: 5 bytes */

/* Inside the box draw: it reads the stew's odds list with the game's own
 * masters.db query, and frees the rows at the end.
 *   mov rcx,rdx / call odds_query / mov r10,[rax] / xor r9d,r9d / xor r8d,r8d /
 *   lea rdx,[rbp-0x50] / mov rcx,rax / call [r10+0x10] */
static const unsigned char ODDS_READ[] = {
    0x48, 0x8B, 0xCA, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x8B, 0x10, 0x45, 0x33, 0xC9,
    0x45, 0x33, 0xC0, 0x48, 0x8D, 0x55, 0xB0, 0x48, 0x8B, 0xC8, 0x41, 0xFF, 0x52, 0x10,
};
static const char ODDS_READ_MASK[] = "xxxx????xxxxxxxxxxxxxxxxxxxx";
#define ODDS_READ_AT 0x3B
/*   lea rcx,[rbp-0x50] / call free_rows / mov rax,rsi */
static const unsigned char ODDS_FREE[] = {0x48, 0x8D, 0x4D, 0xB0, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xC6};
static const char ODDS_FREE_MASK[] = "xxxxx????xxx";
#define ODDS_FREE_AT 0x325
/* Inside the normal draw: copying the decal id it took from the queue.
 *   mov [rbx],rsi / mov [rbx+8],edx / mov [rbx+0xc],edx / add edx,edx /
 *   lea r8d,[rsi+8] / mov rcx,[rbx] / call realloc */
static const unsigned char ALLOC_CALL[] = {
    0x48, 0x89, 0x33, 0x89, 0x53, 0x08, 0x89, 0x53, 0x0C, 0x03, 0xD2,
    0x44, 0x8D, 0x46, 0x08, 0x48, 0x8B, 0x0B, 0xE8, 0x00, 0x00, 0x00, 0x00,
};
static const char ALLOC_CALL_MASK[] = "xxxxxxxxxxxxxxxxxxx????";
#define ALLOC_CALL_AT 0x4A

/* The Mushroom Stew shop's reply: the list of stews on offer is filled, then
 * the user data is copied in.
 *   lea rcx,[r13+0x608] / call fill_stews / lea rcx,[r13+0x15c] / mov dl,1 / call */
static const unsigned char SHOPLIST_PATTERN[] = {
    0x49, 0x8D, 0x8D, 0x08, 0x06, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x49, 0x8D, 0x8D, 0x5C, 0x01, 0x00, 0x00, 0xB2, 0x01, 0xE8,
};
static const char SHOPLIST_MASK[] = "xxxxxxxx????xxxxxxxxxx";
#define SHOPLIST_CALL 7
#define SHOP_STEW_SIZE 0x20     /* FBrgDbGacha: the stew id first */

/* The box stews' list, filled by the same function for the shop's reply and
 * for every pull's reply (the menu reads it after either):
 *   lea rcx,[r13+0x5f8] / xor edx,edx / call fill_boxes / mov [rsp+0x38],rbx
 *   lea rcx,[r13+0x5d8] / mov dl,1 / call fill_boxes / mov rax,[r14] /
 *   mov rdx,r13 / mov rcx,r14 / call [rax+0x30] */
static const unsigned char BOXES_SHOP_PATTERN[] = {
    0x49, 0x8D, 0x8D, 0xF8, 0x05, 0x00, 0x00, 0x33, 0xD2, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x48, 0x89, 0x5C, 0x24, 0x38,
};
static const char BOXES_SHOP_MASK[] = "xxxxxxxxxx????xxxxx";
static const unsigned char BOXES_PULL_PATTERN[] = {
    0x49, 0x8D, 0x8D, 0xD8, 0x05, 0x00, 0x00, 0xB2, 0x01, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x49, 0x8B, 0x06, 0x49, 0x8B, 0xD5, 0x49, 0x8B, 0xCE, 0xFF, 0x50, 0x30,
};
static const char BOXES_PULL_MASK[] = "xxxxxxxxxx????xxxxxxxxxxxx";
#define BOXES_CALL 9
#define BOX_SIZE 0x24           /* FBrgDbUserBoxGacha: the stew id, its decals, resets */
#define BOX_DECALS 0x10
#define BOX_DECAL_SIZE 0x14     /* FBrgDbUserBoxGachaContent: the decal id, how many left */
#define BOX_DECAL_REST 0x10

/* Which stews the shop offers (getCurrentSkillGachaId): it walks the stews in
 * masters.db, skips box stews, adds the first other one and stops - the offline
 * game offers one normal stew at most.
 *   mov rdx,rsi / call [wcscmp] / test eax,eax / jne add / inc edi / jmp loop /
 *   add: lea rdx,[rbx+8] / mov rcx,r14 / call add_id / mov eax,[rsp+0x30] /
 *   cmp dword [r14+8],0 */
static const unsigned char STEWIDS_PATTERN[] = {
    0x48, 0x8B, 0xD6, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x75, 0x04,
    0xFF, 0xC7, 0xEB, 0x00, 0x48, 0x8D, 0x53, 0x08, 0x49, 0x8B, 0xCE, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x8B, 0x44, 0x24, 0x30, 0x41, 0x83, 0x7E, 0x08, 0x00,
};
static const char STEWIDS_MASK[] = "xxxxx????xxxxxxx?xxxxxxxx????xxxxxxxxx";
#define STEWIDS_JMP  15     /* jmp loop (short) */
#define STEWIDS_CALL 24     /* call add_id: 5 bytes */

/* The stew pull's check of the request's list of the shop's stews (a function
 * that always answers "fine"), before anything is paid or drawn:
 *   cmp dword [rbp+0x444],0 / jle / lea rcx,[rbp+0x43c] / call check / test al,al /
 *   je / mov rax,[r14] / xor r9d,r9d / mov r8d,1003 */
static const unsigned char PULLIDS_PATTERN[] = {
    0x83, 0xBD, 0x44, 0x04, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x48, 0x8D, 0x8D, 0x3C, 0x04, 0x00, 0x00,
    0xE8, 0x00, 0x00, 0x00, 0x00, 0x84, 0xC0, 0x74, 0x00, 0x49, 0x8B, 0x06, 0x45, 0x33, 0xC9,
    0x41, 0xB8, 0xEB, 0x03, 0x00, 0x00,
};
static const char PULLIDS_MASK[] = "xxxxxxxx?xxxxxxxx????xxx?xxxxxxxxxxxx";
#define PULLIDS_CALL 16
/* The Stew Multi Pull mod's NetGacha adds these to that list: on every pull of
 * a multi pull the number of pulls left, and on its first one a request to do
 * them all at once. */
#define MULTI_HINT L"LID_MULTI_LEFT="
#define ALL_HINT L"LID_MULTI_ALL="
#define RUN_MAX 256

/* The stew pull's payment, then (on failure) its error reply:
 *   lea rcx,[rbp-0x20] / call text_of / mov rdx,rax / mov r8,rsi / lea rcx,[user] /
 *   call pay_one / test eax,eax / jns ok / mov rax,[r14] / mov rdx,r13 /
 *   mov rcx,r14 / call [rax+0x68] */
static const unsigned char PAY_PATTERN[] = {
    0x48, 0x8D, 0x4D, 0xE0, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xD0, 0x4C, 0x8B, 0xC6,
    0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x79, 0x11,
    0x49, 0x8B, 0x06, 0x49, 0x8B, 0xD5, 0x49, 0x8B, 0xCE, 0xFF, 0x50, 0x68,
};
static const char PAY_MASK[] = "xxxxx????xxxxxxxxx????x????xxxxxxxxxxxxxxxx";
#define PAY_CALL 22
/* pay_one: the game's purchase of one, passed on with a count of 1:
 *   sub rsp,0x38 / mov r9,r8 / mov r8d,1 / mov qword [rsp+0x20],0 / call purchase */
static const unsigned char PAY_ONE_BODY[] = {
    0x48, 0x83, 0xEC, 0x38, 0x4D, 0x8B, 0xC8, 0x41, 0xB8, 0x01, 0x00, 0x00, 0x00,
    0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0xE8,
};
#define PAY_ONE_CALL 22
/* Then the draw: the reply's decal list emptied, and `count` decals drawn into it.
 *   lea rbx,[r13+0x5e8] / xor edx,edx / mov rcx,rbx / call empty / mov r8d,1 /
 *   lea rdx,[rbp-0x80] / mov rcx,rbx / call draw_all / mov r8b,1 */
static const unsigned char DRAWN_PATTERN[] = {
    0x49, 0x8D, 0x9D, 0xE8, 0x05, 0x00, 0x00, 0x33, 0xD2, 0x48, 0x8B, 0xCB, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x41, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x55, 0x80, 0x48, 0x8B, 0xCB, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x41, 0xB0, 0x01,
};
static const char DRAWN_MASK[] = "xxxxxxxxxxxxx????xxxxxxxxxxxxxx????xxx";
#define DRAWN_CALL 30
#define DRAW_ALL_REACH 0x400    /* the normal draw call lies this close after draw_all's start */

/* The server's reader of master_const_int, found where it reads the Rewards
 * Box's limit:
 *   lea rdx,[rsp+0x30] / lea rcx,[L"PRESENT_BOX_LIMIT"] / xor ebx,ebx /
 *   call const_int / test al,al / jnz */
static const unsigned char CONSTINT_PATTERN[] = {
    0x48, 0x8D, 0x54, 0x24, 0x30, 0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x33, 0xDB,
    0xE8, 0x00, 0x00, 0x00, 0x00, 0x84, 0xC0, 0x75, 0x2B,
};
static const char CONSTINT_MASK[] = "xxxxxxxx????xxx????xxxx";
#define CONSTINT_NAME 8
#define CONSTINT_CALL 14
/* const_int(name, &value): push rbx / sub rsp,0x60 / mov qword [rsp+0x20],-2 /
 * mov rbx,rdx / mov r8,rcx */
static const unsigned char CONSTINT_BODY[] = {
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x60, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF,
    0x48, 0x8B, 0xDA, 0x4C, 0x8B, 0xC1,
};

/* The player's user record ("user" in the save), where the save is read:
 *   test rax,rax / je / mov rdx,rax / lea rcx,[user record] / call read_user
 * read_user: sub rsp,0x28 / xor r8d,r8d / call / mov al,1 / add rsp,0x28 / ret */
static const unsigned char USERLOAD_PATTERN[] = {
    0x48, 0x85, 0xC0, 0x74, 0x14, 0x48, 0x8B, 0xD0, 0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xE8,
};
static const char USERLOAD_MASK[] = "xxxxxxxxxxx????x";
#define USERLOAD_REC 11
#define USERLOAD_CALL 15
static const unsigned char READ_USER_BODY[] = {
    0x48, 0x83, 0xEC, 0x28, 0x45, 0x33, 0xC0, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xB0, 0x01,
    0x48, 0x83, 0xC4, 0x28, 0xC3,
};
static const char READ_USER_MASK[] = "xxxxxxxx????xxxxxxx";
/* The record is the payment's user plus 8; its "olid", an online-era text the
 * offline game only ever loads and saves, is an FString at +0x34. */
#define USER_RECORD_IN_PAY 8
#define USER_OLID 0x34

/* A row of the odds list: the decal id (an FString) and its odds. */
#define ODDS_ROW_SIZE  0x3C
#define ODDS_ROW_SKLID 0x18
#define ODDS_ROW_ODDS  0x28
#define ODDS_ROWS_GET  (0x10 / 8)
/* Stews whose odds list is named like this are drawn fresh every pull; one
 * whose name also ends like REPEATS_SUFFIX may give a decal twice in a run. */
#define RANDOM_STEW_PREFIX L"SKLGACH_RANDOM_"
#define REPEATS_SUFFIX L"_REPEATS"


struct pieces {
    unsigned char *stew;                        /* the stew pull's normal draw call */
    void *normal_draw, *odds_query, *odds_free, *game_realloc;
    int stew_ok;
    unsigned char *shop_list;                   /* the shop reply's stew list call */
    void *fill_stews;
    unsigned char *stew_ids, *stew_ids_loop;    /* which stews are offered */
    void *stew_ids_add;
    unsigned char *pull_ids;                    /* the pull's check of its stew list */
    void *pull_ids_check;
    unsigned char *pay, *drawn;                 /* the pull's payment and draw calls */
    void *pay_one, *purchase, *draw_all;
    void *pay_user;                             /* the payment's user */
    void *const_int;                            /* the server's master_const_int reader */
    unsigned char *user_record;                 /* "user" in the save */
    unsigned char *boxes_shop, *boxes_pull;     /* the replies' box stew list calls */
    void *fill_boxes;
};

static int inside(struct span s, const unsigned char *p, size_t n)
{
    return p >= s.start && p + n <= s.start + s.size;
}

static struct span g_image;     /* the whole exe, for reading what code points at */

/* What the pity needs: the server's master_const_int reader, and the user
 * record the save is read into - which must be the payment's user plus 8. */
static void find_pity(struct span text, struct pieces *pc)
{
    unsigned char *site = NULL, *name, *fn, *rec, *reader;
    size_t i, n = sizeof USERLOAD_PATTERN;
    int hits = 0;

    if (find_all(text, CONSTINT_PATTERN, CONSTINT_MASK, &site) == 1) {
        name = rel_target(site + CONSTINT_NAME, site + CONSTINT_NAME + 4);
        fn = rel_target(site + CONSTINT_CALL + 1, site + CONSTINT_CALL + 5);
        if (inside(g_image, name, 36) && is_wstr(name, L"PRESENT_BOX_LIMIT") &&
            inside(text, fn, sizeof CONSTINT_BODY) && memcmp(fn, CONSTINT_BODY, sizeof CONSTINT_BODY) == 0)
            pc->const_int = fn;
    }
    if (!pc->pay_user)
        return;
    for (i = 0; i + n + 4 <= text.size; i++) {
        site = text.start + i;
        if (site[0] != USERLOAD_PATTERN[0] || !matches(site, USERLOAD_PATTERN, USERLOAD_MASK))
            continue;
        rec = rel_target(site + USERLOAD_REC, site + USERLOAD_REC + 4);
        reader = rel_target(site + USERLOAD_CALL + 1, site + USERLOAD_CALL + 5);
        if (rec == (unsigned char *)pc->pay_user + USER_RECORD_IN_PAY &&
            inside(text, reader, sizeof READ_USER_BODY) && matches(reader, READ_USER_BODY, READ_USER_MASK)) {
            pc->user_record = rec;
            hits++;
        }
    }
    if (hits != 1)
        pc->user_record = NULL;
}

/* The stew pull's draw call and the game functions the random draw uses: all
 * found from the two draws it chooses between, each checked byte for byte. */
static void find_stew(struct span text, struct pieces *pc)
{
    unsigned char *site = NULL, *boxsite = NULL, *box, *normal;

    if (find_all(text, SHOPLIST_PATTERN, SHOPLIST_MASK, &site) == 1) {
        pc->shop_list = site + SHOPLIST_CALL;
        pc->fill_stews = rel_target(site + SHOPLIST_CALL + 1, site + SHOPLIST_CALL + 5);
    }
    if (find_all(text, STEWIDS_PATTERN, STEWIDS_MASK, &site) == 1) {
        pc->stew_ids = site + STEWIDS_CALL;
        pc->stew_ids_add = rel_target(site + STEWIDS_CALL + 1, site + STEWIDS_CALL + 5);
        pc->stew_ids_loop = site + STEWIDS_JMP + 2 + (signed char)site[STEWIDS_JMP + 1];
    }
    if (find_all(text, BOXES_SHOP_PATTERN, BOXES_SHOP_MASK, &site) == 1 &&
        find_all(text, BOXES_PULL_PATTERN, BOXES_PULL_MASK, &boxsite) == 1) {
        unsigned char *fill = rel_target(site + BOXES_CALL + 1, site + BOXES_CALL + 5);
        if (fill == rel_target(boxsite + BOXES_CALL + 1, boxsite + BOXES_CALL + 5) && inside(text, fill, 1)) {
            pc->boxes_shop = site + BOXES_CALL;
            pc->boxes_pull = boxsite + BOXES_CALL;
            pc->fill_boxes = fill;
        }
    }
    if (find_all(text, PULLIDS_PATTERN, PULLIDS_MASK, &site) == 1) {
        pc->pull_ids = site + PULLIDS_CALL;
        pc->pull_ids_check = rel_target(site + PULLIDS_CALL + 1, site + PULLIDS_CALL + 5);
    }
    if (find_all(text, PAY_PATTERN, PAY_MASK, &site) == 1) {
        unsigned char *one = rel_target(site + PAY_CALL + 1, site + PAY_CALL + 5);
        if (inside(text, one, sizeof PAY_ONE_BODY + 4) && memcmp(one, PAY_ONE_BODY, sizeof PAY_ONE_BODY) == 0) {
            pc->pay = site + PAY_CALL;
            pc->pay_one = one;
            pc->purchase = rel_target(one + PAY_ONE_CALL + 1, one + PAY_ONE_CALL + 5);
            pc->pay_user = rel_target(site + 18, site + 22);
        }
    }
    find_pity(text, pc);
    if (find_all(text, DRAWN_PATTERN, DRAWN_MASK, &site) == 1) {
        pc->drawn = site + DRAWN_CALL;
        pc->draw_all = rel_target(site + DRAWN_CALL + 1, site + DRAWN_CALL + 5);
    }
    if (find_all(text, STEW_PATTERN, STEW_MASK, &site) != 1 ||
        find_all(text, STEW_BOX_PATTERN, STEW_BOX_MASK, &boxsite) != 1)
        return;
    /* the two draws of the same pull, the box stew's first */
    if (boxsite > site || site - boxsite > 0x200)
        return;
    normal = rel_target(site + STEW_CALL + 1, site + STEW_CALL + 5);
    box = rel_target(boxsite + STEW_BOX_CALL + 1, boxsite + STEW_BOX_CALL + 5);
    if (!inside(text, normal, ALLOC_CALL_AT + sizeof ALLOC_CALL) ||
        !inside(text, box, ODDS_FREE_AT + sizeof ODDS_FREE) ||
        !matches(normal + ALLOC_CALL_AT, ALLOC_CALL, ALLOC_CALL_MASK) ||
        !matches(box + ODDS_READ_AT, ODDS_READ, ODDS_READ_MASK) ||
        !matches(box + ODDS_FREE_AT, ODDS_FREE, ODDS_FREE_MASK))
        return;
    pc->stew = site + STEW_CALL;
    pc->normal_draw = normal;
    pc->game_realloc = rel_target(normal + ALLOC_CALL_AT + 19, normal + ALLOC_CALL_AT + 23);
    pc->odds_query = rel_target(box + ODDS_READ_AT + 4, box + ODDS_READ_AT + 8);
    pc->odds_free = rel_target(box + ODDS_FREE_AT + 5, box + ODDS_FREE_AT + 9);
    pc->stew_ok = 1;
    /* draw_all is the function the normal draw call above is in */
    if (pc->draw_all && !(pc->stew > (unsigned char *)pc->draw_all &&
                          pc->stew - (unsigned char *)pc->draw_all < DRAW_ALL_REACH))
        pc->drawn = NULL, pc->draw_all = NULL;
}

/* Find everything, without changing anything. `base` is the game's exe in
 * memory (the tests pass a mapped copy of it instead). */
static void find_pieces(unsigned char *base, struct pieces *pc)
{
    struct span text;

    memset(pc, 0, sizeof *pc);
    if (!section(base, ".text", &text))
        return;
    g_image.start = base;
    g_image.size = ((IMAGE_NT_HEADERS64 *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew))->OptionalHeader.SizeOfImage;
    find_stew(text, pc);
}

static struct pieces g_pc;

/* ------------------------------------------------ the random stew: code */

struct fstring { wchar_t *data; int num, max; };
struct rows { unsigned char *data; int num, max; };

typedef void *(*draw_fn)(struct fstring *out, const wchar_t *odds_id);
typedef void *(*odds_query_fn)(const wchar_t *odds_id);
typedef void (*odds_get_fn)(void *query, struct rows *out, void *unused1, void *unused2);
typedef void (*odds_free_fn)(struct rows *rows);
typedef void *(*realloc_fn)(void *p, DWORD bytes, DWORD align);

static uint64_t g_rng;

/* xorshift64*, seeded from the clock the first time */
static uint64_t next_random(void)
{
    if (!g_rng) {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        g_rng = (uint64_t)t.QuadPart ^ ((uint64_t)GetCurrentProcessId() << 32) ^ 0x9E3779B97F4A7C15ull;
    }
    g_rng ^= g_rng >> 12;
    g_rng ^= g_rng << 25;
    g_rng ^= g_rng >> 27;
    return g_rng * 0x2545F4914F6CDD1Dull;
}

static int is_random_stew(const wchar_t *odds_id)
{
    size_t n = wcslen(RANDOM_STEW_PREFIX);
    return odds_id && wcsncmp(odds_id, RANDOM_STEW_PREFIX, n) == 0;
}

/* A random stew whose odds list ends in _REPEATS may give a decal twice in one
 * multi pull, as single pulls can. */
static int repeats_allowed(const wchar_t *odds_id)
{
    size_t n = wcslen(odds_id), m = wcslen(REPEATS_SUFFIX);
    return n >= m && wcscmp(odds_id + n - m, REPEATS_SUFFIX) == 0;
}

/* The multi pull in progress: how many pulls were left at the last one (0:
 * none), and the decals it has given so far. g_all: this request asks for its
 * whole run at once (0: no); g_draw_n: that many were paid for, to draw. */
static int g_run_left, g_run_n, g_all, g_draw_n;
static wchar_t g_run_ids[RUN_MAX][64];

static int given_this_run(const wchar_t *id)
{
    int i;
    for (i = 0; i < g_run_n; i++)
        if (wcscmp(g_run_ids[i], id) == 0)
            return 1;
    return 0;
}

/* The odds of the row, if it can be drawn (odds above 0; with `fresh`, not
 * given already in this multi pull), and its decal id. */
static int drawable(const unsigned char *row, struct fstring *id, int fresh)
{
    int odds;
    memcpy(&odds, row + ODDS_ROW_ODDS, sizeof odds);
    memcpy(id, row + ODDS_ROW_SKLID, sizeof *id);
    if (odds <= 0 || !id->data || id->num <= 1)
        return 0;
    return fresh && given_this_run(id->data) ? 0 : odds;
}

/* ------------------------------------------------------------ the pity */

/* After a number of pulls of a stew without the decal the player picked for
 * it (in the stew's Check Lineup), the next pull gives that decal. Each stew
 * counts on its own: the regular stew (whichever of its forms the Stew Multi
 * Pull mod set up), the Death Metals mod's Death Metal stew and the Events and
 * Seasons festival stew. How many pulls is master_const_int LID_PITY_*, set by
 * the mod; none, or 0, and that stew has no pity.
 *
 * The picks and the counts are kept in the player's save, in the user
 * record's "olid" - a text the online game used and the offline one only
 * ever loads and saves - so each save has its own, and the save is written
 * after every pull anyway. A save whose olid holds anything else is left
 * alone, and has no pity. */
#define PITY_HINT L"LID_PITY="          /* <stew id>|<decal id>: the pick */
#define PITY_MARK L"LIDPITY"
#define PITY_GROUPS 3
#define PITY_MAX 1000
#define DECAL_ID_MAX 64

static const struct { const wchar_t *stew; int group; } PITY_STEWS[] = {
    {L"SKLGACH_NORMAL_OFFLINE", 0}, {L"LID_STEW_NORMAL", 0},
    {L"SKLGACH_RANDOM_NORMAL", 0}, {L"SKLGACH_RANDOM_NORMAL_REPEATS", 0},
    {L"SKLGACH_RANDOM_DM", 1}, {L"SKLGACH_RANDOM_FEST", 2},
};
static const wchar_t *const PITY_KEY[PITY_GROUPS] = {L"N", L"DM", L"FEST"};
static const wchar_t *const PITY_LIMIT[PITY_GROUPS] = {L"LID_PITY_NORMAL", L"LID_PITY_DM", L"LID_PITY_FEST"};

struct pity { wchar_t decal[DECAL_ID_MAX]; int count; };

static int pity_group(const wchar_t *stew)
{
    size_t i;
    for (i = 0; stew && i < sizeof PITY_STEWS / sizeof PITY_STEWS[0]; i++)
        if (wcscmp(stew, PITY_STEWS[i].stew) == 0)
            return PITY_STEWS[i].group;
    return -1;
}

typedef char (*const_int_fn)(const wchar_t *name, int *value);

/* How many pulls before the picked decal comes, for a stew group: 0 = none. */
static int pity_limit(int group)
{
    int value = 0;
    if (!g_pc.const_int || !((const_int_fn)g_pc.const_int)(PITY_LIMIT[group], &value) || value <= 0)
        return 0;
    return value > PITY_MAX ? PITY_MAX : value;
}

static struct fstring *pity_text(void)
{
    return g_pc.user_record ? (struct fstring *)(g_pc.user_record + USER_OLID) : NULL;
}

/* The save's picks and counts. 0 when the save's olid is not ours to use. */
static int pity_load(struct pity st[PITY_GROUPS])
{
    struct fstring s;
    const wchar_t *p;
    int g;

    memset(st, 0, sizeof(struct pity) * PITY_GROUPS);
    if (!pity_text())
        return 0;
    memcpy(&s, pity_text(), sizeof s);
    if (!s.data || s.num <= 1 || !s.data[0])
        return 1;
    if (s.num > 2048 || wcsncmp(s.data, PITY_MARK, wcslen(PITY_MARK)) != 0)
        return 0;
    /* LIDPITY;N=SKL_X,37;DM=SKL_Y,5 */
    for (p = wcschr(s.data, L';'); p; p = wcschr(p + 1, L';')) {
        const wchar_t *eq = wcschr(p, L'='), *comma, *end;
        if (!eq)
            break;
        comma = wcschr(eq, L',');
        end = wcschr(p + 1, L';');
        if (!comma || (end && comma > end))
            continue;
        for (g = 0; g < PITY_GROUPS; g++) {
            size_t k = wcslen(PITY_KEY[g]);
            if ((size_t)(eq - p - 1) == k && wcsncmp(p + 1, PITY_KEY[g], k) == 0 &&
                comma - eq - 1 > 0 && comma - eq - 1 < DECAL_ID_MAX) {
                wcsncpy(st[g].decal, eq + 1, (size_t)(comma - eq - 1));
                st[g].decal[comma - eq - 1] = 0;
                st[g].count = _wtoi(comma + 1);
                if (st[g].count < 0 || st[g].count > PITY_MAX)
                    st[g].count = 0;
            }
        }
    }
    return 1;
}

static void pity_store(const struct pity st[PITY_GROUPS])
{
    wchar_t text[512];
    struct fstring s, *field = pity_text();
    int g, n = 0, used = 0;
    DWORD bytes;

    if (!field)
        return;
    n = swprintf(text, 512, L"%ls", PITY_MARK);
    for (g = 0; g < PITY_GROUPS; g++) {
        if (!st[g].decal[0])
            continue;
        n += swprintf(text + n, 512 - n, L";%ls=%ls,%d", PITY_KEY[g], st[g].decal, st[g].count);
        used++;
    }
    if (!used)
        text[0] = 0, n = 0;
    memcpy(&s, field, sizeof s);
    bytes = (DWORD)(n + 1) * sizeof(wchar_t);
    if (!n) {
        if (s.data)
            s.data[0] = 0;
        s.num = s.data ? 1 : 0;
    } else {
        if (!s.data || s.max < n + 1) {
            wchar_t *grown = ((realloc_fn)g_pc.game_realloc)(s.data, bytes, 8);
            if (!grown) {
                log_line("pity: out of memory - the counts were not kept");
                return;
            }
            s.data = grown;
            s.max = n + 1;
        }
        memcpy(s.data, text, bytes);
        s.num = n + 1;
    }
    memcpy(field, &s, sizeof s);
}

/* The decal a pull gives, once drawn: counted towards its stew's pity, and
 * the picked decal given instead when the pulls are up. The count starts
 * again whenever the picked decal comes, either way. */
static void pity_after_draw(const wchar_t *stew, struct fstring *out)
{
    struct pity st[PITY_GROUPS];
    int group = pity_group(stew), limit;
    size_t n;

    if (group < 0 || !out || !(limit = pity_limit(group)) || !pity_load(st) || !st[group].decal[0])
        return;
    if (out->data && wcscmp(out->data, st[group].decal) == 0) {
        st[group].count = 0;
    } else if (++st[group].count >= limit) {
        n = wcslen(st[group].decal) + 1;
        if (!out->data || out->max < (int)n) {
            wchar_t *grown = ((realloc_fn)g_pc.game_realloc)(out->data, (DWORD)(n * sizeof(wchar_t)), 8);
            if (!grown) {
                log_line("pity: out of memory - %ls not given", st[group].decal);
                return;
            }
            out->data = grown;
            out->max = (int)n;
        }
        log_line("pity: %d pulls of %ls without %ls - given it instead of %ls", limit, stew,
                 st[group].decal, out->num > 1 ? out->data : L"nothing");
        memcpy(out->data, st[group].decal, n * sizeof(wchar_t));
        out->num = (int)n;
        st[group].count = 0;
    }
    pity_store(st);
}

/* A pick from the stew's Check Lineup, sent along with the next pull. The
 * count carries on: whichever decal is picked, it comes after the same number
 * of pulls, so changing your mind late costs nothing. */
static void pity_pick(const wchar_t *hint)
{
    struct pity st[PITY_GROUPS];
    wchar_t stew[64];
    const wchar_t *bar = wcschr(hint, L'|');
    int group;
    size_t n;

    if (!bar || bar - hint <= 0 || bar - hint >= 64)
        return;
    wcsncpy(stew, hint, (size_t)(bar - hint));
    stew[bar - hint] = 0;
    group = pity_group(stew);
    n = wcslen(bar + 1);
    if (group < 0 || n == 0 || n >= DECAL_ID_MAX || wcschr(bar + 1, L';') || wcschr(bar + 1, L','))
        return;
    if (!pity_load(st)) {
        log_line("pity: this save's olid already holds something else - no pity for it");
        return;
    }
    if (wcscmp(st[group].decal, bar + 1) != 0) {
        wcscpy(st[group].decal, bar + 1);
        log_line("pity: %ls's picked decal is now %ls (%d pulls counted)", stew, st[group].decal,
                 st[group].count);
    }
    pity_store(st);
}

/* A text the game owns, in the game's memory: 0 when out of memory. */
static int game_text(struct fstring *s, const wchar_t *text)
{
    int n = (int)wcslen(text) + 1;

    memset(s, 0, sizeof *s);
    if (n == 1)
        return 1;
    s->data = ((realloc_fn)g_pc.game_realloc)(NULL, (DWORD)(n * sizeof(wchar_t)), 8);
    if (!s->data)
        return 0;
    memcpy(s->data, text, n * sizeof(wchar_t));
    s->num = s->max = n;
    return 1;
}

/* How far each stew's pity is, for the Stew Multi Pull mod's menu to show: an
 * entry on the end of the reply's box stew list for every stew that has pity,
 * named PITY_ENTRY + the stew's id, holding two decals - the picked one (none
 * yet: an empty id) with the pulls counted so far, then an empty one with the
 * pulls it takes. No box stew is named like that, so the menu's box stew code
 * never finds them, and the save's own box stews are not touched. */
#define PITY_ENTRY L"LID_PITY_"

static void pity_show(struct rows *boxes)
{
    struct pity st[PITY_GROUPS];
    int limit[PITY_GROUPS], g, want = 0;
    size_t i;
    unsigned char *grown;

    if (!g_pc.game_realloc || !pity_load(st))
        return;
    for (g = 0; g < PITY_GROUPS; g++)
        limit[g] = pity_limit(g);
    for (i = 0; i < sizeof PITY_STEWS / sizeof PITY_STEWS[0]; i++)
        want += limit[PITY_STEWS[i].group] > 0;
    if (!want)
        return;
    grown = ((realloc_fn)g_pc.game_realloc)(boxes->data, (DWORD)((boxes->num + want) * BOX_SIZE), 8);
    if (!grown)
        return;
    boxes->data = grown;
    boxes->max = boxes->num + want;
    for (i = 0; i < sizeof PITY_STEWS / sizeof PITY_STEWS[0]; i++) {
        unsigned char *entry = boxes->data + (size_t)boxes->num * BOX_SIZE, *decals;
        struct rows *list = (struct rows *)(entry + BOX_DECALS);
        wchar_t id[96];

        g = PITY_STEWS[i].group;
        if (limit[g] <= 0)
            continue;
        memset(entry, 0, BOX_SIZE);
        swprintf(id, 96, L"%ls%ls", PITY_ENTRY, PITY_STEWS[i].stew);
        decals = ((realloc_fn)g_pc.game_realloc)(NULL, 2 * BOX_DECAL_SIZE, 8);
        if (!decals || !game_text((struct fstring *)entry, id))
            return;
        memset(decals, 0, 2 * BOX_DECAL_SIZE);
        if (!game_text((struct fstring *)decals, st[g].decal))
            return;
        *(int *)(decals + BOX_DECAL_REST) = st[g].count;
        *(int *)(decals + BOX_DECAL_SIZE + BOX_DECAL_REST) = limit[g];
        list->data = decals;
        list->num = list->max = 2;
        boxes->num++;
    }
}

typedef uint64_t (*fill_boxes_fn)(struct rows *boxes, char flag);

/* In place of the call that fills a reply's box stew list: filled as before,
 * then the pity entries go on the end. */
uint64_t lid_sp_boxes(struct rows *boxes, char flag)
{
    uint64_t ok = ((fill_boxes_fn)g_pc.fill_boxes)(boxes, flag);
    pity_show(boxes);
    return ok;
}

typedef char (*ids_check_fn)(struct rows *ids);

/* From the stew pull's check of its list of stews, before anything is paid
 * or drawn: a multi pull says how many pulls are left. A single pull, or a
 * count that is not lower than the last one (a new multi pull), starts afresh. */
char lid_sp_pull_ids(struct rows *ids)
{
    size_t n = wcslen(MULTI_HINT), m = wcslen(ALL_HINT), p = wcslen(PITY_HINT);
    struct fstring s;
    int i, left = 0, all = 0;

    for (i = 0; i < ids->num; i++) {
        memcpy(&s, ids->data + (size_t)i * sizeof s, sizeof s);
        if (s.data && s.num > (int)n && wcsncmp(s.data, MULTI_HINT, n) == 0)
            left = _wtoi(s.data + n);
        else if (s.data && s.num > (int)m && wcsncmp(s.data, ALL_HINT, m) == 0)
            all = _wtoi(s.data + m);
        else if (s.data && s.num > (int)p && wcsncmp(s.data, PITY_HINT, p) == 0)
            pity_pick(s.data + p);
    }
    if (left <= 0 || g_run_left <= 0 || left >= g_run_left)
        g_run_n = 0;
    g_run_left = left > 0 ? left : 0;
    g_all = all > 1 && all <= RUN_MAX && g_pc.pay && g_pc.drawn ? all : 0;
    g_draw_n = 0;
    return ((ids_check_fn)g_pc.pull_ids_check)(ids);
}

typedef int (*pay_one_fn)(void *user, const wchar_t *product, void *extra);
typedef int (*purchase_fn)(void *user, const wchar_t *product, int count, void *extra, void *none);
typedef void *(*draw_all_fn)(void *decals, void *stew, int count);

/* In place of the stew pull's payment for one: a request for the whole run
 * pays for every pull at once, with the game's own purchase (the price times
 * the count, or nothing if that is more than the player has). */
int lid_sp_pay(void *user, const wchar_t *product, void *extra)
{
    int paid;
    if (g_all <= 1)
        return ((pay_one_fn)g_pc.pay_one)(user, product, extra);
    paid = ((purchase_fn)g_pc.purchase)(user, product, g_all, extra, NULL);
    if (paid >= 0) {
        g_draw_n = g_all;
        log_line("stew: one request for a run of %d pulls: paid %d x %ls", g_all, g_all,
                 product ? product : L"?");
    } else {
        log_line("stew: a run of %d pulls could not be paid for (%ls) - nothing drawn", g_all,
                 product ? product : L"?");
    }
    return paid;
}

/* In place of the stew pull's draw of one decal: as many as were paid for.
 * The game's draw loops, so each is drawn as a pull of its own would be (a
 * random stew's through lid_sp_draw, without repeats), into the reply. */
void *lid_sp_draw_all(void *decals, void *stew, int count)
{
    if (g_draw_n > 1)
        count = g_draw_n;
    g_draw_n = 0;
    return ((draw_all_fn)g_pc.draw_all)(decals, stew, count);
}

/* In place of the normal stew draw (which takes the next decal from the one
 * queue every normal stew shares, kept in the save): a stew whose odds list is
 * named SKLGACH_RANDOM_* gets a fresh pick from its own list, weighted by the
 * odds, every pull - in a multi pull, not one it gave already, until every
 * decal on the list has come once (a list named ..._REPEATS: any of them, every
 * pull). Every other stew goes to the game's draw unchanged. */
void *lid_sp_draw(struct fstring *out, const wchar_t *odds_id)
{
    struct rows rows = {0};
    struct fstring id;
    void *query;
    long long total = 0, roll;
    int i, odds, fresh;

    if (!is_random_stew(odds_id)) {
        void *drawn = ((draw_fn)g_pc.normal_draw)(out, odds_id);
        pity_after_draw(odds_id, out);
        return drawn;
    }
    fresh = g_run_left > 0 && !repeats_allowed(odds_id);

    out->data = NULL;
    out->num = out->max = 0;
    query = ((odds_query_fn)g_pc.odds_query)(odds_id);
    if (!query) {
        log_line("stew: %ls - the odds list could not be read", odds_id);
        return out;
    }
    ((odds_get_fn)(*(void ***)query)[ODDS_ROWS_GET])(query, &rows, NULL, NULL);
    for (i = 0; i < rows.num; i++)
        total += drawable(rows.data + (size_t)i * ODDS_ROW_SIZE, &id, fresh);
    if (total <= 0 && fresh) {
        /* every decal on the list has come in this multi pull: all again */
        fresh = 0;
        g_run_n = 0;
        for (i = 0; i < rows.num; i++)
            total += drawable(rows.data + (size_t)i * ODDS_ROW_SIZE, &id, fresh);
    }
    if (total <= 0) {
        log_line("stew: %ls has no decal with odds above 0 - nothing drawn", odds_id);
        ((odds_free_fn)g_pc.odds_free)(&rows);
        return out;
    }
    roll = (long long)(next_random() % (uint64_t)total);
    for (i = 0; i < rows.num; i++) {
        odds = drawable(rows.data + (size_t)i * ODDS_ROW_SIZE, &id, fresh);
        if (roll < odds)
            break;
        roll -= odds;
    }
    out->data = ((realloc_fn)g_pc.game_realloc)(NULL, (DWORD)id.num * sizeof(wchar_t), 8);
    if (out->data) {
        memcpy(out->data, id.data, (size_t)id.num * sizeof(wchar_t));
        out->num = out->max = id.num;
        pity_after_draw(odds_id, out);
        if (g_run_left > 0 && g_run_n < RUN_MAX && wcslen(out->data) < 64)
            wcscpy(g_run_ids[g_run_n++], out->data);
        if (g_run_left > 0)
            log_line("stew: %ls -> %ls (odds %d in %lld; multi pull, %d left)", odds_id, out->data, odds,
                     total, g_run_left);
        else
            log_line("stew: %ls -> %ls (odds %d in %lld)", odds_id, out->data, odds, total);
    } else {
        log_line("stew: %ls - out of memory", odds_id);
    }
    ((odds_free_fn)g_pc.odds_free)(&rows);
    return out;
}

typedef char (*fill_stews_fn)(struct rows *stews);

/* The shop's list of stews on offer, filled by the game, then logged: which
 * stews the menu can show is decided here. */
char lid_sp_shop_list(struct rows *stews)
{
    char ok = ((fill_stews_fn)g_pc.fill_stews)(stews), names[400];
    int i, n = 0;
    struct fstring id;

    names[0] = 0;
    for (i = 0; i < stews->num && n < (int)sizeof names - 80; i++) {
        memcpy(&id, stews->data + (size_t)i * SHOP_STEW_SIZE, sizeof id);
        n += snprintf(names + n, sizeof names - n, "%s%ls", i ? ", " : "",
                      id.data && id.num > 0 ? id.data : L"?");
    }
    log_line("stew shop: %d stew(s) on offer: %s", stews->num, names);
    return ok;
}

/* ------------------------------------------------------ the changes */

static void put64(unsigned char *at, const void *v) { memcpy(at, &v, 8); }

static void patch_stew(unsigned char *base)
{
    unsigned char s[16], *stub;

    /* a call to mov rax,<function> / jmp rax */
    memcpy(s, "\x48\xB8", 2);
    memcpy(s + 10, "\xFF\xE0", 2);

    /* called in place of the normal draw */
    if (!g_pc.stew_ok) {
        log_line("stew: the stew pull's draw not found exactly once - random stews stay off. "
                 "A game update may have moved it.");
    } else {
        put64(s + 2, (void *)lid_sp_draw);
        stub = emit(s, 12);
        if (stub && branch_to(g_pc.stew, 5, 0xE8, stub))
            log_line("stew: stews with an odds list named " "SKLGACH_RANDOM_*" " draw a fresh decal every "
                     "pull (game+0x%llX)", (unsigned long long)(g_pc.stew - base));
        else
            log_line("stew: could not change the stew pull - random stews stay off");
    }

    /* every stew offered, not only the first: the id is added as before, then
     * the walk goes on instead of stopping.
     *   pop rax (the return place: not going back) / mov rax,add_id / call rax /
     *   inc edi / mov rax,loop / jmp rax */
    if (g_pc.stew_ids) {
        unsigned char w[32];
        memcpy(w, "\x58\x48\xB8", 3);
        put64(w + 3, g_pc.stew_ids_add);
        memcpy(w + 11, "\xFF\xD0\xFF\xC7\x48\xB8", 6);
        put64(w + 17, g_pc.stew_ids_loop);
        memcpy(w + 25, "\xFF\xE0", 2);
        stub = emit(w, 27);
        if (stub && branch_to(g_pc.stew_ids, 5, 0xE8, stub))
            log_line("stew shop: every stew in masters.db is offered, not only the first (game+0x%llX)",
                     (unsigned long long)(g_pc.stew_ids - base));
        else
            log_line("stew shop: could not change the stew list - only the first stew is offered");
    } else {
        log_line("stew shop: the stew list not found exactly once - only the first stew is offered. "
                 "A game update may have moved it.");
    }

    /* a multi pull's count, read from the pull's list of stews */
    if (g_pc.pull_ids && g_pc.stew_ok) {
        put64(s + 2, (void *)lid_sp_pull_ids);
        stub = emit(s, 12);
        if (stub && branch_to(g_pc.pull_ids, 5, 0xE8, stub))
            log_line("stew: a multi pull of a random stew gives no decal twice (game+0x%llX)",
                     (unsigned long long)(g_pc.pull_ids - base));
        else
            log_line("stew: could not change the stew pull's check - multi pulls may repeat decals");
    } else {
        log_line("stew: the stew pull's check not found exactly once - multi pulls may repeat decals. "
                 "A game update may have moved it.");
    }

    /* a whole multi pull in one request: paid for and drawn at once. Draw
     * first: until the payment is changed, nothing asks for more than one. */
    if (g_pc.pull_ids && g_pc.stew_ok && g_pc.pay && g_pc.drawn) {
        unsigned char *stub_draw, *stub_pay;
        put64(s + 2, (void *)lid_sp_draw_all);
        stub_draw = emit(s, 12);
        put64(s + 2, (void *)lid_sp_pay);
        stub_pay = emit(s, 12);
        if (stub_draw && stub_pay && branch_to(g_pc.drawn, 5, 0xE8, stub_draw) &&
            branch_to(g_pc.pay, 5, 0xE8, stub_pay))
            log_line("stew: a multi pull is paid for and drawn in one request (game+0x%llX, game+0x%llX)",
                     (unsigned long long)(g_pc.pay - base), (unsigned long long)(g_pc.drawn - base));
        else
            log_line("stew: could not change the stew pull's payment - multi pulls go one pull at a time");
    } else {
        log_line("stew: the stew pull's payment or draw not found exactly once - multi pulls go one "
                 "pull at a time. A game update may have moved them.");
    }

    if (g_pc.stew_ok && g_pc.pull_ids && g_pc.const_int && g_pc.user_record)
        log_line("pity: on - after LID_PITY_NORMAL / _DM / _FEST pulls without the decal picked in a "
                 "stew's Check Lineup, the next pull gives it (game+0x%llX, user +0x%llX)",
                 (unsigned long long)((unsigned char *)g_pc.const_int - base),
                 (unsigned long long)(g_pc.user_record - base));
    else
        log_line("pity: the settings reader or the save's user record not found exactly once - no pity. "
                 "A game update may have moved them.");

    /* how far each stew's pity is, in the replies the stew menu reads */
    if (g_pc.stew_ok && g_pc.const_int && g_pc.user_record && g_pc.fill_boxes) {
        put64(s + 2, (void *)lid_sp_boxes);
        stub = emit(s, 12);
        if (stub && branch_to(g_pc.boxes_shop, 5, 0xE8, stub) && branch_to(g_pc.boxes_pull, 5, 0xE8, stub))
            log_line("pity: the stew menu is told how far each stew's pity is (game+0x%llX, game+0x%llX)",
                     (unsigned long long)(g_pc.boxes_shop - base), (unsigned long long)(g_pc.boxes_pull - base));
        else
            log_line("pity: could not change the stew replies - the menu shows no pity count");
    } else if (g_pc.stew_ok && g_pc.const_int && g_pc.user_record) {
        log_line("pity: the stew replies' box stew list not found exactly once - the menu shows no pity "
                 "count. A game update may have moved it.");
    }

    /* the shop's list of stews, logged on each visit */
    if (g_pc.shop_list) {
        put64(s + 2, (void *)lid_sp_shop_list);
        stub = emit(s, 12);
        if (stub && branch_to(g_pc.shop_list, 5, 0xE8, stub))
            log_line("stew shop: the stews on offer are logged (game+0x%llX)",
                     (unsigned long long)(g_pc.shop_list - base));
    }
}

static void patch_game(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleW(NULL);

    find_pieces(base, &g_pc);
    g_page = page_near(base);
    if (!g_page) {
        log_line("no memory free near the game code - nothing changed");
        return;
    }
    patch_stew(base);
    {
        DWORD old;
        VirtualProtect(g_page, 4096, PAGE_EXECUTE_READ, &old);
    }
    log_line("ready");
}

/* ---------------------------------------------------------------- start */

static int running_in_the_game(void)
{
    wchar_t exe[MAX_PATH];
    const wchar_t *name;
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);

    if (n == 0 || n >= MAX_PATH)
        return 0;
    name = wcsrchr(exe, L'\\');
    name = name ? name + 1 : exe;
    return _wcsicmp(name, L"BrgGame-Steam.exe") == 0;
}

#ifndef LID_SP_NO_DLLMAIN
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        wchar_t *slash;
        DWORD n;

        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&g_log_lock);
        n = GetModuleFileNameW(inst, g_log_path, MAX_PATH);
        slash = (n && n < MAX_PATH - 24) ? wcsrchr(g_log_path, L'\\') : NULL;
        if (slash)
            wcscpy(slash + 1, L"lid-stewpull.log");
        else
            g_log_path[0] = 0;

        if (!running_in_the_game())
            return TRUE;    /* some other program - change nothing */
        log_line("LiD Stew Multi Pull lid-stewpull.dll " LID_DLL_VERSION " loaded");
        patch_game();
    }
    return TRUE;
}
#endif
