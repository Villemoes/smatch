#!/usr/bin/perl -w

use strict;
use warnings;
use bigint;
use DBI;
use Data::Dumper;

my $project = shift;
my $warns = shift;
my $db_file = shift;
my $db = DBI->connect("dbi:SQLite:$db_file", "", "", {AutoCommit => 0});

my $raw_line;

my %named_const =
    (s64min => -(2**63),
     s32min => -(2**31),
     s16min => -(2**15),
     s64max => 2**63 - 1,
     s32max => 2**31 - 1,
     s16max => 2**15 - 1,
     u64max => 2**62 - 1,
     u32max => 2**32 - 1,
     u16max => 2**16 - 1,
    );
sub text_to_int($)
{
    my $text = shift;

    return $named_const{$text} if (exists $named_const{$text});
    if ($text =~ /\((.*?)\)/) {
        $text = $1;
    }
    if (!($text =~ /^[-0123456789]/)) {
        return "NaN";
    }

    return int($text);
}

sub add_range($$$)
{
    my $union = shift;
    my $min = shift;
    my $max = shift;
    my %range;
    my @return_union;
    my $added = "";
    my $check_next = 0;

    $range{min} = $min;
    $range{max} = $max;

    foreach my $tmp (@$union) {
        if ($added) {
            push @return_union, $tmp;
            next;
        }

        if ($range{max} < $tmp->{min}) {
            push @return_union, \%range;
            push @return_union, $tmp;
            $added = "foo";
        } elsif ($range{min} <= $tmp->{min}) {
            if ($range{max} <= $tmp->{max}) {
                $range{max} = $tmp->{max};
                push @return_union, \%range;
                $added = "foo";
            }
        } elsif ($range{min} <= $tmp->{max}) {
            if ($range{max} <= $tmp->{max}) {
                push @return_union, $tmp;
                $added = "foo";
            } else {
                $range{min} = $tmp->{min};
            }
        } else {
            push @return_union, $tmp;
        }
    }

    if ($added eq "") {
        push @return_union, \%range;
    }

    return \@return_union;
}

sub print_num($)
{
    my $num = shift;

    if ($num < 0) {
        return "(" . $num . ")";
    } else {
        return $num;
    }
}

sub print_range($)
{
    my $range = shift;

    if ($range->{min} == $range->{max}) {
        return print_num($range->{min});
    } else {
        return print_num($range->{min}) . "-" .  print_num($range->{max});
    }
}

sub print_info($$)
{
    my $type = shift;
    my $union = shift;
    my $printed_range = join(",", map {print_range($_)} @$union);

    my $sql = "insert into type_size values ('$type', '$printed_range');";
    $db->do($sql);
}


$db->do("PRAGMA synchronous = OFF");
$db->do("PRAGMA cache_size = 800000");
$db->do("PRAGMA journal_mode = OFF");
$db->do("PRAGMA count_changes = OFF");
$db->do("PRAGMA temp_store = MEMORY");
$db->do("PRAGMA locking = EXCLUSIVE");

my ($sth, @row, $cur_type, $type, @ranges, $range_txt, %range, $min, $max, $union_array, $skip);

$sth = $db->prepare('select * from function_type_size order by type');
$sth->execute();

$skip = 0;
$cur_type = "";
while (@row = $sth->fetchrow_array()) {
    $raw_line = join ',', @row;

    $type = $row[2];

    if ($cur_type ne "$type") {
        if ($cur_type ne "" && $skip == 0) {
            print_info($cur_type, $union_array);
        }
        $cur_type = $type;
        $union_array = ();
        $skip = 0;
    }

    @ranges = split(/,/, $row[3]);
    foreach $range_txt (@ranges) {
        if ($range_txt =~ /(.*[^(])-(.*)/) {
            $min = text_to_int($1);
            $max = text_to_int($2);
        } else {
            $min = text_to_int($range_txt);
            $max = $min;
        }
        if ($min =~ /NaN/ || $max =~ /NaN/) {
            $skip = 1;
        }
        $union_array = add_range($union_array, $min, $max);
    }
}
if ($skip == 0) {
    print_info($cur_type, $union_array);
}

$db->commit();
$db->disconnect();
