CIRCLESTDLIBROOT = thirdparty/circle-stdlib
CIRCLEHOME = thirdparty/circle-stdlib/libs/circle
NEWLIBDIR = thirdparty/circle-stdlib/install/aarch64-none-circle

# デフォルトのRaspberry Piのモデル
RPI = 4
EMULATOR = emu6502

.DEFAULT_GOAL := default

default:
	@if [ -f $(CIRCLESTDLIBROOT)/Config.mk ]; then \
		make -f Makefile.$(EMULATOR) RPI=$(RPI); \
		cp kernel*.img image; \
		cp menu.cfg.$(EMULATOR) image/menu.cfg; \
	else \
		make all; \
	fi

all:
	cd $(CIRCLESTDLIBROOT) && git submodule update --init --recursive
	cd $(CIRCLESTDLIBROOT) && ./configure -r $(RPI) -p aarch64-none-elf-
	cd $(CIRCLESTDLIBROOT) && make
	cd $(CIRCLEHOME)/addon/fatfs && make
	cd $(CIRCLEHOME)/addon/SDCard && make
	cd $(CIRCLEHOME)/addon/linux && make
	cd $(CIRCLEHOME)/addon/vc4 && ./makeall --nosample
	make -f Makefile.$(EMULATOR) RPI=$(RPI)
	cp kernel*.img image
	cp menu.cfg.$(EMULATOR) image/menu.cfg

clean:
	@echo "  CLEAN " `pwd`
	@rm -f $(OBJS) $(DEPS) $(TARGET).elf $(TARGET).lst $(TARGET).map $(TARGET).img
	@find src \( -name "*.o" -o -name "*.d" \) | xargs rm -f
	@rm -f kernel*.elf kernel*.img kernel*.lst kernel*.map

cleanall:
	@if [ -f $(CIRCLESTDLIBROOT)/Config.mk ]; then \
		(cd $(CIRCLESTDLIBROOT) && make clean); \
		(cd $(CIRCLESTDLIBROOT) && make mrproper); \
	fi
	(cd $(CIRCLEHOME)/addon/fatfs && make clean)
	(cd $(CIRCLEHOME)/addon/SDCard && make clean)
	(cd $(CIRCLEHOME)/addon/linux && make clean)
	(cd $(CIRCLEHOME)/addon/vc4 && ./makeall clean)
	make clean
