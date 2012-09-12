# extract microcode updates from binary BIOS files
# v 0.1 2012/07/23
# v 0.2 2012/07/23 added VIA Nano support (relaxed some checks)
# Licensed as Public Domain

import ctypes
import struct
import sys
import array

uint8_t  = ctypes.c_ubyte
char     = ctypes.c_char
uint32_t = ctypes.c_uint
uint64_t = ctypes.c_uint64
uint16_t = ctypes.c_ushort

def get_struct(str_, off, struct):
    s = struct()
    slen = ctypes.sizeof(s)
    bytes = str_[off:off+slen]
    fit = min(len(bytes), slen)
    ctypes.memmove(ctypes.addressof(s), bytes, fit)
    return s

def DwordAt(f, off):
    return struct.unpack("<I", f[off:off+4])[0]

# Intel microcode update header
class IntelUcUpdateHeader(ctypes.LittleEndianStructure):
    _fields_ = [
        ("HeaderVersion",       uint32_t), #
        ("UpdateRevision",      uint32_t), #
        ("Date",                uint32_t), #
        ("ProcessorSignature",  uint32_t), #
        ("Checksum",            uint32_t), #
        ("LoaderRevision",      uint32_t), #
        ("ProcessorFlags",      uint32_t), #
        ("DataSize",            uint32_t), #
        ("TotalSize",           uint32_t), #
    ]

def check_valid_mcode(f, off, maxoff, is_via = False):
    hdr = get_struct(f, off, IntelUcUpdateHeader)
    # on Intel, Total Size is always a multiple of 1024.
    if not is_via and hdr.TotalSize & 0x3FF:
        return

    # For microcode updates with a data size field
    # equal to 00000000H, the size of the microcode
    # update is 2048 bytes.
    if hdr.DataSize == 0:
        check_len = 2048
    else:
        check_len = hdr.TotalSize
        if check_len < 2048:
            return False

    # it should not run off the end of file or another update
    if check_len + off > maxoff:
        return

    # update size must be a multiple of DWORD
    if (hdr.DataSize & 3) or (hdr.TotalSize & 3):
        return

    # looks okay. let's check the checksum

    mdata = f[off:off+check_len]
    # make an array of DWORDs
    arr = array.array("I", mdata)
    # sum them
    ck = sum(arr) & 0xFFFFFFFF
    if ck == 0:
        print "%08X: found a valid-looking update" % off
        print ["Date: %02X/%02X/%4X", "Date: %02d/%02d/%4d"][is_via] % ((hdr.Date >> 24)&0xFF, (hdr.Date >> 16)&0xFF, hdr.Date & 0xFFFF)
        print "Processor signature: %08X" % hdr.ProcessorSignature
        if not is_via:
            print "Processor flags:     %08X" % hdr.ProcessorFlags
        print "Length:              %08X" % check_len
        fname = "mcode_upd_%08X.bin" % off
        print "Extracting to %s" % fname
        open(fname, "wb").write(mdata)
        return True
    # nope
    return False

def find_ucode(f, is_via):
    maxoff = len(f)
    # minimal microcode update length is 2048 bytes
    off = (maxoff-2048+8)&(~15)
    # look for BCD date ddmmyyyy
    # yyyy off-2
    # dd   off-1
    # mm   off
    print "Scanning...\n%08X" % off
    while off > 11:
        # looks like a date?
        m = ord(f[off])
        if (1 <= m <= 0x12) and (1 <= ord(f[off-1]) <= 0x31):
            if check_valid_mcode(f, off-11, maxoff, is_via):
                maxoff = off-11
                print "Scanning...\n%08X" % off
        off -= 1
        if (off & 0xFFFFF) == 0:
            print "%08X" % off
    print "\nDone"

if len(sys.argv) < 2:
    print "Usage: microcode_extract.py <file.rom> [-i|-v]"
    print "  -i: look for Intel microcode (default)"
    print "  -v: look for VIA Nano microcode"
    sys.exit(1)
else:
    fn = None
    is_via = False
    for arg in sys.argv[1:]:
        if arg == "-v":
            is_via = True
        elif arg == "-i":
            is_via = False
        else:
            fn = arg

    f = open(fn, "rb").read()
    find_ucode(f, is_via)
