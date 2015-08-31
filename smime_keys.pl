#! /usr/bin/perl -w

# Copyright (C) 2001,2002 Oliver Ehli <elmy@acm.org>
# Copyright (C) 2001 Mike Schiraldi <raldi@research.netsol.com>
# Copyright (C) 2003 Bjoern Jacke <bjoern@j3e.de>
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

use strict;
use File::Copy;
use File::Glob ':glob';
use File::Temp qw(tempfile tempdir);

umask 077;

use Time::Local;

# helper routines
sub usage ();
sub mutt_Q ($);
sub mycopy ($$);
sub query_label ();
sub mkdir_recursive ($);
sub verify_files_exist (@);
sub create_tempfile (;$);
sub new_cert_structure ();
sub create_cert_chains (@);

# openssl helpers
sub openssl_exec (@);
sub openssl_format ($);
sub openssl_x509_query ($@);
sub openssl_hash ($);
sub openssl_fingerprint ($);
sub openssl_emails ($);
sub openssl_p12_to_pem ($$);
sub openssl_verify ($$);
sub openssl_crl_text($);
sub openssl_trust_flag ($$;$);
sub openssl_parse_pem ($$);
sub openssl_dump_cert ($);
sub openssl_purpose_flag ($);

# key/certificate management methods
sub cm_list_certs ();
sub cm_add_entry ($$$$$$;$);
sub cm_add_cert ($);
sub cm_add_indexed_cert ($$$);
sub cm_add_key ($$$$$$);
sub cm_modify_entry ($$$;$);
sub cm_find_entry ($$);
sub cm_refresh_index ();

# op handlers
sub handle_init_paths ();
sub handle_change_label ($);
sub handle_add_cert ($);
sub handle_add_pem ($);
sub handle_add_p12 ($);
sub handle_add_chain ($$$);
sub handle_verify_cert($$);
sub handle_remove_pair ($);
sub handle_add_root_cert ($);


my $mutt = $ENV{MUTT_CMDLINE} || 'mutt';
my $opensslbin = "/usr/bin/openssl";
my $tmpdir;

# Get the directories mutt uses for certificate/key storage.

my $private_keys_path = mutt_Q 'smime_keys';
die "smime_keys is not set in mutt's configuration file"
   if length $private_keys_path == 0;

my $certificates_path = mutt_Q 'smime_certificates';
die "smime_certificates is not set in mutt's configuration file"
   if length $certificates_path == 0;

my $root_certs_path   = mutt_Q 'smime_ca_location';
die "smime_ca_location is not set in mutt's configuration file"
   if length $root_certs_path == 0;

my $root_certs_switch;
if ( -d $root_certs_path) {
  $root_certs_switch = -CApath;
} else {
  $root_certs_switch = -CAfile;
}


######
# OPS
######

if (@ARGV == 1 and $ARGV[0] eq "init") {
  handle_init_paths();
}
elsif (@ARGV == 1 and $ARGV[0] eq "refresh") {
  cm_refresh_index();
}
elsif (@ARGV == 1 and $ARGV[0] eq "list") {
  cm_list_certs();
}
elsif (@ARGV == 2 and $ARGV[0] eq "label") {
  handle_change_label($ARGV[1]);
}
elsif (@ARGV == 2 and $ARGV[0] eq "add_cert") {
  verify_files_exist($ARGV[1]);
  handle_add_cert($ARGV[1]);
}
elsif (@ARGV == 2 and $ARGV[0] eq "add_pem") {
  verify_files_exist($ARGV[1]);
  handle_add_pem($ARGV[1]);
}
elsif ( @ARGV == 2 and $ARGV[0] eq "add_p12") {
  verify_files_exist($ARGV[1]);
  handle_add_p12($ARGV[1]);
}
elsif (@ARGV == 4 and $ARGV[0] eq "add_chain") {
  verify_files_exist($ARGV[1], $ARGV[2], $ARGV[3]);
  handle_add_chain($ARGV[1], $ARGV[2], $ARGV[3]);
}
elsif ((@ARGV == 2 or @ARGV == 3) and $ARGV[0] eq "verify") {
  verify_files_exist($ARGV[2]) if (@ARGV == 3);
  handle_verify_cert($ARGV[1], $ARGV[2]);
}
elsif (@ARGV == 2 and $ARGV[0] eq "remove") {
  handle_remove_pair($ARGV[1]);
}
elsif (@ARGV == 2 and $ARGV[0] eq "add_root") {
  verify_files_exist($ARGV[1]);
  handle_add_root_cert($ARGV[1]);
}
else {
  usage();
  exit(1);
}

