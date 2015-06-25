#!/usr/bin/perl -w

use strict;
use DBI;
use Scalar::Util qw(looks_like_number);

sub usage()
{
    print "usage:  $0 <-p=project> <warns.txt> <db_file>\n";
    exit(1);
}

my %too_common_funcs;
sub get_too_common_functions($$$)
{
    my $path = shift;
    my $project = shift;
    my $warns = shift;
    my %count;

    open(FUNCS, '<', $warns);
    while (<FUNCS>) {
	next unless s/^.*SQL_caller_info:\t//;
	next unless m/%call_marker%/;
	my (undef, undef, $callee, undef) = split /\t/;
	$count{$callee}++;
    }
    close(FUNCS);

    for (keys %count) {
	$too_common_funcs{$_} = 1 if $count{$_} > 200;
    }

    open(FILE, "$path/../$project.common_functions");
    while (<FILE>) {
        s/\n//;
        $too_common_funcs{$_} = 1;
    }
    close(FILE);
}

my $exec_name = $0;
my $path = $exec_name;
$path =~ s/(.*)\/.*/$1/;
my $project = shift;
my $warns = shift;
my $db_file = shift;

if (!defined($db_file)) {
    usage();
}

get_too_common_functions($path, $project, $warns);

my $db = DBI->connect("dbi:SQLite:$db_file", "", "", {AutoCommit => 0});
$db->do("PRAGMA synchronous = OFF");
$db->do("PRAGMA cache_size = 800000");
$db->do("PRAGMA journal_mode = OFF");
$db->do("PRAGMA count_changes = OFF");
$db->do("PRAGMA temp_store = MEMORY");
$db->do("PRAGMA locking = EXCLUSIVE");

my $sth = $db->prepare("insert into caller_info values ('unknown', 'too common', ?, 0, 0, 0, -1, '', '')");
foreach my $func (keys %too_common_funcs) {
    $sth->execute($func);
}

my $call_id = 0;
my ($fn, $dummy, $sql);
my $total = 0;


# file, caller, callee, CALL_ID, static, type, param, key, value
$sth = $db->prepare("insert into caller_info values (?, ?, ?, ?, ?, ?, ?, ?, ?)");

open(WARNS, "<$warns");
while (<WARNS>) {
    chomp;

    next unless s/^.*SQL_caller_info:\t//;
    my ($file, $caller, $callee, $static, $type, $param, $key, $value) = split /\t/;

    next if $callee =~ /^__builtin_/;

    next if $callee =~ /^(printk|memset|memcpy|kfree|printf|dev_err|writel)$/;

    next if exists $too_common_funcs{$callee};


    # file, caller, callee, CALL_ID, static, type, param, key, value

    my $inc = 0;
    if ($key eq '%call_marker%') {
	$key = '';
	$inc = 1;
    }

    $sth->execute($file, $caller, $callee, $call_id, $static, $type, $param, $key, $value);

    $call_id += $inc;
}
$db->commit();
$db->disconnect();
