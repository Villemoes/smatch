#!/bin/bash

stamp() {
    now=$(date '+%F %T')
    echo "$now $1"
}

if echo $1 | grep -q '^-p' ; then
    PROJ=$(echo $1 | cut -d = -f 2)
    shift
fi

info_file=$1

if [[ "$info_file" = "" ]] ; then
    echo "Usage:  $0 -p=<project> <file with smatch messages>"
    exit 1
fi

bin_dir=$(dirname $0)
db_file=smatch_db.sqlite.new

rm -f $db_file

for i in ${bin_dir}/*.schema ; do
    cat $i | sqlite3 $db_file
done

stamp fill_db_sql.sh
${bin_dir}/fill_db_sql.sh $info_file $db_file
stamp fill_db_caller_info.pl
${bin_dir}/fill_db_caller_info.pl "$PROJ" $info_file $db_file
stamp build_early_index.sh
${bin_dir}/build_early_index.sh $db_file

stamp fill_db_type_value.pl
${bin_dir}/fill_db_type_value.pl "$PROJ" $info_file $db_file
stamp fill_db_type_size.pl
${bin_dir}/fill_db_type_size.pl "$PROJ" $info_file $db_file
stamp build_late_index.sh
${bin_dir}/build_late_index.sh $db_file

stamp fixup_all.sh
${bin_dir}/fixup_all.sh $db_file
if [ "$PROJ" != "" ] ; then
    ${bin_dir}/fixup_${PROJ}.sh $db_file
fi

stamp remove_mixed_up_pointer_params.pl
${bin_dir}/remove_mixed_up_pointer_params.pl $db_file
stamp mark_function_ptrs_searchable.pl
${bin_dir}/mark_function_ptrs_searchable.pl $db_file

mv $db_file smatch_db.sqlite

stamp "All done"
