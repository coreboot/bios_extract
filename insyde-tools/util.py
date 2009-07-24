#! /usr/bin/env python

from hashlib import md5
from sys import stdout
from pprint import pprint


def md5sum(data):
    return md5(data).hexdigest()


class FindBadPosition(Exception):
    """Raised to disallow a position from appearing in the result list in `find_all`"""
    pass


class FindStopSearching(Exception):
    """Stop searching in find_all"""
    def __init__(self, result):
        self.result = result


def from_b64(what):
    return str.decode(what + "=" * (3 - len(what) % 3), "base64")


def hexbytes(bytes):
    return " ".join(["%02x" % ord(c) for c in bytes])


def substitute(input, where, what):
    return "".join([input[:where], what, input[where + len(what):]])


def find_all(buffer, what, callback=lambda x: x, start=0, stop=None):
    if stop is None:
        stop = len(buffer)

    position = start
    result = []

    while position < stop and what in buffer[position:stop + 1]:

        position = buffer.index(what, position, stop)

        try:
            result.append(callback(position))
        except FindStopSearching, exc:
            return exc.result
        except FindBadPosition:
            pass

        position += 1

    return result


def find_all_backwards(buffer, what, callback=lambda x: x, start=None, stop=0):
    "Beware! This function is not fully tested. I should really write some unit tests for the edge cases"

    if start is None:
        start = len(buffer)

    position = start
    result = []

    while position > stop and what in buffer[stop:position]:

        position = buffer.rindex(what, stop, position)

        try:
            result.append(callback(position))
        except FindStopSearching, exc:
            return exc.result
        except FindBadPosition:
            pass

        position += 1

    return result


def read_with_progress(file_handle, total, chunk_size):
    assert not total % chunk_size, "chunk_size must be a divisor of total!"
    data = []
    length = (80 - 6)
    for i in xrange(0, total, chunk_size):
        data.append(file_handle.read(chunk_size))
        done = length * i // total
        print "\r[ %s>%s ]" % ("-" * done, " " * (length - done)),
        stdout.flush()
    print "\r[ %s> ]" % ("-" * length)
    return "".join(data)

from struct import unpack, Struct as pyStruct


class StructMeta(type):
    def __new__(meta, classname, bases, classDict):

        is_structitem = lambda x: isinstance(x[1], SI)
        struct_items = filter(is_structitem, classDict.iteritems())
        struct_items.sort(key=lambda x: x[1].position)

        struct_format = "".join(map(lambda x: x[1].packstr, struct_items))

        struct = pyStruct(struct_format)

        def __init__(self, data):

            data_tuple = struct.unpack_from(data)
            for (name, structitem), item_data in zip(struct_items, data_tuple):
                setattr(self, name, item_data)

            self.rest_of_data = data[struct.size:]

        #def repack(self):

        cls = type.__new__(meta, classname, bases, classDict)
        cls.py_struct = struct
        cls.struct_size = struct.size
        cls.__init__ = __init__

        return cls


class SI(object):
    "StructItem"
    counter = 0

    def __init__(self, packstr):
        self.position = SI.counter
        SI.counter += 1
        self.packstr = packstr


class Struct(object):
    __metaclass__ = StructMeta


def hexdump(s, sep=" "):
    return sep.join(map(lambda x: "%02x" % ord(x), s))


def ascii(s):
    s2 = ""
    for c in s:
        if ord(c) < 0x20 or ord(c) > 0x7e:
            s2 += "."
        else:
            s2 += c
    return s2


def pad(s, c, l):
    if len(s) < l:
        s += c * (l - len(s))
    return s


def chexdump(s, ts="", off=0):
    for i in range(0, len(s), 16):
        print ts + "%08x  %s  %s  |%s|" % (i + off, pad(hexdump(s[i:i + 8], ' '), " ", 23), pad(hexdump(s[i + 8:i + 16], ' '), " ", 23), pad(ascii(s[i:i + 16]), " ", 16))

if __name__ == "__main__":

    s = Header("test")
    print "I have something:", s
