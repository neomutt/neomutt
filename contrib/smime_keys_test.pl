#! /usr/bin/perl -W

# by Mike Schiraldi <raldi@research.netsol.com>

use strict;
use Expect;

sub run ($;$ );

umask 077; # probably not necc. but can't hurt

my $tmpdir = "/tmp/smime_keys_test-$$-" . time;

mkdir $tmpdir or die;
chdir $tmpdir or die;

open TMP, '>muttrc' or die;
print TMP <<EOF;
set smime_ca_location="$tmpdir/ca-bundle.crt"
set smime_certificates="$tmpdir/certificates"
set smime_keys="$tmpdir/keys"
EOF
close TMP;

$ENV{MUTT_CMDLINE} = "mutt -F $tmpdir/muttrc";

# make a user key
run 'smime_keys init';
run 'openssl genrsa -out user.key 1024';

# make a request for this key to be signed
run 'openssl req -new -key user.key -out newreq.pem', "\n\nx\n\nx\nx\nuser\@smime.mutt\n\nx\n";

mkdir 'demoCA' or die;
mkdir 'demoCA/certs' or die;
mkdir 'demoCA/crl' or die;
mkdir 'demoCA/newcerts' or die;
mkdir 'demoCA/private' or die;
open OUT, '>demoCA/serial' or die;
print OUT "01\n";
close OUT;
open OUT, '>demoCA/index.txt' or die;
close OUT;

# make the CA 
run 'openssl req -new -x509 -keyout demoCA/private/cakey.pem -out demoCA/cacert.pem -days 7300 -nodes', 
    "\n\nx\n\nx\nx\n\n";

# trust it
run 'smime_keys add_root demoCA/cacert.pem', "root_CA\n";

# have the CA process the request
run 'openssl ca -batch -startdate 000101000000Z -enddate 200101000000Z -days 7300 ' .
    '-policy policy_anything -out newcert.pem -infiles newreq.pem';

unlink 'newreq.pem' or die;

# put it all in a .p12 bundle
run 'openssl pkcs12 -export -inkey user.key -in newcert.pem -out cert.p12 -CAfile demoCA/cacert.pem -chain', "pass1\n" x 2;
unlink 'newcert.pem' or die;
unlink 'demoCA/cacert.pem' or die;
unlink 'demoCA/index.txt' or die;
unlink 'demoCA/index.txt.old' or die;
unlink 'demoCA/serial' or die;
unlink 'demoCA/serial.old' or die;
unlink 'demoCA/newcerts/01.pem' or die;
unlink 'demoCA/private/cakey.pem' or die;
rmdir  'demoCA/certs' or die;
rmdir  'demoCA/crl' or die;
rmdir  'demoCA/private' or die;
rmdir  'demoCA/newcerts' or die;
rmdir  'demoCA' or die;

# have smime_keys process it
run 'smime_keys add_p12 cert.p12', "pass1\n" . "pass2\n" x 2 . "old_label\n";
unlink 'cert.p12' or die;

# make sure it showed up
run 'smime_keys list > list';

open IN, 'list' or die;
<IN> eq "\n" or die;
<IN> =~ /^(.*)\: Issued for\: user\@smime\.mutt \"old_label\" \(Unverified\)\n/ or die;
close IN;

my $keyid = $1;

# see if we can rename it
run "smime_keys label $keyid", "new_label\n";

# make sure it worked
run 'smime_keys list > list';

open IN, 'list' or die;
<IN> eq "\n" or die;
<IN> =~ /^$keyid\: Issued for\: user\@smime\.mutt \"new_label\" \(Unverified\)\n/ or die;
close IN;

unlink 'list' or die;

# try signing something
run "openssl smime -sign -signer certificates/$keyid -inkey user.key -in /etc/passwd -certfile certificates/37adefc3.0  > signed";
unlink 'user.key' or die;

# verify it 
run 'openssl smime -verify -out /dev/null -in signed -CAfile ca-bundle.crt';
unlink 'signed' or die;

# clean up
unlink 'ca-bundle.crt' or die;
unlink 'muttrc' or die;
unlink 'keys/.index' or die;
unlink 'certificates/.index' or die;
unlink <keys/*> or die;
unlink <certificates/*> or die;
rmdir  'keys' or die;
rmdir  'certificates' or die;
chdir  '/' or die;
rmdir  $tmpdir or die;


sub run ($;$) {
    my $cmd = shift or die;
    my $input = shift;
    
    print "\n\nRunning [$cmd]\n";

    my $exp = Expect->spawn ($cmd);
    if (defined $input) {
        print $exp $input;
    }
    $exp->soft_close;
    $? and die "$cmd returned $?";
}
