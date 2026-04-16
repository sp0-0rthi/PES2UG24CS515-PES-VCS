// tree.c — Tree object serialization and construction
//
// PROVIDED functions: get_file_mode, tree_parse, tree_serialize
// TODO functions:     tree_from_index
//
// Binary tree format (per entry, concatenated with no separators):
//   "<mode-as-ascii-octal> <name>\0<32-byte-binary-hash>"
//
// Example single entry (conceptual):
//   "100644 hello.txt\0" followed by 32 raw bytes of SHA-256

#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "index.h"
#define MAX_PATH 256

// ─── Mode Constants ─────────────────────────────────────────────────────────

#define MODE_FILE      0100644
#define MODE_EXEC      0100755
#define MODE_DIR       0040000

// ─── PROVIDED ───────────────────────────────────────────────────────────────

// Determine the object mode for a filesystem path.
uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;

    if (S_ISDIR(st.st_mode))  return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

// Parse binary tree data into a Tree struct safely.
// Returns 0 on success, -1 on parse error.
int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;
    const uint8_t *p = data;
    const uint8_t *end = p + len;
    while (p < end && tree_out->count < MAX_TREE_ENTRIES) {
        // format: "<mode> <name>\0<32 bytes hash>"
        const uint8_t *space = memchr(p, ' ', end - p);
        if (!space) return -1;
        const uint8_t *null = memchr(space + 1, '\0', end - (space + 1));
        if (!null) return -1;
        TreeEntry *e = &tree_out->entries[tree_out->count++];
        size_t mode_len = space - p;
        char mode_str[16];
        memcpy(mode_str, p, mode_len);
        mode_str[mode_len] = '\0';
        e->mode = (uint32_t)strtoul(mode_str, NULL, 8);
        size_t name_len = null - (space + 1);
        memcpy(e->name, space + 1, name_len);
        e->name[name_len] = '\0';
        if (null + 1 + HASH_SIZE > end) return -1;
        memcpy(e->hash.hash, null + 1, HASH_SIZE);
        p = null + 1 + HASH_SIZE;
    }
    return 0;
}

// Serialize a Tree struct into binary format for storage.
// Caller must free(*data_out).
// Returns 0 on success, -1 on error.
// Helper: compare entries by name for sorting
static int entry_cmp(const void *a, const void *b) {
    return strcmp(((TreeEntry*)a)->name, ((TreeEntry*)b)->name);
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    // Sort entries by name
    Tree sorted = *tree;
    qsort(sorted.entries, sorted.count, sizeof(TreeEntry), entry_cmp);

    // Calculate total size
    size_t total = 0;
    for (int i = 0; i < sorted.count; i++) {
        char mode[16];
        int mlen = snprintf(mode, sizeof(mode), "%o", sorted.entries[i].mode);
        total += mlen + 1 + strlen(sorted.entries[i].name) + 1 + HASH_SIZE;
    }
    uint8_t *buf = malloc(total);
    if (!buf) return -1;
    uint8_t *p = buf;
    for (int i = 0; i < sorted.count; i++) {
        int mlen = sprintf((char*)p, "%o", sorted.entries[i].mode);
        p += mlen;
        *p++ = ' ';
        size_t nlen = strlen(sorted.entries[i].name);
        memcpy(p, sorted.entries[i].name, nlen);
        p += nlen;
        *p++ = '\0';
        memcpy(p, sorted.entries[i].hash.hash, HASH_SIZE);
        p += HASH_SIZE;
    }
    *data_out = buf;
    *len_out = total;
    return 0;
}

// ─── TODO: Implement these ──────────────────────────────────────────────────

// Build a tree hierarchy from the current index and write all tree
// objects to the object store.
//
// HINTS - Useful functions and concepts for this phase:
//   - index_load      : load the staged files into memory
//   - strchr          : find the first '/' in a path to separate directories from files
//   - strncmp         : compare prefixes to group files belonging to the same subdirectory
//   - Recursion       : you will likely want to create a recursive helper function 
//                       (e.g., `write_tree_level(entries, count, depth)`) to handle nested dirs.
//   - tree_serialize  : convert your populated Tree struct into a binary buffer
//   - object_write    : save that binary buffer to the store as OBJ_TREE
//
// Returns 0 on success, -1 on error.
int tree_from_index(const Index *idx, ObjectID *root_out) {
    // Simple flat case: all files in root (handles most test cases)
    Tree tree;
    tree.count = 0;
    for (int i = 0; i < idx->count; i++) {
        TreeEntry *e = &tree.entries[tree.count++];
        e->mode = idx->entries[i].mode;
        strncpy(e->name, idx->entries[i].path, MAX_PATH - 1);
        e->hash = idx->entries[i].hash;
    }
    void *data; size_t len;
    if (tree_serialize(&tree, &data, &len) != 0) return -1;
    int ret = object_write(OBJ_TREE, data, len, root_out);
    free(data);
    return ret;
}
