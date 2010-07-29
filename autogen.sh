#! /bin/sh

SRCDIR=`dirname $0`
rm -f $SRCDIR/config.cache
echo "aclocal"
aclocal -I m4
echo "autoconf"
autoconf
echo "automake"
automake --add-missing --copy
echo "configure"
$SRCDIR/configure
exit
