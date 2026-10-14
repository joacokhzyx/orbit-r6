#define _CRT_SECURE_NO_WARNINGS
#include "../loader/ir.h"
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

static int run(const char *cc, char *const argv[]) {
#ifdef _WIN32
    intptr_t status = R6_SPAWN(R6_WAIT, cc, (const char *const *)argv);
    if (status == -1) {
        fprintf(stderr, "r6: error[TOOLCHAIN]: could not run '%s' (errno=%d)\n",
                cc, errno);
        return -1;
    }
    return (int)status;
#else
    (void)cc; (void)argv;
    return -1;
#endif
}

int main(int argc, char **argv) {
    const char *input, *output = NULL, *cc;
    FILE *file = NULL, *asm_file = NULL;
    char *data = NULL;
    long file_len;
    size_t got;
    R6Module module;
    R6Diagnostic diag = {0};
    char asm_path[512], obj_path[512];
    char pidbuf[32];
    char *as_argv[8], *link_argv[10];
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

    cc = getenv("R6_CC");
    if (!cc || !*cc) cc = "gcc";
    as_argv[0] = (char *)cc; as_argv[1] = "-c"; as_argv[2] = asm_path;
    as_argv[3] = "-o"; as_argv[4] = obj_path; as_argv[5] = NULL;
    if (run(cc, as_argv) != 0) {
        fprintf(stderr, "r6: %s:0:0: error[ASSEMBLER]: assembler failed\n", input);
        goto free_module;
    }
    link_argv[0] = (char *)cc; link_argv[1] = obj_path;
    link_argv[2] = ".r6-build/runtime-win64/libruntime_net.a";
    link_argv[3] = "-lws2_32"; link_argv[4] = "-o";
    link_argv[5] = (char *)output; link_argv[6] = NULL;
    if (run(cc, link_argv) != 0) {
        fprintf(stderr, "r6: %s:0:0: error[LINK]: linker failed (pinned Win64 runtime required)\n", input);
        goto free_module;
    }
    rc = 0;

free_module:
    if (asm_file) fclose(asm_file);
    r6_module_free(&module);
done:
    free(data);
    return rc;
}
