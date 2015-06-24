#!/bin/bash

bin_dir=$(dirname $0)
info_file=$1
db_file=$2

grep '\(\) SQL: ' $info_file | cut -f3 -d: | ${bin_dir}/do_sql.sh $db_file
