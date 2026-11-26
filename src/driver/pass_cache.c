#define _CRT_SECURE_NO_WARNINGS
#include "pass_cache.h"
#include "../codegen-x64/target.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#define cache_pid _getpid
#else
#include <unistd.h>
#define cache_pid getpid
#endif

static const uint64_t hash_start = UINT64_C(14695981039346656037);
static const uint64_t cache_magic = UINT64_C(0x5236504153533031);
enum { MAX_CACHE_BYTES = 256 * 1024 * 1024 };

typedef struct {
    uint64_t key, input_size, output_size, output_hash;
} CacheHeader;

typedef struct {
    CacheHeader header;
    size_t input_offset, output_offset;
} CacheRecord;

typedef struct {
    const char *data;
    size_t remaining;
} CacheReader;

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    hash ^= value + UINT64_C(0x9e3779b97f4a7c15) + (hash << 6) + (hash >> 2);
    return hash * UINT64_C(1099511628211);
}

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const unsigned char *bytes = data;
    while (size >= sizeof(uint64_t)) {
        uint64_t word;
        memcpy(&word, bytes, sizeof word);
        hash = hash_u64(hash, word);
        bytes += sizeof word;
        size -= sizeof word;
    }
    for (size_t i = 0; i < size; ++i) hash = hash_u64(hash, bytes[i]);
    return hash;
}

static uint64_t hash_name(uint64_t hash, const char *name) {
    return name ? hash_bytes(hash, name, strlen(name) + 1) : hash_bytes(hash, "", 1);
}

static bool driver_key(const char *version, uint64_t *key) {
#ifdef _WIN32
    char executable[4096];
    DWORD count = GetModuleFileNameA(NULL, executable, sizeof executable);
    if (count == 0 || count >= sizeof executable) return false;
#else
    const char *executable = "/proc/self/exe";
#endif
    FILE *driver = fopen(executable, "rb");
    if (!driver) return false;
    uint64_t hash = hash_u64(hash_name(hash_start, version), R6_TARGET_WIN64);
    unsigned char buffer[32768];
    size_t count_read;
    while ((count_read = fread(buffer, 1, sizeof buffer, driver)) > 0)
        hash = hash_bytes(hash, buffer, count_read);
    bool ok = feof(driver) && !ferror(driver);
    fclose(driver);
    *key = hash;
    return ok;
}

/* Hash the normalized IR as well as comparing the exact unoptimized assembly.
 * The assembly captures ABI choices from the other functions, globals, and
 * data layout used here without invalidating unrelated functions on each edit. */
static uint64_t function_key(const R6Function *fn) {
    uint64_t hash = hash_name(hash_start, fn->name);
    hash = hash_name(hash, fn->return_type);
    hash = hash_u64(hash, fn->param_count);
    for (size_t i = 0; i < fn->param_count; ++i) {
        hash = hash_name(hash, fn->params[i]);
        hash = hash_name(hash, fn->param_types[i]);
    }
    hash = hash_u64(hash, fn->register_type_count);
    for (size_t i = 0; i < fn->register_type_count; ++i)
        hash = hash_name(hash, fn->register_types[i]);
    hash = hash_u64(hash, fn->instruction_count);
    for (size_t i = 0; i < fn->instruction_count; ++i) {
        const R6Instruction *ins = &fn->instructions[i];
        hash = hash_u64(hash, (uint64_t)ins->opcode);
        hash = hash_u64(hash, ins->has_dest ? (uint64_t)ins->dest : UINT64_MAX);
        hash = hash_u64(hash, (uint64_t)ins->elem_kind);
        hash = hash_u64(hash, (uint64_t)ins->source_line);
        for (size_t j = 0; j < 3; ++j) {
            const R6Value *value = &ins->operands[j];
            hash = hash_u64(hash, (uint64_t)value->kind);
            switch (value->kind) {
            case R6_VAL_STRING: case R6_VAL_SYMBOL:
                hash = hash_u64(hash, value->string_len);
                hash = hash_bytes(hash, value->as.string, value->string_len);
                break;
            case R6_VAL_INT: hash = hash_u64(hash, (uint64_t)value->as.i64); break;
            case R6_VAL_FLOAT64: hash = hash_u64(hash, value->as.f64_bits); break;
            case R6_VAL_BOOL: hash = hash_u64(hash, value->as.boolean); break;
            case R6_VAL_REG: case R6_VAL_LABEL: hash = hash_u64(hash, (uint64_t)value->as.i32); break;
            case R6_VAL_NONE: break;
            }
        }
    }
    return hash;
}

static char *read_cache(const char *path, uint64_t key, CacheReader *reader) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    char *storage = NULL;
    if (fseek(file, 0, SEEK_END) == 0) {
        long len = ftell(file);
        uint64_t head[2];
        if (len >= (long)sizeof head && len <= MAX_CACHE_BYTES &&
            fseek(file, 0, SEEK_SET) == 0 &&
            fread(head, sizeof head, 1, file) == 1 &&
            head[0] == cache_magic && head[1] == key) {
            size_t payload = (size_t)len - sizeof head;
            if (payload && (storage = malloc(payload)) != NULL) {
                if (fread(storage, 1, payload, file) == payload)
                    *reader = (CacheReader){.data = storage, .remaining = payload};
                else {
                    free(storage);
                    storage = NULL;
                }
            }
        }
    }
    fclose(file);
    return storage;
}

