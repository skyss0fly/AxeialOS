# Makefile — Firmware Blob Copy Ritual

RamRootDir := RamRoot
BootImgDir := ../BootImg/.Build
FirmBlobsDir := $(BootImgDir)/firmblobs

.PHONY: \
all \
clean

all: \
PrepareFirmBlobs \
CopyRamRoot

PrepareFirmBlobs:
	@mkdir -p $(FirmBlobsDir)
CopyRamRoot: PrepareFirmBlobs
	@cp -r $(RamRootDir)/* $(FirmBlobsDir)/

clean:
	@rm -rf $(FirmBlobsDir)