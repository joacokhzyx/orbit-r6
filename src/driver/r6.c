#define _CRT_SECURE_NO_WARNINGS
#include "../loader/ir.h"
#include "../codegen-x64/asm_win64.h"
#include "../codegen-x64/peephole.h"
#include "../codegen-x64/regalloc.h"
#include "../codegen-x64/win64.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define R6_PID _getpid
#define R6_SPAWN _spawnvp
#define R6_WAIT _P_WAIT
#else
#include <sys/stat.h>
#include <sys/types.h>
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
    (void)cc; (void)argv;
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
    char asm_path[512], obj_path[512], ld_path[1024];
    char crt2[1024], crtbegin[1024], crtend[1024], libdir[1024], gcclibdir[1024];
    char pidbuf[32];
    char *link_argv[64];
    size_t link_argc = 0;
    R6CoffObject object;
    char asm_error[256];
    int argi = 1, rc = 1;

    if (argi < argc && strcmp(argv[argi], "build") == 0) ++argi;
    if (argi >= argc) {
        fprintf(stderr, "usage: r6 [build] <input.ir> -o <output.exe>\n");
        return 2;
    }
    input = argv[argi++];
    while (argi < argc) {
        if (strcmp(argv[argi], "-o") == 0 && argi + 1 < argc && !output) {
            output = argv[argi + 1];
            argi += 2;
        } else {
            fprintf(stderr, "r6: error[CLI]: unexpected argument '%s'\n", argv[argi]);
            return 2;
        }
    }
    if (!output) {
        fprintf(stderr, "r6: error[CLI]: missing required -o <output.exe>\n");
        return 2;
    }

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

    /* All intermediates remain inside the R6 checkout. */
#ifdef _WIN32
    _mkdir(".r6-build");
#else
    mkdir(".r6-build", 0777);
#endif
    snprintf(pidbuf, sizeof pidbuf, "%ld", (long)R6_PID());
    snprintf(asm_path, sizeof asm_path, ".r6-build/r6-%s.s", pidbuf);
    snprintf(obj_path, sizeof obj_path, ".r6-build/r6-%s.o", pidbuf);
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
    if (!r6_peephole_win64(asm_path, asm_error, sizeof asm_error)) {
        fprintf(stderr, "r6: %s:0:0: error[PEEPHOLE]: %s\n", input, asm_error);
        goto free_module;
    }
    if (!r6_regalloc_win64(asm_path, asm_error, sizeof asm_error)) {
        fprintf(stderr, "r6: %s:0:0: error[REGALLOC]: %s\n", input, asm_error);
        goto free_module;
    }

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
    link_argv[link_argc++] = ld_path;
    link_argv[link_argc++] = "-m"; link_argv[link_argc++] = "i386pep";
    link_argv[link_argc++] = "-Bdynamic"; link_argv[link_argc++] = "-o";
    link_argv[link_argc++] = (char *)output;
    link_argv[link_argc++] = crt2; link_argv[link_argc++] = crtbegin;
    link_argv[link_argc++] = "-L"; link_argv[link_argc++] = gcclibdir;
    link_argv[link_argc++] = "-L"; link_argv[link_argc++] = libdir;
    link_argv[link_argc++] = obj_path;
    link_argv[link_argc++] = ".r6-build/runtime-win64/libruntime_net.a";
    link_argv[link_argc++] = "-lws2_32"; link_argv[link_argc++] = "-lmingw32";
    link_argv[link_argc++] = "-lgcc"; link_argv[link_argc++] = "-lmingwex";
    link_argv[link_argc++] = "-lmsvcrt"; link_argv[link_argc++] = "-lkernel32";
    link_argv[link_argc++] = "-lpthread"; link_argv[link_argc++] = "-ladvapi32";
    link_argv[link_argc++] = "-lshell32"; link_argv[link_argc++] = "-luser32";
    link_argv[link_argc++] = "-lkernel32"; link_argv[link_argc++] = "-lmingw32";
    link_argv[link_argc++] = "-lgcc"; link_argv[link_argc++] = "-lmingwex";
    link_argv[link_argc++] = "-lmsvcrt"; link_argv[link_argc++] = "-lkernel32";
    link_argv[link_argc++] = crtend; link_argv[link_argc] = NULL;
    if (run(ld_path, link_argv) != 0) {
        fprintf(stderr, "r6: %s:0:0: error[LINK]: linker failed (pinned Win64 runtime required)\n", input);
        goto free_module;
    }
    rc = 0;

free_module:
    if (asm_file) fclose(asm_file);
    if (obj_file) fclose(obj_file);
    r6_module_free(&module);
done:
    free(data);
    return rc;
}
