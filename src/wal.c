#include "wal.h"
#include "table.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define WAL_RECORD_INSERT 1
#define WAL_RECORD_DELETE 2

struct Wal {
  int fd;
  char path[512];
};

static void wal_build_path(const char *db_path, char *wal_path, size_t size) {
  snprintf(wal_path, size, "%s.wal", db_path);
}

Wal *wal_open(const char *db_path) {
  Wal *wal = malloc(sizeof(Wal));
  wal_build_path(db_path, wal->path, sizeof(wal->path));

  wal->fd = open(wal->path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  if (wal->fd == -1) {
    printf("Unable to open WAL file: %s (%d)\n", wal->path, errno);
    free(wal);
    return NULL;
  }

  return wal;
}

void wal_close(Wal *wal) {
  if (wal == NULL) {
    return;
  }

  if (close(wal->fd) == -1) {
    printf("Error closing WAL file.\n");
  }

  free(wal);
}

static int wal_sync(Wal *wal) {
  if (fsync(wal->fd) == -1) {
    printf("Error syncing WAL file: %d\n", errno);
    return -1;
  }

  return 0;
}

static int wal_write_all(Wal *wal, const void *buffer, size_t size) {
  const char *cursor = buffer;
  size_t remaining = size;

  while (remaining > 0) {
    ssize_t written = write(wal->fd, cursor, remaining);
    if (written == -1) {
      printf("Error writing WAL record: %d\n", errno);
      return -1;
    }

    cursor += written;
    remaining -= (size_t)written;
  }

  return wal_sync(wal);
}

int wal_log_insert(Wal *wal, const Row *row) {
  uint8_t record_type = WAL_RECORD_INSERT;
  if (wal_write_all(wal, &record_type, sizeof(record_type)) != 0) {
    return -1;
  }

  return wal_write_all(wal, row, sizeof(Row));
}

int wal_log_delete(Wal *wal, int id) {
  uint8_t record_type = WAL_RECORD_DELETE;
  if (wal_write_all(wal, &record_type, sizeof(record_type)) != 0) {
    return -1;
  }

  return wal_write_all(wal, &id, sizeof(id));
}

int wal_replay(Table *table) {
  Wal *wal = table->wal;
  if (wal == NULL) {
    return 0;
  }

  if (lseek(wal->fd, 0, SEEK_SET) == -1) {
    printf("Error seeking WAL file: %d\n", errno);
    return -1;
  }

  while (true) {
    uint8_t record_type = 0;
    ssize_t bytes_read = read(wal->fd, &record_type, sizeof(record_type));
    if (bytes_read == 0) {
      break;
    }
    if (bytes_read != (ssize_t)sizeof(record_type)) {
      printf("WAL replay failed: truncated record header.\n");
      return -1;
    }

    if (record_type == WAL_RECORD_INSERT) {
      Row row;
      bytes_read = read(wal->fd, &row, sizeof(Row));
      if (bytes_read != (ssize_t)sizeof(Row)) {
        printf("WAL replay failed: truncated insert record.\n");
        return -1;
      }

      if (!table_apply_insert(table, &row)) {
        printf("WAL replay failed while applying insert for id=%d.\n", row.id);
        return -1;
      }
    } else if (record_type == WAL_RECORD_DELETE) {
      int id = 0;
      bytes_read = read(wal->fd, &id, sizeof(id));
      if (bytes_read != (ssize_t)sizeof(id)) {
        printf("WAL replay failed: truncated delete record.\n");
        return -1;
      }

      table_apply_delete(table, id);
    } else {
      printf("WAL replay failed: unknown record type %u.\n", record_type);
      return -1;
    }
  }

  return 0;
}

void wal_checkpoint(Wal *wal) {
  if (wal == NULL) {
    return;
  }

  if (ftruncate(wal->fd, 0) == -1) {
    printf("Error truncating WAL file: %d\n", errno);
    return;
  }

  if (lseek(wal->fd, 0, SEEK_SET) == -1) {
    printf("Error rewinding WAL file: %d\n", errno);
  }
}
