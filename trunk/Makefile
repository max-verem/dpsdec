#DEBUG=-g3 -ggdb3 -D_DEBUG_

TARGETS=dps2jpgs dps2yuv

OBJS_DPS2JPEGS=dps_io.o dps2jpgs.o
OBJS_DPS2YUV=dps_io.o dps2yuv.o

FFMPEG_CFLAGS=$(shell pkg-config --cflags libavformat) $(shell pkg-config --cflags libavcodec) $(shell pkg-config --cflags libswscale)
FFMPEG_LDFLAGS=$(shell pkg-config --libs libavformat) $(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libswscale)

LARGE_FILES_SUPPORT=-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGEFILE_SOURCE

# compose final
CFLAGS=$(DEBUG) -I. $(FFMPEG_CFLAGS) $(LARGE_FILES_SUPPORT)
LDFLAGS=$(DEBUG) -ldl $(FFMPEG_LDFLAGS)

all: $(TARGETS)

dps2jpgs: $(OBJS_DPS2JPEGS)
	$(CC) -o $@ $(OBJS_DPS2JPEGS) $(LDFLAGS)

dps2yuv: $(OBJS_DPS2YUV)
	$(CC) -o $@ $(OBJS_DPS2YUV) $(LDFLAGS)

dps_io.o: src/dps_io.c src/dps_io.h
	$(CC) -c -o $@ src/dps_io.c $(CFLAGS)

dps2jpgs.o: src/dps2jpgs.c src/dps_io.h svnversion.h
	$(CC) -c -o $@ src/dps2jpgs.c $(CFLAGS)

dps2yuv.o: src/dps2yuv.c src/dps_io.h svnversion.h
	$(CC) -c -o $@ src/dps2yuv.c $(CFLAGS)

svnversion.h:
	echo -e "#ifndef SVNVERSION_H\n#define SVNVERSION_H\n#define SVNVERSION \""`svnversion -nc . | sed -e s/^[^:]*://`"\"\n#endif\n" > svnversion.h

clean:
	rm -f $(TARGETS)
	rm -f svnversion.h
	rm -f *.o *~ src/*~
