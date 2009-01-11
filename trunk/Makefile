DEBUG=-g3 -ggdb3 -D_DEBUG_

OBJS_DPSDEC=dps_io.o dps2jpgs.o
TARGETS=dps2jpgs

FFMPEG_CFLAGS=$(shell pkg-config --cflags libavformat) $(shell pkg-config --cflags libavcodec) $(shell pkg-config --cflags libswscale)
FFMPEG_LDFLAGS=$(shell pkg-config --libs libavformat) $(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libswscale)

# compose final 
CFLAGS=$(DEBUG) -I. $(FFMPEG_CFLAGS)
LDFLAGS=$(DEBUG) -ldl $(FFMPEG_LDFLAGS)

all: $(TARGETS)

src/instance.h: svnversion.h

dps2jpegs: $(OBJS_DPSDEC)
	$(CC) -o $@ $(OBJS_DPSDEC) $(LDFLAGS)

dps_io.o: src/dps_io.c src/dps_io.h svnversion.h
	$(CC) -c -o $@ src/dps_io.c $(CFLAGS)

svnversion.h:
	echo -e "#ifndef SVNVERSION_H\n#define SVNVERSION_H\n#define SVNVERSION \""`svnversion -nc . | sed -e s/^[^:]*://`"\"\n#endif\n" > svnversion.h

clean:
	rm -f $(TARGETS)
	rm -f svnversion.h
	rm -f *.o *~ src/*~
