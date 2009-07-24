#!/usr/bin/python

import sys
import struct
import codecs

sys.stdout = codecs.getwriter('utf-8')(sys.stdout)

STRING_TABLE = 0x6F80  # FaithX
STRING_TABLE = 0x10b60  # Original
#STRING_TABLE = 0x108e0 + 12 # No idea
STRING_TABLE = 0x10b30  # Mine?

storage_map = {}

FORMS = [
    ("Exit", 0xC600),
    #('Exit',    0xc640),
    #('Boot',    0xc6f0),
    #('Power',    0xc810),
    #('Security',0xd140),
    #('Advanced',0xd390),
    #('Main',    0x106b0),
    #('OEM',        0x10a20)
]


def fguid(s):
    a, b, c, d = struct.unpack("<IHH8s", s)
    ds = ''.join('%02x' % ord(c) for c in d)
    return "%08x-%04x-%04x-%s-%s" % (a, b, c, ds[:4], ds[4:])


def hexdump(s):
    return ' '.join('%02x' % ord(c) for c in s)


class HiiPack(object):
    def __init__(self, data, offset):
        self.offset = offset
        #print "  Constructing HiiPack at 0x%x" % offset
        hdr = data[offset:offset + 6]
        #print "   Has HDRlen: 0x%x DataLen: 0x%x" % (len(hdr), len(data))
        #print repr(hdr)
        self.length, self.type = struct.unpack("<IH", hdr)
        #print "   Real Length: 0x%x, type = 0x%x" % (self.length, self.type)
        #print "   Table end 0x%x" % (offset+len(hdr)+self.length)
        self.end_offset = offset + len(hdr) + self.length
        assert self.length + len(hdr) < len(data)
        self.data = data[offset + 6:offset + self.length]


class StringTable(HiiPack):
    def __init__(self, data, offset):
        #print "Constructing StringTable"
        HiiPack.__init__(self, data, offset)
        assert self.type == 0x2
        self.strings = []
        hdr = self.data[:16]
        lnoff, plnoff, count, attributes = struct.unpack("<IIII", hdr)
        #print type(hdr), len(hdr), len(self.data), count
        offsets = struct.unpack("<%dI" % count, self.data[16:16 + count * 4])
        self.name = self._getstring(lnoff)
        self.printablename = self._getstring(plnoff)
        for i in range(count):
            self.strings.append(self._getstring(offsets[i]))

    def _getstring(self, off):
        return self.data[off - 6:].decode('utf-16le').split('\0')[0]

    def __getitem__(self, a):
        return self.strings.__getitem__(a)

    def showinfo(self, ts=''):
        print ts + "String table:"
        print ts + " Language: %s (%s)" % (self.name, self.printablename)
        print ts + " String count: %d" % len(self.strings)