exit(0);


##############  sub-routines  ########################


###################
#  helper routines
###################

sub usage () {
    print <<EOF;

Usage: smime_keys <operation>  [file(s) | keyID [file(s)]]

        with operation being one of:

        init      : no files needed, inits directory structure.
        refresh   : refreshes certificate and key index files.
                    Updates trust flag (expiration).
                    Adds purpose flag if missing.

        list      : lists the certificates stored in database.
        label     : keyID required. changes/removes/adds label.
        remove    : keyID required.
        verify    : 1=keyID and optionally 2=CRL
                    Verifies the certificate chain, and optionally wether
                    this certificate is included in supplied CRL (PEM format).
                    Note: to verify all certificates at the same time,
                    replace keyID with "all"

        add_cert  : certificate required.
        add_chain : three files reqd: 1=Key, 2=certificate
                    plus 3=intermediate certificate(s).
        add_p12   : one file reqd. Adds keypair to database.
                    file is PKCS12 (e.g. export from netscape).
        add_pem   : one file reqd. Adds keypair to database.
                    (file was converted from e.g. PKCS12).

        add_root  : one file reqd. Adds PEM root certificate to the location
                    specified within muttrc (smime_verify_* command)

EOF
}

sub mutt_Q ($) {
  my ($var) = @_;

  my $cmd = "$mutt -v >/dev/null 2>/dev/null";
  system ($cmd) == 0 or die<<EOF;
Couldn't launch mutt. I attempted to do so by running the command "$mutt".
If that's not the right command, you can override it by setting the
environment variable \$MUTT_CMDLINE
EOF

  $cmd = "$mutt -Q $var 2>/dev/null";
  my $answer = `$cmd`;

  $? and die<<EOF;
Couldn't look up the value of the mutt variable "$var".
You must set this in your mutt config file. See contrib/smime.rc for an example.
EOF

  $answer =~ /\"(.*?)\"/ and return bsd_glob($1, GLOB_TILDE | GLOB_NOCHECK);

  $answer =~ /^Mutt (.*?) / and die<<EOF;
This script requires mutt 1.5.0 or later. You are using mutt $1.
EOF

  die "Value of $var is weird\n";
}

sub mycopy ($$) {
  my ($source, $dest) = @_;

  copy $source, $dest or die "Problem copying $source to $dest: $!\n";
}

sub query_label () {
  my $input;
  my $label;
  my $junk;

  print "\nYou may assign a label to this key, so you don't have to remember\n";
  print "the key ID. This has to be _one_ word (no whitespaces).\n\n";

  print "Enter label: ";
  $input = <STDIN>;

  if (defined($input) && ($input !~ /^\s*$/)) {
    chomp($input);
    $input =~ s/^\s+//;
    ($label, $junk) = split(/\s/, $input, 2);

    if (defined($junk)) {
      print "\nUsing '$label' as label; ignoring '$junk'\n";
    }
  }

  if ((! defined($label)) || ($label =~ /^\s*$/)) {
    $label =  "-";
  }

  return $label;
}

sub mkdir_recursive ($) {
  my ($path) = @_;
  my $tmp_path;

  for my $dir (split /\//, $path) {
    $tmp_path .= "$dir/";

    -d $tmp_path
      or mkdir $tmp_path, 0700
        or die "Can't mkdir $tmp_path: $!";
  }
}

sub verify_files_exist (@) {
  my (@files) = @_;

  foreach my $file (@files) {
    if ((! -e $file) || (! -s $file)) {
      die("$file is nonexistent or empty.");
    }
  }
}

# Returns a list ($fh, $filename)
sub create_tempfile (;$) {
  my ($directory) = @_;

  if (! defined($directory)) {
    if (! defined($tmpdir)) {
      $tmpdir = tempdir(CLEANUP => 1);
    }
    $directory = $tmpdir;
  }

  return tempfile(DIR => $directory);
}

# Creates a cert data structure used by openssl_parse_pem
sub new_cert_structure () {
  my $cert_data = {};

  $cert_data->{datafile} = "";
  $cert_data->{type} = "";
  $cert_data->{localKeyID} = "";
  $cert_data->{subject} = "";
  $cert_data->{issuer} = "";

  return $cert_data;
}

