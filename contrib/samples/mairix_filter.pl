#! /usr/bin/env perl

use strict;

use Cwd qw(abs_path);

my $indexer = $ENV{'MAIL_INDEXER'} || 'mairix';
my $folder = shift(@ARGV) || '/';
my $folder_dir = ($folder eq '/' ? '/' abs_path($folder) . '/');

sub starts_with {
  my ($hay, $needle) = @_;
  my $needlen = length($needle);
  return (substr($hay, 0, $needlen) eq $needle);
}

sub snag_msgid {
  open (my $mail, "<$_[0]");
  if (!$mail) {
    warn("failed to open $_[0]: $!");
    return undef;
  }
  my $msgid = undef;
 HEADERS:
  for (<$mail>) {
    chomp; last HEADERS unless $_;
    $msgid = $1, last HEADERS if m(^message-id:(.*))i;
  }
  $mail->close;
  return $msgid;
}

open(my $fh, "$indexer " . join(' ', @ARGV) . '|')
  or die("failed to run $indexer: $!");
LINES:
for (<$fh>) {
  chomp;
  my $realmsg = abs_path($_);
  next LINES unless starts_with($realmsg, $folder_dir);
  my $msgid = snag_msgid($realmsg);
  printf("%s\n", $msgid) if $msgid;
}
my $closed = $fh->close;
die("failed to close pipe from $indexer: $!") unless $closed || $! == 0;
