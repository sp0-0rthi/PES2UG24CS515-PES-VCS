// index.c — Staging area implementation
#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

int index_status(const Index *idx) {
    printf("Staged changes:\n");
    if (idx->count == 0) printf("    (nothing to show)\n");
    else for (int i = 0; i < idx->count; i++)
        printf("    new file:   %s\n", idx->entries[i].path);

    printf("\nUnstaged changes:\n    (nothing to show)\n");
    printf("\nUntracked files:\n    (nothing to show)\n");
    return 0;
}

int index_load(Index *idx) {
    idx->count = 0;
    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0;
    char line[1024];
    while (fgets(line, sizeof(line), f) && idx->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &idx->entries[idx->count];
        char hex[HASH_HEX_SIZE + 1];
        if (sscanf(line, "%o %64s %lu %u %511s",
                   &e->mode, hex, &e->mtime_sec, &e->size, e->path) == 5) {
            hex_to_hash(hex, &e->id);
            idx->count++;
        }
    }
    fclose(f);
    return 0;
}
int index_save(const Index *idx) {
    // Allocate on heap instead of stack
    Index *sorted = malloc(sizeof(Index));
    if (!sorted) return -1;
    *sorted = *idx;

    // Sort by path
    for (int i = 0; i < sorted->count - 1; i++)
        for (int j = i+1; j < sorted->count; j++)
            if (strcmp(sorted->entries[i].path, sorted->entries[j].path) > 0) {
                IndexEntry tmp = sorted->entries[i];
                sorted->entries[i] = sorted->entries[j];
                sorted->entries[j] = tmp;
            }

    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", INDEX_FILE);
    FILE *f = fopen(tmp_path, "w");
    if (!f) { free(sorted); return -1; }

    for (int i = 0; i < sorted->count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&sorted->entries[i].id, hex);
        fprintf(f, "%o %s %lu %u %s\n",
                sorted->entries[i].mode, hex,
                sorted->entries[i].mtime_sec,
                sorted->entries[i].size,
                sorted->entries[i].path);
    }
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    rename(tmp_path, INDEX_FILE);
    free(sorted);
    return 0;
}

int index_add(Index *idx, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);
    uint8_t *data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    ObjectID id;
    object_write(OBJ_BLOB, data, size, &id);
    free(data);

    struct stat st;
    stat(path, &st);

    IndexEntry *e = index_find(idx, path);
    if (!e) {
        if (idx->count >= MAX_INDEX_ENTRIES) return -1;
        e = &idx->entries[idx->count++];
        strncpy(e->path, path, 511);
        e->path[511] = '\0';
    }
    e->id = id;
    e->mode = 0100644;  // octal literal
    e->mtime_sec = (uint64_t)st.st_mtime;
    e->size = (uint32_t)size;
    return index_save(idx);
}
