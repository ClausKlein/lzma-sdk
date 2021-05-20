
#include "LzmaDec.h"
#include "LzmaEnc.h"
#include "LzmaLib.h"

#include <assert.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef USE_LIBTOMCRYPT
extern "C"
{
#    include "mysha384.h"
}
#endif

#undef min
#undef max

using namespace std;

static void *AllocForLzma(void *p, size_t size) { return malloc(size); }
static void FreeForLzma(void *p, void *address) { free(address); }
static ISzAlloc SzAllocForLzma = {&AllocForLzma, &FreeForLzma};

static SRes res;
static unsigned outPos = 0, inPos = 0 /*LZMA_PROPS_SIZE*/;
const unsigned IN_BUF_SIZE = 0x600000;
const unsigned OUT_BUF_SIZE = 0x600000;
const char *optDeflate = "d";
const char *optCompress = "c";

static int UncompressInc(unsigned char *outBuf, unsigned int outBufsize,
                         unsigned char *inBuf, unsigned int inBufsize)
{
    ELzmaStatus status;
    unsigned char *pinBuf = inBuf;
    static CLzmaDec dec;
    int i;
    unsigned char outb[0x2000] = {0};
    for (i = 0; i < sizeof(CLzmaDec); i++) {
        char *p = (char *)&dec;
        if (p[i] != 0x0) {
            i = -1;
            break;
        }
    }
    if (i != -1) {
        LzmaDec_Construct(&dec);
        res =
            LzmaDec_Allocate(&dec, &inBuf[0], LZMA_PROPS_SIZE, &SzAllocForLzma);
        assert(res == SZ_OK);
        LzmaDec_Init(&dec);
        outPos = 0;
        inPos = LZMA_PROPS_SIZE;
        // pinBuf += LZMA_PROPS_SIZE
    }

#undef BIGLOOP
#define BIGLOOP
#ifdef BIGLOOP
    while (outPos < outBufsize) {
#endif
        unsigned srcLen;
        while ((srcLen = min(inBufsize, inBufsize - inPos)) > 0) {
            /*
                        unsigned destLen = min(OUT_BUF_SIZE, outBufsize -
               outPos); unsigned srcLenOld = srcLen, destLenOld = destLen;

                        res = LzmaDec_DecodeToBuf(&dec,
                              &outBuf[outPos], &destLen,
                              &pinBuf[inPos], &srcLen,
                              (outPos + destLen == outBufsize)
                              ? LZMA_FINISH_END : LZMA_FINISH_ANY, &status);
            */
            unsigned destLen = min(OUT_BUF_SIZE, outBufsize - outPos);
            // unsigned srcLenOld = srcLen, destLenOld = destLen;
            res = LzmaDec_DecodeToBuf(
                &dec, &outb[0], &destLen, &pinBuf[inPos], &srcLen,
                (outPos + destLen == outBufsize) ? LZMA_FINISH_END
                                                 : LZMA_FINISH_ANY,
                &status);
            assert(res == SZ_OK);
            if (status != SZ_OK) {
                break;
            }

            memcpy(&outBuf[outPos], outb, destLen);
            // printf("Processing OutBuffer... \r\n");
            inPos += srcLen;
            outPos += destLen;
            if (outPos >= outBufsize) {
                break;
            }
            if (status == LZMA_STATUS_FINISHED_WITH_MARK) {
                break;
            }
        }
        inPos = 0;
#ifdef BIGLOOP
        if (status != SZ_OK) {
            break;
        }
        pinBuf += IN_BUF_SIZE;
        if (outPos >= outBufsize) {
            break;
        }
    }
#endif

    if (outPos >= outBufsize) {
        LzmaDec_Free(&dec, &SzAllocForLzma);
    }
    return outPos;
    //  outBuf.resize(outPos);
}

SRes OnProgress(void *p, UInt64 inSize, UInt64 outSize)
{
    // Update progress bar.
    static int count = 0;
    const int divi = 0x10;
    if (!(count % divi)) {
        cout << ".";
    }
    count++;
    return SZ_OK;
}
static ICompressProgress g_ProgressCallback = {&OnProgress};