class FormOp(object):
    EFI_IFR_FORM_OP = 0x01
    EFI_IFR_SUBTITLE_OP = 0x02
    EFI_IFR_TEXT_OP = 0x03
    EFI_IFR_GRAPHIC_OP = 0x04
    EFI_IFR_ONE_OF_OP = 0x05
    EFI_IFR_CHECKBOX_OP = 0x06
    EFI_IFR_NUMERIC_OP = 0x07
    EFI_IFR_PASSWORD_OP = 0x08
    EFI_IFR_ONE_OF_OPTION_OP = 0x09  # ONEOF OPTION field
    EFI_IFR_SUPPRESS_IF_OP = 0x0A
    EFI_IFR_END_FORM_OP = 0x0B
    EFI_IFR_HIDDEN_OP = 0x0C
    EFI_IFR_END_FORM_SET_OP = 0x0D
    EFI_IFR_FORM_SET_OP = 0x0E
    EFI_IFR_REF_OP = 0x0F
    EFI_IFR_END_ONE_OF_OP = 0x10
    EFI_IFR_END_OP = EFI_IFR_END_ONE_OF_OP
    EFI_IFR_INCONSISTENT_IF_OP = 0x11
    EFI_IFR_EQ_ID_VAL_OP = 0x12
    EFI_IFR_EQ_ID_ID_OP = 0x13
    EFI_IFR_EQ_ID_LIST_OP = 0x14
    EFI_IFR_AND_OP = 0x15
    EFI_IFR_OR_OP = 0x16
    EFI_IFR_NOT_OP = 0x17
    EFI_IFR_END_IF_OP = 0x18  # for endif of
                                    # inconsistentif,
                                    # suppressif, grayoutif
    EFI_IFR_GRAYOUT_IF_OP = 0x19
    EFI_IFR_DATE_OP = 0x1A
    EFI_IFR_TIME_OP = 0x1B
    EFI_IFR_STRING_OP = 0x1C
    EFI_IFR_LABEL_OP = 0x1D
    EFI_IFR_SAVE_DEFAULTS_OP = 0x1E
    EFI_IFR_RESTORE_DEFAULTS_OP = 0x1F
    EFI_IFR_BANNER_OP = 0x20
    EFI_IFR_INVENTORY_OP = 0x21
    EFI_IFR_EQ_VAR_VAL_OP = 0x22
    EFI_IFR_ORDERED_LIST_OP = 0x23
    EFI_IFR_VARSTORE_OP = 0x24
    EFI_IFR_VARSTORE_SELECT_OP = 0x25
    EFI_IFR_VARSTORE_SELECT_PAIR_OP = 0x26
    EFI_IFR_LAST_OPCODE = EFI_IFR_VARSTORE_SELECT_PAIR_OP
    EFI_IFR_OEM_OP = 0xFE
    EFI_IFR_NV_ACCESS_COMMAND = 0xFF

    INDENTS = {
        #0 : 0,
        #0x73 : 0,
        EFI_IFR_FORM_OP: 1,
        EFI_IFR_SUBTITLE_OP: 0,
        EFI_IFR_TEXT_OP: 0,
        EFI_IFR_GRAPHIC_OP: 0,
        EFI_IFR_ONE_OF_OP: 1,
        EFI_IFR_CHECKBOX_OP: 0,
        EFI_IFR_NUMERIC_OP: 0,
        EFI_IFR_PASSWORD_OP: 0,
        EFI_IFR_ONE_OF_OPTION_OP: 0,
        EFI_IFR_SUPPRESS_IF_OP: 1,
        EFI_IFR_END_FORM_OP: -1,
        EFI_IFR_HIDDEN_OP: 0,
        EFI_IFR_END_FORM_SET_OP: -1,
        EFI_IFR_FORM_SET_OP: 1,
        EFI_IFR_REF_OP: 0,
        EFI_IFR_END_OP: -1,
        EFI_IFR_INCONSISTENT_IF_OP: 0,
        EFI_IFR_EQ_ID_VAL_OP: 0,
        EFI_IFR_EQ_ID_ID_OP: 0,
        EFI_IFR_EQ_ID_LIST_OP: 0,
        EFI_IFR_AND_OP: 0,
        EFI_IFR_OR_OP: 0,
        EFI_IFR_NOT_OP: 0,
        EFI_IFR_END_IF_OP: -1,
        EFI_IFR_GRAYOUT_IF_OP: 1,
        EFI_IFR_DATE_OP: 0,
        EFI_IFR_TIME_OP: 0,
        EFI_IFR_STRING_OP: 0,
        EFI_IFR_LABEL_OP: 0,
        EFI_IFR_SAVE_DEFAULTS_OP: 0,
        EFI_IFR_RESTORE_DEFAULTS_OP: 0,
        EFI_IFR_BANNER_OP: 0,
        EFI_IFR_INVENTORY_OP: 0,
        EFI_IFR_EQ_VAR_VAL_OP: 0,
        EFI_IFR_ORDERED_LIST_OP: 0,
        EFI_IFR_VARSTORE_OP: 0,
        EFI_IFR_VARSTORE_SELECT_OP: 0,
        EFI_IFR_VARSTORE_SELECT_PAIR_OP: 0,
        EFI_IFR_LAST_OPCODE: 0,
        EFI_IFR_OEM_OP: 0,
        EFI_IFR_NV_ACCESS_COMMAND: 0,
    }

    def __init__(self, data, stable):
        self.stable = stable
        self.opcode, self.length = struct.unpack("<BB", data[:2])
        self.payload = data[2:self.length]
        if self.opcode not in self.INDENTS:
            raise RuntimeError("Undefined opcode: 0x%x" % self.opcode)
        self.indent = self.INDENTS[self.opcode]

    def get_info(self):
        guid, fsid, hid, cb, cls, subcls, nvsize = struct.unpack(
            "<16sHHQHHH", self.payload)
        return self.stable[fsid]

    def showinfo(self, s, ts=''):
        if self.opcode == self.EFI_IFR_FORM_OP:
            id, title = struct.unpack("<HH", self.payload)
            print ts + "Form ID:0x%04x Name:'%s'" % (id, s[title])
        elif self.opcode == self.EFI_IFR_SUBTITLE_OP:
            print ts + "Subtitle: '%s'" % s[struct.unpack("<H",
                                                          self.payload)[0]]
        elif self.opcode == self.EFI_IFR_TEXT_OP:
            if len(self.payload) != 9:
                print ts + "BROKEN TEXT OP %r" % self.payload
            else:
                hid, tid, t2id, flags, key = struct.unpack(
                    "<HHHBH", self.payload)
                print ts + "Text: '%s','%s' Flags:0x%x Key:0x%x" % (
                    s[tid], s[t2id], flags, key)
                if s[hid] and s[hid] != ' ':
                    print ts + "\Help text: '%s'" % s[hid]
        elif self.opcode == self.EFI_IFR_FORM_SET_OP:
            guid, fsid, hid, cb, cls, subcls, nvsize = struct.unpack(
                "<16sHHQHHH", self.payload)
            print ts + "Form Set '%s' Class %d-%d NvSize 0x%x Callback 0x%x" % (s[fsid], cls, subcls, nvsize, cb)
            print ts + "\GUID: %s" % fguid(guid)
            if s[hid] and s[hid] != ' ':
                print ts + "\Help text: '%s'" % s[hid]
        elif self.opcode == self.EFI_IFR_END_FORM_SET_OP:
            print ts + "End Form Set"
        elif self.opcode == self.EFI_IFR_END_FORM_OP:
            print ts + "End Form"
        elif self.opcode == self.EFI_IFR_GRAYOUT_IF_OP:
            print ts + "Grayout If"
        elif self.opcode == self.EFI_IFR_SUPPRESS_IF_OP:
            print ts + "Suppress If"
        elif self.opcode == self.EFI_IFR_END_IF_OP:
            print ts + "End If", hexdump(self.payload)
        elif self.opcode == self.EFI_IFR_EQ_ID_VAL_OP:
            qid, width, val = struct.unpack("<HBH", self.payload)
            print ts + "EQ [0x%x<%d>] == 0x%x" % (qid, width, val)
        elif self.opcode == self.EFI_IFR_EQ_ID_ID_OP:
            qid, width, qid2, width2, val = struct.unpack(
                "<HBHB", self.payload)
            print ts + "EQ [0x%x<%d>] == [0x%x.%d]" % (
                qid, width, qid2, width2, val)
        elif self.opcode == self.EFI_IFR_EQ_ID_LIST_OP:
            qid, width, length = struct.unpack("<HBH", self.payload[:5])
            l = struct.unpack("<%dH" % length, self.payload[5:])
            print ts + "LIST [0x%x<%d>] in (%s)" % (
                qid, width, ','.join(["0x%x" % i for i in l]))
        elif self.opcode == self.EFI_IFR_AND_OP:
            print ts + "AND"
        elif self.opcode == self.EFI_IFR_OR_OP:
            print ts + "OR"
        elif self.opcode == self.EFI_IFR_NOT_OP:
            print ts + "NOT"
        elif self.opcode == self.EFI_IFR_ONE_OF_OP:
            qid, width, pid, hid = struct.unpack("<HBHH", self.payload)
            storage_map[qid] = s[pid]
            print ts + "One Of [0x%x<%d>] '%s'" % (qid, width, s[pid])
            if s[hid] and s[hid] != ' ':
                print ts + "\Help text: '%s'" % s[hid]
        elif self.opcode == self.EFI_IFR_ONE_OF_OPTION_OP:
            oid, value, flags, key = struct.unpack("<HHBH", self.payload)
            print ts + "Option '%s' = 0x%x Flags 0x%x Key 0x%x" % (
                s[oid], value, flags, key)
        elif self.opcode == self.EFI_IFR_END_ONE_OF_OP:
            print ts + "End One Of"
        elif self.opcode == self.EFI_IFR_LABEL_OP:
            lid = struct.unpack("<H", self.payload)[0]
            print ts + "Label ID: 0x%x" % lid
        elif self.opcode == self.EFI_IFR_REF_OP:
            fid, pid, hid, flags, key = struct.unpack("<HHHBH", self.payload)
            print ts + "Reference: '%s' Form ID 0x%x Flags 0x%x Key 0x%x" % (
                s[pid], fid, flags, key)
            if s[hid] and s[hid] != ' ':
                print ts + "\Help text: '%s'" % s[hid]
        elif self.opcode in (self.EFI_IFR_TIME_OP, self.EFI_IFR_DATE_OP, self.EFI_IFR_NUMERIC_OP):
            qid, width, pid, hid, flags, key, min, max, step, default = struct.unpack("<HBHHBHHHHH", self.payload)
            t = {self.EFI_IFR_TIME_OP: 'Time', self.EFI_IFR_DATE_OP:
                 'Date', self.EFI_IFR_NUMERIC_OP: 'Numeric'}[self.opcode]
            print ts + "%s: '%s' [0x%x<%d>] %d-%d Step %d Default %d Flags 0x%x" % (t, s[pid], qid, width, min, max, step, default, flags)
            if s[hid] and s[hid] != ' ':
                print ts + "\Help text: '%s'" % s[hid]
        elif self.opcode == self.EFI_IFR_PASSWORD_OP:
            qid, width, pid, hid, flags, key, mins, maxs, encoding = struct.unpack("<HBHHBHBBH", self.payload)
            storage_map[qid] = s[pid]
            print ts + "Password: '%s' [0x%x<%d>] Flags 0x%x Key 0x%x Size %d-%d Encoding %d" % (s[pid], qid, width, flags, key, mins, maxs, encoding)
            if s[hid] and s[hid] != ' ':
                print ts + "\Help text: '%s'" % s[hid]
        else:
            print ts + "Opcode 0x%x (%d)" % (
                self.opcode, self.length), hexdump(self.payload)


