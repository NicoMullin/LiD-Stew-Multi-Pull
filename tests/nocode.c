/* Stand-in for a BrgGame-Steam.exe the plugin does not recognise, as after a
 * game update that moves the code. The plugin must change nothing and say so in
 * its log. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char log[8192] = {0};
    HMODULE dll;
    FILE *f;
    int fails = 0;

    dll = LoadLibraryW(L"lid-stewpull.dll");
    if (!dll) { printf("LoadLibrary failed %lu\n", GetLastError()); return 2; }

    f = fopen("lid-stewpull.log", "rb");
    if (f) { fread(log, 1, sizeof log - 1, f); fclose(f); }
    printf("%s", log);
    if (!strstr(log, "random stews stay off") || !strstr(log, "only the first stew is offered") ||
        !strstr(log, "multi pulls may repeat") || !strstr(log, "multi pulls go one pull at a time")) {
        puts("FAIL: the log does not say the code was not found");
        fails++;
    }
    if (strstr(log, "draw a fresh") || strstr(log, "is offered, not only") || strstr(log, "are logged") ||
        strstr(log, "gives no decal twice") || strstr(log, "in one request")) {
        puts("FAIL: the log says something was changed");
        fails++;
    }
    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails != 0;
}
