export APP_VERSION	:=	1.5.0
export APP_TITLE	:=	Homebrew Launcher
export APP_AUTHOR	:=	Dimok / switchbrew

ifeq ($(RELEASE),)
	GIT_REV := $(shell git describe --dirty --always 2>/dev/null)
	ifneq ($(GIT_REV),)
		export APP_VERSION	:=	$(APP_VERSION)-$(GIT_REV)
	endif
endif

.PHONY: clean all nx pc dist-bin

all: nx pc

romfs:
	@mkdir -p romfs

romfs/assets.zip	:	romfs assets
	@rm -f romfs/assets.zip
	@zip -rj romfs/assets.zip assets

dist-bin:	romfs/assets.zip
	$(MAKE) -f Makefile.nx dist-bin

nx:	romfs/assets.zip
	$(MAKE) -f Makefile.nx

pc:	romfs/assets.zip
	$(MAKE) -f Makefile.pc

clean:
	@rm -f romfs/assets.zip
	$(MAKE) -f Makefile.pc clean
	$(MAKE) -f Makefile.nx clean
