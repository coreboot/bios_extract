#!/usr/bin/python

import sys
import os
from struct import unpack

fvh_count = 0

### Formatting: GUIDs

def format_guid(guid_s):
    parts = unpack("<LHH8B", guid_s)
    return "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X" % parts

### Formatting: file types

filetypes = ("ALL ", "RAW ", "FREE", "SECc", "PEIc", "DXEc", "PEIM", "DRVR", "CPDR", "APPL", "SMM", "FVIM", "SMMDXE", "SMMCORE")
filetype_exts = ("all", "raw", "free", "seccore", "peicore", "dxecore", "peim", "drv", "comb_peim_drv", "app", "unkn0a", "fd")

def format_filetype(filetype_num):
    if filetype_num < len(filetypes):
        return filetypes[filetype_num]
    if filetype_num == 0xF0:
        return "PAD"
    return "??%02X" % filetype_num

def extention_filetype(filetype_num):
    if filetype_num < len(filetype_exts):
        return filetype_exts[filetype_num]
    return "unkn%02x" % filetype_num

### Main function to analyze a host disk file

def analyze_diskfile(filename, offset = 0):
    f = file(filename, "rb")
    fvdata = f.read()
    f.close()
    
    print "Analyzing %s, 0x%X bytes" % (filename, len(fvdata))
    
    if fvdata[offset:offset+16] == "\xBD\x86\x66\x3B\x76\x0D\x30\x40\xB7\x0E\xB5\x51\x9E\x2F\xC5\xA0":
        # EFI capsule
        (capguid, capheadsize, capflags, capimagesize, capseqno, capinstance, capsplitinfooffset, capbodyoffset, cap3offset, cap4offset, cap5offset, cap6offset, cap7offset, cap8offset) = unpack("< 16s L L L L 16s L L 6L", fvdata[offset:offset+80])
        print "EFI Capsule, %d bytes" % capimagesize
        handle_fv(fvdata[offset+capbodyoffset:offset+capbodyoffset+capimagesize], format_guid(capguid))
        
    else:
        # treat as sequence of firmware volumes
        while offset < len(fvdata):
            print "offset: %08X" % offset
            usedsize = handle_fv(fvdata[offset:], "%08X" % offset)
            if usedsize == 0:
                offset += 0x10000
            else:
                offset += usedsize

### Handle a firmware volume


def handle_fv(fvdata, name='default'):

    ### Check header
    
    (fvzero, fvfstype, fvlen, fvsig, fvattr, fvhdrlen, fvchecksum, fvrev) = unpack("< 16s 16s Q 4s L H H 3x B", fvdata[0:0x38])
    if fvsig != "_FVH" and fvfstype != '\xD9\x54\x93\x7A\x68\x04\x4A\x44\x81\xCE\x0B\xF6\x17\xD8\x90\xDF':
        if fvdata[0] == '\xFF':
            print "Skipping FFs"
            offset = 0
            while fvdata[offset] == '\xFF':
                offset += 1
            return offset
        else:
            print "Not a EFI firmware volume (sig and GUID missing)"
            return 0
    if fvlen > len(fvdata):
        print "WARNING: File too short, header gives length as 0x%X bytes" % fvlen
    else:
        print "Size per header: 0x%X bytes" % fvlen
    offset = fvhdrlen

    global fvh_count
    #fvhdir = "fvh-%d" % fvh_count
    fvhdir = "fvh-" + name
    fvh_count = fvh_count + 1
    try:
      os.mkdir(fvhdir)
    except:
      pass
    os.chdir(fvhdir)

    ### Decode files

    print "Listing files"
    print "-----"

    while True:
        if offset == fvlen:
            print "-----"
            print "End of volume (size reached cleanly)"
            break
        if offset + 24 > fvlen:
            print "-----"
            print "End of volume (size reached uncleanly)"
            break
        
        (fileguid, fileintcheck, filetype, fileattr, filelenandstate) = unpack("< 16s H B B L", fvdata[offset:offset+24])
        if filetype == 0xff:
            print "-----"
            print "End of volume (filler data found)"
            break
        fileentrylen = filelenandstate & 0xffffff
        filestate = filelenandstate >> 24
        filelen = fileentrylen - 24
        if fileattr & 1:
            print "  has tail!"
            filelen = filelen - 2

        fileoffset = offset + 24
        nextoffset = (offset + fileentrylen + 7) & ~7

        print "%08X %08X" % (fileoffset, nextoffset)
        
        filedata = fvdata[fileoffset:fileoffset+filelen]
        compressed = False
        if filetype != 1 and filelen > 3 and filedata[3] == "\x01":
            compressed = True
            filedata = decompress(filedata)

        if compressed:
            print "%08X %s  %s  C %d (%d)" % (offset, format_guid(fileguid), format_filetype(filetype), len(filedata), filelen)
        else:
            print "%08X %s  %s  U %d" % (offset, format_guid(fileguid), format_filetype(filetype), filelen)
        
        if filetype != 0xF0:
          handle_file("file-%s.%s" % (format_guid(fileguid), extention_filetype(filetype)), filetype, filedata)
        else:
          print "(skipping)"
        
        offset = nextoffset

    os.chdir('..')
    return fvlen