int Compress2(unsigned char *outBuf, const unsigned char *inBuf, size_t inBufsize)
{
    unsigned propsSize = LZMA_PROPS_SIZE;
    unsigned destLen = inBufsize + inBufsize / 3 + 128;
    // outBuf.resize(propsSize + destLen);

    CLzmaEncProps props;
    LzmaEncProps_Init(&props);
    props.dictSize = 1 << 16; // 64 KB
    props.writeEndMark = 1;   // 0 or 1

    res =
        LzmaEncode(&outBuf[LZMA_PROPS_SIZE], &destLen, &inBuf[0], inBufsize,
                   &props, &outBuf[0], &propsSize, props.writeEndMark,
                   &g_ProgressCallback, &SzAllocForLzma, &SzAllocForLzma);
    assert(res == SZ_OK && propsSize == LZMA_PROPS_SIZE);
    return destLen + propsSize;
    // outBuf.resize(propsSize + destLen);
}

int main(int argc, char *argv[])
{
    char *opt = argv[1];
    char *infilename = argv[2];
    char *outfilename = argv[3];
    if ((argc < 3) || (strcmp(opt, optDeflate) && strcmp(opt, optCompress))) {
        cout << "Missing argument(s).\r\nUsage: " << argv[0]
             << " [c d] infile outfile" << endl;
        return -1;
    }
    unsigned char *outbuf;
    unsigned char *inbuf;

    fstream a, b;

    a.open(infilename, ios::in | ios::binary | ios::ate);
    if (!a.good()) {
        cout << "Error opening file " << infilename << endl;
        a.close();
        return -1;
    }
    a.seekg(0, ios::end);
    size_t flen = a.tellg();

    assert(flen != ~0UL);

    inbuf = new unsigned char[flen];
    a.seekg(0, ios::beg);
    a.read((char *)inbuf, flen);
    if (a.bad()) {
        cout << "Error reading file " << infilename << endl;
        a.close();
        return -1;
    } else {
        printf("Read file with %d bytes.\r\n", flen);
    }
    a.close();

    size_t oflen;

    if (!strcmp(opt, optCompress)) {
        printf("Compressing...\r\n");
        outbuf = new unsigned char[flen];
        oflen = Compress2(outbuf, inbuf, flen);
    } else if (!strcmp(opt, optDeflate)) {
        oflen = flen * 10;
        outbuf = new unsigned char[oflen];
        printf("Deflating...\r\n");
        LzmaUncompress(&outbuf[0], &oflen, &inbuf[LZMA_PROPS_SIZE], &flen,
                       &inbuf[0], LZMA_PROPS_SIZE);
    }
    printf("\r\nDone.\r\n");

#ifdef USE_LIBTOMCRYPT
    printf("Calculating sha384sums...\r\n");
    unsigned char hash_buffer[48];

    calcHash(inbuf, flen, hash_buffer);

    printf("Input: sha384sum=");
    for (int n = 0; n < 48; n++) {
        printf("%02x", hash_buffer[n]);
    }
    printf("\r\n");

    calcHash(outbuf, oflen, hash_buffer);

    printf("Output: sha384sum=");
    for (int n = 0; n < 48; n++) {
        printf("%02x", hash_buffer[n]);
    }
    printf("\r\n");
#endif

    printf("\r\nWriting output file %s...\r\n", outfilename);
    b.open(outfilename, ios::out | ios::binary | ios::trunc);
    b.write((char *)outbuf, oflen);
    b.close();
    cout << "Done." << endl;
    cout << "Input size:  " << flen << endl;
    cout << "Output size: " << oflen << endl;
    double ratio = (double)oflen / (double)flen;
    cout << "Compression ratio: " << 100 * ratio << " %" << endl;
    delete[] inbuf;
    delete[] outbuf;

    /*ENTPACKEN :
    unsigned int srcLen = oflen;

    unsigned int dstLen = outbufsize;
    srcLen -= LZMA_PROPS_SIZE;


    start = time(0);
    int res = LzmaUncompress(
    &outbuf[0], &dstLen,
    &inbuf[LZMA_PROPS_SIZE], &srcLen,
    &inbuf[0], LZMA_PROPS_SIZE);
    end = time(0);


    cout << "Entpacken: " << end-start << " s" << endl;
    cout << "Err: " << res << ", len: " <<  dstLen/1024 << endl;
    cout << "Val: " << std::hex << (uint32_t)outbuf[0] << endl;

    of.open("rio.elf_dec",ios::out|ios::binary|ios::trunc );
    of.write((char*)outbuf, dstLen);
    of.close();
    */
    return 0;
}
