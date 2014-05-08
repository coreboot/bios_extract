#!/usr/bin/env python
#
# parse CSMCORE.raw from AMI UEFI
# 3-clause BSD license
# roxfan@skynet.be

import struct, sys
if len(sys.argv) < 2:
  fn = "CSMCORE.raw"
else:
  fn = sys.argv[1]
f = open(fn, "rb")
while True:
  print "%08X"%f.tell(),
  hdr = f.read(10)
  if len(hdr) == 0:
    break
  typ, vid, did, size = struct.unpack("<HHHI", hdr)
  if (typ & 0xFF00) == 0xA000:
    #print "Multi-ID module"
    count = typ & 0xFF
    print "M %2X %04X %04X %8X" % (count, vid, did, size),
    for i in range(count):
      typ, vid, did = struct.unpack("<HHH", f.read(6))
      print "\n         %4X %04X %04X         " % (typ, vid, did),
  else:
    print "%4X %04X %04X %8X" % (typ, vid, did, size),
  fname = "csm_%02X_%04X_%04X.rom" % (typ, vid, did)
  print " => %s" % fname
  if size == 0xFFFFFFFF:
    d = f.read()
  else:
    d = f.read(size)
  open(fname, "wb").write(d)
