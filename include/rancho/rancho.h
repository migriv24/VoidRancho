/* rancho.h — Void Rancho's C ABI (okf/design/open-questions.md §5).
 *
 * C++ inside, C at the boundary, as Void Core, Void Palabra and Void Maiz do:
 * so a Python, Rust or C# host can bind Rancho without a C++ compiler of the
 * same make. Small on purpose — the store's reads, its import and export, the
 * snapshot pair and the training export. The write-back path stays C++
 * (WorkingSet needs a CoreLink), until a host on the far side of an FFI asks.
 *
 * Every char* returned is the library's: free it with rancho_free_str.
 * Functions that can fail return 0 / NULL and leave a message for
 * rancho_error(). A handle is not thread-safe; distinct handles are. */
#ifndef VOIDRANCHO_H
#define VOIDRANCHO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RanchoStore RanchoStore;

const char* rancho_version(void);

/* Open (or create) the store at `path` (UTF-8). NULL on failure; the reason
 * is rancho_last_open_error(). */
RanchoStore* rancho_open(const char* path);
const char* rancho_last_open_error(void);
void rancho_close(RanchoStore* s);
/* The last error on this handle, "" if none. Owned by the handle. */
const char* rancho_error(RanchoStore* s);

/* Replace the store with the state document (or .miga v3) in the file at
 * `path`. Returns 1 on success. */
int rancho_import_file(RanchoStore* s, const char* path);
/* The whole state document. Caller frees. */
char* rancho_export_state(RanchoStore* s);
/* JSON array of the names `ls --tag <expr>` would list in `mantle`. Caller frees. */
char* rancho_select(RanchoStore* s, const char* mantle, const char* expr);
/* JSON array of the mantles with their counts. Caller frees. */
char* rancho_mantles(RanchoStore* s);

/* Leiden clustering with leaves of at most `target` runes. Returns 1 on success. */
int rancho_cluster(RanchoStore* s, const char* mantle, unsigned target);

/* Commit the store as the next snapshot in directory `dir`; returns the
 * snapshot's number, 0 on failure. */
long long rancho_snapshot(RanchoStore* s, const char* dir);
/* Replace the store with snapshot `n` of `dir` (0: the current). 1 on success. */
int rancho_restore(RanchoStore* s, const char* dir, long long n);

/* Write the mantle's matrix view as .npy arrays + manifest.json in `dir`. */
int rancho_export_npy(RanchoStore* s, const char* mantle, const char* dir);

void rancho_free_str(char* p);

#ifdef __cplusplus
}
#endif

#endif /* VOIDRANCHO_H */
