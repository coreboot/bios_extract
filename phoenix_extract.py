# Phoenix FFV BIOS dumper/extractor by roxfan
# 2012-09-12 version 0.1
# 3-clause BSD license

import sys, struct, ctypes
import os.path

uint8_t  = ctypes.c_ubyte
char     = ctypes.c_char
uint32_t = ctypes.c_uint
uint64_t = ctypes.c_uint64
uint16_t = ctypes.c_ushort

def read_struct(li, struct):
    s = struct()
    slen = ctypes.sizeof(s)
    bytes = li.read(slen)
    fit = min(len(bytes), slen)
    ctypes.memmove(ctypes.addressof(s), bytes, fit)
    return s

def get_struct(str_, off, struct):
    s = struct()
    slen = ctypes.sizeof(s)
    bytes = str_[off:off+slen]
    fit = min(len(bytes), slen)
    ctypes.memmove(ctypes.addressof(s), bytes, fit)
    return s

def strguid(raw):
  return "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}" % struct.unpack("<IHH8B", raw)

GUID_ESCD  = "FDE821FD2525954ABB9047EC5763FF9E".decode('hex')
GUID_SETUP = "D01023C054D73945B0CF9F9F2618D4A9".decode('hex')
GUID_UEFIV = "112BF272ABCEE242958A0DA1622D94E3".decode('hex')
GUID_DMIV  = "12ED2C42E5AEB94384E0AFB3E416254D".decode('hex')
GUID_HOLE  = "630FAEF68C5F1643A2EA76B9AF762756".decode('hex')

def guid2type(guid):
  gmap = {
    GUID_ESCD:  "ESCD",
    GUID_SETUP: "SETUP",
    GUID_UEFIV: "UEFIV",
    GUID_DMIV:  "DMIV",
    GUID_HOLE:  "HOLE",
  }
  if type(guid) != type(""):
      guid = "".join(map(chr, guid))
  if guid in gmap:
      return gmap[guid]
  elif guid[0] == '\xBA':
      return "FFV"
  else:
      return None

def guidname2str(s):
  s2 = s.rstrip('\0')
  if s[8] == '\xFF':
    return (s2[:8] + s2[9:]).rstrip('\0')
  elif s2.isalnum():
    return s2.rstrip('\0')
  else:
    return strguid(s)

LETTER_FILE_TYPE = {
  'A': 'ACPI',
  'B': 'BIOSCODE',
  'C': 'UPDATE',
  'D': 'DISPLAY',
  'E': 'SETUP',
  'F': 'PIF',
  'F': 'MARKS',
  'G': 'DECOMPCODE',
  'I': 'BOOTBLOCK',
  'L': 'LOGO',
  'M': 'MISER',
  'N': 'ROMPILOTLOAD',
  'O': 'NETWORK',
  'P': 'PSI',
  'P': 'ROMPILOTINT',
  'R': 'OPROM',
  'S': 'STRINGS',
  'T': 'TEMPLATE',
  'U': 'USER',
  'W': 'WAV',
  'X': 'ROMEXEC',
  '*': 'AUTOGEN',
  '$': 'BIOSENTRY',
}

FILE_TYPE_NAMES = {
  0x00  : "ALL",
  0x01  : "BIN",
  0x02  : "SECTION",
  0x03  : "CEIMAIN",
  0x04  : "PEIMAIN",
  0x05  : "DXEMAIN",
  0x06  : "PEI",
  0x07  : "DXE",
  0x08  : "COMBINED_PEIM_DRIVER",
  0x09  : "APP",
  0x0B  : "FFV",
  0xC2  : "CEI",
  0xC3  : "XIP",
  0xC4  : "BB",
  0xD0  : "SDXE",
  0xD1  : "DXESDXE",
  0xF0  : "GAP",
}

FILETYPE_BIN      = 0x01
FILETYPE_SECTION  = 0x02
FILETYPE_CEIMAIN  = 0x03
FILETYPE_PEIMAIN  = 0x04
FILETYPE_DXEMAIN  = 0x05
FILETYPE_PEI      = 0x06
FILETYPE_DXE      = 0x07
FILETYPE_COMBINED_PEIM_DRIVER  = 0x08
FILETYPE_APP      = 0x09
FILETYPE_FFV      = 0x0B
FILETYPE_CEI      = 0xC2
FILETYPE_XIP      = 0xC3
FILETYPE_BB       = 0xC4
FILETYPE_SDXE     = 0xD0
FILETYPE_DXESDXE  = 0xD1
FILETYPE_GAP      = 0xF0

