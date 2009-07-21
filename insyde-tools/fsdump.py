#! /usr/bin/env python

from __future__ import with_statement

from util import from_b64
import sys
import struct


def fguid(s):
    a, b, c, d, e = struct.unpack("<IHHH6s", s)
    return "%08x-%04x-%04x-%04x-%s" % (a, b, c, d, ''.join('%02x' % ord(c) for c in e))


class FFSSection(object):
    MAGIC_GUIDEF = from_b64("sM0b/DF9qkmTaqRgDZ3Qgw")

    def __init__(self, data):
        hdr = data[:0x4]
        self.size, self.type = struct.unpack("<3sB", hdr)
        self.size = struct.unpack("<I", self.size + "\x00")[0]
        data = data[0x4:self.size]
        self.data = data
        self.sections = []
        self.name = None
        if self.type == 0x02:
            dguid = self.data[:0x10]
            if dguid == FFSSection.MAGIC_GUIDEF:
                data = data[0x18:]
                while len(data):
                    s = FFSSection(data)
                    self.sections.append(s)
                    data = data[(s.size + 3) & (~3):]
        elif self.type == 0x15:
            self.name = self.data.decode("utf-16le").split("\0")[0]

    TI = {
        0x01: ("compression", "Compression section"),
        0x02: ("guiddef", "GUID-defined section"),
        0x03: ("disp", "Disposable section"),
        0x10: ("pe", "PE Image"),
        0x11: ("pic.pe", "PE PIC Image"),
        0x12: ("te", "TE Image"),
        0x13: ("dxe.depex", "DXE Dependency Expression"),
        0x14: ("ver", "Version"),
        0x15: ("name", lambda self: "User Interface name: '%s'" % self.name),
        0x16: ("16bit", "Compatibility16"),
        0x17: ("fvi", "Firmware Volume Image"),
        0x18: ("guid", "Freeform Subtype GUID"),
        0x19: ("raw", "RAW"),
        0x1b: ("pei.depex", "PEI Dependency Expression"),
        0x1c: ("smm.depex", "SMM Dependency Expression"),
    }

    def get_type_info(self):

        if self.type not in self.TI:
            return ("unknown.0x%02x.bin" % self.type,
                    "<Unknown type=0x%02x>" % self.type)

        small, large = self.TI[self.type]
        if callable(large):
            large = large(self)
        return small, large

    def showinfo(self, ts=''):
        if self.type == 0x02:
            print ts + "GUID-defined section"
            if self.sections:
                print ts + " CRC32 subsection container:"
                for i, s in enumerate(self.sections):
                    print ts + "  Subsection %d: type 0x%02x, size 0x%x" % (
                        i, s.type, s.size)
                    s.showinfo(ts + "   ")

        else:
            print ts + self.get_type_info()[1]

    def dump(self, base):

        if self.sections:
            for i, s in enumerate(self.sections):
                s.dump("%s.sub%d" % (base, i))
            return

        name = "%s.%s" % (base, self.get_type_info()[0])
        with open(name, "wb") as fd:
            fd.write(self.data)

        print name

    def __repr__(self):
        return "<FFS Section name=%s type=%s size=0x%x>" % (self.name, self.get_type_info()[0], self.size)


class FFSFile(object):
    def __repr__(self):
        return "<FFS File size=0x%x(0x%x) type=0x%02x state=0x%02x GUID=%s>" % (self.size - 0x18, len(self.data), self.type, self.state, fguid(self.guid))

    def __init__(self, data):
        hdr = data[:0x18]
        self.guid, self.checksum, self.type, self.attributes, self.size, self.state = struct.unpack("<16sHBB3sB", hdr)
        self.size = struct.unpack("<I", self.size + "\x00")[0]
        data = data[0x18:self.size]
        self.data = data
        self.sections = []
        if self.type == 0xf0:
            # used to set self.sections
            pass
        else:
            while len(data):
                s = FFSSection(data)
                self.sections.append(s)
                data = data[(s.size + 3) & (~3):]

    def showinfo(self, ts=''):
        print ts + "GUID:", fguid(self.guid)
        print ts + "Size: 0x%x (data 0x%x)" % (self.size, len(self.data))
        print ts + "Type: 0x%02x" % self.type
        print ts + "Attributes: 0x%02x" % self.attributes
        print ts + "State: 0x%02x" % (self.state ^ 0xFF)
        if self.sections:
            for i, s in enumerate(self.sections):
                print ts + " Section %d: type 0x%02x, size 0x%x" % (
                    i, s.type, s.size)
                s.showinfo(ts + "  ")
        else:
            print ts + "This is a padding file"

    def dump(self):
        if self.sections:
            appn = ""
            for s in self.sections:
                if s.name is not None:
                    appn = "-" + s.name
                elif s.sections is not None:
                    for s in s.sections:
                        if s.name is not None:
                            appn = "-" + s.name
            for i, s in enumerate(self.sections):
                s.dump("%s%s.sec%d" % (fguid(self.guid), appn, i))

    def __iter__(self):
        return iter(self.sections)


class FS(object):
    def __init__(self, data):
        self.files = []
        while len(data) and data[:16] != ("\xff" * 16):
            f = FFSFile(data)
            self.files.append(f)
            data = data[(f.size + 7) & (~7):]

    def showinfo(self, ts=''):
        for i, f in enumerate(self.files):
            print ts + "File %d:" % i
            f.showinfo(ts + ' ')

    def dump(self):
        for f in self.files:
            f.dump()

    def __iter__(self):
        return iter(self.files)

if __name__ == "__main__":
    f = open(sys.argv[1], "rb")

    d = f.read()

    fs = FS(d)

    print "Filesystem:"
    fs.showinfo(' ')
    print "Dumping..."
    fs.dump()
