#!/bin/sh

# must not use ./configure --solo
# Z_SOLO makes functions behave differently

if [ ! -f zlib2ansi ] ; then
    echo "Run this script inside the zlib source tree"
    exit 1
fi

# linux
git checkout -- .
./configure

sed -i '/OBJG = /d' Makefile

deflate='compress.c deflate.c deflate.h trees.c trees.h'

if [ -f deflate.h ] ; then
    sed -i '/define GZIP$/d' deflate.h
fi

# don't need crc32.c/crc32.h only adler32.c
sed -i '/define GUNZIP/d' inflate.h
#gzip='crc32.c crc32.h'

#  Normally either infback.o or inflate.o would be linked into an application--not both
#extra='infback.c'

mkdir -p zlib-cut
cp -fv ${deflate} uncompr.c \
	adler32.c \
	${gzip} \
	${extra} \
	inffast.c inffast.h \
	inffixed.h \
	inflate.c inflate.h \
	inftrees.c inftrees.h \
	zutil.c zutil.h \
	zlib.h \
	zlib-cut

# special case 
cp -fv zconf.h.in zlib-cut/zconf.h

patch zlib-cut/zconf.h << EEOOF
--- zconf.h.in	2017-01-01 15:37:10.000000000 +0800
+++ zconf.h.in-w	2022-05-25 14:49:30.000000000 +0800
@@ -8,6 +8,19 @@
 #ifndef ZCONF_H
 #define ZCONF_H
 
+#ifdef HAVE_CONFIG_H
+#include <config.h>
+#endif
+
+#ifndef HAVE_UNISTD_H
+# if defined(__unix__) || defined(__unix) || defined(unix) || defined(__MINGW32__)
+#  define HAVE_UNISTD_H 1
+# endif
+#endif
+#ifndef HAVE_STDARG_H
+# define HAVE_STDARG_H 1
+#endif
+
 /*
  * If you *really* need a unique prefix for all types and library functions,
  * compile with -DZ_PREFIX. The "standard" zlib should be compiled without it.
EEOOF

echo "Files are in zlib-cut"

# copy zlib-cut/* to cut/lib/zlib/
# try to compile: run ./rebuild.sh and fix any issues until it compiles

# ----------------------------------
# reduce size (not really necessary)
#
# zlib.h
#    /* basic functions */
#        delete only deflate*
#    /* Advanced functions */
#        (delete all deflate* functions)
#    /* utility functions */
#        (until /* gzip file access functions */)
#    /* gzip file access functions */
#        (keep only: typedef struct gzFile_s *gzFile;