static const char *lookup(CacheReader *reader, CacheRecord *record, const char *input) {
    CacheHeader header;
    if (reader->remaining < sizeof header) return NULL;
    memcpy(&header, reader->data, sizeof header);
    reader->data += sizeof header;
    reader->remaining -= sizeof header;
    if (header.input_size > reader->remaining ||
        header.output_size > reader->remaining - header.input_size) {
        reader->remaining = 0;
        return NULL;
    }
    const char *original = reader->data;
    const char *output = original + header.input_size;
    size_t step = (size_t)(header.input_size + header.output_size);
    reader->data += step;
    reader->remaining -= step;
    if (header.key != record->header.key || header.input_size != record->header.input_size ||
        memcmp(original, input, (size_t)header.input_size) != 0 ||
        hash_bytes(hash_start, output, (size_t)header.output_size) != header.output_hash)
        return NULL;
    record->header = header;
    return output;
}

static bool append(TextBuf *to, const char *data, size_t size) {
    if (size == 0) return true;
    if (!text_reserve(to, size)) return false;
    memcpy(to->p + to->n, data, size);
    to->n += size;
    return true;
}

static bool optimize_slice(TextBuf *output, const char *input, size_t size,
                           R6Passes passes, void *context) {
    if (!size) return true;
    TextBuf slice = {0};
    bool ok = append(&slice, input, size) && passes(&slice, context) &&
              append(output, slice.p, slice.n);
    text_release(&slice);
    return ok;
}

static void write_cache(const char *path, uint64_t key, const CacheRecord *records,
                        size_t count, const TextBuf *input, const TextBuf *output) {
    uint64_t head[] = {cache_magic, key};
    size_t available = MAX_CACHE_BYTES - sizeof head;
    if (input->n > available || output->n > available - input->n ||
        count > (available - input->n - output->n) / sizeof(CacheHeader))
        return;
    size_t path_len = strlen(path);
    if (path_len >= 4096) return;
    char temporary[4128];
    snprintf(temporary, sizeof temporary, "%s.%ld.tmp", path, (long)cache_pid());
    FILE *file = fopen(temporary, "wb");
    if (!file) return;
    bool ok = fwrite(head, sizeof head, 1, file) == 1;
    for (size_t i = 0; i < count && ok; ++i) {
        const CacheRecord *record = &records[i];
        const CacheHeader *h = &record->header;
        ok = fwrite(h, sizeof *h, 1, file) == 1 &&
             fwrite(input->p + record->input_offset, 1, (size_t)h->input_size, file) == h->input_size &&
             fwrite(output->p + record->output_offset, 1, (size_t)h->output_size, file) == h->output_size;
    }
    ok &= fclose(file) == 0;
    if (ok) {
#ifdef _WIN32
        ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING) != 0;
#else
        ok = rename(temporary, path) == 0;
#endif
    }
    if (!ok) remove(temporary);
}

bool r6_cached_passes(TextBuf *text, const R6Module *module, const R6FunctionSpan *spans,
                      const char *path, const char *version, R6Passes passes,
                      void *context, R6CacheStats *stats) {
    uint64_t version_key;
    if (!spans || !driver_key(version, &version_key)) return passes(text, context);
    CacheRecord *records = calloc(module->function_count, sizeof(*records));
    if (!records) return passes(text, context);

    CacheReader reader = {0};
    char *previous = read_cache(path, version_key, &reader);
    TextBuf output = {0};
    size_t position = 0, count = 0;
    bool ok = true;
    for (size_t i = 0; i < module->function_count && ok; ++i) {
        R6FunctionSpan span = spans[i];
        if (span.begin == span.end) continue;
        ok = optimize_slice(&output, text->p + position, span.begin - position, passes, context);
        if (!ok) break;
        CacheRecord *record = &records[count++];
        record->input_offset = span.begin;
        record->output_offset = output.n;
        record->header.key = function_key(&module->functions[i]);
        record->header.input_size = span.end - span.begin;
        const char *cached = lookup(&reader, record, text->p + span.begin);
        if (cached) {
            stats->hits++;
            ok = append(&output, cached, (size_t)record->header.output_size);
        } else {
            stats->misses++;
            ok = optimize_slice(&output, text->p + span.begin, span.end - span.begin, passes, context);
            if (ok) {
                size_t len = output.n - record->output_offset;
                record->header.output_size = len;
                record->header.output_hash = hash_bytes(hash_start, output.p + record->output_offset, len);
            }
        }
        position = span.end;
    }
    if (ok) ok = optimize_slice(&output, text->p + position, text->n - position, passes, context);
    if (ok) {
        if (stats->misses || reader.remaining)
            write_cache(path, version_key, records, count, text, &output);
        text_release(text);
        *text = output;
    } else {
        text_release(&output);
    }
    free(previous);
    free(records);
    return ok;
}
