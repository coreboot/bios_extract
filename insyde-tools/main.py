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

import fsdump

from os import makedirs
from os.path import exists
from lzma import get_lzma_chunks
from struct import unpack_from
from pprint import pprint
from array import array
from dumpsetup import StringTable, Form, FormOp
from util import (md5sum, read_with_progress, find_all, find_all_backwards,
                  Struct, SI, from_b64, hexbytes, substitute, chexdump, FindBadPosition,
                  FindStopSearching
                  )

#~ SAVE_PATH  = "data/original_bios-zchef.fd"; WHAT = "zchef"
#~ SAVE_PATH  = "data/original_bios-dhlacik.fd"; WHAT = "dhlacik"
#~ SAVE_PATH  = "data/original_bios-FaithX.fd"; WHAT = "FaithX"
#~ SAVE_PATH  = "data/original_bios-mine.fd"; WHAT = "mine"
#~ SAVE_PATH  = "data/KM2_110.fd"; WHAT = "v110"

SAVE_PATH = "data/original_bios-mine.fd"
WHAT = "mine"


KB, MB, GB = 2 ** 10, 2 ** 20, 2 ** 30

SAVE_PATH = "data/original_bios_backup1.fd"
BIOS_START = 4 * GB - 2 * MB
BIOS_SIZE = 2 * MB

# Only necessary to modify these if you are reading from /dev/mem
# If reading from BIOS dump, they are ignored.
# If someone knows how to detect the bios size automatically, that would
# be very useful
BIOS_SIZE = 2 * MB
BIOS_START = 4 * GB - BIOS_SIZE


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
    # Some bioses do not have a checksum.
    # Comment me out if your bios does not work
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
            #print ">>BAD FV at 0x%08x" % position, e
            self.good = False

    def has_VSS(self):
        return "$VSS" in self.data

    def __repr__(self):
        hasvss = " [VSS]" if self.has_vss() else ""
        args = (self.position, len(self.data), self.where, hasvss)
        return "<FirmVol position=0x%06x size=0x%06x where=%s%s>" % args


class Variable(object):
    GLOBAL_VARIABLE = from_b64('Yd/ki8qT0hGqDQDgmAMrjA')
    HEADER_MAGIC = "\xAA\x55"
    ACTIVE = 0x7F

    def __init__(self, complete_data):
        header_size = VariableHeader.struct_size

        header = complete_data[:header_size]
        self.vh = vh = VariableHeader(header)
        print "Blah:", hexbytes(vh.guid)

        assert vh.magic == self.HEADER_MAGIC, "bad magic 0x%x" % vh.magic

        total_length = vh.dsize + vh.nsize
        assert len(complete_data) >= total_length, "input not long enough"

        data = complete_data[header_size:]
        data = data[:total_length]

        nullterm = data.index("\x00\x00") + 1
        strend = nullterm if nullterm < vh.nsize else vh.nsize

        self.name = data[:strend].decode("utf-16le")
        self.value = data[vh.nsize:total_length]

        # Set the checksum to 0, and the status to 0x7F
        fdata = substitute(header + data, header_size - 2, "\x00\x00")
        fdata = substitute(fdata, 2, "\x7F\x00")

        self.ccsum = self.checksum(fdata)

        #assert self.ccsum == vh.cs, "Checksum Error"

    def __repr__(self):
        return "<Variable status=0x%02x size=0x%03x name=%s>" % (self.vh.status, self.vh.dsize, self.name)

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
            print ts + (" VendorGUID: EFI_GLOBAL_VARIABLE (%s)" %
                        ' '.join('%02x' % ord(c) for c in self.vh.guid))
        else:
            print ts + (" VendorGUID: %s" %
                        ' '.join('%02x' % ord(c) for c in self.vh.guid))
        #print ts+" Checksum: 0x%02x"%self.vh.cs
        #print ts+"  calc 0x%04x"%self.ccsum
        print ts + " Value (0x%x bytes):" % (len(self.value))
        chexdump(self.value, ts + "  ")