sub create_cert_chains (@) {
  my (@certs) = @_;

  my (%subject_hash, @leaves, @chains);

  foreach my $cert (@certs) {
    $cert->{children} = 0;
    if ($cert->{subject}) {
      $subject_hash{$cert->{subject}} = $cert;
    }
  }

  foreach my $cert (@certs) {
    my $parent = $subject_hash{$cert->{issuer}};
    if (defined($parent)) {
      $parent->{children} += 1;
    }
  }

  @leaves = grep { $_->{children} == 0 } @certs;
  foreach my $leaf (@leaves) {
    my $chain = [];
    my $cert = $leaf;

    while (defined($cert)) {
      push @$chain, $cert;

      $cert = $subject_hash{$cert->{issuer}};
      if (defined($cert) &&
          (scalar(grep {$_ == $cert} @$chain) != 0)) {
        $cert = undef;
      }
    }

    push @chains, $chain;
  }

  return @chains;
}


##################
# openssl helpers
##################

sub openssl_exec (@) {
  my (@args) = @_;

  my $fh;

  open($fh, "-|", $opensslbin, @args)
    or die "Failed to run '$opensslbin @args': $!";
  my @output = <$fh>;
  if (! close($fh)) {
    # NOTE: Callers should check the value of $? for the exit status.
    if ($!) {
      die "Syserr closing '$opensslbin @args' pipe: $!";
    }
  }

  return @output;
}

sub openssl_format ($) {
  my ($filename) = @_;

  return -B $filename ? 'DER' : 'PEM';
}

sub openssl_x509_query ($@) {
  my ($filename, @query) = @_;

  my $format = openssl_format($filename);
  my @args = ("x509", "-in", $filename, "-inform", $format, "-noout", @query);
  return openssl_exec(@args);
}

sub openssl_hash ($) {
  my ($filename) = @_;

  my $cert_hash = join("", openssl_x509_query($filename, "-hash"));
  $? and die "openssl -hash '$filename' returned $?";

  chomp($cert_hash);
  return $cert_hash;
}

sub openssl_fingerprint ($) {
  my ($filename) = @_;

  my $fingerprint = join("", openssl_x509_query($filename, "-fingerprint"));
  $? and die "openssl -fingerprint '$filename' returned $?";

  chomp($fingerprint);
  return $fingerprint;
}

sub openssl_emails ($) {
  my ($filename) = @_;

  my @mailboxes = openssl_x509_query($filename, "-email");
  $? and die "openssl -email '$filename' returned $?";

  chomp(@mailboxes);
  return @mailboxes;
}

sub openssl_p12_to_pem ($$) {
  my ($p12_file, $pem_file) = @_;

  my @args = ("pkcs12", "-in", $p12_file, "-out", $pem_file);
  openssl_exec(@args);
  $? and die "openssl pkcs12 conversion returned $?";
}

sub openssl_verify ($$) {
  my ($issuer_path, $cert_path) = @_;

  my @args = ("verify", $root_certs_switch, $root_certs_path,
              "-purpose", "smimesign", "-purpose", "smimeencrypt", "-untrusted",
              $issuer_path, $cert_path);
  my $output = join("", openssl_exec(@args));

  chomp($output);
  return $output;
}

sub openssl_crl_text($) {
  my ($crl) = @_;

  my @args = ("crl", "-text", "-noout", "-in", $crl);
  my @output = openssl_exec(@args);
  $? and die "openssl crl -text '$crl' returned $?";

  return @output;
}

