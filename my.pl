while (<>)
{
    if (m/^[\ ]*([A-Z0-9_]+)[\ ]*\+=[\ ]*(.*)/) {
	$identitifier{$1} = $identitifier{$1} . " " . $2;
    } else { $slurp[$#slurp + 1]=$_; }
}

print "# This Makefile has been mangled by a very rickety perl script.\n";
print "# Combined += 's: \n";

while (($a,$b,%identitifier)=%identitifier) {
    print $a, " = ", $b, "\n";
}

print "# The rest of the Makefile: \n";

print @slurp;
