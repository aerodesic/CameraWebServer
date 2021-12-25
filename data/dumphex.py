#!/usr/bin/env python3

# Convert a file of bytes into a gziped, text file.

import sys
import os

if len(sys.argv) < 1:
    print("Missing file name")
else:
    pathname = sys.argv[1]
    gzipname = pathname + ".gz"
    outputname = pathname + ".txt"

    try:
        os.remove(gzipname)
    except:
        pass # Ignore non-existant file

    # Convert to gzip format
    if os.system("gzip -k %s" % pathname) == 0:
        with open(gzipname, "rb") as f:
           data = f.read()

           with open(outputname, "w") as out:
 
               print("// File: %s, Size: %d" % (gzipname, len(data)), file=out)
               print("#define %s_len %d" % (gzipname.replace('.', '_'), len(data)), file=out)
               print("const uint8_t %s[] = {" % (gzipname.replace('.', '_')), file=out)

               for pos in range(0, len(data), 16):
                   avail = 16

                   if len(data) - pos < avail:
                       avail = len(data) - pos

                   print("  ", end='', file=out)

                   for byte in range(0, avail):
                       print("0x%02x, " % data[pos + byte], end='', file=out)

                   print(file=out)

               print("};", file=out)
               print(file=out)
               
    else:
        print("gzip failed")

