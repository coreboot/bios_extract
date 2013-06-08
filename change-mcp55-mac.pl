#!/usr/bin/perl
#
# Update the onboard NIC mac address in a coreboot rom image for mcp55-based boards
#
# This program is free software (GPLv2 or higher).
#
# 2008-01-30 Ward Vandewege (ward@gnu.org)

my $mac = $ARGV[0];
my $file = $ARGV[1];

if (($mac eq '') or ($file eq '')) {
	print "\nSyntax: $0 <mac address> <rom image>\n";
	exit 1;
}

if (! -f $file) {
	print "\nSyntax: $0 <mac address> <rom image>\n";
	print "\nERROR: Could not find file '$file'.\n\n";
	exit 1;
}

if (!($mac =~ /^[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}$/)) {
	print "\nSyntax: $0 <mac address> <rom image>\n";
	print "\nERROR: The mac address you specified ($mac) is not a valid mac address.\n\n";
	exit 1;
}

my @mac = split(/:/,$mac);

my $newmac = '';

for (my $c = 5; $c >= 0; $c--) {
	$newmac .= chr(hex($mac[$c]));
}

open(ROMIMAGE,"+<",$file) or die "Can't open file $file for writing\n";
seek(ROMIMAGE,-48,2);

print ROMIMAGE $newmac;
close(ROMIMAGE);

print "Mac address succesfully updated to $mac in $file\n";
exit 0;