sub openssl_trust_flag ($$;$) {
  my ($cert, $issuerid, $crl) = @_;

  print "==> about to verify certificate of $cert\n";

  my $result = 't';
  my $issuer_path;
  my $cert_path = "$certificates_path/$cert";

  if ($issuerid eq '?') {
    $issuer_path = "$certificates_path/$cert";
  } else {
    $issuer_path = "$certificates_path/$issuerid";
  }

  my $output = openssl_verify($issuer_path, $cert_path);
  if ($?) {
    print "openssl verify returned exit code " . ($? >> 8) . " with output:\n";
    print "$output\n\n";
    print "Marking certificate as invalid\n";
    return 'i';
  }
  print "\n$output\n";

  if ($output !~ /OK/) {
    return 'i';
  }

  my ($not_before, $not_after, $serial_in) = openssl_x509_query($cert_path, "-dates", "-serial");
  $? and die "openssl -dates -serial '$cert_path' returned $?";

  if ( defined $not_before and defined $not_after ) {
    my %months = ('Jan', '00', 'Feb', '01', 'Mar', '02', 'Apr', '03',
                  'May', '04', 'Jun', '05', 'Jul', '06', 'Aug', '07',
                  'Sep', '08', 'Oct', '09', 'Nov', '10', 'Dec', '11');

    my @tmp = split (/\=/, $not_before);
    my $not_before_date = $tmp[1];
    my @fields =
      $not_before_date =~ /(\w+)\s*(\d+)\s*(\d+):(\d+):(\d+)\s*(\d+)\s*GMT/;
    if ($#fields == 5) {
      if (timegm($fields[4], $fields[3], $fields[2], $fields[1],
                 $months{$fields[0]}, $fields[5]) > time) {
        print "Certificate is not yet valid.\n";
        return 'e';
      }
    } else {
      print "Expiration Date: Parse Error :  $not_before_date\n\n";
    }

    @tmp = split (/\=/, $not_after);
    my $not_after_date = $tmp[1];
    @fields =
      $not_after_date =~ /(\w+)\s*(\d+)\s*(\d+):(\d+):(\d+)\s*(\d+)\s*GMT/;
    if ($#fields == 5) {
      if (timegm($fields[4], $fields[3], $fields[2], $fields[1],
                 $months{$fields[0]}, $fields[5]) < time) {
        print "Certificate has expired.\n";
        return 'e';
      }
    } else {
      print "Expiration Date: Parse Error :  $not_after_date\n\n";
    }
  }

  if ( defined $crl ) {
    chomp($serial_in);
    my @serial = split (/\=/, $serial_in);
    my $match_line = undef;
    my @crl_lines = openssl_crl_text($crl);
    for (my $index = 0; $index <= $#crl_lines; $index++) {
      if ($crl_lines[$index] =~ /Serial Number:\s*\Q$serial[1]\E\b/) {
        $match_line = $crl_lines[$index + 1];
        last;
      }
    }

    if ( defined $match_line ) {
      my @revoke_date = split (/:\s/, $match_line);
      print "FAILURE: Certificate $cert has been revoked on $revoke_date[1]\n";
      $result = 'r';
    }
  }
  print "\n";

  return $result;
}

sub openssl_parse_pem ($$) {
  my ($filename, $attrs_required) = @_;

  my $state = 0;
  my $cert_data;
  my @certs;
  my $cert_count = 0;
  my $bag_count = 0;
  my $cert_tmp_fh;
  my $cert_tmp_filename;

  $cert_data = new_cert_structure();
  ($cert_tmp_fh, $cert_data->{datafile}) = create_tempfile();

  open(PEM_FILE, "<$filename") or die("Can't open $filename: $!");
  while (<PEM_FILE>) {
    if (/^Bag Attributes/) {
      $bag_count++;
      $state == 0 or  die("PEM-parse error at: $.");
      $state = 1;
    }

    # Allow attributes without the "Bag Attributes" header
    if ($state != 2) {
      if (/localKeyID:\s*(.*)/) {
        $cert_data->{localKeyID} = $1;
      }

      if (/subject=\s*(.*)/) {
        $cert_data->{subject} = $1;
      }

      if (/issuer=\s*(.*)/) {
        $cert_data->{issuer} = $1;
      }
    }


    if (/^-----/) {
      if (/BEGIN/) {
        print $cert_tmp_fh $_;
        $state = 2;

        if (/PRIVATE/) {
            $cert_data->{type} = "K";
            next;
        }
        if (/CERTIFICATE/) {
            $cert_data->{type} = "C";
            next;
        }
        die("What's this: $_");
      }
      if (/END/) {
        $state = 0;
        print $cert_tmp_fh $_;
        close($cert_tmp_fh);

        $cert_count++;
        push (@certs, $cert_data);

        $cert_data = new_cert_structure();
        ($cert_tmp_fh, $cert_data->{datafile}) = create_tempfile();
        next;
      }
    }
    print $cert_tmp_fh $_;
  }
  close($cert_tmp_fh);
  close(PEM_FILE);

  if ($attrs_required && ($bag_count != $cert_count)) {
    die("Not all contents were bagged. can't continue.");
  }

  return @certs;
}

sub openssl_dump_cert ($) {
  my ($filename) = @_;

  my $format = openssl_format($filename);
  my @args = ("x509", "-in", $filename, "-inform", $format);
  my $output = join("", openssl_exec(@args));
  $? and die "openssl x509 certicate dump returned $?";

  return $output;
}

