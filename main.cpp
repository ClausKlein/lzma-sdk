#include "LzmaDec.h"
#include "LzmaEnc.h"
#include "LzmaLib.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
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

static void* AllocForLzma(void* p, size_t size) { return malloc(size); }
static void FreeForLzma(void* p, void* address) { free(address); }
static ISzAlloc SzAllocForLzma = {&AllocForLzma, &FreeForLzma};

const char* optDeflate = "d";
const char* optCompress = "c";

#ifdef USE_UNUSED_FUNCTION
static SRes res;
static SizeT outPos = 0, inPos = 0 /*LZMA_PROPS_SIZE*/;
const SizeT IN_BUF_SIZE = 0x600000;
const SizeT OUT_BUF_SIZE = 0x600000;

static int UncompressInc(unsigned char* outBuf, SizeT outBufsize, unsigned char* inBuf,
                         SizeT inBufsize)
{
    ELzmaStatus status;
    unsigned char* pinBuf = inBuf;
    static CLzmaDec dec;
    int i;
    unsigned char outb[0x2000] = {0};
    for (i = 0; i < sizeof(CLzmaDec); i++) {
        char* p = (char*)&dec;
        if (p[i] != 0x0) {
            i = -1;
            break;
        }
    }
    if (i != -1) {
        LzmaDec_Construct(&dec);
        res = LzmaDec_Allocate(&dec, &inBuf[0], LZMA_PROPS_SIZE, &SzAllocForLzma);
        assert(res == SZ_OK); // NOLINT
        LzmaDec_Init(&dec);
        outPos = 0;
        inPos = LZMA_PROPS_SIZE;
        // pinBuf += LZMA_PROPS_SIZE
    }

#    undef BIGLOOP
#    define BIGLOOP
#    ifdef BIGLOOP
    while (outPos < outBufsize) {
#    endif

        SizeT srcLen;
        while ((srcLen = min(inBufsize, inBufsize - inPos)) > 0) {
            /*
               SizeT destLen = min(OUT_BUF_SIZE, outBufsize - outPos);
               SizeT srcLenOld = srcLen, destLenOld = destLen;

               res = LzmaDec_DecodeToBuf(&dec,
                     &outBuf[outPos], &destLen,
                     &pinBuf[inPos], &srcLen,
                     (outPos + destLen == outBufsize)
                     ? LZMA_FINISH_END : LZMA_FINISH_ANY, &status);
            */
            SizeT destLen = min(OUT_BUF_SIZE, outBufsize - outPos);
            // XXX prevent unused warning! CK SizeT srcLenOld = srcLen, destLenOld = destLen;
            res = LzmaDec_DecodeToBuf(
                &dec, &outb[0], &destLen, &pinBuf[inPos], &srcLen,
                (outPos + destLen == outBufsize) ? LZMA_FINISH_END : LZMA_FINISH_ANY, &status);
            assert(res == SZ_OK); // NOLINT

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

#    ifdef BIGLOOP
        if (status != SZ_OK) {
            break;
        }
        pinBuf += IN_BUF_SIZE;
        if (outPos >= outBufsize) {
            break;
        }
    }
#    endif

    if (outPos >= outBufsize) {
        LzmaDec_Free(&dec, &SzAllocForLzma);
    }
    return outPos;
    // outBuf.resize(outPos);
}
#endif

SRes OnProgress(void* p, UInt64 inSize, UInt64 outSize)
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

int Compress2(unsigned char* outBuf, const unsigned char* inBuf, SizeT inBufsize)
{
    SizeT propsSize = LZMA_PROPS_SIZE;
    SizeT destLen = inBufsize + inBufsize / 3 + 128;
    // outBuf.resize(propsSize + destLen);

    CLzmaEncProps props;
    LzmaEncProps_Init(&props);
    props.dictSize = 1 << 16; // 64 KB
    props.writeEndMark = 1;   // 0 or 1

    int res = LzmaEncode(&outBuf[LZMA_PROPS_SIZE], &destLen, &inBuf[0], inBufsize, &props, &outBuf[0],
                         &propsSize, props.writeEndMark, &g_ProgressCallback, &SzAllocForLzma,
                         &SzAllocForLzma);
    assert(res == SZ_OK && propsSize == LZMA_PROPS_SIZE); // NOLINT
    return destLen + propsSize;
    // outBuf.resize(propsSize + destLen);
}

int main(int argc, char* argv[])
{
    char* opt = argv[1];
    char* infilename = argv[2];
    char* outfilename = argv[3];
    if ((argc < 3) || (strcmp(opt, optDeflate) && strcmp(opt, optCompress))) {
        cout << "Missing argument(s).\r\nUsage: " << argv[0] << " [c d] infile outfile" << endl;
        return -1;
    }
    unsigned char* outbuf = NULL;
    unsigned char* inbuf = NULL;

    fstream a, b;

    a.open(infilename, ios::in | ios::binary | ios::ate);
    if (!a.good()) {
        cout << "Error opening file " << infilename << endl;
        a.close();
        return -1;
    }
    a.seekg(0, ios::end);
    streampos flen = a.tellg();
    assert(flen != -1); // NOLINT

    SizeT buf_len = static_cast<SizeT>(flen);
    inbuf = new unsigned char[buf_len];
    a.seekg(0, ios::beg);
    a.read((char*)inbuf, flen);
    if (a.bad()) {
        cout << "Error reading file " << infilename << endl;
        a.close();
        return -1;
    } else {
        // printf("Read file with %ld bytes.\r\n", flen);
    }
    a.close();

    SizeT oflen = 0;

    if (!strcmp(opt, optCompress)) {
        // printf("Compressing...\r\n");
        outbuf = new unsigned char[buf_len];
        oflen = Compress2(outbuf, inbuf, buf_len);
    } else if (!strcmp(opt, optDeflate)) {
        oflen = buf_len * 10;
        outbuf = new unsigned char[oflen];
        // printf("Deflating...\r\n");
        int res = LzmaUncompress(&outbuf[0], &oflen, &inbuf[LZMA_PROPS_SIZE], &buf_len, &inbuf[0],
                                 LZMA_PROPS_SIZE);
        assert(res == SZ_OK); // NOLINT
    }

#ifdef USE_LIBTOMCRYPT
    printf("\r\nDone.\r\n");
    printf("Calculating sha384sums...\r\n");
    unsigned char hash_buffer[48];

    calcHash(inbuf, buf_len, hash_buffer);

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
    printf("\r\nWriting output file %s...\r\n", outfilename);
#endif

    b.open(outfilename, ios::out | ios::binary | ios::trunc);
    b.write((char*)outbuf, oflen); // NOLINT
    b.close();
    cout << "Done." << endl;
    cout << "Input size:  " << buf_len << endl;
    cout << "Output size: " << oflen << endl;
    double ratio = (double)oflen / (double)buf_len;
    cout << "Compression ratio: " << 100 * ratio << " %" << endl;
    delete[] inbuf;
    delete[] outbuf;

    /* ENTPACKEN:
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
