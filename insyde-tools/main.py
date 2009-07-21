#! /usr/bin/env python

# Copyright (c) 2009 d6z <d6z@tnymail.com>

# MIT License. Based on code found found at
# http://marcansoft.com/blog/2009/06/enabling-intel-vt-on-the-aspire-8930g/

#~ Permission is hereby granted, free of charge, to any person
#~ obtaining a copy of this software and associated documentation
#~ files (the "Software"), to deal in the Software without
#~ restriction, including without limitation the rights to use,
#~ copy, modify, merge, publish, distribute, sublicense, and/or sell
#~ copies of the Software, and to permit persons to whom the
#~ Software is furnished to do so, subject to the following
#~ conditions:

#~ The above copyright notice and this permission notice shall be
#~ included in all copies or substantial portions of the Software.

#~ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#~ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
#~ OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#~ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
#~ HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#~ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#~ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
#~ OTHER DEALINGS IN THE SOFTWARE.

from __future__ import with_statement

from os import makedirs
from os.path import exists
from lzma import get_lzma_chunks
from util import md5sum, read_with_progress, find_all, Struct, SI, from_b64, hexbytes, substitute, chexdump
from struct import unpack_from
from pprint import pprint
from array import array

KB, MB, GB = 2 ** 10, 2 ** 20, 2 ** 30

SAVE_PATH = "data/original_bios_backup1.fd"
BIOS_START = 4 * GB - 2 * MB
BIOS_SIZE = 2 * MB


class FirmwareVolumeHeader(Struct):
    rsvd = SI("<16s")
    guid = SI("16s")
    size = SI("Q")
    magic = SI("4s")
    attributes = SI("I")
    hdrlen = SI("H")
    checksum = SI("H")
    rsvd2 = SI("3s")
    revision = SI("B")
    cruft = SI("16s")

    def showinfo(self, depth=0):
        print " " * depth, "Reserved boot zone:", hexbytes(self.rsvd)
        print " " * depth, "GUID:", hexbytes(self.guid)
        print " " * depth, "Size: 0x%x (data 0x%x)" % (self.size,
                                                       len(self.data))
        print " " * depth, "Attributes: 0x%08x" % self.attributes
        print " " * depth, "Revision: %d" % self.revision


class VariableHeader(Struct):
    magic = SI("<2s")
    status = SI("H")
    attributes = SI("I")
    nsize = SI("I")
    dsize = SI("I")
    guid = SI("16s")
    cs = SI("H")


class FirmwareVolume(object):
    def __init__(self, buffer, position, where=None):
        buffer = buffer[position:]

        try:
            fvh = FirmwareVolumeHeader(buffer)
            assert fvh.magic == "_FVH", "Invalid FirmwareVolume, wrong magic"
            assert fvh.hdrlen == FirmwareVolumeHeader.struct_size, (
                "Invalid FirmwareVolume, wrong header length "
                "0x%04x, expected 0x%04x" %
                (fvh.hdrlen, FirmwareVolumeHeader.struct_size)
            )
            assert fvh.size <= len(buffer), (
                "FirmwareVolume too big? "
                "size=0x%08x buflen=0x%08x" % (fvh.size, len(buffer))
            )

            #blockdata = buffer[fvh.py_struct.size:fvh.hdrlen]
            self.data = buffer[fvh.hdrlen:fvh.size]
            self.position = position
            self.where = where
            self.good = True
        except AssertionError, e:
            print ">>BAD FV at 0x%08x" % position, e
            self.good = False

    def has_VSS(self):
        return "$VSS" in self.data

    def __repr__(self):
        hasvss = " [VSS]" if self.has_VSS() else ""
        args = (self.position, len(self.data), self.where, hasvss)
        return "<FirmVol position=0x%06x size=0x%06x where=%s%s>" % args


class Variable(object):
    GLOBAL_VARIABLE = from_b64('Yd/ki8qT0hGqDQDgmAMrjA')
    HEADER_MAGIC = "\xAA\x55"

    def __init__(self, complete_data):
        header_size = VariableHeader.struct_size

        header = complete_data[:header_size]
        self.vh = vh = VariableHeader(header)

        assert vh.magic == self.HEADER_MAGIC, "bad magic 0x%x" % vh.magic

        total_length = vh.dsize + vh.nsize
        assert len(complete_data) >= total_length, "input not long enough"

        data = complete_data[header_size:]
        data = data[:total_length]

        self.name = data[:vh.nsize].decode("utf-16")
        self.value = data[vh.nsize:total_length]

        assert self.name.endswith(chr(0))
        self.name = self.name[:-1]

        fdata = substitute(header + data, header_size - 2, "\x00\x00")
        fdata = substitute(fdata, 2, "\x7f\x00")

        self.ccsum = self.checksum(fdata)

        if self.ccsum != vh.cs:
            raise ValueError("Checksum error")

    def __len__(self):
        return VariableHeader.struct_size + self.vh.nsize + self.vh.dsize

    def checksum(self, data):
        if len(data) % 2:
            data += chr(0)
        shorts = array("H", [])
        shorts.fromstring(data)
        return -sum(shorts) & 0xFFFF

    def showinfo(self, ts=''):
        print ts + "Variable %s" % repr(self.name)
        print ts + " Attributes: 0x%08x" % self.vh.attributes
        print ts + " Status: 0x%02x" % self.vh.status
        if self.vh.guid == self.GLOBAL_VARIABLE:
            print ts + " VendorGUID: EFI_GLOBAL_VARIABLE (%s)" % ' '.join(
                '%02x' % ord(c) for c in self.vh.guid)
        else:
            print ts + " VendorGUID: %s" % ' '.join(
                '%02x' % ord(c) for c in self.vh.guid)
        print ts + " Checksum: 0x%02x" % self.vh.cs
        print ts + "  calc 0x%04x" % self.ccsum
        print ts + " Value (0x%x bytes):" % (len(self.value))
        chexdump(self.value, ts + "  ")


