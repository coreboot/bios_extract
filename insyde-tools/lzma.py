#! /usr/bin/env python

# Written for LZMA Utils. Widely available for different distros, see
# Written and tested with version 4.32.7 on x86_64.
# Untested on x86_32.
# http://tukaani.org/lzma/download

# Copyright (c) 2009 d6z <d6z@tnymail.com>

# MIT License.

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

from ctypes import (CDLL, c_int8, c_uint32, c_uint64, c_void_p, c_char_p,
                    c_size_t, cast, pointer, POINTER, Structure, create_string_buffer)

from ctypes.util import find_library

from hashlib import md5


def md5sum(data):
    return md5(data).hexdigest()


class lzmadec_info_t(Structure):
    _fields_ = [('uncompressed_size', c_uint64),
                ('dictionary_size', c_uint32),
                ('internal_data_size', c_uint32),
                ('is_streamed', c_uint32),
                ('pb', c_uint32),
                ('lp', c_uint32),
                ('lc', c_uint32), ]

    def __repr__(self):
        bits = []
        for fieldname, fieldtype in self._fields_:
            bits.append("%s=%s" % (fieldname, getattr(self, fieldname)))
        return "<lmzadec_info %s>" % ", ".join(bits)

lzmadec_info_p = POINTER(lzmadec_info_t)


class lzmadec_stream_t(Structure):
    _fields_ = [('next_in', c_char_p),
                ('avail_in', c_size_t),
                ('total_in', c_uint64),
                ('next_out', c_char_p),
                ('avail_out', c_size_t),
                ('total_out', c_uint64),
                ('state', c_void_p),
                ('lzma_alloc', c_void_p),
                ('lzma_free', c_void_p),
                ('opaque', c_void_p)]
lzmadec_stream_p = POINTER(lzmadec_stream_t)


class ctypes_function(object):
    def __init__(self, lib, restype, argtypes):
        self.lib, self.restype, self.argtypes = lib, restype, argtypes

    def __call__(self, function):
        func_name = function.__name__
        f = getattr(self.lib, func_name)
        f.restype, f.argtypes = self.restype, self.argtypes
        return f

library_path = find_library("lzmadec")
assert library_path, (
    "Couldn't find `liblzmadec.so`. Please install lzma_utils.\n"
    "                it can be found at http://tukaani.org/lzma/download"
)
lzma = CDLL(library_path)

# I tried the simpler lzmadec_buffer function but it didn't like that I was
# providing too much data and crashed, so I switched to the stream instead.


@ctypes_function(lzma, c_int8, [lzmadec_info_p, c_char_p, c_size_t])
def lzmadec_buffer_info():
    pass


@ctypes_function(lzma, c_int8, [lzmadec_stream_p])
def lzmadec_init():
    pass


@ctypes_function(lzma, c_int8, [lzmadec_stream_p, c_int8])
def lzmadec_decode():
    pass


@ctypes_function(lzma, c_int8, [lzmadec_stream_p])
def lzmadec_end():
    pass

#
# USEFUL CODE STARTS HERE
#

# Based on concepts from scanlzma.c
# scanlzma, scan for lzma compressed data in stdin and echo it to stdout.
# Copyright (C) 2006 Timo Lindfors


def find_lzma_headers(buffer):
    MAGIC_CHAR = chr(0x5D)

    position = 0
    positions = []

    while position < len(buffer) and MAGIC_CHAR in buffer[position:]:

        position = buffer.index(MAGIC_CHAR, position) + 1

        if (ord(buffer[position + 3]) < 0x20 and
            (buffer[position + 9:].startswith("\x00" * 3) or
             buffer[position + 4:].startswith("\xFF" * 8))):
            positions.append(position - 1)

    return positions


def lzma_decompressed_size(buffer):
    "Given `buffer`, return the decompressed size"
    lzmadec_info = lzmadec_info_t()
    result = lzmadec_buffer_info(pointer(lzmadec_info), buffer, len(buffer))
    assert not result, "lzmadec_buffer_info failed"
    #print lzmadec_info
    assert lzmadec_info.dictionary_size > lzmadec_info.uncompressed_size, (
        "This probably doesn't make sense.."
    )
    #print "Here..", lzmadec_info
    return lzmadec_info.uncompressed_size


def lzma_decode(input_buffer):
    """
    `input_buffer`: string.
    Return Value: (decompressed string, amount of `input_buffer` used)
    """

    result_size = lzma_decompressed_size(input_buffer)

    assert result_size

    result_data = create_string_buffer(result_size)

    lzmadec_stream = lzmadec_stream_t()

    assert not lzmadec_init(lzmadec_stream)

    lzmadec_stream.next_in = input_buffer
    lzmadec_stream.avail_in = len(input_buffer)

    lzmadec_stream.next_out = cast(result_data, c_char_p)
    lzmadec_stream.avail_out = result_size

    result = lzmadec_decode(lzmadec_stream, 1)

    #s = lzmadec_stream
    #print s.avail_in, s.total_in, s.avail_out, s.total_out

    assert not lzmadec_end(lzmadec_stream)

    result_data = result_data.raw
    amount_read = lzmadec_stream.total_in

    assert result == 1

    return result_data, amount_read


def get_lzma_chunks(input_buffer):
    """
    Scans `input_buffer` for LZMA-like data.
    Returns a list of (position, "the data found decompressed").
    """

    ph = possible_headers = find_lzma_headers(input_buffer)
    # Not a real header location, but allows one last iteration to end of file
    # in the following loop
    ph.append(len(input_buffer) - 1)

    result = []

    for this_header, next_header in zip(ph, ph[1:]):
        try:
            #print this_header, next_header
            data, length = lzma_decode(input_buffer[this_header:next_header])
        except AssertionError:
            continue

        result.append((this_header, data))

    return result


def test():
    bios_data = open("data/original_bios_backup.fd", "rb").read()

    results = get_lzma_chunks(bios_data)
    print map(md5sum, zip(*results)[1])

    #~ headers = find_lzma_headers(bios_data)
    #~ decompr_data, amoun_read = lzma_decode(bios_data[headers[0]:])
    #~ print md5sum(decompr_data)

if __name__ == "__main__":
    test()