sub openssl_purpose_flag ($) {
  my ($filename) = @_;

  my $purpose = "";

  my @output = openssl_x509_query($filename, "-purpose");
  $? and die "openssl -purpose '$filename' returned $?";

  foreach my $line (@output) {
    if ($line =~ /^S\/MIME signing\s*:\s*Yes/) {
      $purpose .= "s";
    }
    elsif ($line =~ /^S\/MIME encryption\s*:\s*Yes/) {
      $purpose .= "e";
    }
  }

  if (! $purpose) {
    $purpose = "-";
  }

  return $purpose;
}


#################################
# certificate management methods
#################################

sub cm_list_certs () {
  my %keyflags = ( 'i', '(Invalid)',  'r', '(Revoked)', 'e', '(Expired)',
                   'u', '(Unverified)', 'v', '(Valid)', 't', '(Trusted)');

  open(INDEX, "<$certificates_path/.index") or
    die "Couldn't open $certificates_path/.index: $!";

  print "\n";
  while (<INDEX>) {
    my $tmp;
    my @tmp;
    my $tab = "            ";
    my @fields = split;

    if ($fields[2] eq '-') {
      print "$fields[1]: Issued for: $fields[0] $keyflags{$fields[4]}\n";
    } else {
      print "$fields[1]: Issued for: $fields[0] \"$fields[2]\" $keyflags{$fields[4]}\n";
    }

    my $certfile = "$certificates_path/$fields[1]";
    my $cert;
    {
        open F, $certfile or
            die "Couldn't open $certfile: $!";
        local $/;
        $cert = <F>;
        close F;
    }

    my ($subject_in, $issuer_in, $date1_in, $date2_in) =
      openssl_x509_query($certfile, "-subject", "-issuer", "-dates");
    $? and print "ERROR: openssl -subject -issuer -dates '$certfile' returned $?\n\n" and next;


    my @subject = split(/\//, $subject_in);
    while (@subject) {
      $tmp = shift @subject;
      ($tmp =~ /^CN\=/) and last;
      undef $tmp;
    }
    defined $tmp and @tmp = split (/\=/, $tmp) and
      print $tab."Subject: $tmp[1]\n";

    my @issuer = split(/\//, $issuer_in);
    while (@issuer) {
      $tmp = shift @issuer;
      ($tmp =~ /^CN\=/) and last;
      undef $tmp;
    }
    defined $tmp and @tmp = split (/\=/, $tmp) and
      print $tab."Issued by: $tmp[1]";

    if ( defined $date1_in and defined $date2_in ) {
      @tmp = split (/\=/, $date1_in);
      $tmp = $tmp[1];
      @tmp = split (/\=/, $date2_in);
      print $tab."Certificate is not valid before $tmp".
        $tab."                      or after  ".$tmp[1];
    }

    -e "$private_keys_path/$fields[1]" and
      print "$tab - Matching private key installed -\n";

    my @purpose = openssl_x509_query($certfile, "-purpose");
    $? and die "openssl -purpose '$certfile' returned $?";
    chomp(@purpose);

    print "$tab$purpose[0] (displays S/MIME options only)\n";
    while (@purpose) {
      $tmp = shift @purpose;
      ($tmp =~ /^S\/MIME/ and $tmp =~ /Yes/) or next;
      my @tmptmp = split (/:/, $tmp);
      print "$tab  $tmptmp[0]\n";
    }

    print "\n";
  }

  close(INDEX);
}

sub cm_add_entry ($$$$$$;$) {
  my ($mailbox, $hashvalue, $use_cert, $label, $trust, $purpose, $issuer_hash) = @_;

  if (! defined($issuer_hash) ) {
    $issuer_hash = "?";
  }

  if ($use_cert) {
    open(INDEX, "+<$certificates_path/.index") or
        die "Couldn't open $certificates_path/.index: $!";
  }
  else {
    open(INDEX, "+<$private_keys_path/.index") or
        die "Couldn't open $private_keys_path/.index: $!";
  }

  while (<INDEX>) {
    my @fields = split;
    if (($fields[0] eq $mailbox) && ($fields[1] eq $hashvalue)) {
      close(INDEX);
      return;
    }
  }

  print INDEX "$mailbox $hashvalue $label $issuer_hash $trust $purpose\n";

  close(INDEX);
}

# Returns the hashvalue.index of the stored cert
sub cm_add_cert ($) {
  my ($filename) = @_;

  my $iter = 0;
  my $hashvalue = openssl_hash($filename);
  my $fp1 = openssl_fingerprint($filename);

  while (-e "$certificates_path/$hashvalue.$iter") {
    my $fp2 = openssl_fingerprint("$certificates_path/$hashvalue.$iter");

    last if $fp1 eq $fp2;
    $iter++;
  }
  $hashvalue .= ".$iter";

  if (-e "$certificates_path/$hashvalue") {
    print "\nCertificate: $certificates_path/$hashvalue already installed.\n";
  }
  else {
    mycopy $filename, "$certificates_path/$hashvalue";
  }

  return $hashvalue;
}

# Returns a reference containing the hashvalue, mailboxes, trust flag, and purpose
# flag of the stored cert.
sub cm_add_indexed_cert ($$$) {
  my ($filename, $label, $issuer_hash) = @_;

  my $cert_data = {};

  $cert_data->{hashvalue} = cm_add_cert($filename);
  $cert_data->{mailboxes} = [ openssl_emails($filename) ];
  $cert_data->{trust} = openssl_trust_flag($cert_data->{hashvalue}, $issuer_hash);
  $cert_data->{purpose} = openssl_purpose_flag($filename);

  foreach my $mailbox (@{$cert_data->{mailboxes}}) {
    cm_add_entry($mailbox, $cert_data->{hashvalue}, 1, $label,
                 $cert_data->{trust}, $cert_data->{purpose}, $issuer_hash);
    print "\ncertificate ", $cert_data->{hashvalue}, " ($label) for $mailbox added.\n";
  }

  return $cert_data;
}

sub cm_add_key ($$$$$$) {
    my ($file, $hashvalue, $mailbox, $label, $trust, $purpose) = @_;

    unless (-e "$private_keys_path/$hashvalue") {
        mycopy $file, "$private_keys_path/$hashvalue";
    }

    cm_add_entry($mailbox, $hashvalue, 0, $label, $trust, $purpose);
    print "added private key: " .
      "$private_keys_path/$hashvalue for $mailbox\n";
}

sub cm_modify_entry ($$$;$) {
  my ($op, $hashvalue, $use_cert, $opt_param) = @_;

  my $label;
  my $trust;
  my $purpose;
  my $path;
  my @fields;

  $op eq 'L' and ($label = $opt_param);
  $op eq 'T' and ($trust = $opt_param);
  $op eq 'P' and ($purpose = $opt_param);

  if ($use_cert) {
    $path = $certificates_path;
  }
  else {
    $path = $private_keys_path;
  }

  open(INDEX, "<$path/.index") or
    die "Couldn't open $path/.index: $!";
  my ($newindex_fh, $newindex) = create_tempfile();

  while (<INDEX>) {
    chomp;

    # fields: mailbox hash label issuer_hash trust purpose
    @fields = split;

    if ($fields[1] eq $hashvalue or $hashvalue eq 'all') {
      $op eq 'R' and next;

      if ($op eq 'L') {
        $fields[2] = $label;
      }

      if ($op eq 'T') {
        $fields[3] = "?" if ($#fields < 3);
        $fields[4] = $trust;
      }

      if ($op eq 'P') {
        $fields[3] = "?" if ($#fields < 3);
        $fields[4] = "u" if ($#fields < 4);
        $fields[5] = $purpose;
      }

      print $newindex_fh join(" ", @fields), "\n";
    }
    else {
      print $newindex_fh $_, "\n";
    }
  }
  close(INDEX);
  close($newindex_fh);

  move $newindex, "$path/.index"
      or die "Couldn't move $newindex to $path/.index: $!\n";
}

# This returns the first matching entry.
sub cm_find_entry ($$) {
  my ($hashvalue, $use_cert) = @_;

  my ($path, $index_fh);

  if ($use_cert) {
    $path = $certificates_path;
  }
  else {
    $path = $private_keys_path;
  }

  open($index_fh, "<$path/.index") or
    die "Couldn't open $path/.index: $!";

  while (<$index_fh>) {
    chomp;
    my @fields = split;
    if ($fields[1] eq $hashvalue) {
      close($index_fh);
      return @fields;
    }
  }

  close($index_fh);
  return;
}

# Refreshes trust flags, and adds purpose if missing
# (e.g. from an older index format)
sub cm_refresh_index () {
  my $index_fh;

  my ($last_hash, $last_trust, $last_purpose) = ("", "", "");

  open($index_fh, "<$certificates_path/.index") or
    die "Couldn't open $certificates_path/.index: $!";
  my ($newindex_fh, $newindex) = create_tempfile();

  while (<$index_fh>) {
    chomp;

    # fields: mailbox hash label issuer_hash trust purpose
    my @fields = split;

    if ($fields[1] eq $last_hash) {
      $fields[4] = $last_trust;
      $fields[5] = $last_purpose;
    }
    else {
      # Don't overwrite a revoked flag, because we don't have the CRL
      if ($fields[4] ne "r") {
        $fields[4] = openssl_trust_flag($fields[1], $fields[3]);
      }

      if ($#fields < 5) {
        $fields[5] = openssl_purpose_flag("$certificates_path/$fields[1]");
      }

      # To update an old private keys index format, always push the trust
      # and purpose out.
      if (-e "$private_keys_path/$fields[1]") {
        cm_modify_entry ("T", $fields[1], 0, $fields[4]);
        cm_modify_entry ("P", $fields[1], 0, $fields[5]);
      }

      $last_hash = $fields[1];
      $last_trust = $fields[4];
      $last_purpose = $fields[5];
    }

    print $newindex_fh join(" ", @fields), "\n";
  }
  close($index_fh);
  close($newindex_fh);

  move $newindex, "$certificates_path/.index"
      or die "Couldn't move $newindex to $certificates_path/.index: $!\n";
}


##############
# Op handlers
##############

sub handle_init_paths () {
  mkdir_recursive($certificates_path);
  mkdir_recursive($private_keys_path);

  my $file;

  $file = $certificates_path . "/.index";
  -f $file or open(TMP_FILE, ">$file") and close(TMP_FILE)
      or die "Can't touch $file: $!";

  $file = $private_keys_path . "/.index";
  -f $file or open(TMP_FILE, ">$file") and close(TMP_FILE)
      or die "Can't touch $file: $!";
}

sub handle_change_label ($) {
  my ($keyid) = @_;

  my $label = query_label();

  if (-e "$certificates_path/$keyid") {
    cm_modify_entry('L', $keyid, 1, $label);
    print "Changed label for certificate $keyid.\n";
  }
  else {
    die "No such certificate: $keyid";
  }

  if (-e "$private_keys_path/$keyid") {
    cm_modify_entry('L', $keyid, 0, $label);
    print "Changed label for private key $keyid.\n";
  }
}

sub handle_add_cert($) {
  my ($filename) = @_;

  my $label = query_label();
  my @cert_contents = openssl_parse_pem($filename, 0);
  @cert_contents = grep { $_->{type} eq "C" } @cert_contents;

  my @cert_chains = create_cert_chains(@cert_contents);
  print "Found " . scalar(@cert_chains) . " certificate chains\n";

  foreach my $chain (@cert_chains) {
    my $leaf = shift(@$chain);
    my $issuer_chain_hash = "?";

    print "Processing chain:\n";
    if ($leaf->{subject}) {
      print "subject=" . $leaf->{subject} . "\n";
    }

    if (scalar(@$chain) > 0) {
      my ($issuer_chain_fh, $issuer_chain_file) = create_tempfile();

      foreach my $issuer (@$chain) {
        my $issuer_datafile = $issuer->{datafile};
        open(my $issuer_fh, "< $issuer_datafile") or
            die "can't open $issuer_datafile: $?";
        print $issuer_chain_fh $_ while (<$issuer_fh>);
        close($issuer_fh);
      }

      close($issuer_chain_fh);
      $issuer_chain_hash = cm_add_cert($issuer_chain_file);
    }

    cm_add_indexed_cert($leaf->{datafile}, $label, $issuer_chain_hash);
  }
}

sub handle_add_pem ($) {
  my ($filename) = @_;

  my @pem_contents;
  my $iter;
  my $key;
  my $certificate;
  my $root_cert;
  my $issuer_cert_file;

  @pem_contents = openssl_parse_pem($filename, 1);

  # look for key
  $iter = 0;
  while ($iter <= $#pem_contents) {
    if ($pem_contents[$iter]->{type} eq "K") {
      $key = $pem_contents[$iter];
      splice(@pem_contents, $iter, 1);
      last;
    }
    $iter++;
  }
  defined($key) or die("Couldn't find private key!");
  $key->{localKeyID} or die("Attribute 'localKeyID' wasn't set.");

  # private key and certificate use the same 'localKeyID'
  $iter = 0;
  while ($iter <= $#pem_contents) {
    if (($pem_contents[$iter]->{type} eq "C") &&
        ($pem_contents[$iter]->{localKeyID} eq $key->{localKeyID})) {
      $certificate = $pem_contents[$iter];
      splice(@pem_contents, $iter, 1);
      last;
    }
    $iter++;
  }
  defined($certificate) or die("Couldn't find matching certificate!");

  if ($#pem_contents < 0) {
    die("No root and no intermediate certificates. Can't continue.");
  }

  # Look for a self signed root certificate
  $iter = 0;
  while ($iter <= $#pem_contents) {
    if ($pem_contents[$iter]->{subject} eq $pem_contents[$iter]->{issuer}) {
      $root_cert = $pem_contents[$iter];
      splice(@pem_contents, $iter, 1);
      last;
    }
    $iter++;
  }
  if (defined($root_cert)) {
    $issuer_cert_file = $root_cert->{datafile};
  } else {
    print "Couldn't identify root certificate!\n";
  }

  # what's left are intermediate certificates.
  if ($#pem_contents >= 0) {
    my ($tmp_issuer_cert_fh, $tmp_issuer_cert) = create_tempfile();
    $issuer_cert_file = $tmp_issuer_cert;

    $iter = 0;
    while ($iter <= $#pem_contents) {
      my $cert_datafile = $pem_contents[$iter]->{datafile};
      open (CERT, "< $cert_datafile") or die "can't open $cert_datafile: $?";
      print $tmp_issuer_cert_fh $_ while (<CERT>);
      close CERT;

      $iter++;
    }
    close $tmp_issuer_cert_fh;
  }

  handle_add_chain($key->{datafile}, $certificate->{datafile}, $issuer_cert_file);
}

sub handle_add_p12 ($) {
  my ($filename) = @_;

  print "\nNOTE: This will ask you for two passphrases:\n";
  print "       1. The passphrase you used for exporting\n";
  print "       2. The passphrase you wish to secure your private key with.\n\n";

  my ($pem_fh, $pem_file) = create_tempfile();
  close($pem_fh);

  openssl_p12_to_pem($filename, $pem_file);
  -e $pem_file and -s $pem_file or die("Conversion of $filename failed.");

  handle_add_pem($pem_file);
}

sub handle_add_chain ($$$) {
  my ($key_file, $cert_file, $issuer_file) = @_;

  my $label = query_label();

  my $issuer_hash = cm_add_cert($issuer_file);
  my $cert_data = cm_add_indexed_cert($cert_file, $label, $issuer_hash);

  foreach my $mailbox (@{$cert_data->{mailboxes}}) {
    cm_add_key($key_file, $cert_data->{hashvalue}, $mailbox, $label,
               $cert_data->{trust}, $cert_data->{purpose});
  }
}

sub handle_verify_cert ($$) {
  my ($keyid, $crl) = @_;

  -e "$certificates_path/$keyid" or $keyid eq 'all'
    or die "No such certificate: $keyid";

  my @fields = cm_find_entry($keyid, 1);
  if (scalar(@fields)) {
    my $issuer_hash = $fields[3];
    my $trust = openssl_trust_flag($keyid, $issuer_hash, $crl);

    cm_modify_entry('T', $keyid, 0, $trust);
    cm_modify_entry('T', $keyid, 1, $trust);
  }
}

sub handle_remove_pair ($) {
  my ($keyid) = @_;

  if (-e "$certificates_path/$keyid") {
    unlink "$certificates_path/$keyid";
    cm_modify_entry('R', $keyid, 1);
    print "Removed certificate $keyid.\n";
  }
  else {
    die "No such certificate: $keyid";
  }

  if (-e "$private_keys_path/$keyid") {
    unlink "$private_keys_path/$keyid";
    cm_modify_entry('R', $keyid, 0);
    print "Removed private key $keyid.\n";
  }
}

sub handle_add_root_cert ($) {
  my ($root_cert) = @_;

  my $root_hash = openssl_hash($root_cert);

  if (-d $root_certs_path) {
    -e "$root_certs_path/$root_hash" or
        mycopy $root_cert, "$root_certs_path/$root_hash";
  }
  else {
    open(ROOT_CERTS, ">>$root_certs_path") or
      die ("Couldn't open $root_certs_path for writing");

    my $md5fp = openssl_fingerprint($root_cert);

    my @cert_text = openssl_x509_query($root_cert, "-text");
    $? and die "openssl -text '$root_cert' returned $?";

    print "Enter a label, name or description for this certificate: ";
    my $input = <STDIN>;

    my $line = "=======================================\n";
    print ROOT_CERTS "\n$input$line$md5fp\nPEM-Data:\n";

    my $cert = openssl_dump_cert($root_cert);
    print ROOT_CERTS $cert;
    print ROOT_CERTS @cert_text;
    close (ROOT_CERTS);
  }
}
