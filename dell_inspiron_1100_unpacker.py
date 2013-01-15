# Dell/Phoenix ROM BIOS PLUS unpacker
# 2012-09-12 version 0.1
# 2012-10-10 version 0.2 added support for older BIOSes with 16-bit length (Dell Inspiron 1100)
# 3-clause BSD license
# roxfan@skynet.be

import array
import struct
import sys

def memcpy(arr1, off1, arr2, off2, count):
    while count:
        if off1 < len(arr1):
            arr1[off1] = arr2[off2]
        elif off1 == len(arr1):
            arr1.append(arr2[off2])
        else:
            raise Exception("Trying to write out of bounds")
        off1 += 1
        off2 += 1
        count -=1

# looks like some lzss variation
def dell_unpack(indata):
    srcoff = 0
    dstoff = 0
    src = array.array('B', indata)
    dst = array.array('B')
    inlen = len(indata)
    while srcoff < inlen:
        b = src[srcoff]
        nibl, nibh = b & 0x0F, (b >> 4) & 0x0F
        srcoff += 1
        if nibl:
            if nibl == 0xF:
                al = src[srcoff]
                ah = src[srcoff+1]
                srcoff += 2
                cx = nibh | (ah << 4)
                count = (cx & 0x3F) + 2
                delta = ((ah >> 2) << 8) | al
            else:
                count = nibl + 1
                delta = (nibh << 8) | src[srcoff]
                srcoff += 1
            memcpy(dst, dstoff, dst, dstoff - delta - 1, count)
            dstoff += count
        elif nibh == 0x0E:
            count = src[srcoff] + 1
            srcoff += 1
            memcpy(dst, dstoff, dst, dstoff - 1, count)
            dstoff += count
        else:
            if nibh == 0x0F:
                count = src[srcoff] + 15
                srcoff += 1
            else:
                count = nibh + 1
            memcpy(dst, dstoff, src, srcoff, count)
            dstoff += count
            srcoff += count

    return dst.tostring()

mod_types = {
  0x01: "Main ROM",
  0x0C: "Microcode update",
}

print "Dell/Phoenix ROM BIOS PLUS unpacker"
if len(sys.argv) < 2:
  print "Usage: dell_unpack.py bios.bin [offset]"
  sys.exit(1)
fname = sys.argv[1]
offs = 0
f = open(fname, "rb").read()
if len(sys.argv) > 2:
  offs = int(sys.argv[2], 16)
else:
  offs = f.find("\xF0\x00Copyright 1985-\x02\x04\xF0\x0F8 Phoenix Technologies Ltd.")
  if offs == -1:
    print "Does not look like a Dell/Phoenix ROM BIOS PLUS"
    sys.exit(2)
  if f[offs-5] == '\x01':
    hlen = 5 # 32-bit length
    offs -= 5
    fmt = "<BI"
  elif f[offs-3] == '\x01':
    hlen = 3 # 16-bit length
    offs -= 3
    fmt = "<BH"
  else:
    print "Unhandled format!"
    sys.exit(1)
  print "Found compressed module at %08X" % offs
if offs > 0:
    fn = "EC.bin"
    print "%08X EC code, %08X  %s" % (0, offs, fn)
    open(fn, "wb").write(f[:offs])
while True:
  type, leng = struct.unpack(fmt, f[offs:offs+hlen])
  print "%08X type %02X" % (offs, type),
  offs += hlen
  if type == 0xFF:
      print "<end of chain>"
      break
  data = f[offs:offs+leng]
  offs += leng
  if type != 0xC:
      odata = dell_unpack(data)
  else:
      odata = data
  print " %08X -> %08X" % (leng, len(odata)),
  fn = "mod_%02X.bin" % type
  print " %s" % fn,
  if type in mod_types:
      print "(%s)" % mod_types[type]
  else:
      print ""
  open(fn, "wb").write(odata)
