#define _CRT_SECURE_NO_WARNINGS
#include "../loader/ir.h"
#include "../codegen-x64/asm_win64.h"
#include "../codegen-x64/elf64.h"
#include "../codegen-x64/peephole.h"
#include "../codegen-x64/regalloc.h"
#include "../codegen-x64/target.h"
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
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <glob.h>
#define R6_PID getpid
#endif

static void print_error(const char *path, const R6Diagnostic *d) {
    fprintf(stderr, "r6: %s:%zu:%zu: error[%s]: %s\n", path,
            d->line, d->column, d->code[0] ? d->code : "DRIVER",
            d->message[0] ? d->message : "operation failed");
}

enum { T_LOAD, T_CODEGEN, T_PEEPHOLE, T_REGALLOC, T_ASM, T_OBJECT, T_LD, T_COUNT };
static const char *const stage_names[T_COUNT] = {
    "load", "codegen", "peephole", "regalloc", "asm", "object", "ld"
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
    pid_t pid = fork();
    int status, code;
    if (pid < 0) {
        fprintf(stderr, "r6: error[TOOLCHAIN]: could not fork for '%s' (errno=%d)\n",
                tool, errno);
        return -1;
    }
    if (pid == 0) {
        execvp(tool, argv);
        fprintf(stderr, "r6: error[TOOLCHAIN]: could not run '%s' (errno=%d)\n",
                tool, errno);
        _exit(127);
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
        if (code == 127) {
            fprintf(stderr, "r6: error[TOOLCHAIN]: could not run '%s'\n", tool);
            return -1;
        }
        return code;
    }
    return 128;
#endif
}

#if R6_TARGET_WIN64
/* D-M1-104: a module linked without the MinGW start-up must not carry what only
 * that start-up runs: constructors and destructors. Thread-local storage stays,
 * because the loader reads the TLS directory itself. Returns the name of the
 * first such section, or NULL when the image has none or is not a readable PE
 * file. */
static const char *pe_startup_section(const char *path, char *name, size_t cap) {
    static const char *const forbidden[] = {
        ".ctors", ".dtors", ".init_array", ".fini_array", ".preinit_array", NULL
    };
    unsigned char dos[64], coff[24], sec[40];
    unsigned long pe_off, symtab, nsyms, table_at;
    unsigned nsec, opt, k, f;
    FILE *fp = fopen(path, "rb");
    const char *found = NULL;
    if (!fp) return NULL;
    if (fread(dos, 1, sizeof dos, fp) != sizeof dos || dos[0] != 'M' || dos[1] != 'Z') goto out;
    pe_off = dos[60] | (dos[61] << 8) | (dos[62] << 16) | ((unsigned long)dos[63] << 24);
    if (fseek(fp, (long)pe_off, SEEK_SET) != 0 || fread(coff, 1, sizeof coff, fp) != sizeof coff ||
        memcmp(coff, "PE\0\0", 4) != 0) goto out;
    nsec = coff[6] | (coff[7] << 8);
    symtab = coff[12] | (coff[13] << 8) | (coff[14] << 16) | ((unsigned long)coff[15] << 24);
    nsyms = coff[16] | (coff[17] << 8) | (coff[18] << 16) | ((unsigned long)coff[19] << 24);
    opt = coff[20] | (coff[21] << 8);
    table_at = pe_off + 24 + opt;
    for (k = 0; k < nsec; ++k) {
        char raw[9];
        if (fseek(fp, (long)(table_at + 40ul * k), SEEK_SET) != 0 ||
            fread(sec, 1, sizeof sec, fp) != sizeof sec) goto out;
        memcpy(raw, sec, 8);
        raw[8] = '\0';
        if (raw[0] == '/' && symtab) {
            /* Long name: an offset into the string table after the symbols. */
            long off = atol(raw + 1);
            if (fseek(fp, (long)(symtab + 18ul * nsyms) + off, SEEK_SET) != 0 ||
                !fgets(name, (int)cap, fp)) goto out;
        } else {
            snprintf(name, cap, "%s", raw);
        }
        for (f = 0; forbidden[f]; ++f) {
            size_t len = strlen(forbidden[f]);
            if (strncmp(name, forbidden[f], len) == 0 && (name[len] == '\0' || name[len] == '$')) {
                found = name;
                goto out;
            }
        }
    }
out:
    fclose(fp);
    return found;
}
#else
/* The multiarch directory glibc's crt1.o lives in, on the layouts R6 has
 * measured: Debian/Ubuntu, Fedora/RHEL and a plain /usr/lib. */
