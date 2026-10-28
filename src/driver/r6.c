#define _CRT_SECURE_NO_WARNINGS
#include "../loader/ir.h"
#include "../codegen-x64/asm_win64.h"
#include "../codegen-x64/peephole.h"
#include "../codegen-x64/regalloc.h"
#include "../codegen-x64/win64.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <process.h>
#define R6_PID _getpid
#define R6_SPAWN _spawnvp
#define R6_WAIT _P_WAIT
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define R6_PID getpid
#define R6_SPAWN _spawnvp
#define R6_WAIT 0
#endif

static void print_error(const char *path, const R6Diagnostic *d) {
    fprintf(stderr, "r6: %s:%zu:%zu: error[%s]: %s\n", path,
            d->line, d->column, d->code[0] ? d->code : "DRIVER",
            d->message[0] ? d->message : "operation failed");
}

enum { T_LOAD, T_CODEGEN, T_PEEPHOLE, T_REGALLOC, T_ASM, T_COFF, T_LD, T_COUNT };
static const char *const stage_names[T_COUNT] = {
    "load", "codegen", "peephole", "regalloc", "asm", "coff", "ld"
};
static double stage_ms[T_COUNT];
static bool stage_ran[T_COUNT];

static double now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}

static void stage_add(int stage, double since) {
    stage_ms[stage] += now_ms() - since;
    stage_ran[stage] = true;
}

/* D-M1-98: the three runtime inputs of the link, from one directory. */
static bool file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static void full_path(const char *path, char *out, size_t cap) {
#ifdef _WIN32
    if (_fullpath(out, path, cap)) return;
#endif
    snprintf(out, cap, "%s", path);
}

static int run(const char *tool, char *const argv[]) {
#ifdef _WIN32
    intptr_t status = R6_SPAWN(R6_WAIT, tool, (const char *const *)argv);
    if (status == -1) {
        fprintf(stderr, "r6: error[TOOLCHAIN]: could not run '%s' (errno=%d)\n",
                tool, errno);
        return -1;
    }
    return (int)status;
#else
    (void)tool; (void)argv;
    return -1;
#endif
}

