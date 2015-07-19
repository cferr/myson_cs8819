
include Makefile.inc

DRIVER_DIR=${DESTDIR}/${USBDROPDIR}/ifd-Myson-CS881x.bundle/Contents
CC=${BUILD}-gcc
SOURCES=ifdhandler.c myson.c



all:	libMyson.so

libMyson.so: ${SOURCES}
	${CC} -o libMyson.so  ${SOURCES} -lusb-1.0 -fPIC -D_REENTRANT -DIFDHANDLERv2 -Wall -I. ${CFLAGS} ${LDFLAGS} -shared

clean-all:	clean
	rm Makefile.inc || true

clean:
	rm -f *~ *.o *.so || true

install:	all
	install -c -d "${DRIVER_DIR}"
	install -c -d "${DRIVER_DIR}/Linux"
	install -c -m 0755 libMyson.so "${DRIVER_DIR}/Linux/"
	install -c Info.plist "${DRIVER_DIR}"
	
