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

umask 077;

require "timelocal.pl";

sub usage ();
sub newfile ($;$$);
sub mutt_Q ($ );
sub mycopy ($$);

#  directory setup routines
sub mkdir_recursive ($ );
sub init_paths ();

# key/certificate management methods
sub list_certs ();
sub query_label ();
sub add_entry ($$$$$ );
sub add_certificate ($$$$;$ );
sub add_key ($$$$);
sub add_root_cert ($ );
sub parse_pem (@ );
sub handle_pem (@ );
sub modify_entry ($$$;$ );
sub remove_pair ($ );
sub change_label ($ );
sub verify_cert($$);
sub do_verify($$$ );
              
# Get the directories mutt uses for certificate/key storage.

my $mutt = $ENV{MUTT_CMDLINE} || 'mutt';
my $opensslbin = "/usr/bin/openssl";
my @tempfiles = ();
my @cert_tmp_file = ();

my $tmpdir;
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


#
# OPS
#

if(@ARGV == 1 and $ARGV[0] eq "init") {
    init_paths;
}
elsif(@ARGV == 1 and $ARGV[0] eq "list") {
    list_certs;
}
elsif(@ARGV == 2 and $ARGV[0] eq "label") {
    change_label($ARGV[1]);
}
elsif(@ARGV == 2 and $ARGV[0] eq "add_cert") {
    my $format = -B $ARGV[1] ? 'DER' : 'PEM'; 
    my $cmd = "$opensslbin x509 -noout -hash -in $ARGV[1] -inform $format";
    my $cert_hash = `$cmd`;
    $? and die "'$cmd' returned $?";
    chomp($cert_hash); 
    my $label = query_label;
    &add_certificate($ARGV[1], \$cert_hash, 1, $label, '?');
}
elsif(@ARGV == 2 and $ARGV[0] eq "add_pem") {
    -e $ARGV[1] and -s $ARGV[1] or die("$ARGV[1] is nonexistent or empty.");
    open(PEM_FILE, "<$ARGV[1]") or die("Can't open $ARGV[1]: $!");
    my @pem = <PEM_FILE>;
    close(PEM_FILE);
    handle_pem(@pem);
}
elsif( @ARGV == 2 and $ARGV[0] eq "add_p12") {
    -e $ARGV[1] and -s $ARGV[1] or die("$ARGV[1] is nonexistent or empty.");

    print "\nNOTE: This will ask you for two passphrases:\n";
    print "       1. The passphrase you used for exporting\n";
    print "       2. The passphrase you wish to secure your private key with.\n\n";

    my $pem_file = "$ARGV[1].pem";
    
    my $cmd = "$opensslbin pkcs12 -in $ARGV[1] -out $pem_file";
    system $cmd and die "'$cmd' returned $?";
    
    -e $pem_file and -s $pem_file or die("Conversion of $ARGV[1] failed.");
    open(PEM_FILE, $pem_file) or die("Can't open $pem_file: $!");
    my @pem = <PEM_FILE>;
    close(PEM_FILE);
    unlink $pem_file;
    handle_pem(@pem);
}
elsif(@ARGV == 4 and $ARGV[0] eq "add_chain") {
    my $mailbox;
    my $format = -B $ARGV[2] ? 'DER' : 'PEM'; 
    my $cmd = "$opensslbin x509 -noout -hash -in $ARGV[2] -inform $format";
    my $cert_hash = `$cmd`;

    $? and die "'$cmd' returned $?";

    $format = -B $ARGV[3] ? 'DER' : 'PEM'; 

    $cmd = "$opensslbin x509 -noout -hash -in $ARGV[3] -inform $format";
    my $issuer_hash = `$cmd`;
    $? and die "'$cmd' returned $?";
    
    chomp($cert_hash); 
    chomp($issuer_hash);

    my $label = query_label;
    
    add_certificate($ARGV[3], \$issuer_hash, 0, $label); 
    my @mailbox = &add_certificate($ARGV[2], \$cert_hash, 1, $label, $issuer_hash);
    
    foreach $mailbox (@mailbox) {
      chomp($mailbox);
      add_key($ARGV[1], $cert_hash, $mailbox, $label);
    }
}
elsif((@ARGV == 2 or @ARGV == 3) and $ARGV[0] eq "verify") {
    verify_cert($ARGV[1], $ARGV[2]);
}
elsif(@ARGV == 2 and $ARGV[0] eq "remove") {
    remove_pair($ARGV[1]);
}
elsif(@ARGV == 2 and $ARGV[0] eq "add_root") {
    add_root_cert($ARGV[1]);
}
else {    
    usage;
    exit(1);
}