SectionedTypes = [FILETYPE_PEIMAIN, FILETYPE_SECTION, FILETYPE_CEIMAIN,
                  FILETYPE_DXEMAIN, FILETYPE_PEI, FILETYPE_DXE, FILETYPE_CEI,
                  FILETYPE_BB, FILETYPE_SDXE, FILETYPE_DXESDXE]

SECTION_TYPE_NAMES = {
  0x01 : "COMPRESSION",
  0x02 : "GUID_DEFINED",
  0x10 : "PE32",
  0x11 : "PIC",
  0x12 : "TE",
  0x13 : "DXE_DEPEX",
  0x14 : "VERSION",
  0x15 : "USER_INTERFACE",
  0x16 : "COMPATIBILITY16",
  0x17 : "FIRMWARE_VOLUME_IMAGE",
  0x18 : "FREEFORM_SUBTYPE_GUID",
  0x19 : "BIN",
  0x1A : "PE64",
  0x1B : "PEI_DEPEX",
  0xC0 : "SOURCECODE",
  0xC1 : "FFV",
  0xC2 : "RE32",
  0xC3 : "XIP16",
  0xC4 : "XIP32",
  0xC5 : "XIP64",
  0xC6 : "PLACE16",
  0xC7 : "PLACE32",
  0xC8 : "PLACE64",
  0xCF : "PCI_DEVICE",
  0xD0 : "PDB",
}

SECTION_COMPRESSION            = 0x01
SECTION_GUID_DEFINED           = 0x02
SECTION_PE32                   = 0x10
SECTION_PIC                    = 0x11
SECTION_TE                     = 0x12
SECTION_DXE_DEPEX              = 0x13
SECTION_VERSION                = 0x14
SECTION_USER_INTERFACE         = 0x15
SECTION_COMPATIBILITY16        = 0x16
SECTION_FIRMWARE_VOLUME_IMAGE  = 0x17
SECTION_FREEFORM_SUBTYPE_GUID  = 0x18
SECTION_BIN                    = 0x19
SECTION_PE64                   = 0x1A
SECTION_PEI_DEPEX              = 0x1B
SECTION_SOURCECODE             = 0xC0
SECTION_FFV                    = 0xC1
SECTION_RE32                   = 0xC2
SECTION_XIP16                  = 0xC3
SECTION_XIP32                  = 0xC4
SECTION_XIP64                  = 0xC5
SECTION_PLACE16                = 0xC6
SECTION_PLACE32                = 0xC7
SECTION_PLACE64                = 0xC8
SECTION_PCI_DEVICE             = 0xCF
SECTION_PDB                    = 0xD0


"""
struct FlashFileHeader
{
  uint8 FileState;
  uint8 Flags;
  uint8 HeaderChecksum;
  uint8 DataChecksum;
  DWORD SizeAndType;
  UUID GuidName;
};
"""

class FfsFileHeader(ctypes.LittleEndianStructure):
    _fields_ = [
        ("FileState",      uint8_t), #
        ("Flags",          uint8_t), #
        ("HeaderChecksum", uint8_t), #
        ("DataChecksum",   uint8_t), #
        ("Size",           uint8_t*3), #
        ("Type",           uint8_t), #
        ("Name",           uint8_t * 16), #
    ]

    def name2str(self):
      s = "".join(map(chr, self.Name))
      return guidname2str(s)

    def is_sectioned(self):
      return self.Type in SectionedTypes

    def size(self):
      s = self.Size[:]
      return ((s[2]<<16) | (s[1] << 8) | s[0]) & 0xFFFFFF

    def pprint(self):
      nm = self.name2str()
      if nm[0] == '_' and nm[1] in LETTER_FILE_TYPE and nm[2:].isdigit():
        print "File: '%s' (%s)" % (nm, LETTER_FILE_TYPE[nm[1]])
      else:
        print "File: '%s'" % nm
      print "Type: %s (0x%02X)" % (FILE_TYPE_NAMES.get(self.Type, "Unknown"), self.Type)
      print "Flags: 0x%02X" % self.Flags
      print "Size:  0x%06X" % self.size()