static bool find_crt_dir(char *out, size_t cap) {
    static const char *const dirs[] = {
        "/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/usr/lib", NULL
    };
    char probe[1024];
    size_t k;
    for (k = 0; dirs[k]; ++k) {
        snprintf(probe, sizeof probe, "%s/crt1.o", dirs[k]);
        if (file_exists(probe)) {
            snprintf(out, cap, "%s", dirs[k]);
            return true;
        }
    }
    return false;
}

/* crtbegin.o/crtend.o define __dso_handle, which libc_nonshared's atexit
 * member needs. They live in the GCC version directory, and R6 cannot ask gcc
 * for it. R6_CRT_DIR may name the directory that holds all five objects. */
static bool find_gcc_crt(char *begin, size_t bcap, char *end, size_t ecap) {
    static const char *const patterns[] = {
        "/usr/lib/gcc/x86_64-linux-gnu/*/crtbegin.o",
        "/usr/lib64/gcc/x86_64-linux-gnu/*/crtbegin.o",
        "/usr/lib/gcc/x86_64-pc-linux-gnu/*/crtbegin.o",
        NULL
    };
    char endpat[1024];
    glob_t g;
    size_t k, pick;
    bool ok = false;
    for (k = 0; patterns[k] && !ok; ++k) {
        const char *slash;
        if (glob(patterns[k], 0, NULL, &g) != 0) continue;
        if (g.gl_pathc == 0) { globfree(&g); continue; }
        pick = g.gl_pathc - 1;
        snprintf(begin, bcap, "%s", g.gl_pathv[pick]);
        slash = strrchr(begin, '/');
        if (slash) {
            size_t dir = (size_t)(slash - begin) + 1;
            if (dir + 10 < sizeof endpat) {
                memcpy(endpat, begin, dir);
                snprintf(endpat + dir, sizeof endpat - dir, "crtend.o");
                if (file_exists(endpat)) {
                    snprintf(end, ecap, "%s", endpat);
                    ok = true;
                }
            }
        }
        globfree(&g);
    }
    return ok;
}
#endif

/* D-M1-100: the line `--version` prints. The protocol number is what the
 * language checks before it hands r6 a module; docs/DECISIONS.md says what
 * bumps it. */
#define R6_VERSION "0.1.0"
#define R6_IR_PROTOCOL 3

