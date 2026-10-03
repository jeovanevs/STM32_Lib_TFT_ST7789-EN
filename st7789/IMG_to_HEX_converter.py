#!/usr/bin/env python

#	Convert a color image using the IMG_to_HEX_converter.py file
#	start the CMD command line
#   install the Image module if it's missing -> pip3 install image
#	place IMG_to_HEX_converter.py and the image in the same folder
#	then, in the command line, go to that folder and run:
#	> python   IMG_to_HEX_converter.py   (file_name)logo.jpg    (width)85    (height)80
#
#	example:    D:\IMG>python  IMG_to_HEX_converter.py  logo.jpg  85  80
#
#	and in the same folder a file with our array picFile.txt will appear
#
#	GENERATES DATA IN   RGB565 (16-bit) FORMAT


from PIL import Image
import sys
import os



if len(sys.argv) != 4:
    print("Usage: {} <image-file> <width> <height>".format(sys.argv[0]))
    sys.exit(1)

fname = sys.argv[1]

W = sys.argv[2]
print("\r\n")
print("<width> " + str(W))

H = sys.argv[3]
print("<height> " + str(H))

img = Image.open(fname)
if img.width != int(W) or img.height != int(H):
    print("Error: Resolution is incorrect (it must match the image dimensions)!!!");
    sys.exit(2)
	
f=open("picFile.txt", "a")

f.write("// size " + str(W) + " x " + str(H) + " \n");
f.write("const unsigned short img[] = { \n ");

for y in range(0, img.height):
    s = ""
    for x in range(0, img.width):
        (r, g, b) = img.getpixel( (x, y) )
        color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3)
        # for right endiness, so ST7735_DrawImage would work
        color565 = ((color565 & 0xFF00) >> 8) | ((color565 & 0xFF) << 8)
        s += "0x{:04X},".format(color565)
    s += "  \n"
    f.write(s)

f.write("}; \r\n")
f.close()
print("\r\n Done !!! OK \r\n")