class Xip1632(ctypes.LittleEndianStructure):
    _fields_ = [
        ("Name",           uint8_t * 16), #
        ("Offset",         uint32_t), #
        ("Subsystem",      uint16_t), #
    ]

    def name2str(self):
      s = "".join(map(chr, self.Name))
      return guidname2str(s)

    def pprint(self):
      print "File: '%s'" % self.name2str()
      print "Offset: 0x%08X" % (self.Offset)
      print "Subsystem: 0x%04X" % (self.Subsystem)

"""

struct FlashSectionHeader
{
  char Size[3];
  char Type;
};

"""

class FfsSectionHeader(ctypes.LittleEndianStructure):
    _fields_ = [
        ("Size",           uint8_t*3), #
        ("Type",           uint8_t), #
    ]
    def size(self):
      s = self.Size[:]
      return ((s[2]<<16) | (s[1] << 8) | s[0]) & 0xFFFFFF

    def sectype(self):
      return SECTION_TYPE_NAMES.get(self.Type, "%02X" % self.Type)

    def pprint(self):
      print "Type: %s (0x%02X)" % (SECTION_TYPE_NAMES.get(self.Type, "Unknown"), self.Type)
      print "Size:  0x%06X" % self.size()

def strdepex(data):
  off = 0
  opnames = [
   "BEFORE",
   "AFTER",
   "PUSH",
   "AND",
   "OR",
   "NOT",
   "TRUE",
   "FALSE",
   "END",
   "SOR",
  ]
  ops = []
  while off < len(data):
    opc = ord(data[off])
    off += 1
    s = "0x%02X " % opc
    if opc < len(opnames):
      s += opnames[opc]
      if opc in [0,1,2]:
        s += " " + strguid(data[off:off+16])
        off += 16
    else:
      s += " <bad opcode>"
    ops.append(s)
  return "\n".join(ops)

lzint_path = os.path.join(os.path.dirname(__file__), "unlzint")