int main(int argc, char **argv) {
    const char *input, *output = NULL;
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
#if R6_TARGET_WIN64
    const char *root, *version;
    char crt2[1024], crtbegin[1024], crtend[1024], libdir[1024], gcclibdir[1024];
    char bad_section[64], msvcrt_dll[1024];
    const char *bad;
#else
    const char *crt_dir_env, *ld_env;
    char crt1[1024], crti[1024], crtn[1024], libdir[1000], ldso[1024];
    char crtbegin[1024], crtend[1024];
#endif
    char pidbuf[32];
    char *link_argv[64];
    size_t link_argc = 0;
    R6CoffObject object;
    char asm_error[256];
    int argi = 1, rc = 1, stage;
    bool uses_db = false, uses_net, own_entry, timings, object_mode = false;
    const char *obj_out;
    const char *timings_env = getenv("R6_TIMINGS");
    double t0;
    size_t fn;

    timings = timings_env && strcmp(timings_env, "1") == 0;

    if (argi < argc && (strcmp(argv[argi], "--version") == 0 || strcmp(argv[argi], "-V") == 0)) {
        printf("r6 %s ir-protocol %d\n", R6_VERSION, R6_IR_PROTOCOL);
        return 0;
    }
    if (argi < argc && (strcmp(argv[argi], "--help") == 0 || strcmp(argv[argi], "-h") == 0)) {
        fputs("usage: r6 [build] <input.ir> -o <output> [--runtime-dir <dir>] [--timings]\n"
              "       r6 build --object <runtime-module.ir> -o <output.o>   no linking\n"
              "       r6 --version   prints `r6 <semver> ir-protocol <N>` and exits 0\n"
              "  --runtime-dir <dir>  directory with libruntime_net.a, libruntime_net_db.a and\n"
              "                       (Windows) sqlite3.lib (else R6_RUNTIME_DIR, else\n"
              "                       .r6-build/runtime-win64 on Windows, runtime-linux-x64 on Linux)\n"
              "  --timings            print per-stage timings (or R6_TIMINGS=1)\n"
              "  R6_TMPDIR            directory of the .s/.o temporaries (else the system temp dir)\n"
              "  Linux only: R6_CRT_DIR overrides the glibc start-up directory, R6_LD the linker.\n",
              stdout);
        return 0;
    }
    if (argi < argc && strcmp(argv[argi], "build") == 0) ++argi;
    if (argi >= argc) {
        fprintf(stderr, "usage: r6 [build] <input.ir> -o <output.exe> "
                        "[--runtime-dir <dir>] [--timings]\n");
        return 2;
    }
    if (strcmp(argv[argi], "--object") == 0) {
        object_mode = true;
        ++argi;
        if (argi >= argc) {
            fprintf(stderr, "r6: error[CLI]: missing input file\n");
            return 2;
        }
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
        } else if (strcmp(argv[argi], "--object") == 0 && !object_mode) {
            object_mode = true;
            ++argi;
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
    if (module.runtime_module != object_mode) {
        fprintf(stderr, "r6: %s:0:0: error[CLI]: %s\n", input,
                object_mode ? "--object builds a runtime module (\"runtimeModule\": true) only"
                            : "a runtime module has no main: build it with --object");
        goto free_module;
    }

    /* D-M1-87: a module that declares an orbit_db_* extern links the DB
     * variant of the runtime archive and the SQLite import library, as the
     * language does for a program that calls the database (pipeline.orb). */
    for (fn = 0; fn < module.function_count; ++fn)
        if (module.functions[fn].is_extern && module.functions[fn].name &&
            strncmp(module.functions[fn].name, "orbit_db_", 9) == 0)
            uses_db = true;
    /* D-M1-102: ws2_32 only for a module the pin's rule calls a network
     * program; a database program is one. */
    uses_net = r6_module_uses_network(&module);
    own_entry = r6_module_native_entry(&module);

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

    if (!runtime_dir) {
        runtime_dir = getenv("R6_RUNTIME_DIR");
        if (!runtime_dir || !*runtime_dir)
            runtime_dir = R6_TARGET_WIN64 ? ".r6-build/runtime-win64"
                                          : ".r6-build/runtime-linux-x64";
    }
    snprintf(runtime_a, sizeof runtime_a, "%s/%s", runtime_dir,
             uses_db ? "libruntime_net_db.a" : "libruntime_net.a");
#if R6_TARGET_WIN64
    snprintf(sqlite_lib, sizeof sqlite_lib, "%s/sqlite3.lib", runtime_dir);
    if (!object_mode && (!file_exists(runtime_a) || (uses_db && !file_exists(sqlite_lib)))) {
        full_path(file_exists(runtime_a) ? sqlite_lib : runtime_a, full, sizeof full);
        fprintf(stderr, "r6: %s:0:0: error[LINK]: runtime archive not found: %s\n",
                input, full);
        goto free_module;
    }
#else
    (void)uses_net;
    (void)own_entry;
    (void)sqlite_lib;
    if (!object_mode && !file_exists(runtime_a)) {
        full_path(runtime_a, full, sizeof full);
        fprintf(stderr, "r6: %s:0:0: error[LINK]: runtime archive not found: %s\n",
                input, full);
        goto free_module;
    }
#endif
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
    obj_out = object_mode ? output : obj_path;
    obj_file = fopen(obj_out, "wb");
#if R6_TARGET_WIN64
    if (!obj_file || !r6_coff_write(&object, obj_file)) {
#else
    if (!obj_file || !r6_elf_write(&object, obj_file)) {
#endif
        fprintf(stderr, "r6: %s:0:0: error[OBJECT]: %s\n", input,
                obj_file ? r6_coff_error(&object) : strerror(errno));
        if (obj_file) fclose(obj_file);
        obj_file = NULL;
        r6_coff_free(&object);
        goto free_module;
    }
    if (fclose(obj_file) != 0) {
        obj_file = NULL;
        fprintf(stderr, "r6: %s:0:0: error[OBJECT]: failed closing the object file\n", input);
        r6_coff_free(&object);
        goto free_module;
    }
    obj_file = NULL;
    r6_coff_free(&object);
    stage_add(T_OBJECT, t0);
    if (object_mode) {
        remove(asm_path);
        rc = 0;
        goto free_module;
    }
#if R6_TARGET_WIN64
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
    link_argv[link_argc++] = ld_path;
    link_argv[link_argc++] = "-m"; link_argv[link_argc++] = "i386pep";
    link_argv[link_argc++] = "-Bdynamic";
    link_argv[link_argc++] = "--stack"; link_argv[link_argc++] = "16777216";
    link_argv[link_argc++] = "-o";
    link_argv[link_argc++] = (char *)output;
    if (own_entry) {
        /* D-M1-104: R6's `orbit_start` is the entry, so the start-up objects
         * stay out. An import that needs a runtime pseudo-relocation is a link
         * error instead of a silent miss. */
        link_argv[link_argc++] = "-e"; link_argv[link_argc++] = "orbit_start";
        link_argv[link_argc++] = "--subsystem"; link_argv[link_argc++] = "console";
        link_argv[link_argc++] = "--disable-auto-import";
        link_argv[link_argc++] = "--disable-runtime-pseudo-reloc";
    } else {
        link_argv[link_argc++] = crt2; link_argv[link_argc++] = crtbegin;
    }
    link_argv[link_argc++] = "-L"; link_argv[link_argc++] = gcclibdir;
    link_argv[link_argc++] = "-L"; link_argv[link_argc++] = libdir;
    link_argv[link_argc++] = obj_path;
    link_argv[link_argc++] = runtime_a;
    if (uses_db) link_argv[link_argc++] = sqlite_lib;
    if (uses_net) link_argv[link_argc++] = "-lws2_32";
    link_argv[link_argc++] = "-lmingw32";
    link_argv[link_argc++] = "-lgcc"; link_argv[link_argc++] = "-lmingwex";
    link_argv[link_argc++] = "-lmsvcrt"; link_argv[link_argc++] = "-lkernel32";
    link_argv[link_argc++] = "-lpthread"; link_argv[link_argc++] = "-ladvapi32";
    link_argv[link_argc++] = "-lshell32"; link_argv[link_argc++] = "-luser32";
    link_argv[link_argc++] = "-lkernel32"; link_argv[link_argc++] = "-lmingw32";
    link_argv[link_argc++] = "-lgcc"; link_argv[link_argc++] = "-lmingwex";
    link_argv[link_argc++] = "-lmsvcrt"; link_argv[link_argc++] = "-lkernel32";
    if (own_entry) {
        /* libmingwex's gdtoa calls atexit, which crt2.o defines and no import
         * library carries. msvcrt.dll exports it, so ld imports it from the
         * DLL itself. */
        const char *sysroot = getenv("SystemRoot");
        snprintf(msvcrt_dll, sizeof msvcrt_dll, "%s/System32/msvcrt.dll",
                 sysroot && *sysroot ? sysroot : "C:/Windows");
        link_argv[link_argc++] = msvcrt_dll;
    } else {
        link_argv[link_argc++] = crtend;
    }
    link_argv[link_argc] = NULL;
    t0 = now_ms();
    if (run(ld_path, link_argv) != 0) {
        fprintf(stderr, "r6: %s:0:0: error[LINK]: linker failed (pinned Win64 runtime required)\n", input);
        goto free_module;
    }
    stage_add(T_LD, t0);
    if (own_entry && (bad = pe_startup_section(output, bad_section, sizeof bad_section)) != NULL) {
        remove(output);
        fprintf(stderr, "r6: %s:0:0: error[LINK]: the runtime needs the MinGW start-up "
                        "(section %s)\n", input, bad);
        goto free_module;
    }
#else
    crt_dir_env = getenv("R6_CRT_DIR");
    ld_env = getenv("R6_LD");
    snprintf(ld_path, sizeof ld_path, "%s", ld_env && *ld_env ? ld_env : "ld");
    if (crt_dir_env && *crt_dir_env) {
        snprintf(crt1, sizeof crt1, "%s/crt1.o", crt_dir_env);
        snprintf(crti, sizeof crti, "%s/crti.o", crt_dir_env);
        snprintf(crtn, sizeof crtn, "%s/crtn.o", crt_dir_env);
    } else if (!find_crt_dir(libdir, sizeof libdir)) {
        fprintf(stderr, "r6: %s:0:0: error[LINK]: glibc start-up objects not found (set R6_CRT_DIR)\n",
                input);
        goto free_module;
    } else {
        snprintf(crt1, sizeof crt1, "%s/crt1.o", libdir);
        snprintf(crti, sizeof crti, "%s/crti.o", libdir);
        snprintf(crtn, sizeof crtn, "%s/crtn.o", libdir);
    }
    if (!file_exists(crt1) || !file_exists(crti) || !file_exists(crtn)) {
        fprintf(stderr, "r6: %s:0:0: error[LINK]: crt1.o/crti.o/crtn.o not found in %s\n",
                input, crt_dir_env && *crt_dir_env ? crt_dir_env : libdir);
        goto free_module;
    }
    snprintf(crtbegin, sizeof crtbegin, "%s/crtbegin.o",
             crt_dir_env && *crt_dir_env ? crt_dir_env : libdir);
    snprintf(crtend, sizeof crtend, "%s/crtend.o",
             crt_dir_env && *crt_dir_env ? crt_dir_env : libdir);
    if (!file_exists(crtbegin) || !file_exists(crtend)) {
        if (!find_gcc_crt(crtbegin, sizeof crtbegin, crtend, sizeof crtend)) {
            fprintf(stderr, "r6: %s:0:0: error[LINK]: crtbegin.o/crtend.o not found (set R6_CRT_DIR)\n",
                    input);
            goto free_module;
        }
    }
    snprintf(ldso, sizeof ldso, "%s",
             file_exists("/lib64/ld-linux-x86-64.so.2") ? "/lib64/ld-linux-x86-64.so.2"
                                                        : "/lib/ld-linux-x86-64.so.2");
    if (!file_exists(ldso)) {
        fprintf(stderr, "r6: %s:0:0: error[LINK]: dynamic linker not found\n", input);
        goto free_module;
    }
    link_argv[link_argc++] = ld_path;
    link_argv[link_argc++] = "-o";
    link_argv[link_argc++] = (char *)output;
    link_argv[link_argc++] = "-dynamic-linker";
    link_argv[link_argc++] = ldso;
    link_argv[link_argc++] = crt1;
    link_argv[link_argc++] = crti;
    link_argv[link_argc++] = crtbegin;
    link_argv[link_argc++] = obj_path;
    link_argv[link_argc++] = "--start-group";
    link_argv[link_argc++] = runtime_a;
    if (uses_db) link_argv[link_argc++] = "-lsqlite3";
    link_argv[link_argc++] = "--end-group";
    link_argv[link_argc++] = "-lc";
    link_argv[link_argc++] = "-lm";
    link_argv[link_argc++] = "-ldl";
    link_argv[link_argc++] = "-lpthread";
    link_argv[link_argc++] = crtend;
    link_argv[link_argc++] = crtn;
    link_argv[link_argc] = NULL;
    t0 = now_ms();
    if (run(ld_path, link_argv) != 0) {
        fprintf(stderr, "r6: %s:0:0: error[LINK]: linker failed (Linux x64 runtime and glibc required)\n", input);
        goto free_module;
    }
    stage_add(T_LD, t0);
#endif
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
