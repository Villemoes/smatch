#!/usr/bin/perl

use strict;
use warnings;

use DBI;

# cat smatch_data/db/*.schema | perl -pe 's/^.*CREATE TABLE //; $c = scalar (split /,/); s/\(.*\);/=> $c,/;'
#
# followed by a little hand editing

my %table_sizes =
    (caller_info => 9,
     call_implies => 8,
     data_info => 4,
     function_ptr => 4,
     function_type_info => 5,
     function_type_size => 4,
     function_type_value => 4,
     local_values => 3,
     return_states => 10,
     type_size => 2,
     type_value => 2,
    );

my %statement_handles;

my $warns = shift;
my $db_file = shift;
my $db = DBI->connect("dbi:SQLite:$db_file", "", "", {AutoCommit => 0});

$db->do("PRAGMA synchronous = OFF");
$db->do("PRAGMA cache_size = 800000");
$db->do("PRAGMA journal_mode = OFF");
$db->do("PRAGMA count_changes = OFF");
$db->do("PRAGMA temp_store = MEMORY");
$db->do("PRAGMA locking = EXCLUSIVE");


foreach (keys %table_sizes) {
    my $sql = sprintf("INSERT INTO %s VALUES (%s)", $_, join(", ", ('?')x$table_sizes{$_}));
    $statement_handles{$_} = $db->prepare($sql);
}

open(my $fh, '<', $warns)
    or die "cannot open $warns: $!";

while (<$fh>) {
    chomp;
    next unless s/^.* SQL:([a-z_]+)\t//;
    my $table = $1;
    if (!exists $table_sizes{$table}) {
	printf STDERR "unknown table name '%s'\n", $table;
	next;
    }
    # specify negative limit to preserve empty trailing fields.
    my @vals = split /\t/, $_, -1;
    if (scalar @vals != $table_sizes{$table}) {
	printf STDERR "expected %d values, got %d (table '%s')\n",
	    $table_sizes{$table}, scalar(@vals), $table;
	next;
    }
    my $sth = $statement_handles{$table};
    $sth->execute(@vals);
}

$db->commit();
$db->disconnect();