class VSSData(object):
    def __init__(self, data):
        (size,) = unpack_from("I", data[4:])
        vssdata = data[16:size]

        self.vars = []

        position = 0
        while position < len(data) and vssdata[position:].startswith(Variable.HEADER_MAGIC):
            v = Variable(vssdata[position:])
            position += len(v)
            self.vars.append(v)

    def __iter__(self):
        return iter(self.vars)


class BIOS(object):

    def __init__(self):

        bios_data = self.load_bios()

        print "Loading compressed sections"
        chunks = get_lzma_chunks(bios_data)
        print " .. found %i compressed sections" % len(chunks)

        print "Locating Firmware Volumes"

        volumes = self.locate_firmware_volumes(bios_data)

        for position, data in chunks:
            with open("data/fv-compr-0x%08x" % position, "wb") as f:
                # Dump the executable with the PE header in the right place
                f.write(data[data.index("MZ"):])

            where = "[compr at 0x%x]" % position
            volumes.extend(self.locate_firmware_volumes(data, where))

        # Only good volumes
        volumes = filter(lambda fv: fv.good, volumes)
        vol_compr = filter(
            lambda fv: fv.where and "compr" in fv.where, volumes)

        print "  .. found %i FirmwareVolumes (%i compressed)" % (
            len(volumes), len(vol_compr))
        pprint(volumes)

        #~ def get_sections(fs, base=()):
            #~ result = []
            #~ if fs.subsections is not None:
                #~ for subsection in fs.subsections:
                    #~ more = (subsection.name,) if subsection.name else () #subsection.guid)
                    #~ result.append(get_sections(subsection, base + more))
            #~ else:
                #~ #more = (fs.name,) if fs.name else ()
                #~ result.append((base, fs))
            #~ return result

        def get_sections(container):
            result = []
            for section in container.sections:
                result.append(section)
                if section.sections:
                    result.append(get_sections(section))
            return result

        #from pprint import pprint

        for vol in volumes:
            if vol.position == 0x10:
                import fsdump
                fs = fsdump.FS(vol.data)

                for file in fs:
                    if file.type == 0x07:
                        print file
                        pprint(get_sections(file))
                        print
                    #for section in file:
                        #print " ", section
                        #for subsection in section.subsections:
                            #print "  ", subsection

                #fs.showinfo()
                #~ for file in fs.files:
                    #~ for section in file.sections:
                        #~ sections = get_sections(section, ())

                        #~ names, sections = zip(*sections)
                        #eprint names
                        #if ("SetupUtility",) in names:
                            #i = list(names).index(("SetupUtility",))
                            #s = sections[i]
                            #print s, s.type

                        #if ("SetupUtility",) in sections:
                            #print sections

                    #print len(file.sections)
                    #for section in file.sections:
                        #if section.type == 0x10:
                            #print section.name

            pass
            #if vol.has_VSS():
                #print "In volume:", vol
                #with open("data/fv-0x%08x" % vol.position, "wb") as f:
                #    f.write(vol.data)
                #vssd = VSSData(vol.data)
                #for var in vssd:
                    #print " %20s - 0x%04x" % (repr(var.name), len(var))
                    #if var.name == "Setup": var.showinfo()

    def locate_firmware_volumes(self, data, where=None):

        subtract_offset = lambda x: x - 0x28
        items = find_all(data, "_FVH", subtract_offset)

        print "Found the following:", items
        return [FirmwareVolume(data, position, where) for position in items]

    def retrieve_from_memory(self):
        try:
            with open("/dev/mem", "rb") as memory:
                memory.seek(BIOS_START)

                print "Reading BIOS data.."
                bios_data = read_with_progress(memory, BIOS_SIZE, 2 ** 6 * KB)

        except IOError, err:
            if err.errno == 13:
                print "Read error reading '%s' Are you root?" % path
            else:
                print "Unexpected error"
            raise

        return bios_data

    def load_bios(self):
        if not exists(SAVE_PATH):
            bios_data = self.retrieve_from_memory()

            print "Saving BIOS to '%s' md5:%s" % (SAVE_PATH, md5sum(bios_data))
            with open(SAVE_PATH, "wb") as f_bios:
                f_bios.write(bios_data)

        else:
            with open(SAVE_PATH, "rb") as f_bios:
                bios_data = f_bios.read()

            print "Opened BIOS '%s' with md5:%s" % (
                SAVE_PATH, md5sum(bios_data))

        return bios_data


def main():
    if not exists("./data/"):
        makedirs("./data/")

    bios = BIOS()

    print "Done"

if __name__ == "__main__":
    main()
