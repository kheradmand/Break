PROG_ALIAS = "./pbzip2-inst-alias"
PROG_HB = "./pbzip2-inst-hb"
PROG_PORTEND = "./pbzip2-inst-portend"

ORIGPROGNAME = "pbzip2"

testList = [
    ["rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG_ALIAS + " -v ./compressThis",
     "rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG_ALIAS + " -b15vk ./compressThis",
     "rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG_ALIAS + " -p4 -r -5 -v ./compressThis",
     "rm -f ./decompressThis ; cp ./file.bz2 ./decompressThis.bz2 && time " + PROG_ALIAS + " -dv -m500 ./decompressThis.bz2"],
    ["rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG_HB + " -v ./compressThis",
     "rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG_HB + " -b15vk ./compressThis",
     "rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG_HB + " -p4 -r -5 -v ./compressThis",
     "rm -f ./decompressThis ; cp ./file.bz2 ./decompressThis.bz2 && time " + PROG_HB + " -dv -m500 ./decompressThis.bz2"],
    ["rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG_PORTEND + " -v ./compressThis",
     "rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG_PORTEND + " -b15vk ./compressThis",
     "rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG_PORTEND + " -p4 -r -5 -v ./compressThis",
     "rm -f ./decompressThis ; cp ./file.bz2 ./decompressThis.bz2 && time " + PROG_PORTEND + " -dv -m500 ./decompressThis.bz2"],
    ]

# testList = ["rm -f ./compressThis.bz2 ; cp ./file ./compressThis && time " + PROG + " -p32 -r -5 -v ./compressThis"]
