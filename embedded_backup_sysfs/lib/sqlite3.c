/**
 * sqlite3.c — SQLite3 合并源码占位文件
 *
 * 实际部署时替换为完整的 SQLite3 amalgamation:
 *   下载: https://www.sqlite.org/amalgamation.html
 *   或从系统包安装: apt install libsqlite3-dev
 *
 * 此占位文件提供最小的 API 桩, 供本地编译测试使用.
 * 注意: 桩函数不执行任何实际的数据库操作.
 */

#include <stddef.h>
#include <stdint.h>

/* ---- 类型占位 ---- */
typedef struct sqlite3 { int dummy; } sqlite3;
typedef int (*sqlite3_callback)(void*, int, char**, char**);

/* ---- API 桩 ---- */
int sqlite3_open(const char *filename, sqlite3 **ppDb) {
    (void)filename;
    *ppDb = (sqlite3*)1; /* non-NULL 哨兵 */
    return 0; /* SQLITE_OK */
}

int sqlite3_close(sqlite3 *db) {
    (void)db;
    return 0;
}

int sqlite3_exec(sqlite3 *db, const char *sql, sqlite3_callback cb,
                 void *arg, char **errmsg) {
    (void)db; (void)sql; (void)cb; (void)arg;
    if (errmsg) *errmsg = NULL;
    return 0;
}

const char *sqlite3_errmsg(sqlite3 *db) {
    (void)db;
    return "stub: no real sqlite3";
}

int sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nByte,
                       void **ppStmt, const char **pzTail) {
    (void)db; (void)zSql; (void)nByte;
    *ppStmt = NULL;
    if (pzTail) *pzTail = NULL;
    return 1; /* SQLITE_ERROR */
}

int sqlite3_step(void *pStmt) {
    (void)pStmt;
    return 101; /* SQLITE_DONE */
}

int sqlite3_finalize(void *pStmt) {
    (void)pStmt;
    return 0;
}

int sqlite3_bind_text(void *pStmt, int idx, const char *val,
                      int len, void (*dtor)(void*)) {
    (void)pStmt; (void)idx; (void)val; (void)len; (void)dtor;
    return 0;
}

int sqlite3_bind_double(void *pStmt, int idx, double val) {
    (void)pStmt; (void)idx; (void)val;
    return 0;
}

int sqlite3_bind_int(void *pStmt, int idx, int val) {
    (void)pStmt; (void)idx; (void)val;
    return 0;
}

const char *sqlite3_column_name(void *pStmt, int N) {
    (void)pStmt; (void)N;
    return "stub";
}

int sqlite3_column_count(void *pStmt) {
    (void)pStmt;
    return 0;
}

int sqlite3_column_type(void *pStmt, int iCol) {
    (void)pStmt; (void)iCol;
    return 5; /* SQLITE_NULL */
}

double sqlite3_column_double(void *pStmt, int iCol) {
    (void)pStmt; (void)iCol;
    return 0.0;
}

int sqlite3_column_int(void *pStmt, int iCol) {
    (void)pStmt; (void)iCol;
    return 0;
}

const unsigned char *sqlite3_column_text(void *pStmt, int iCol) {
    (void)pStmt; (void)iCol;
    return (const unsigned char*)"";
}

void sqlite3_free(void *ptr) {
    (void)ptr;
}
