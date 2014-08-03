#!/bin/sh
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#
# Takes a path to a file as first and only argument, removes the first 2048 B
# from that file and stores the result with the same name plus a '.bin' suffix.
# Useable to remove the header from UEFI Capsule files to use the resulting
# binary with flashrom.

main () {
	if [ "$#" -lt 1 -o ! -r "$1" ]; then
		echo "Removes the 2048 B header of UEFI Capsule files.\n"\
		     "Usage: $0 <FILE.CAP>"
		return 1
	fi

	capsize=$(wc -c "$1" | cut -f 1 -d ' ')
	binsize=$(($capsize-2048))
	ispowoftwo=$(($binsize & ($binsize-1)))
	if [ $ispowoftwo -ne 0 -o $binsize -eq 0 ]; then
		echo "The size of the resulting file would not be a power of 2 (but $binsize B)."
		return 1
	fi

	dd bs=2048 skip=1 if="$1" of="$1.bin"
}

main $*
