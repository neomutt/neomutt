#!/usr/bin/perl -w
#
# Copyright (C) 2020 Kevin J. McCarthy <kevin@8t8.us>
#
#     This program is free software; you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation; either version 2 of the License, or
#     (at your option) any later version.
#
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with this program; if not, write to the Free Software
#     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

# This file is a rewrite of hcachever.sh.in in perl.
# The rewrite removes the dependency on mutt_md5, in order to
# improve cross-compilation support.

use strict;
use warnings;
# note Digest::MD5 is standard in perl since 5.8.0 (July 18, 2002)
use Digest::MD5;


sub read_line() {
  my $line;

  while (1) {
    $line = <STDIN>;
    return "" if (!$line);

    chomp($line);
    $line =~ s/^\s+//;
    $line =~ s/\s+$//;
    $line =~ s/\s{2,}//g;

    return $line if ($line ne "");
  }
}


sub process_struct($$) {
  my ($line, $md5) = @_;
  my $struct = "";
  my @body;
  my $bodytxt;
  my $inbody = 0;

  return if $line =~ /;$/;
  if ($line =~ /{$/) {
    $inbody = 1;
  }

  while (($line = read_line()) ne "") {
      if (!$inbody) {
      return if $line =~ /;$/;
      if ($line =~ /{$/) {
        $inbody = 1;
      }
    }

    if ($line =~ /^} (.*);$/) {
      $struct = $1;
      last;
    }
    elsif ($line =~ /^}/) {
      $struct = read_line();
      if ($struct ne "") {
        $struct =~ s/;$//;
      }
      last;
    }
    elsif (($line !~ /^#/) && ($line !~ /^{/)) {
      if ($inbody) {
        push @body, $line;
      }
    }
  }

  if ($struct =~ /^(ADDRESS|LIST|BUFFER|PARAMETER|BODY|ENVELOPE|HEADER)$/) {
    $bodytxt = join(" ", @body);
    print " * ${struct}: ${bodytxt}\n";

    $md5->add(" ${struct} {${bodytxt}}");
  }
}


my $md5;
my $line;
my $BASEVERSION = "2";

$md5 = Digest::MD5->new;

$md5->add($BASEVERSION);
print "/* base version: $BASEVERSION\n";

while (($line = read_line()) ne "") {
  if ($line =~ /^typedef struct/) {
    process_struct($line, $md5);
  }
}

$md5->add("\n");
my $digest = substr($md5->hexdigest, 0, 8);

print " */\n";
print "#define HCACHEVER 0x${digest}\n";
