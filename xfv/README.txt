This kit contains the tools to extract files from an EFI firmware
image.

 Step 1: Getting the image
---------------------------
You can get a firmware image by extracting it from any vendor updater
package, or by using the 'dumpfv.efi' program from rEFIt.
The suffix of the image name is often ".fd".

 Step 2: Extracting the image
------------------------------
Run "xfv.py" with the image file name as a parameter.
You may want to capture output from xfv for later reference, e.g.:

  ./xfv.py MBP11_0044_02B.fd | tee mbp11-output.txt

 Some notes
------------
Early Apple firmware images (e.g. the original images for the iMac, MBP
and Mac mini models as present on the Firmware Restoration CD) include
file names for drivers and applications. Later images (e.g. the Boot
Camp firmware updates) don't. However, the GUID of the files is fixed,
so a future version of xfv may support building a dictionary of file
names and using them for images that don't have embedded file names.

The current version only supports one "firmware volume" per file. The
firmware images actually contain 4 parts:

  Offset   Size   Content
  000000  1A0000  Main firmware volume: DXE core, DXE drivers
  1A0000  010000  Firmware volume, use unknown
  1B0000  030000  Contents unknown
  1E0000  020000  Firmware volume: PEI core, PEI drivers