class VSSData(object):
    def __init__(self, data):
        (size,) = unpack_from("I", data[4:])
        assert size < len(data), (
            "Too big! size = %i len = %i" % (size, len(data))
        )

        vssdata = data[0x10:size]

        self.vars = []
        self.size = size

        position = 0
        while (position < len(data) and
               vssdata[position:].startswith(Variable.HEADER_MAGIC)):
            print "Creating variable at", position
            v = Variable(vssdata[position:])
            position += len(v)
            self.vars.append(v)

    def __repr__(self):
        return "<VSSData len=%i size=0x%x>" % (len(self.vars), self.size)

    def __iter__(self):
        return iter(self.vars)


class BIOS(object):
    def __init__(self, from_where=None):
        "Create a BIOS object"

        bios_data = self.load_bios(from_where)

        print "Operating on BIOS %s size = 0x%x" % (SAVE_PATH, len(bios_data))

        print "Loading compressed sections"
        compressed_chunks = get_lzma_chunks(bios_data)
        print " .. found %i compressed sections" % len(compressed_chunks)

        print "Locating Firmware Volumes"
        volumes = self.locate_firmware_volumes(bios_data)

        for position, data in compressed_chunks:
            #if False:
                #with open("data/fv-compr-0x%08x" % position, "wb") as f:
                    # Dump the executable with the PE header in the right place
                    #f.write(data[data.index("MZ"):])

            where = "[compr at 0x%x]" % position
            volumes.extend(self.locate_firmware_volumes(data, where))

        # Only good volumes
        volumes = filter(lambda fv: fv.good, volumes)
        vol_compr = filter(
            lambda fv: fv.where and "compr" in fv.where, volumes)

        print ("  .. found %i FirmwareVolumes (%i compressed)" %
               (len(volumes), len(vol_compr)))

        setup_utility = self.locate_setup_utility(vol_compr)

        TYPE_PE = 0x10
        setup_utility_pe = self.get_section_type(setup_utility[1], TYPE_PE)

        dump_filename = "data/SetupUtility-%s.pe" % WHAT
        if not exists(dump_filename):

            pe = setup_utility_pe

            with open(dump_filename, "wb") as fd:
                fd.write(pe.data)

            print "Wrote SetupUtility to %s" % dump_filename
            print "  Size = 0x%x MD5: %s" % (len(pe.data), md5sum(pe.data))

        self.locate_packs(setup_utility_pe.data)

        self.locate_vss(volumes)

    def locate_vss(self, volumes):

        for vss_volume in filter(FirmwareVolume.has_vss, volumes):
            print "Have vss_volume:", vss_volume
            try:
                vssdata = VSSData(vss_volume.data)
            except AssertionError, e:
                #print " .. failed to load: '%s'" % e
                continue

            print vss_volume, vssdata

            for var in vssdata:
                if var.vh.status == Variable.ACTIVE:
                    print var
                    if var.name == "Setup":
                        var.showinfo()

    def locate_packs(self, setuputility_binary):
        "Searches for Forms and the English StringTable using a set of heuristics"

        # 1st byte: upper bits from length: almost certainly zero
        # 2-3: Short typecode, 3 == form
        # 4-5: 0x0e is the formset opcode, and 0x24 is is length
        # This magic string appears three bytes into the header
        form_magic = "\x00\x03\x00\x0e\x24"
        form_magic_offset = -3

        # HiipackHeaderSize
        HHS = 6
        english_attribute_magic = "\x00" * 4

        def create_stringtable(magic_location):
            def test_stringtable(poss_header_location):
                # We started at attributes, start at lnoff
                poss_header_location -= 12

                dat = setuputility_binary[poss_header_location:]
                lnoff, plnoff, count, attributes = unpack_from("<IIII", dat)

                # This check is extraordinarily unlikely to succeed in the case
                # we haven't actually found the string table header.
                if (magic_location - poss_header_location + HHS == lnoff and
                        magic_location - poss_header_location + HHS + 8 == plnoff):

                    string_table_loc = poss_header_location - HHS
                    stable = StringTable(setuputility_binary, string_table_loc)
                    raise FindStopSearching(stable)

                raise FindBadPosition

            result = find_all_backwards(setuputility_binary,
                                        english_attribute_magic,
                                        test_stringtable, magic_location)

            if result:
                raise FindStopSearching(result)
            raise FindBadPosition

        string_magic = u"eng\x00English".encode("utf-16le")
        english_stringtable = find_all(setuputility_binary, string_magic,
                                       create_stringtable)

        if not english_stringtable:
            raise RuntimeError

        #~ english_stringtable.showinfo()

        def create_form(location):
            location += form_magic_offset
            try:
                return Form(setuputility_binary, location, english_stringtable)
            except AttributeError:
                raise FindBadPosition

        forms = find_all(setuputility_binary, form_magic, create_form)

        found = False

        # We've finally reached the bottom layer!
        # Now we just search for the location of the VT switch..
        for form in forms:
            for opcode in form.fetch_opcodes(FormOp.EFI_IFR_ONE_OF_OP):
                qid, width, pid, hid = unpack_from("<HBHH", opcode.payload)
                prnt_string = english_stringtable[pid]
                help_string = english_stringtable[hid]
                args = (qid, width, prnt_string, help_string)
                if "Vanderpool" in help_string:
                    found = True
                    print "Location = 0x%03x<%d>, name='%s' help='%s'" % args

        if not found:
            print "Sorry, I couldn't locate the VT flag? :("

    def get_sections(self, container):
        "Return a recursive list of sections"
        result = []
        for section in container.sections:
            result.append(section)
            if section.sections:
                result.append(self.get_sections(section))
        return result

    def get_section_type(self, sections, type):
        "Return the first section that has type `type`"
        for section in sections:
            if section.type == type:
                return section

    def locate_setup_utility(self, volumes):
        "Locate the SetupUtility section within `volumes`"

        for vol in volumes:
            if vol.position != 0x10:
                continue

            for file in fsdump.FS(vol.data):
                FILE_TYPE_CONTAINS_PE = 0x07
                if file.type != FILE_TYPE_CONTAINS_PE:
                    continue

                sections = self.get_sections(file)

                TYPE_NAME = 0x15
                name_section = self.get_section_type(sections[1], TYPE_NAME)

                if name_section.name == "SetupUtility":
                    return sections

        raise RuntimeError("Shouldn't get here, seems we couldn't "
                           "find the SetupUtility :/")

    def locate_firmware_volumes(self, data, where=None):
        """Search through `data` looking for firmware volume headers
        `where` optionally specifies where it came from (e.g. compressed section)
        """
        FVH_OFFSET_WITHIN_HEADER = 0x28
        subtract_offset = lambda x: x - FVH_OFFSET_WITHIN_HEADER
        items = find_all(data, "_FVH", subtract_offset)

        #print "Found the following:", items

        return [FirmwareVolume(data, position, where) for position in items]

    def retrieve_from_memory(self):
        "Download the BIOS directly from memory. Must be root to do this."
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

    def load_bios(self, from_where=None):
        "If the desired bios file doesn't exist, try to load it from memory"

        if from_where is None:
            from_where = SAVE_PATH

        if not exists(from_where):
            bios_data = self.retrieve_from_memory()

            print "Saving BIOS to '%s' md5:%s" % (SAVE_PATH, md5sum(bios_data))
            with open(SAVE_PATH, "wb") as f_bios:
                f_bios.write(bios_data)

        else:
            with open(from_where, "rb") as f_bios:
                bios_data = f_bios.read()

            print "Opened BIOS '%s' with md5:%s" % (
                from_where, md5sum(bios_data))

        return bios_data


def main():
    if not exists("./data/"):
        makedirs("./data/")

    bios = BIOS()

    print "Done"

if __name__ == "__main__":
    main()