if sys.platform == 'win32':
  efidecomp_path = os.path.join(os.path.dirname(__file__), "UEFI_Decompressor")
  efidecomp_cmd = '"%s" %s %s'
else:
  efidecomp_path = os.path.dirname(os.path.realpath(__file__)) + "/efidecomp"
  efidecomp_cmd = "%s < %s > %s"

### Handle decompression of a compressed section

def decompress2(compdata):
    f = file("_tmp_decompress", "wb")
    f.write(compdata)
    f.close()
    
    cmd = efidecomp_cmd % ( efidecomp_path, '_tmp_decompress', '_tmp_result')
    print "cmd: %r" % cmd
    os.system(cmd)
    
    f = file("_tmp_result", "rb")
    decompdata = f.read()
    f.close()
    return decompdata

def decompress(compdata):
    (sectlenandtype, uncomplen, comptype) = unpack("< L L B", compdata[0:9])
    sectlen = sectlenandtype & 0xffffff
    if sectlen < len(compdata):
        print "WARNING: Compressed section is not the only section! (%d/%d)" % (sectlen, len(compdata))
    if comptype == 0:
        return compdata[9:]
    elif comptype == 1:
        print "WARNING: this code path might not work";
        f = file("_tmp_decompress", "wb")
        f.write(compdata[9:])
        f.close()
        
        cmd = efidecomp_cmd % ( efidecomp_path, '_tmp_decompress', '_tmp_result')
        #cmd = "./efidecomp <_tmp_decompress >_tmp_result"
        os.system(cmd)
        
        f = file("_tmp_result", "rb")
        decompdata = f.read()
        f.close()

        if len(decompdata) < uncomplen:
            print "WARNING: Decompressed data too short!"
        return decompdata

    elif comptype == 2:
        f = file("_tmp_decompress", "wb")
        f.write(compdata[13:sectlen+4]) # for some reason there is junk in 9:13 that I don't see in the raw files?! yuk.
        f.close()
        
        os.system("lzmadec <_tmp_decompress >_tmp_result")
        
        f = file("_tmp_result", "rb")
        decompdata = f.read()
        f.close()
        
        if len(decompdata) < uncomplen:
            print "WARNING: Decompressed data too short!"
        return decompdata
    else:
        print "ERROR: Unknown compression type %d" % comptype
        return compdata

### Handle the contents of one firmware file


def handle_file(filename, filetype, filedata):
    if filetype == 1:
        f = file("%s.raw" % filename, "wb")
        f.write(filedata)
        f.close()
    if filetype != 1:
        handle_sections(filename, 0, filedata)

def get_filename(imagedata):
    imagelen = len(imagedata)
    filename_override = None
    
    # first try to find a filename
    offset = 0
    while offset + 4 <= imagelen:
        (sectlenandtype,) = unpack("< L", imagedata[offset:offset + 4])
        sectlen = sectlenandtype & 0xffffff
        secttype = sectlenandtype >> 24
        nextoffset = (offset + sectlen + 3) & ~3
        dataoffset = offset + 4
        datalen = sectlen - 4
        sectdata = imagedata[dataoffset:dataoffset + datalen]

        if secttype == 0x15:
            filename_override = sectdata[:-2].decode(
                "utf-16le").encode("utf-8")
            print "  Filename '%s'" % filename_override

        offset = nextoffset
    return filename_override

