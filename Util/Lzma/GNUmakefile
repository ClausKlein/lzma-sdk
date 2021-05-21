PROG = ../../lzma
CXX = g++
RM = rm -f
CFLAGS = -c -O2 -Wall -Wextra -Wpedantic -D_7ZIP_ST -std=c++14 -x c++

OBJS = \
  LzmaUtil.o \
  Alloc.o \
  LzFind.o \
  LzmaDec.o \
  LzmaEnc.o \
  7zFile.o \
  7zStream.o \


all: $(PROG)

$(PROG): $(OBJS)
	$(CXX) -o $(PROG) $(LDFLAGS) $(OBJS)

LzmaUtil.o: LzmaUtil.c
	$(CXX) $(CFLAGS) $(CURDIR)/LzmaUtil.c

Alloc.o: ../../Alloc.c
	$(CXX) $(CFLAGS) $(CURDIR)/../../Alloc.c

LzFind.o: ../../LzFind.c
	$(CXX) $(CFLAGS) $(CURDIR)/../../LzFind.c

LzmaDec.o: ../../LzmaDec.c
	$(CXX) $(CFLAGS) $(CURDIR)/../../LzmaDec.c

LzmaEnc.o: ../../LzmaEnc.c
	$(CXX) $(CFLAGS) $(CURDIR)/../../LzmaEnc.c

7zFile.o: ../../7zFile.c
	$(CXX) $(CFLAGS) $(CURDIR)/../../7zFile.c

7zStream.o: ../../7zStream.c
	$(CXX) $(CFLAGS) $(CURDIR)/../../7zStream.c

clean:
	-$(RM) $(PROG) $(OBJS)