class Form(HiiPack):
    def __init__(self, data, offset, stable=None):
        #print "Constructing form.."
        HiiPack.__init__(self, data, offset)
        data = self.data
        self.opcodes = []
        while len(data):
            op = FormOp(data, stable)
            #print "Created ", len(self.opcodes), op.length
            assert op.length
            data = data[op.length:]
            self.opcodes.append(op)

    def fetch_opcodes(self, opcode_wanted):
        return filter(lambda x: x.opcode == opcode_wanted, self.opcodes)

    def __repr__(self):
        formset = self.fetch_opcodes(FormOp.EFI_IFR_FORM_SET_OP)
        assert len(formset) == 1
        return "<FormSet name=%s offset=0x%x>" % (formset[0].get_info(), self.offset)
        return "%s" % formset

    def locate_formset(self):
        pass

    def showinfo(self, stringtable, ts=''):
        ind = 0
        in_if = False
        fstk = []
        for op in self.opcodes:
            if op.opcode == op.EFI_IFR_FORM_OP:
                fstk.append(ind)
            if op.indent < 0:
                ind += op.indent
            ots = ts + ' ' * ind
            if in_if and op.opcode in (op.EFI_IFR_SUPPRESS_IF_OP, op.EFI_IFR_GRAYOUT_IF_OP):
                ots = ts + ' ' * (ind - 1) + '+'
            try:
                op.showinfo(stringtable, ots)
            #except:
            #    print ts+"ERROR DECODING OPCODE 0x%x LEN 0x%x"%(op.opcode, op.length)
            finally:
                pass
            if (not in_if or op.opcode not in (op.EFI_IFR_SUPPRESS_IF_OP, op.EFI_IFR_GRAYOUT_IF_OP)) and op.indent > 0:
                ind += op.indent
            if op.opcode in (op.EFI_IFR_SUPPRESS_IF_OP, op.EFI_IFR_GRAYOUT_IF_OP):
                in_if = True
            elif op.opcode == op.EFI_IFR_END_IF_OP:
                in_if = False
            if op.opcode == op.EFI_IFR_END_FORM_OP:
                xind = fstk.pop()
                if xind != ind:
                    print "WARNING: Indentation mismatch"
                    ind = xind

#filename = "vt_bios.fd"
#filename = sys.argv[1]
#filename = "../test1/fv-00000010.bin"

#filename = "../test1/fe3542fe-c1d3-4ef8-7c65-8048606ff670-SetupUtility.sec0.sub2.pe"

#pe = open(filename, "rb").read()


def dump_setup(pe):
    strings = StringTable(pe, STRING_TABLE)
    strings.showinfo()

    for fn, off in FORMS[0:]:
        print
        print "Reading form '%s'" % fn, off
        f = Form(pe, off)
        #f.showinfo(strings, ' ')

    #print "Storage map:"
    #for k in sorted(storage_map.keys()):
        #print " 0x%x: %s"%(k,storage_map[k])