### Handle section data (i.e. multiple sections), recurse if necessary

def handle_sections(filename, sectindex, imagedata):
    imagelen = len(imagedata)

    # first try to find a filename
    filename_override = get_filename(imagedata)
   
    # then analyze the sections for good
    offset = 0
    while offset + 4 <= imagelen:
        (sectlenandtype,) = unpack("< L", imagedata[offset:offset + 4])
        sectlen = sectlenandtype & 0xffffff
        secttype = sectlenandtype >> 24
        nextoffset = (offset + sectlen + 3) & ~3
        dataoffset = offset + 4
        datalen = sectlen - 4

        if secttype == 2:
            (sectguid, sectdataoffset, sectattr) = unpack("< 16s H H", imagedata[offset+4:offset+24])
            dataoffset = offset + sectdataoffset
            datalen = sectlen - sectdataoffset
            if sectguid == "\xB0\xCD\x1B\xFC\x31\x7D\xAA\x49\x93\x6A\xA4\x60\x0D\x9D\xD0\x83":
                # CRC32 section
                sectindex = handle_sections(filename, sectindex, imagedata[
                                            dataoffset:dataoffset + datalen])
            else:
                print "  %02d  GUID %s" % (sectindex, format_guid(sectguid))
                sectindex += 1
                sectindex = handle_sections(filename, sectindex, imagedata[
                                            dataoffset:dataoffset + datalen])
        elif secttype == 1: # compressed
            sectdata = imagedata[dataoffset:dataoffset+datalen]
            decdata = decompress2(sectdata)
            print "  %02d  COMPRESSED %d => %d" % (sectindex, datalen, len(decdata))
            if filename_override == None:
                filename_override = get_filename(decdata)
            sectindex += 1
            sectindex = handle_sections(filename, sectindex, decdata)
        else:
            secttype_name = "UNKNOWN(%02X)" % secttype
            ext = "data"
            sectdata = imagedata[dataoffset:dataoffset + datalen]
            extraprint = ""
            
            if secttype == 0x1:
                secttype_name = "COMPRESSED"
                ext = "comp"
            if secttype == 0x10:
                secttype_name = "PE32"
                ext = "efi"
            elif secttype == 0x11:
                secttype_name = "PIC"
                ext = "pic.efi"
            elif secttype == 0x12:
                secttype_name = "TE"
                ext = "te"
            elif secttype == 0x13:
                secttype_name = "DXE_DEPEX"
                ext = "depex"
            elif secttype == 0x14:
                secttype_name = "VERSION"
                ext = "ver"
            elif secttype == 0x15:
                secttype_name = "USER_INTERFACE"
                ext = None
            elif secttype == 0x16:
                secttype_name = "COMPATIBILITY16"
                ext = "bios"
            elif secttype == 0x17:
                secttype_name = "FIRMWARE_VOLUME_IMAGE"
                ext = "fd"
            elif secttype == 0x18:
                secttype_name = "FREEFORM_SUBTYPE_GUID"
                ext = "guid"
            elif secttype == 0x19:
                secttype_name = "RAW"
                ext = "raw"
                if sectdata[0:8] == "\x89PNG\x0D\x0A\x1A\x0A":
                    ext = "png"
                elif sectdata[0:4] == "icns":
                    ext = "icns"
            elif secttype == 0x1B:
                secttype_name = "PEI_DEPEX"
                ext = None

            print "  %02d  %s  %d%s" % (
                sectindex, secttype_name, datalen, extraprint)

            if ext is not None:
                use_filename = "%s-%02d" % (filename, sectindex)
                if filename_override is not None:
                    use_filename = filename_override
                f = file("%s.%s" % (use_filename, ext), "wb")
                f.write(sectdata)
                f.close()

            if secttype == 0x17:
                print "*** Recursively analyzing the contained firmware volume..."
                handle_fv(sectdata, filename)

            sectindex += 1

        offset = nextoffset

    return sectindex

### main code

if __name__ == '__main__':
    if len(sys.argv) > 1:
        filename = sys.argv[1]
        if len(sys.argv) > 2:
            offset = int(sys.argv[2], 16)
        else:
            offset = 0
        analyze_diskfile(filename, offset)
    else:
        print "Usage: xfv.py bios.rom [start offset]"