exit(0);





##############  sub-routines  ########################

sub usage () {
    print <<EOF;

Usage: smime_keys <operation>  [file(s) | keyID [file(s)]]

        with operation being one of:

        init      : no files needed, inits directory structure.

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
    my $var = shift or die;

    my $cmd = "$mutt -v >/dev/null 2>/dev/null";
    system ($cmd) == 0 
        or die<<EOF;
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
#'

    $answer =~ /\"(.*?)\"/ and return bsd_glob($1, GLOB_TILDE | GLOB_NOCHECK);
    
    $answer =~ /^Mutt (.*?) / and die<<EOF;
This script requires mutt 1.5.0 or later. You are using mutt $1.
EOF
    
    die "Value of $var is weird\n";
}

sub mycopy ($$) {
    my $source = shift or die;
    my $dest = shift or die;

    copy $source, $dest or die "Problem copying $source to $dest: $!\n";
}

#
#  directory setup routines
#


sub mkdir_recursive ($) {
    my $path = shift or die;
    my $tmp_path;
    
    for my $dir (split /\//, $path) {
        $tmp_path .= "$dir/";

        -d $tmp_path 
            or mkdir $tmp_path, 0700
                or die "Can't mkdir $tmp_path: $!";
    }
}

sub init_paths () {
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



#
# certificate management methods
#

sub list_certs () {
  my %keyflags = ( 'i', '(Invalid)',  'r', '(Revoked)', 'e', '(Expired)',
		   'u', '(Unverified)', 'v', '(Valid)', 't', '(Trusted)');

  open(INDEX, "<$certificates_path/.index") or 
    die "Couldn't open $certificates_path/.index: $!";
  
  print "\n";
  while(<INDEX>) {
    my $tmp;
    my @tmp;
    my $tab = "            ";
    my @fields = split;

    if($fields[2] eq '-') {
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

    my $subject_in;
    my $issuer_in;
    my $date1_in;
    my $date2_in;

    my $format = -B $certfile ? 'DER' : 'PEM'; 
    my $cmd = "$opensslbin x509 -subject -issuer -dates -noout -in $certfile -inform $format";
    ($subject_in, $issuer_in, $date1_in, $date2_in) = `$cmd`;
    $? and print "ERROR: '$cmd' returned $?\n\n" and next;


    my @subject = split(/\//, $subject_in);
    while(@subject) {
      $tmp = shift @subject;
      ($tmp =~ /^CN\=/) and last;
      undef $tmp;
    }
    defined $tmp and @tmp = split (/\=/, $tmp) and
      print $tab."Subject: $tmp[1]\n";

    my @issuer = split(/\//, $issuer_in);
    while(@issuer) {
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

    $format = -B "$certificates_path/$fields[1]" ? 'DER' : 'PEM'; 
    $cmd = "$opensslbin x509 -purpose -noout -in $certfile -inform $format";
    my $purpose_in = `$cmd`;
    $? and die "'$cmd' returned $?";

    my @purpose = split (/\n/, $purpose_in);
    print "$tab$purpose[0] (displays S/MIME options only)\n";
    while(@purpose) {
      $tmp = shift @purpose;
      ($tmp =~ /^S\/MIME/ and $tmp =~ /Yes/) or next;
      my @tmptmp = split (/:/, $tmp);
      print "$tab  $tmptmp[0]\n";
    }

    print "\n";
  }
  
  close(INDEX);
}



sub query_label () {
    my @words;
    my $input;

    print "\nYou may assign a label to this key, so you don't have to remember\n";
    print "the key ID. This has to be _one_ word (no whitespaces).\n\n";

    print "Enter label: ";
    chomp($input = <STDIN>);

    my ($label, $junk) = split(/\s/, $input, 2);     
    
    defined $junk 
        and print "\nUsing '$label' as label; ignoring '$junk'\n";

    defined $label || ($label =  "-");

    return $label;
}



sub add_entry ($$$$$) {
    my $mailbox = shift or die;
    my $hashvalue = shift or die;
    my $use_cert = shift;
    my $label = shift or die;
    my $issuer_hash = shift;

    my @fields;

    if ($use_cert) {
        open(INDEX, "+<$certificates_path/.index") or 
            die "Couldn't open $certificates_path/.index: $!";
    }
    else {
        open(INDEX, "+<$private_keys_path/.index") or 
            die "Couldn't open $private_keys_path/.index: $!";
    }

    while(<INDEX>) {
        @fields = split;
        return if ($fields[0] eq $mailbox && $fields[1] eq $hashvalue);
    }

    if ($use_cert) {
        print INDEX "$mailbox $hashvalue $label $issuer_hash u\n";
    }
    else {
        print INDEX "$mailbox $hashvalue $label \n";
    }

    close(INDEX);
}


sub add_certificate ($$$$;$) {
    my $filename = shift or die;
    my $hashvalue = shift or die;
    my $add_to_index = shift;
    my $label = shift or die;
    my $issuer_hash = shift;

    my $iter = 0;
    my @mailbox;
    my $mailbox;

    while(-e "$certificates_path/$$hashvalue.$iter") {
        my ($t1, $t2);
        my $format = -B $filename ? 'DER' : 'PEM'; 
        my $cmd = "$opensslbin x509 -in $filename -inform $format -fingerprint -noout";
        $t1 = `$cmd`;
        $? and die "'$cmd' returned $?";

        $format = -B "$certificates_path/$$hashvalue.$iter" ? 'DER' : 'PEM'; 
        $cmd = "$opensslbin x509 -in $certificates_path/$$hashvalue.$iter -inform $format -fingerprint -noout";
        $t2 = `$cmd`;
        $? and die "'$cmd' returned $?";
        
        $t1 eq $t2 and last;

        $iter++;
    }
    $$hashvalue .= ".$iter";
    
    if (-e "$certificates_path/$$hashvalue") {
            print "\nCertificate: $certificates_path/$$hashvalue already installed.\n";
    }
    else {
        mycopy $filename, "$certificates_path/$$hashvalue";

        if ($add_to_index) {
            my $format = -B $filename ? 'DER' : 'PEM'; 
	    my $cmd = "$opensslbin x509 -in $filename -inform $format -email -noout";
	    @mailbox = `$cmd`;
	    $? and die "'$cmd' returned $?";

	    foreach $mailbox (@mailbox) {
	      chomp($mailbox);
	      add_entry($mailbox, $$hashvalue, 1, $label, $issuer_hash);

	      print "\ncertificate $$hashvalue ($label) for $mailbox added.\n";
	    }
	    verify_cert($$hashvalue, undef);
        }
        else {
            print "added certificate: $certificates_path/$$hashvalue.\n";
        }
    }

    return @mailbox;
}


sub add_key ($$$$) {
    my $file = shift or die;
    my $hashvalue = shift or die;
    my $mailbox = shift or die;
    my $label = shift or die;

    unless (-e "$private_keys_path/$hashvalue") {
        mycopy $file, "$private_keys_path/$hashvalue";
    }    

    add_entry($mailbox, $hashvalue, 0, $label, "");
    print "added private key: " .
      "$private_keys_path/$hashvalue for $mailbox\n";
} 






sub parse_pem (@) {
    my $state = 0;
    my $cert_iter = 0;
    my @bag_attribs;
    my $numBags = 0;

    $cert_tmp_file[$cert_iter] = newfile("cert_tmp.$cert_iter","temp");
    my $cert_tmp_iter = $cert_tmp_file[$cert_iter];
    open(CERT_FILE, ">$cert_tmp_iter") 
        or die "Couldn't open $cert_tmp_iter: $!";

    while($_ = shift(@_)) {
        if(/^Bag Attributes/) {
            $numBags++;
            $state == 0 or  die("PEM-parse error at: $.");
	    $state = 1;
            $bag_attribs[$cert_iter*4+1] = "";
            $bag_attribs[$cert_iter*4+2] = "";
            $bag_attribs[$cert_iter*4+3] = "";
        }

        ($state == 1) and /localKeyID:\s*(.*)/ 
            and ($bag_attribs[$cert_iter*4+1] = $1);

        ($state == 1) and /subject=\s*(.*)/    
            and ($bag_attribs[$cert_iter*4+2] = $1);

        ($state == 1) and /issuer=\s*(.*)/     
            and ($bag_attribs[$cert_iter*4+3] = $1);
        
        if(/^-----/) {
            if(/BEGIN/) {
                print CERT_FILE;
                $state = 2;

                if(/PRIVATE/) {
                    $bag_attribs[$cert_iter*4] = "K";
                    next;
                }
                if(/CERTIFICATE/) {
                    $bag_attribs[$cert_iter*4] = "C";
                    next;
                }
                die("What's this: $_");
            }
            if(/END/) {
                $state = 0;
                print CERT_FILE;
                close(CERT_FILE);
                $cert_iter++;
	        $cert_tmp_file[$cert_iter] = newfile("cert_tmp.$cert_iter","temp");
	        $cert_tmp_iter = $cert_tmp_file[$cert_iter];
                open(CERT_FILE, ">$cert_tmp_iter")
                    or die "Couldn't open $cert_tmp_iter: $!";
                next;
            }
        }
        print CERT_FILE;
    }
    close(CERT_FILE);

    # I'll add support for unbagged cetificates, in case this is needed.
    $numBags == $cert_iter or 
        die("Not all contents were bagged. can't continue.");

    return @bag_attribs;
}


# This requires the Bag Attributes to be set
sub handle_pem (@) {

    my @pem_contents;
    my $iter=0;
    my $root_cert;
    my $key;
    my $certificate;
    my $intermediate;
    my @mailbox;
    my $mailbox;

    @pem_contents = &parse_pem(@_);

    # private key and certificate use the same 'localKeyID'
    while($iter <= $#pem_contents / 4) {
        if($pem_contents[$iter * 4] eq "K") {
            $key = $iter;
            last;
        }
        $iter++;
    }
    ($iter > $#pem_contents / 2) and die("Couldn't find private key!");

    $pem_contents[($key * 4)+1] or die("Attribute 'localKeyID' wasn't set.");

    $iter = 0;
    while($iter <= $#pem_contents / 4) {
        $iter == $key and ($iter++) and next;
        if($pem_contents[($iter * 4)+1] eq $pem_contents[($key * 4)+1]) {
            $certificate = $iter;
            last;
        }
        $iter++;
    }
    ($iter > $#pem_contents / 4) and die("Couldn't find matching certificate!");

    my $tmp_key = newfile("tmp_key","temp");
    mycopy $cert_tmp_file[$key], $tmp_key;
    my $tmp_certificate = newfile("tmp_certificate","temp");
    mycopy $cert_tmp_file[$certificate], $tmp_certificate;

    # root certificate is self signed
    $iter = 0;

    while($iter <= $#pem_contents / 4) {
        if ($iter == $key or $iter == $certificate) {
            $iter++; 
            next;
        }

        if($pem_contents[($iter * 4)+2] eq $pem_contents[($iter * 4)+3]) {
            $root_cert = $iter;
            last;
        }
        $iter++;
    }
    if ($iter > $#pem_contents / 4) {
      print "Couldn't identify root certificate!\n";
      $root_cert = -1;      
    }

    # what's left are intermediate certificates.
    $iter = 0;

    # needs to be set, so we can check it later
    $intermediate = $root_cert;
    my $tmp_issuer_cert = newfile("tmp_issuer_cert","temp");
    while($iter <= $#pem_contents / 4) {
        if ($iter == $key or $iter == $certificate or $iter == $root_cert) {
            $iter++; 
            next;
        }

	open (IC, ">> $tmp_issuer_cert") or die "can't open $tmp_issuer_cert: $?";
	my $cert_tmp_iter = $cert_tmp_file[$iter];
	open (CERT, "< $cert_tmp_iter") or die "can't open $cert_tmp_iter: $?";
	print IC while (<CERT>);
	close IC;
	close CERT;

	# although there may be many, just need to know if there was any
	$intermediate = $iter;

        $iter++;
    }

    # no intermediate certificates ? use root-cert instead (if that was found...)
    if($intermediate == $root_cert) {
        if ($root_cert == -1) {
	  die("No root and no intermediate certificates. Can't continue.");
	}
        mycopy $cert_tmp_file[$root_cert], $tmp_issuer_cert;
    }

    my $label = query_label;

    my $format = -B $tmp_certificate ? 'DER' : 'PEM'; 
    my $cmd = "$opensslbin x509 -noout -hash -in $tmp_certificate -inform $format";
    my $cert_hash = `$cmd`;
    $? and die "'$cmd' returned $?";

    $format = -B $tmp_issuer_cert ? 'DER' : 'PEM'; 
    $cmd = "$opensslbin x509 -noout -hash -in $tmp_issuer_cert -inform $format";
    my $issuer_hash = `$cmd`;
    $? and die "'$cmd' returned $?";

    chomp($cert_hash); chomp($issuer_hash);

    # Note: $cert_hash will be changed to reflect the correct filename
    #       within add_cert() ONLY, so these _have_ to get called first..
    add_certificate($tmp_issuer_cert, \$issuer_hash, 0, $label);
    @mailbox = &add_certificate("$tmp_certificate", \$cert_hash, 1, $label, $issuer_hash); 
    foreach $mailbox (@mailbox) {
      chomp($mailbox);
      add_key($tmp_key, $cert_hash, $mailbox, $label);
    }
}






sub modify_entry ($$$;$ ) {
    my $op = shift or die;
    my $hashvalue = shift or die;
    my $use_cert = shift;
    my $crl;
    my $label;
    my $path;
    my @fields;

    $op eq 'L' and ($label = shift or die);
    $op eq 'V' and ($crl = shift);


    if ($use_cert) {
        $path = $certificates_path;
    }
    else {
        $path = $private_keys_path;
    }

    open(INDEX, "<$path/.index") or  
      die "Couldn't open $path/.index: $!";
    my $newindex = newfile("$path/.index.tmp");
    open(NEW_INDEX, ">$newindex") or 
      die "Couldn't create $newindex: $!";

    while(<INDEX>) {
        @fields = split;
        if($fields[1] eq $hashvalue or $hashvalue eq 'all') {
	  $op eq 'R' and next;
	  print NEW_INDEX "$fields[0] $fields[1]";
	  if($op eq 'L') {
	    if($use_cert) {
	      print NEW_INDEX " $label $fields[3] $fields[4]";
	    }
	    else {
	      print NEW_INDEX " $label";
	    }
	  }
	  if ($op eq 'V') {
	    print "\n==> about to verify certificate of $fields[0]\n";
	    my $flag = &do_verify($fields[1], $fields[3], $crl);
	    print NEW_INDEX " $fields[2] $fields[3] $flag";
	  }
	  print NEW_INDEX "\n";
	  next;
	}
	print NEW_INDEX;
    }
    close(INDEX);
    close(NEW_INDEX);

    rename $newindex, "$path/.index" 
        or die "Couldn't rename $newindex to $path/.index: $!\n";

    print "\n";
}




sub remove_pair ($ ) {
  my $keyid = shift or die;

  if (-e "$certificates_path/$keyid") {
    unlink "$certificates_path/$keyid";
    modify_entry('R', $keyid, 1);
    print "Removed certificate $keyid.\n";
  }
  else {
    die "No such certificate: $keyid";
  }

  if (-e "$private_keys_path/$keyid") {
    unlink "$private_keys_path/$keyid";
    modify_entry('R', $keyid, 0);
    print "Removed private key $keyid.\n";
  }
}



sub change_label ($ ) {
  my $keyid = shift or die;
  
  my $label = query_label;

  if (-e "$certificates_path/$keyid") {
    modify_entry('L', $keyid, 1, $label);
    print "Changed label for certificate $keyid.\n";
  }
  else {
    die "No such certificate: $keyid";
  }

  if (-e "$private_keys_path/$keyid") {
    modify_entry('L', $keyid, 0, $label);
    print "Changed label for private key $keyid.\n";
  }

}




sub verify_cert ($$) {
  my $keyid = shift or die;
  my $crl = shift;

  -e "$certificates_path/$keyid" or $keyid eq 'all'
    or die "No such certificate: $keyid";
  modify_entry('V', $keyid, 1, $crl);
}




sub do_verify($$$) {

  my $cert = shift or die;
  my $issuerid = shift or die;
  my $crl = shift;

  my $result = 'i';
  my $trust_q;
  my $issuer_path;
  my $cert_path = "$certificates_path/$cert";

  if($issuerid eq '?') {
    $issuer_path = "$certificates_path/$cert";
  } else {
    $issuer_path = "$certificates_path/$issuerid";
  }

  my $cmd = "$opensslbin verify $root_certs_switch $root_certs_path -purpose smimesign -purpose smimeencrypt -untrusted $issuer_path $cert_path";
  my $output = `$cmd`;
  $? and die "'$cmd' returned $?";
  chop $output;
  print "\n$output\n";

  ($output =~ /OK/) and ($result = 'v');

  $result eq 'i' and return $result;

  my $format = -B $cert_path ? 'DER' : 'PEM'; 
  $cmd = "$opensslbin x509 -dates -serial -noout -in $cert_path -inform $format";
  (my $date1_in, my $date2_in, my $serial_in) = `$cmd`;
  $? and die "'$cmd' returned $?";

  if ( defined $date1_in and defined $date2_in ) {
    my @tmp = split (/\=/, $date1_in);
    my $tmp = $tmp[1];
    @tmp = split (/\=/, $date2_in);
    my %months = ('Jan', '00', 'Feb', '01', 'Mar', '02', 'Apr', '03',
		  'May', '04', 'Jun', '05', 'Jul', '06', 'Aug', '07',
		  'Sep', '08', 'Oct', '09', 'Nov', '10', 'Dec', '11');

    my @fields =
      $tmp =~ /(\w+)\s*(\d+)\s*(\d+):(\d+):(\d+)\s*(\d+)\s*GMT/;

    $#fields != 5 and print "Expiration Date: Parse Error :  $tmp\n\n" or
      timegm($fields[4], $fields[3], $fields[2], $fields[1],
	     $months{$fields[0]}, $fields[5]) > time and $result = 'e';
    $result eq 'e' and print "Certificate is not yet valid.\n" and return $result;

    @fields =
      $tmp[1] =~ /(\w+)\s*(\d+)\s*(\d+):(\d+):(\d+)\s*(\d+)\s*GMT/;

    $#fields != 5 and print "Expiration Date: Parse Error :  $tmp[1]\n\n" or
      timegm($fields[4], $fields[3], $fields[2], $fields[1],
	     $months{$fields[0]}, $fields[5]) < time and $result = 'e';
    $result eq 'e' and print "Certificate has expired.\n" and return $result;

  }
    
  if ( defined $crl ) {
    my @serial = split (/\=/, $serial_in);
    my $cmd = "$opensslbin crl -text -noout -in $crl | grep -A1 $serial[1]";
    (my $l1, my $l2) = `$cmd`;
    $? and die "'$cmd' returned $?";
    
    if ( defined $l2 ) {
      my @revoke_date = split (/:\s/, $l2);
      print "FAILURE: Certificate $cert has been revoked on $revoke_date[1]\n";
      $result = 'r';
    }
  }    
  print "\n";

  if ($result eq 'v') {
    return 't';
  }

  return $result;
}



sub add_root_cert ($) {
  my $root_cert = shift or die;

  my $format = -B $root_cert ? 'DER' : 'PEM'; 

  my $cmd = "$opensslbin x509 -noout -hash -in $root_cert -inform $format";
  my $root_hash = `$cmd`;
  $? and die "'$cmd' returned $?";

  if (-d $root_certs_path) {
    -e "$root_certs_path/$root_hash" or
        mycopy $root_cert, "$root_certs_path/$root_hash";
  }
  else {
    open(ROOT_CERTS, ">>$root_certs_path") or 
      die ("Couldn't open $root_certs_path for writing");

    $cmd = "$opensslbin x509 -in $root_cert -inform $format -fingerprint -noout";
    $? and die "'$cmd' returned $?";
    chomp(my $md5fp = `$cmd`);

    $cmd = "$opensslbin x509 -in $root_cert -inform $format -text -noout";
    $? and die "'$cmd' returned $?";
    my @cert_text = `$cmd`;

    print "Enter a label, name or description for this certificate: ";
    my $input = <STDIN>;

    my $line = "=======================================\n";
    print ROOT_CERTS "\n$input$line$md5fp\nPEM-Data:\n";

    $cmd = "$opensslbin x509 -in $root_cert -inform $format";
    my $cert = `$cmd`;
    $? and die "'$cmd' returned $?";
    print ROOT_CERTS $cert;
    print ROOT_CERTS @cert_text;
    close (ROOT_CERTS);
  }
  
}

sub newfile ($;$$) {
	# returns a file name which does not exist for tmp file creation
	my $filename = shift;
	my $option = shift;
	$option = "notemp" if (not defined($option));
	if (! $tmpdir and $option eq "temp") {
		$tmpdir = mutt_Q 'tmpdir';
		$tmpdir = newfile("$tmpdir/smime");
		mkdir $tmpdir, 0700 || die "Can't create $tmpdir: $!\n";
	}
	$filename = "$tmpdir/$filename" if ($option eq "temp");
	my $newfilename = $filename;
	my $count = 0;
	while (-e $newfilename) {
		$newfilename = "$filename.$count";
		$count++;
	}
	unshift(@tempfiles,$newfilename);
	return $newfilename;
}


END {
	# remove all our temporary files in the end:
	for (@tempfiles){
		if (-f) {
			unlink;
		} elsif (-d) { 
			rmdir;
		}
	}
}
