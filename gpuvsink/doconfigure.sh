#!/bin/sh
# you can either set the environment variables AUTOCONF, AUTOHEADER, AUTOMAKE,
# ACLOCAL, AUTOPOINT and/or LIBTOOLIZE to the right versions, or leave them
# unset and get the defaults

./configure --build=i686-linux --host=arm-arago-linux-gnueabi --prefix=/home/a0756700/nfs/am335x_0505_fs/usr/  || {
 echo 'configure failed';
 exit 1;
}

echo
echo "Now type 'make' to compile this module."
echo
