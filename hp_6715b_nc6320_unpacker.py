# HP Compaq 6715b ROM unpacker by roxfan
# compressed data begins like this:
# 0000010000: 01 00 14 01 70 00 00 00 | 60 00 00 00 00 00 02 00  O .Op   .     O
# 0000010010: 50 4F 53 54 FF B8 00 20 | 8E D8 66 B8 10 FF 20 00  POST..  ..f...

import struct, array, sys

def unpack1(cdata, ulen):
  pos = 0
  odata = ""
  while ulen:
    a = ord(cdata[pos])
    # print "%x: %x" % (pos, a)
    pos += 1
    if a == 0xFF:
      odata += cdata[pos:pos+8]
      ulen -= 8
      pos += 8
    else:
      mask = a | 0x100
      while mask and ulen:
        # print hex(mask), hex(pos)
        b = mask & 1
        mask >>= 1
        if mask == 0:
          break
        if b:
          odata += cdata[pos]
          pos += 1
          ulen -= 1
        else:
          delta = ord(cdata[pos])
          pos += 1
          delta |= ord(cdata[pos])<<8
          pos += 1
          count = (delta & 0xF) + 3
          delta >>= 4
          # print "d: %d, c: %d" % (delta, count)
          opos = len(odata)-delta
          while count:
            odata += odata[opos]
            opos += 1
            count -= 1
            ulen -= 1
  return odata

f = open(sys.argv[1], "rb")
f.seek(0x10000)

while True:
  flags, ulen, clen, dest = struct.unpack("<IIII", f.read(0x10))
  comp = flags & 0xFF
  if comp > 1:
    break
  hdrlen = (flags>>16) & 0xFF
  unk = (flags>>24) & 0xFF
  print "comp: %d, hdr len: 0x%X, unk: %d, ulen: 0x%X, clen: 0x%X, dest: 0x%X" % (comp, hdrlen, unk, ulen, clen, 
dest)
  extra = f.read(hdrlen-0x10).rstrip('\0')
  print "  %s" % extra
  cdata = f.read(clen)
  fname = "%04X_%s.bin" % (dest>>4, extra)
  if comp == 1:
    cdata = unpack1(cdata, ulen)
  open(fname, "wb").write(cdata)
