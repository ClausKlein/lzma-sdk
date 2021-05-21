#include "LzmaDec.h"
#include "LzmaEnc.h"
#include "LzmaLib.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string.h>

#ifdef USE_LIBTOMCRYPT
extern "C"
{
#    include "mysha384.h"
}
#endif

#ifdef _WIN32
#    undef min
#    undef max
#endif

using namespace std;

static void* AllocForLzma(void* p, size_t size)
{
    return malloc(size); // NOLINT(cppcoreguidelines-no-malloc)
}
static void FreeForLzma(void* p, void* address)
{
    free(address); // NOLINT(cppcoreguidelines-no-malloc)
}
static ISzAlloc SzAllocForLzma = {&AllocForLzma, &FreeForLzma};

//
// FIXME: this does not work! CK
//
#undef USE_UNUSED_FUNCTION
#ifdef USE_UNUSED_FUNCTION
static SRes res;
static SizeT outPos = 0, inPos = 0 /* TODO: Why not inPos = LZMA_PROPS_SIZE; ? CK */;

static SizeT UncompressInc(unsigned char* outBuf, SizeT outBufsize, unsigned char* inBuf,
                           SizeT inBufsize)
{
    const SizeT OUT_BUF_SIZE = 0x600000;

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

#    define BIGLOOP
#    ifdef BIGLOOP
    while (outPos < outBufsize) {
    const SizeT IN_BUF_SIZE = 0x600000;
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

//
// XXX
// Attention: this is incompatible to Lzma! CK
// see Util/Lzma/LzmaUtil.c #134 Encode()
// XXX
//
SizeT Compress2(unsigned char* outBuf, const unsigned char* inBuf, SizeT inBufsize)
{
    SizeT propsSize = LZMA_PROPS_SIZE;
    SizeT destLen = inBufsize + inBufsize / 3 + 128;
    // outBuf.resize(propsSize + destLen);

    CLzmaEncProps props;
    LzmaEncProps_Init(&props);
    props.dictSize = 1 << 16; // 64 KB
    props.writeEndMark = 1;   // 0 or 1

    int res =
        LzmaEncode(&outBuf[LZMA_PROPS_SIZE], &destLen, inBuf, inBufsize, &props, outBuf, &propsSize,
                   props.writeEndMark, &g_ProgressCallback, &SzAllocForLzma, &SzAllocForLzma);
    assert(res == SZ_OK && propsSize == LZMA_PROPS_SIZE); // NOLINT
    return destLen + propsSize;
    // outBuf.resize(propsSize + destLen);
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cout << "Missing argument(s).\nUsage: " << *argv << " [c d] infile outfile" << endl;
        return 0;
    }
    const char* opt = argv[1];
    const char* optDeflate = "d";
    const char* optCompress = "c";

    if ((argc < 4) || (strcmp(opt, optDeflate) && strcmp(opt, optCompress))) {
        cout << "Wrong arguments.\nUsage: " << *argv << " [c d] infile outfile" << endl;
        return 0;
    }
    const char* infilename = argv[2];
    const char* outfilename = argv[3];

    unsigned char* outbuf = NULL;
    unsigned char* inbuf = NULL;
    fstream in, out;

    in.open(infilename, ios::in | ios::binary | ios::ate);
    if (!in.good()) {
        cout << "Error opening file " << infilename << endl;
        in.close();
        return -1;
    }

    in.seekg(0, ios::end);
    streampos flen = in.tellg();
    assert(flen != -1); // NOLINT

    SizeT buf_len = static_cast<SizeT>(flen);
    inbuf = new unsigned char[buf_len];
    in.seekg(0, ios::beg);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    in.read(reinterpret_cast<char*>(inbuf), flen);
    if (in.bad()) {
        delete[] inbuf;
        cout << "Error reading file " << infilename << endl;
        in.close();
        return -1;
    }

    // printf("Read file with %ld bytes.\r\n", flen);
    in.close();

    SizeT oflen = 0;

    if (!strcmp(opt, optCompress)) {
        // printf("Compressing...\r\n");
        outbuf = new unsigned char[buf_len];
        oflen = Compress2(outbuf, inbuf, buf_len);
    } else if (!strcmp(opt, optDeflate)) {
        oflen = buf_len * 10;
        outbuf = new unsigned char[oflen];

#ifdef USE_UNUSED_FUNCTION
        printf("Deflating...\n");
        oflen = UncompressInc(outbuf, oflen, inbuf, buf_len);
        printf("\nDone\n");
#else
        int res =
            LzmaUncompress(outbuf, &oflen, &inbuf[LZMA_PROPS_SIZE], &buf_len, inbuf, LZMA_PROPS_SIZE);
        assert(res == SZ_OK); // NOLINT
#endif
    }

#ifdef USE_LIBTOMCRYPT
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

    out.open(outfilename, ios::out | ios::binary | ios::trunc);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    out.write(reinterpret_cast<char*>(outbuf), static_cast<streamsize>(oflen));
    out.close();
    cout << "Done." << endl;
    cout << "Input size:  " << buf_len << endl;
    cout << "Output size: " << oflen << endl;
    double ratio = (double)oflen / (double)buf_len;
    cout << "Compression ratio: " << 100 * ratio << " %" << endl;
    delete[] inbuf;
    delete[] outbuf;

    /*** ENTPACKEN:
     * unsigned int srcLen = oflen;
     * unsigned int dstLen = outbufsize;
     * srcLen -= LZMA_PROPS_SIZE;

     * time_t start = time(0);
     * int res = LzmaUncompress(
     *     &outbuf[0], &dstLen,
     *     &inbuf[LZMA_PROPS_SIZE], &srcLen,
     *     &inbuf[0], LZMA_PROPS_SIZE
     * );
     * time_t end = time(0);

     * cout << "Entpacken: " << end-start << " s" << endl;
     * cout << "Err: " << res << ", len: " <<  dstLen/1024 << endl;
     * cout << "Val: " << std::hex << (uint32_t)outbuf[0] << endl;

     * fstream of;
     * of.open("rio.elf_dec",ios::out|ios::binary|ios::trunc );
     * of.write((char*)outbuf, dstLen);
     * of.close();
     ***/

    return 0;
}