def unlzint(data):
    import subprocess
    try:
      clen, ulen = struct.unpack("<II", data[:8])
      if clen + 8 > len(data):
          print "<bad compressed data>"
          return data
      if clen + 8 < len(data):
          data = data[:clen + 8]
      p = subprocess.Popen([lzint_path, "-", "-"], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
      outd, errd = p.communicate(input=data)
      return outd
    except:
      print "<decompression error>"
      return data

def parseSectionedFile(infile, pos1, endpos):
    sections = []
    i = 0
    while pos1 < endpos:
        sh = get_struct(infile, pos1, FfsSectionHeader)
        print "\nSection %d" % i
        sh.pprint()
        dlen = sh.size() - ctypes.sizeof(sh)
        data = infile[pos1 + ctypes.sizeof(sh):pos1 + ctypes.sizeof(sh) + dlen]
        if sh.Type == SECTION_PLACE16:
            offset = 0xFFFFFFFF
            segment = 0xFFFF
            if len(data) == 4:
              addr = struct.unpack("<I", data)[0]
              seg = addr >> 4
              offset = addr - (seg<<4)
              print "  Address: %08X (%04X:%04X)" % (addr, seg, offset)
            elif len(data) == 6:
              offset, segment = struct.unpack("<IH", data)
              print "  Address: %04X:%08X" % (segment, offset)
            else:
              print "  PLACE16 bad data length(%d)" % len(data)
        elif sh.Type in [SECTION_XIP16, SECTION_XIP32]:
            xip = get_struct(data, 0, Xip1632)
            xip.pprint()
        elif sh.Type == SECTION_COMPRESSION:
            data = unlzint(data)
            pass
        elif sh.Type == SECTION_USER_INTERFACE:
            name = data.decode('utf-16le').rstrip('\0')
            print "  Name: '%s'" % name
        elif sh.Type in [SECTION_DXE_DEPEX, SECTION_PEI_DEPEX]:
            print "  Depex:\n%s" % strdepex(data)
        sh.Data = data
        sections.append(sh)
        i += 1
        pos1 += sh.size()
    return sections

"""

struct VolumeDirHeader
{
  uint8_t field_18;
  uint8_t field_19;
  uint16_t HeaderSize;
  uint32_t VolDirSize;
};

"""

class VolumeDirHeader(ctypes.LittleEndianStructure):
    _fields_ = [
        ("f0",             uint8_t), #
        ("f1",             uint8_t), #
        ("HeaderSize",     uint16_t), #
        ("VolDirSize",     uint32_t), #
    ]

"""
struct VolumeDir2Entry
{
  UUID volumeGuid;
  _DWORD volumeStart;
  _DWORD volumeSize;
};
"""

class VolumeDir2Entry(ctypes.LittleEndianStructure):
    _fields_ = [
        ("Guid",           uint8_t * 16), #
        ("VolStart",       uint32_t), #
        ("VolSize",        uint32_t), #
    ]

    def pprint(self):
      print "Guid: '%s'" % strguid(self.Guid)
      print "Start: 0x%06X" % self.VolStart
      print "Size:  0x%06X" % self.VolSize

def parseVolumeDir2(infile, pos1, endpos, verbose):
    entries = []
    dirhdr = get_struct(infile, pos1, VolumeDirHeader)
    pos1 += dirhdr.HeaderSize
    numVols = dirhdr.VolDirSize // 0x18
    i = 0
    while i < numVols:
        ve = get_struct(infile, pos1, VolumeDir2Entry)
        if verbose:
            print "\nEntry %d" % i
            ve.pprint()
        entries.append(ve)
        i += 1
        pos1 += 0x18
    return entries

"""
struct FlashSectorInfo
{
  unsigned __int8 iGuid;
  char field_1;
  unsigned __int16 numRepeats;
  int byteSize;
  int flags;
};
"""

class FlashSectorInfo(ctypes.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("iGuid",          uint8_t),  #
        ("unk1",           uint8_t),  #
        ("numRepeats",     uint16_t), #
        ("byteSize",       uint32_t), #
        ("flags",          uint32_t), #
    ]

    def pprint(self, verbose):
        if verbose:
            print "iGuid: %d" % self.iGuid
            print "unk1:  %d" % self.unk1
            print "repeats: %d" % self.numRepeats
            print "size:  0x%08X" % self.byteSize
            print "flags: 0x%08X" % self.flags
        else:
            print "%d * 0x%08X bytes, flags: %08X" % (self.numRepeats, self.byteSize, self.flags)


"""
struct VolumeInfoBinHeader
{
  UUID volname;
  int totalHeaderSize;
  int field_14;
  int field_18;
  __int64 volumeAddress;
  unsigned __int16 numGuids;
  unsigned __int16 numSectorInfos;
};
"""

class VolumeInfoBinHeader(ctypes.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("Name",           uint8_t * 16), #
        ("HeaderSize",     uint32_t), #
        ("unk14",          uint32_t), #
        ("unk18",          uint32_t), #
        ("VolumeAddress",  uint64_t), #
        ("numGuids",       uint16_t), #
        ("numSectorInfos", uint16_t), #
    ]

    def name2str(self):
        s = "".join(map(chr, self.Name))
        return guidname2str(s)

    def parse_extras(self, infile, pos):
        pos += ctypes.sizeof(self)
        self.guids = []
        for i in range(self.numGuids):
            self.guids.append(infile[pos:pos+16])
            pos += 16
        self.sectors = []
        for i in range(self.numSectorInfos):
            si = get_struct(infile, pos, FlashSectorInfo)
            self.sectors.append(si)
            pos += 12

    def pprint(self, verbose):
        if verbose:
            print "Name: '%s'" % self.name2str()
            print "Hdr len: 0x%02X" % self.HeaderSize
            print "Unk14:   0x%02X" % self.unk14
            print "Unk18:   0x%02X" % self.unk18
            print "VolAddr: 0x%08X" % self.VolumeAddress
            print "nGuids:  0x%02X" % self.numGuids
            print "nSectorInfos: 0x%02X" % self.numSectorInfos
        else:
            print "    volume '%s' @ 0x%08X" % (self.name2str(), self.VolumeAddress)
        for i in range(len(self.sectors)):
            si = self.sectors[i]
            if si.iGuid == 0:
                sguid = "{no GUID}"
            elif si.iGuid <= len(self.guids):
                sguid = strguid(self.guids[si.iGuid-1])
            else:
                sguid = "{iGuid out of range}"
            if verbose:
                print "[sector info %d %s]" % (i, sguid)
            else:
                print "    sector %s:" % (sguid),
            si.pprint(verbose)

def replace_bad(value, deletechars):
    for c in deletechars:
        value = value.replace(c,'_')
    return value

vollist = []
filelist = []

class FfsFile:
    def __init__(self, infile, pos):
        self.filepos = pos
        self.infile = infile
        self.header = get_struct(infile, pos, FfsFileHeader)

    def get_endpos(self):
        return self.filepos + self.header.size()

    def get_romaddr(self, bioslen):
        return (self.filepos - bioslen)&0xFFFFFFFF

    def parse_sections(self):
        hdrlen = ctypes.sizeof(self.header)
        pos = self.filepos
        pos1 = pos + hdrlen
        endpos = self.get_endpos()
        self.sections = []
        self.secmap = {}
        while pos1 < endpos:
            sh = get_struct(self.infile, pos1, FfsSectionHeader)
            sh.filepos = pos1
            self.sections.append(sh)
            self.secmap[sh.Type] = sh
            pos1 += sh.size()

    def sect_data(self, stype):
        sh = self.secmap[stype]
        pos1 = sh.filepos
        return self.infile[pos1 + ctypes.sizeof(sh):pos1 + sh.size()]

    def raw_data(self):
        datapos = self.filepos + ctypes.sizeof(self.header)
        endpos = self.get_endpos()
        return self.infile[datapos:endpos]

    def pprint(self, bioslen, detailed = False):
        if detailed:
            print "ROM address: %08X, File offset: %08X" % (self.get_romaddr(bioslen), self.filepos)
            print "Header:"
            self.header.pprint()
        addr = self.get_romaddr(bioslen)
        enda = addr + self.header.size() - 1
        dsize = self.header.size() - ctypes.sizeof(self.header)
        namestr = self.header.name2str()
        if namestr[0] == '_' and namestr[1] in LETTER_FILE_TYPE and namestr[2:].isdigit():
            namestr = "%s (%s)" % (namestr, LETTER_FILE_TYPE[namestr[1]])

        s = "%08X-%08X %08X %-40s %-8s %d(0x%x)" % (self.get_romaddr(bioslen), enda, self.filepos, namestr, FILE_TYPE_NAMES.get(self.header.Type, "Unknown"), dsize, dsize)
        print s
        if self.header.is_sectioned():
            self.parse_sections()
            if detailed:
                for i in range(len(self.sections)):
                    print "    section %d:" % i
                    self.sections[i].pprint()
            # unhandled sections
            us = set(self.secmap.keys())
            if SECTION_COMPRESSION in self.secmap:
                print "    compressed payload"
                us.remove(SECTION_COMPRESSION)
            if SECTION_BIN in self.secmap:
                print "    raw payload"
                us.remove(SECTION_BIN)
            if SECTION_PE32 in self.secmap:
                print "    PE32 payload"
                us.remove(SECTION_PE32)
            if SECTION_DXE_DEPEX in self.secmap:
                print "    DXE DEPEX present"
                us.remove(SECTION_DXE_DEPEX)
            if SECTION_PEI_DEPEX in self.secmap:
                print "    PEI DEPEX present"
                us.remove(SECTION_PEI_DEPEX)
            if SECTION_USER_INTERFACE in self.secmap:
                data = self.sect_data(SECTION_USER_INTERFACE)
                name = data.decode('utf-16le').rstrip('\0')
                print "    UI name: %s" % name
                us.remove(SECTION_USER_INTERFACE)
            if SECTION_PLACE16 in self.secmap:
                data = self.sect_data(SECTION_PLACE16)
                offset = 0xFFFFFFFF
                segment = 0xFFFF
                if len(data) == 4:
                  addr = struct.unpack("<I", data)[0]
                  seg = addr >> 4
                  offset = addr - (seg<<4)
                  print "    PLACE16: %08X (%04X:%04X)" % (addr, seg, offset)
                elif len(data) == 6:
                  offset, segment = struct.unpack("<IH", data)
                  print "    PLACE16: %04X:%08X" % (segment, offset)
                else:
                  print "    PLACE16 bad data length(%d)" % len(data)
                us.remove(SECTION_PLACE16)
            if SECTION_XIP16 in self.secmap:
                xip = get_struct(self.sect_data(SECTION_XIP16), 0, Xip1632)
                print "    XIP16 file '%s', entry = 0x%08X, subsystem = %02X" % (xip.name2str(), xip.Offset, xip.Subsystem)
                us.remove(SECTION_XIP16)
            if SECTION_XIP32 in self.secmap:
                xip = get_struct(self.sect_data(SECTION_XIP32), 0, Xip1632)
                print "    XIP32 file '%s', entry = 0x%08X, subsystem = %02X" % (xip.name2str(), xip.Offset, xip.Subsystem)
                us.remove(SECTION_XIP32)
            if us:
                print "    unhandled sections:", map(hex,us)

def parseFfsFile(infile, pos, bioslen, volno = None):
    ff = get_struct(infile, pos, FfsFileHeader)
    if ff.size() == 0 or ff.FileState != 0xF8:
      return
    ff.pprint()
    pos0 = pos
    hdrlen = ctypes.sizeof(ff)
    pos1 = pos + hdrlen
    pos += ff.size()
    fname = ff.name2str()
    if ff.Type in [FILETYPE_PEIMAIN, FILETYPE_SECTION, FILETYPE_CEIMAIN,
                   FILETYPE_DXEMAIN, FILETYPE_PEI, FILETYPE_DXE, FILETYPE_CEI,
                   FILETYPE_BB, FILETYPE_SDXE, FILETYPE_DXESDXE]:
        ss = parseSectionedFile(infile, pos1, pos)
        for i in range(len(ss)):
            fname2 = "%08X_%s_S%d_%s.bin" % (pos0, fname, i, ss[i].sectype())
            if volno != None:
                fname2 = "V%d_" % volno + fname2
            fname2 = replace_bad(fname2, '\/:*?"<>|')
            print " ==> %s" % fname2
            open(fname2,"wb").write(ss[i].Data)

    if ff.Type == FILETYPE_BIN:
        if ff.name2str() == "volumedir.bin2" and volno == None:
            vols = parseVolumeDir2(infile, pos1, pos, False)
            for i in range(len(vols)):
                v = vols[i]
                s = (v.VolStart + bioslen) & 0xFFFFFFFF # v.VolStart - 0xFF800000
                print "\n***\nVolume %d: address %08X, file offset %08X, size %08X\n***\n" % (i, v.VolStart, s, v.VolSize)
                parse_range(infile, s, s + v.VolSize, bioslen, i)
            vollist.extend(vols)

        fname2 = "%08X_%s.bin" % (pos0, fname)
        if volno != None:
            fname2 = "V%d_" % volno + fname2

        fname2 = replace_bad(fname2, '\/:*?"<>|')

        print " ==> %s" % fname2
        dlen = ff.size() - hdrlen
        data = infile[pos1:pos1 + dlen]
        open(fname2,"wb").write(data)
    return pos

class FfsVolume:
    def __init__(self, infile, pos, size, guid, ord):
        self.filepos = pos
        self.infile = infile
        self.size   = size
        self.guid   = guid
        self.volInfo = None
        self.ord = ord
        self.files = []

    def parse_volinfo(self, data):
        self.volInfo = get_struct(data, 0, VolumeInfoBinHeader)
        self.volInfo.parse_extras(data, 0)

    def parse_files(self):
        pos = self.filepos
        maxpos = pos + self.size
        while True:
            while pos < maxpos and self.infile[pos] in ['\xFF', '\x00']:
                pos += 1
            if self.infile[pos] != '\xF8':
                return
            if pos >= maxpos:
                break
            ff = FfsFile(self.infile, pos)
            pos = ff.get_endpos()
            self.files.append(ff)
            if ff.header.Type == FILETYPE_BIN and ff.header.name2str() == "volumeinfo.bin":
                self.parse_volinfo(ff.raw_data())
            # skip padding after file
            while pos < maxpos and infile[pos] in ['\xFF', '\x00']:
                pos += 1

    def pprint(self, bioslen, verbose):
        if self.guid[0] == 0xBA:
            self.parse_files()
        addr = (self.filepos - bioslen)&0xFFFFFFFF
        namestr = strguid(self.guid)
        gt = guid2type(self.guid)
        if gt == None:
            gt = "VOL"
        s = "%08X-%08X %08X %-40s %-8s %d(0x%x)" % (addr, addr + self.size-1, self.filepos, namestr, "<%s %d>" % (gt, self.ord), self.size, self.size)
        # print "\n***\nVolume address %08X, file offset %08X, size %08X\n***\n" % (addr, self.filepos, self.size)
        print s
        if self.volInfo:
            self.volInfo.pprint(verbose)
        for f in self.files:
            f.pprint(bioslen, verbose)

def parse_range(infile, pos, maxpos, bioslen, volno = None):
    start = pos
    while pos < maxpos:
        nextpos = parseFfsFile(infile, pos, bioslen, volno)
        if nextpos == None:
            break
        print "\nfile offset: %08X" % pos
        pos = nextpos
        while pos < maxpos and infile[pos] in ['\xFF', '\x00']:
            pos += 1
    if volno != None and start == pos and pos < maxpos:
        fname = "V%d_%08X.bin" % (volno, pos)
        print " ==> %s" % fname
        open(fname,"wb").write(infile[pos:maxpos])

class PhoenixModuleHeader(ctypes.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("Signature",     uint32_t),  #
        ("Signature2",    uint8_t*3), #
        ("Id",            uint8_t),   #
        ("Type",          uint8_t),   #
        ("HeadLen",       uint8_t),   #
        ("Compression",   uint8_t),   #
    ("Address",       uint32_t),
    ("ExpLen",        uint32_t),
    ("FragLen",       uint32_t),
    ("NextFrag",      uint32_t),
    ]

    def pprint(self):
        print "Signature: 0x%08X" % self.Signature
        print "Id: %d" % self.Id
        print "Type: %02X" % self.Type
        print "HeadLen: %02X" % self.HeadLen
        print "Compression: %d" % self.Compression
        addr = self.Address
        seg = addr >> 4
        offset = addr - (seg<<4)
        print "Address: %08X (%04X:%04X)" % (addr, seg, offset)
        print "ExpLen: %X" % (self.ExpLen)
        print "FragLen: %X" % (self.FragLen)
        print "NextFrag: %X" % (self.NextFrag)

"""
   struct PhoenixModule {
    uint32_t Signature;
    uint8_t Signature2[3];
    uint8_t Id;
    uint8_t Type;
    uint8_t HeadLen;
    uint8_t Compression;
    uint16_t Offset;
    uint16_t Segment;
    uint32_t ExpLen;
    uint32_t FragLength;
    uint32_t NextFrag;
    } *Module;
"""

def parse_trailer(infile, pos, maxpos):
    start = pos
    i = 0
    while pos < maxpos:
        if infile[pos:pos+4] == "BC\xD6\xF1":
            modhdr = get_struct(infile, pos, PhoenixModuleHeader)
            modhdr.pprint()
            pos += modhdr.HeadLen
            addr = modhdr.Address
            seg = addr >> 4
            offset = addr - (seg<<4)
            fname = "%08X_%04X_%04X.bin" % (pos, seg, offset)
            print " ==> %s" % fname
            open(fname,"wb").write(infile[pos:pos+modhdr.ExpLen])
            pos += modhdr.ExpLen
        elif infile[pos:pos+8] == "FLASHDXE":
            pos += 8
            while pos < maxpos:
                nextpos = parseFfsFile(infile, pos, maxpos)
                if nextpos == None:
                    break
                print "\nfile offset: %08X" % pos
                pos = nextpos
        else:
            print "\nunknown header at %08X" % pos
            break


if len(sys.argv) < 1:
    print "Usage: phoenix_scan.py BIOS.BIN [-d] [-t] [-v]"
    print "-d: extract modules into files"
    print "-t: extract the trailer parts"
    print "-v: verbose info about flash layout"
    sys.exit(1)

inf = open(sys.argv[1],"rb")
infile = inf.read()
pos = infile.find("volumedi\xFFr.bin2")
if pos != -1:
    pos -= 8
    print "Found Volume Directory v2 at %08X\n" % (pos)
else:
    print "Volume dir not found; FFS dump won't be available"
    #sys.exit(1)

dump_all = False
dump_trailer = False
print_verbose = False
for a in sys.argv[2:]:
    if a == '-d':
        dump_all = True
    elif a == '-t':
        dump_trailer = True
    elif a == '-v':
        print_verbose = True

alllen  = len(infile)
bioslen = alllen & 0xFFFF0000
if dump_all and pos != -1:
    parse_range(infile, pos, alllen, bioslen)
if dump_trailer:
    parse_trailer(infile, bioslen, alllen)

if pos != -1:
    voldir = FfsFile(infile, pos)
    vols = parseVolumeDir2(voldir.raw_data(), 0, 0, print_verbose)
    ffvols = []
    for i in range(len(vols)):
        v = vols[i]
        pos = (v.VolStart + bioslen) & 0xFFFFFFFF
        ffv = FfsVolume(infile, pos, v.VolSize, v.Guid, i)
        ffvols.append(ffv)

    print "FFS contents:"
    for fv in ffvols:
        print ""
        fv.pprint(bioslen, print_verbose)