int main(int argc, char **argv) {
    const char *input, *output = NULL, *root, *version;
    FILE *file = NULL, *asm_file = NULL, *obj_file = NULL;
    char *data = NULL;
    long file_len;
    size_t got;
    R6Module module;
    R6Diagnostic diag = {0};
    char asm_path[1024], obj_path[1024], ld_path[1024];
    char tmpdir[900], runtime_a[1024], sqlite_lib[1024], full[1024];
    const char *runtime_dir = NULL, *tmp_env;
    size_t tmp_len;
    char crt2[1024], crtbegin[1024], crtend[1024], libdir[1024], gcclibdir[1024];
    char pidbuf[32];
    char *link_argv[64];
    size_t link_argc = 0;
    R6CoffObject object;
    char asm_error[256];
    int argi = 1, rc = 1, stage;
    bool uses_db = false, timings;
    const char *timings_env = getenv("R6_TIMINGS");
    double t0;
    size_t fn;

    timings = timings_env && strcmp(timings_env, "1") == 0;

    if (argi < argc && (strcmp(argv[argi], "--help") == 0 || strcmp(argv[argi], "-h") == 0)) {
        fputs("usage: r6 [build] <input.ir> -o <output.exe> [--runtime-dir <dir>] [--timings]\n"
              "  --runtime-dir <dir>  directory with libruntime_net.a, libruntime_net_db.a and\n"
              "                       sqlite3.lib (else R6_RUNTIME_DIR, else .r6-build/runtime-win64)\n"
              "  --timings            print per-stage timings (or R6_TIMINGS=1)\n"
              "  R6_TMPDIR            directory of the .s/.o temporaries (else the system temp dir)\n",
              stdout);
        return 0;
    }
    if (argi < argc && strcmp(argv[argi], "build") == 0) ++argi;
    if (argi >= argc) {
        fprintf(stderr, "usage: r6 [build] <input.ir> -o <output.exe> "
                        "[--runtime-dir <dir>] [--timings]\n");
        return 2;
    }
    input = argv[argi++];
    while (argi < argc) {
        if (strcmp(argv[argi], "-o") == 0 && argi + 1 < argc && !output) {
            output = argv[argi + 1];
            argi += 2;
        } else if (strcmp(argv[argi], "--runtime-dir") == 0 && argi + 1 < argc &&
                   !runtime_dir) {
            runtime_dir = argv[argi + 1];
            argi += 2;
        } else if (strcmp(argv[argi], "--timings") == 0) {
            timings = true;
            ++argi;
        } else {
            fprintf(stderr, "r6: error[CLI]: unexpected argument '%s'\n", argv[argi]);
            return 2;
        }
    }
    if (!output) {
        fprintf(stderr, "r6: error[CLI]: missing required -o <output.exe>\n");
        return 2;
    }

    t0 = now_ms();
    file = fopen(input, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0 || (file_len = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "r6: %s:0:0: error[INPUT]: cannot read input (%s)\n",
                input, strerror(errno));
        if (file) fclose(file);
        return 1;
    }
    data = (char *)malloc((size_t)file_len + 1);
    if (!data) { fclose(file); fputs("r6: error[OOM]: cannot allocate input\n", stderr); return 1; }
    got = fread(data, 1, (size_t)file_len, file);
    fclose(file);
    data[got] = '\0';
    if (got != (size_t)file_len) {
        fprintf(stderr, "r6: %s:0:0: error[INPUT]: short read\n", input);
        goto done;
    }
    if (!r6_ir_parse(data, got, &module, &diag)) {
        print_error(input, &diag);
        goto done;
    }
    stage_add(T_LOAD, t0);

    /* D-M1-87: a module that declares an orbit_db_* extern links the DB
     * variant of the runtime archive and the SQLite import library, as the
     * language does for a program that calls the database (pipeline.orb). */
    for (fn = 0; fn < module.function_count; ++fn)
        if (module.functions[fn].is_extern && module.functions[fn].name &&
            strncmp(module.functions[fn].name, "orbit_db_", 9) == 0)
            uses_db = true;

    /* D-M1-98: R6_TMPDIR, else the system temporary directory; never the
     * project of the program being built. */
    tmp_env = getenv("R6_TMPDIR");
    if (tmp_env && *tmp_env) {
        snprintf(tmpdir, sizeof tmpdir, "%s", tmp_env);
#ifdef _WIN32
        _mkdir(tmpdir);
#else
        mkdir(tmpdir, 0777);
#endif
    } else {
#ifdef _WIN32
        DWORD n = GetTempPathA((DWORD)sizeof tmpdir, tmpdir);
        if (n == 0 || n >= sizeof tmpdir) snprintf(tmpdir, sizeof tmpdir, ".");
#else
        snprintf(tmpdir, sizeof tmpdir, "/tmp");
#endif
    }
    tmp_len = strlen(tmpdir);
    while (tmp_len > 1 && (tmpdir[tmp_len - 1] == '/' || tmpdir[tmp_len - 1] == '\\'))
        tmpdir[--tmp_len] = '\0';
    snprintf(pidbuf, sizeof pidbuf, "%ld", (long)R6_PID());
    snprintf(asm_path, sizeof asm_path, "%s/r6-%s.s", tmpdir, pidbuf);
    snprintf(obj_path, sizeof obj_path, "%s/r6-%s.o", tmpdir, pidbuf);
    t0 = now_ms();
    asm_file = fopen(asm_path, "wb");
    if (!asm_file) {
        snprintf(diag.code, sizeof diag.code, "OUTPUT");
        snprintf(diag.message, sizeof diag.message, "cannot create assembler input: %s", strerror(errno));
        print_error(input, &diag);
        goto free_module;
    }
    if (!r6_codegen_win64(asm_file, &module, &diag)) {
        fclose(asm_file);
        asm_file = NULL;
        print_error(input, &diag);
        goto free_module;
    }
    if (fclose(asm_file) != 0) {
        asm_file = NULL;
        snprintf(diag.code, sizeof diag.code, "OUTPUT");
        snprintf(diag.message, sizeof diag.message, "failed writing assembler input");
        print_error(input, &diag);
        goto free_module;
    }
    asm_file = NULL;
    stage_add(T_CODEGEN, t0);
    t0 = now_ms();
    if (!r6_peephole_win64(asm_path, asm_error, sizeof asm_error)) {
        fprintf(stderr, "r6: %s:0:0: error[PEEPHOLE]: %s\n", input, asm_error);
        goto free_module;
    }
    stage_add(T_PEEPHOLE, t0);
    t0 = now_ms();
    if (!r6_regalloc_win64(asm_path, asm_error, sizeof asm_error)) {
        fprintf(stderr, "r6: %s:0:0: error[REGALLOC]: %s\n", input, asm_error);
        goto free_module;
    }
    stage_add(T_REGALLOC, t0);
    t0 = now_ms();
    if (!r6_peephole_final(asm_path, asm_error, sizeof asm_error)) {
        fprintf(stderr, "r6: %s:0:0: error[PEEPHOLE]: %s\n", input, asm_error);
        goto free_module;
    }
    stage_add(T_PEEPHOLE, t0);

    root = getenv("R6_MINGW_ROOT");
    if (!root || !*root) root = "C:/w64devkit";
    version = getenv("R6_MINGW_GCC_VERSION");
    if (!version || !*version) version = "16.2.0";
    snprintf(ld_path, sizeof ld_path, "%s/bin/ld.exe", root);
    snprintf(crt2, sizeof crt2, "%s/lib/crt2.o", root);
    snprintf(crtbegin, sizeof crtbegin, "%s/lib/gcc/x86_64-w64-mingw32/%s/crtbegin.o", root, version);
    snprintf(crtend, sizeof crtend, "%s/lib/gcc/x86_64-w64-mingw32/%s/crtend.o", root, version);
    snprintf(libdir, sizeof libdir, "%s/lib", root);
    snprintf(gcclibdir, sizeof gcclibdir, "%s/lib/gcc/x86_64-w64-mingw32/%s", root, version);
    if (!runtime_dir) {
        runtime_dir = getenv("R6_RUNTIME_DIR");
        if (!runtime_dir || !*runtime_dir) runtime_dir = ".r6-build/runtime-win64";
    }
    snprintf(runtime_a, sizeof runtime_a, "%s/%s", runtime_dir,
             uses_db ? "libruntime_net_db.a" : "libruntime_net.a");
    snprintf(sqlite_lib, sizeof sqlite_lib, "%s/sqlite3.lib", runtime_dir);
    if (!file_exists(runtime_a) || (uses_db && !file_exists(sqlite_lib))) {
        full_path(file_exists(runtime_a) ? sqlite_lib : runtime_a, full, sizeof full);
        fprintf(stderr, "r6: %s:0:0: error[LINK]: runtime archive not found: %s\n",
                input, full);
        goto free_module;
    }
    t0 = now_ms();
    asm_file = fopen(asm_path, "rb");
    if (!asm_file) {
        fprintf(stderr, "r6: %s:0:0: error[INPUT]: cannot reopen generated assembler text\n", input);
        goto free_module;
    }
    if (!r6_assemble_win64(asm_file, &object, asm_error, sizeof asm_error)) {
        fclose(asm_file); asm_file = NULL;
        fprintf(stderr, "r6: %s:0:0: error[ASSEMBLER]: %s\n", input, asm_error);
        goto free_module;
    }
    fclose(asm_file); asm_file = NULL;
    stage_add(T_ASM, t0);
    t0 = now_ms();
    obj_file = fopen(obj_path, "wb");
    if (!obj_file || !r6_coff_write(&object, obj_file)) {
        fprintf(stderr, "r6: %s:0:0: error[OBJECT]: %s\n", input,
                obj_file ? r6_coff_error(&object) : strerror(errno));
        if (obj_file) fclose(obj_file);
        obj_file = NULL;
        r6_coff_free(&object);
        goto free_module;
    }
    if (fclose(obj_file) != 0) {
        obj_file = NULL;
        fprintf(stderr, "r6: %s:0:0: error[OBJECT]: failed closing COFF object\n", input);
        r6_coff_free(&object);
        goto free_module;
    }
    obj_file = NULL;
    r6_coff_free(&object);
    stage_add(T_COFF, t0);
    link_argv[link_argc++] = ld_path;
    link_argv[link_argc++] = "-m"; link_argv[link_argc++] = "i386pep";
    link_argv[link_argc++] = "-Bdynamic";
    link_argv[link_argc++] = "--stack"; link_argv[link_argc++] = "16777216";
    link_argv[link_argc++] = "-o";
    link_argv[link_argc++] = (char *)output;
    link_argv[link_argc++] = crt2; link_argv[link_argc++] = crtbegin;
    link_argv[link_argc++] = "-L"; link_argv[link_argc++] = gcclibdir;
    link_argv[link_argc++] = "-L"; link_argv[link_argc++] = libdir;
    link_argv[link_argc++] = obj_path;
    link_argv[link_argc++] = runtime_a;
    if (uses_db) link_argv[link_argc++] = sqlite_lib;
    link_argv[link_argc++] = "-lws2_32"; link_argv[link_argc++] = "-lmingw32";
    link_argv[link_argc++] = "-lgcc"; link_argv[link_argc++] = "-lmingwex";
    link_argv[link_argc++] = "-lmsvcrt"; link_argv[link_argc++] = "-lkernel32";
    link_argv[link_argc++] = "-lpthread"; link_argv[link_argc++] = "-ladvapi32";
    link_argv[link_argc++] = "-lshell32"; link_argv[link_argc++] = "-luser32";
    link_argv[link_argc++] = "-lkernel32"; link_argv[link_argc++] = "-lmingw32";
    link_argv[link_argc++] = "-lgcc"; link_argv[link_argc++] = "-lmingwex";
    link_argv[link_argc++] = "-lmsvcrt"; link_argv[link_argc++] = "-lkernel32";
    link_argv[link_argc++] = crtend; link_argv[link_argc] = NULL;
    t0 = now_ms();
    if (run(ld_path, link_argv) != 0) {
        fprintf(stderr, "r6: %s:0:0: error[LINK]: linker failed (pinned Win64 runtime required)\n", input);
        goto free_module;
    }
    stage_add(T_LD, t0);
    /* A failed build keeps its .s/.o for inspection; a good one owes nothing. */
    remove(asm_path);
    remove(obj_path);
    rc = 0;

free_module:
    if (asm_file) fclose(asm_file);
    if (obj_file) fclose(obj_file);
    r6_module_free(&module);
done:
    free(data);
    if (timings)
        for (stage = 0; stage < T_COUNT; ++stage)
            if (stage_ran[stage])
                fprintf(stderr, "r6: timing %-8s %10.3f ms\n",
                        stage_names[stage], stage_ms[stage]);
    return rc;
}
