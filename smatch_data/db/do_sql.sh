#!/bin/bash

sqlite3 -cmd '
PRAGMA synchronous = OFF;
PRAGMA cache_size = 800000;
PRAGMA journal_mode = OFF;
PRAGMA count_changes = OFF;
PRAGMA temp_store = MEMORY;
PRAGMA locking = EXCLUSIVE;' $1
