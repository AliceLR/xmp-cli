
#MAIN_OBJS	= getopt.o getopt1.o options.o drivers.o main.o
MAIN_OBJS	= sound_alsa.o terminal.o info.o options.o main.o
MAIN_DFILES	= Makefile $(MAIN_OBJS:.o=.c)
MAIN_PATH	= players/xmp

dist-main:
	mkdir -p $(DIST)/$(MAIN_PATH)
	cp -RPp $(addprefix $(MAIN_PATH)/,$(MAIN_DFILES)) $(DIST)/$(MAIN_PATH)

$(MAIN_PATH)/options.o: Makefile

M_OBJS	= $(addprefix $(MAIN_PATH)/,$(MAIN_OBJS))

$(MAIN_PATH)/xmp: $(M_OBJS)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(M_OBJS) -Llib -lxmp -lm -lasound'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

install-xmp: $(MAIN_PATH)/xmp
	@echo Installing xmp in $(DESTDIR)$(BINDIR)
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	@$(INSTALL_PROGRAM) $+ $(DESTDIR)$(BINDIR)

