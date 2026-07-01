# Build Your Own Database (BYOD)

A disk-backed relational storage engine written in C, initially started as a summer project in the summer of 2025, under Programming Club, IIT Kanpur. BYOD implements the core layers found in production databases: paging, buffer management, B+ tree indexing, and a minimal SQL interface.

## Architecture

```text
SQL shell / REPL
      |
  Table API (insert, delete, lookup, scan)
      |
  +------------------+------------------+
  |                  |                  |
  B+ Tree Index   Slotted Data Pages   WAL (.wal)
      |                  |                  |
      +--------+---------+------------------+
               |
         Buffer Pool (CLOCK, 100 frames)
               |
            Pager (4 KB pages, POSIX I/O)
               |
           Database file
```

## Project layout

```text
byod/
├── include/          Public headers
├── src/              Engine source + REPL entrypoint
├── tests/            Automated test suite
├── bench/            Benchmark harness
├── build/            Object files (generated)
├── bin/              Executables (generated)
├── Makefile
└── README.md
```

| Path | Purpose |
|---|---|
| `include/` | Headers (`pager.h`, `buffer.h`, `btree.h`, `table.h`, `wal.h`, …) |
| `src/` | Core implementation and `main.c` |
| `tests/test_runner.c` | Persistence, WAL, SQL, and index tests |
| `bench/benchmark.c` | Performance measurements |

## Quick start

```bash
make
make test
make bench
./bin/byod mydb.db
```

### Example session

```sql
db > INSERT INTO users VALUES (1, 'alice', 'alice@example.com');
db > SELECT * FROM users WHERE id = 1;
db > .selectall
db > .printtree
db > .stats
db > .exit
```

## Features

| Component | Details |
|---|---|
| **Pager** | 4,096-byte pages, extend-on-write, corruption checks |
| **Buffer pool** | 100 frames (~400 KB), pin counts, dirty tracking, CLOCK eviction |
| **B+ tree** | Leaf/internal splits, primary-key insert/find/delete, ordered scan |
| **Storage** | Slotted pages with bitmap row allocation (14 rows/page) |
| **WAL** | Append-only log with `fsync`; replay on startup; checkpoint on clean shutdown |
| **SQL** | `INSERT`, `SELECT ... WHERE id =`, `DELETE ... WHERE id =` |

## Benchmark results

Measured locally with `make bench` (5,000 rows, 5,000 lookups, `/tmp` filesystem):

| Metric | Result |
|---|---|
| Rows inserted | 5,000 |
| Database pages | 398 |
| Insert throughput | ~450,000 rows/sec |
| Indexed lookup (avg) | **0.6 µs** |
| Full-table scan lookup (avg) | **24 µs** |
| Indexed speedup | **~39×** |
| B+ tree leaf capacity | 255 keys/page |
| Buffer pool | 100 frames / 4 KB pages |

## Makefile targets

| Target | Description |
|---|---|
| `make` | Build `bin/byod`, `bin/benchmark`, and `bin/test_runner` |
| `make test` | Run automated tests |
| `make bench` | Run benchmark and print resume snippets |
| `make run` | Build and start the REPL (pass DB path separately) |
| `make clean` | Remove `build/` and `bin/` |

## Design notes

- **Durability model:** mutations append to the WAL and `fsync` before updating in-memory pages; dirty pages flush on buffer eviction and shutdown.
- **Recovery:** on `db_open`, any records in `<db>.wal` are replayed idempotently, then the log is truncated after a clean `db_close`.
- **Index semantics:** internal-node traversal uses separator-key routing with correct bound handling at equal keys.

## Future work

- Multi-table catalog and schema metadata
- B+ tree merge/redistribution on delete underflow
- Transaction boundaries (`BEGIN` / `COMMIT` / `ROLLBACK`)
- Secondary indexes and a simple query planner
