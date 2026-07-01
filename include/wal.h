#ifndef WAL_H
#define WAL_H

#include "dbutils.h"

typedef struct Wal Wal;

Wal *wal_open(const char *db_path);
void wal_close(Wal *wal);
int wal_log_insert(Wal *wal, const Row *row);
int wal_log_delete(Wal *wal, int id);
int wal_replay(Table *table);
void wal_checkpoint(Wal *wal);

#endif
