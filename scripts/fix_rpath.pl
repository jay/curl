#!/usr/bin/env perl

=begin comment
README

This script fixes lt-curl so that the first rpath it checks for dependencies
will be lib/.libs. Without this fix libtool makes that the last rpath checked.

This fix is only necessary if other rpaths were specified during the build.

Bug: https://github.com/curl/curl/issues/432

Copyright (C) 2016 Jay Satiro <raysatiro@yahoo.com>
http://curl.haxx.se/docs/copyright.html

https://gist.github.com/jay/d88d74b6807544387a6c
=end comment
=cut

use 5.014; # needed for /r regex modifier

use strict;
use warnings;

use File::Spec;

defined($ARGV[0]) || die "Usage: fix_rpath.pl <top_builddir>\n";

my $abs_top_builddir = File::Spec->rel2abs($ARGV[0]);
-e $abs_top_builddir || die "Fatal: Builddir not found: $abs_top_builddir";

my $rpath = "$abs_top_builddir/lib/.libs";
my $curl = "$abs_top_builddir/src/curl";

# Make sure $curl is a libtool wrapper and not a static binary
`file "$curl"` =~ /shell script/ || die "Fatal: $curl is not a shell script";

# Read in the curl script, modify its relink_command so our rpath comes first
open(my $fh, "+<", $curl) || die "Fatal: Can't open $curl: $!";
my @lines = map {
  if(/^[ \t]*relink_command=\"/) {
    my $gccopt = "-Wl,-rpath \\\"-Wl,$rpath\\\"";
    s/((?:\(|;) *gcc +)(?!\Q$gccopt\E)/$1$gccopt /gr
  }
  else {
    $_
  }
} <$fh>;

# Write out the modified curl script
seek($fh, 0, 0) || die "Fatal: Seek failed: $!";
if(@lines) {
  print $fh $_ for @lines;
}
truncate($fh, tell($fh)) || die "Fatal: Failed to truncate $curl: $!";
close($fh);

# Remove lt-curl
my $ltcurl = "$abs_top_builddir/src/.libs/lt-curl";
! -e $ltcurl || unlink($ltcurl) || die "Fatal: Removing $ltcurl: $!";

# Build lt-curl (the relink command in src/curl rebuilds it since it's missing)
`"$curl" -V` || die "Fatal: The curl wrapper script failed";

# Confirm lt-curl load path for libcurl.so is our rpath
my $soname = "libcurl.so.4";
my ($soinfo) = grep {/^\t\Q$soname\E => /} `ldd "$ltcurl"`;
defined $soinfo || die "Fatal: Unable to find $soname dependency in $ltcurl";
print $soinfo;
my $expected = "$rpath/$soname";
$soinfo =~ / \Q$expected\E( |$)/ || die "Fatal: $soname path != $expected";

print "Success: lt-curl loads $soname from $rpath\n";
