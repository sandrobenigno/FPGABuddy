#include "utils.h"
#include <string.h>
#include <stdio.h>

// MD5 basic transformation constants and rotates
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void md5_transform(uint32_t state[4], const uint8_t block[64]);
static void md5_encode(uint8_t* output, const uint32_t* input, size_t len);
static void md5_decode(uint32_t* output, const uint8_t* input, size_t len);

// F, G, H and I are basic MD5 functions.
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

// ROTATE_LEFT rotates x left n bits.
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

// FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (uint32_t)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

void md5_init(MD5_CTX* context) {
    context->count[0] = context->count[1] = 0;
    // Load magic initialization constants.
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}

void md5_update(MD5_CTX* context, const uint8_t* input, size_t input_len) {
    size_t i, index, part_len;

    // Compute number of bytes mod 64
    index = (size_t)((context->count[0] >> 3) & 0x3F);

    // Update number of bits
    if ((context->count[0] += ((uint32_t)input_len << 3)) < ((uint32_t)input_len << 3)) {
        context->count[1]++;
    }
    context->count[1] += ((uint32_t)input_len >> 29);

    part_len = 64 - index;

    // Transform as many times as possible.
    if (input_len >= part_len) {
        memcpy(&context->buffer[index], input, part_len);
        md5_transform(context->state, context->buffer);

        for (i = part_len; i + 63 < input_len; i += 64) {
            md5_transform(context->state, &input[i]);
        }

        index = 0;
    } else {
        i = 0;
    }

    // Buffer remaining input
    memcpy(&context->buffer[index], &input[i], input_len - i);
}

void md5_final(uint8_t digest[16], MD5_CTX* context) {
    uint8_t bits[8];
    size_t index, pad_len;
    static const uint8_t PADDING[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    // Save number of bits
    md5_encode(bits, context->count, 8);

    // Pad out to 56 mod 64.
    index = (size_t)((context->count[0] >> 3) & 0x3f);
    pad_len = (index < 56) ? (56 - index) : (120 - index);
    md5_update(context, PADDING, pad_len);

    // Append length (before padding)
    md5_update(context, bits, 8);

    // Store state in digest
    md5_encode(digest, context->state, 16);

    // Zeroize sensitive information.
    memset(context, 0, sizeof(*context));
}

void md5_to_str(const uint8_t digest[16], char out_str[33]) {
    for (int i = 0; i < 16; i++) {
        sprintf(&out_str[i * 2], "%02x", digest[i]);
    }
    out_str[32] = '\0';
}

// MD5 basic transformation. Transforms state based on block.
static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    md5_decode(x, block, 64);

    // Round 1
    FF(a, d, c, b, x[ 0], S11, 0xd76aa478); /* 1 */
    FF(b, a, d, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    FF(c, b, a, d, x[ 2], S13, 0x242070db); /* 3 */
    FF(d, c, b, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    FF(a, d, c, b, x[ 4], S11, 0xf57c0faf); /* 5 */
    FF(b, a, d, c, x[ 5], S12, 0x4787c62a); /* 6 */
    FF(c, b, a, d, x[ 6], S13, 0xa8304613); /* 7 */
    FF(d, c, b, a, x[ 7], S14, 0xfd469501); /* 8 */
    FF(a, d, c, b, x[ 8], S11, 0x698098d8); /* 9 */
    FF(b, a, d, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    FF(c, b, a, d, x[10], S13, 0xffff5bb1); /* 11 */
    FF(d, c, b, a, x[11], S14, 0x895cd7be); /* 12 */
    FF(a, d, c, b, x[12], S11, 0x6b901122); /* 13 */
    FF(b, a, d, c, x[13], S12, 0xfd987193); /* 14 */
    FF(c, b, a, d, x[14], S13, 0xa679438e); /* 15 */
    FF(d, c, b, a, x[15], S14, 0x49b40821); /* 16 */

    // Round 2
    GG(a, d, c, b, x[ 1], S21, 0xf61e2562); /* 17 */
    GG(b, a, d, c, x[ 6], S22, 0xc040b340); /* 18 */
    GG(c, b, a, d, x[11], S23, 0x265e5a51); /* 19 */
    GG(d, c, b, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG(a, d, c, b, x[ 5], S21, 0xd62f105d); /* 21 */
    GG(b, a, d, c, x[10], S22,  0x2441453); /* 22 */
    GG(c, b, a, d, x[15], S23, 0xd8a1e681); /* 23 */
    GG(d, c, b, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG(a, d, c, b, x[ 9], S21, 0x21e1cde6); /* 25 */
    GG(b, a, d, c, x[14], S22, 0xc33707d6); /* 26 */
    GG(c, b, a, d, x[ 3], S23, 0xf4d50d87); /* 27 */
    GG(d, c, b, a, x[ 8], S24, 0x455a14ed); /* 28 */
    GG(a, d, c, b, x[13], S21, 0xa9e3e905); /* 29 */
    GG(b, a, d, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    GG(c, b, a, d, x[ 7], S23, 0x676f02d9); /* 31 */
    GG(d, c, b, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    // Round 3
    HH(a, d, c, b, x[ 5], S31, 0xfffa3942); /* 33 */
    HH(b, a, d, c, x[ 8], S32, 0x8771f681); /* 34 */
    HH(c, b, a, d, x[11], S33, 0x6d9d6122); /* 35 */
    HH(d, c, b, a, x[14], S34, 0xfde5380c); /* 36 */
    HH(a, d, c, b, x[ 1], S31, 0xa4beea44); /* 37 */
    HH(b, a, d, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    HH(c, b, a, d, x[ 7], S33, 0xf6bb4b60); /* 39 */
    HH(d, c, b, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH(a, d, c, b, x[13], S31, 0x289b7ec6); /* 41 */
    HH(b, a, d, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    HH(c, b, a, d, x[ 3], S33, 0xd4ef3085); /* 43 */
    HH(d, c, b, a, x[ 6], S34,  0x4881d05); /* 44 */
    HH(a, d, c, b, x[ 9], S31, 0xd9d4d039); /* 45 */
    HH(b, a, d, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH(c, b, a, d, x[15], S33, 0x1fa27cf8); /* 47 */
    HH(d, c, b, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    // Round 4
    II(a, d, c, b, x[ 0], S41, 0xf4292244); /* 49 */
    II(b, a, d, c, x[ 7], S42, 0x432aff97); /* 50 */
    II(c, b, a, d, x[14], S43, 0xab9423a7); /* 51 */
    II(d, c, b, a, x[ 5], S44, 0xfc93a039); /* 52 */
    II(a, d, c, b, x[12], S41, 0x655b59c3); /* 53 */
    II(b, a, d, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    II(c, b, a, d, x[10], S43, 0xffeff47d); /* 55 */
    II(d, c, b, a, x[ 1], S44, 0x85845dd1); /* 56 */
    II(a, d, c, b, x[ 8], S41, 0x6fa87e4f); /* 57 */
    II(b, a, d, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II(c, b, a, d, x[ 6], S43, 0xa3014314); /* 59 */
    II(d, c, b, a, x[13], S44, 0x4e0811a1); /* 60 */
    II(a, d, c, b, x[ 4], S41, 0xf7537e82); /* 61 */
    II(b, a, d, c, x[11], S42, 0xbd3af235); /* 62 */
    II(c, b, a, d, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    II(d, c, b, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    // Zeroize sensitive information
    memset(x, 0, sizeof(x));
}

// Encodes input (uint32_t) into output (uint8_t). Assumes len is a multiple of 4.
static void md5_encode(uint8_t* output, const uint32_t* input, size_t len) {
    size_t i, j;
    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j] = (uint8_t)(input[i] & 0xff);
        output[j + 1] = (uint8_t)((input[i] >> 8) & 0xff);
        output[j + 2] = (uint8_t)((input[i] >> 16) & 0xff);
        output[j + 3] = (uint8_t)((input[i] >> 24) & 0xff);
    }
}

// Decodes input (uint8_t) into output (uint32_t). Assumes len is a multiple of 4.
static void md5_decode(uint32_t* output, const uint8_t* input, size_t len) {
    size_t i, j;
    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[i] = ((uint32_t)input[j]) | (((uint32_t)input[j + 1]) << 8) |
                    (((uint32_t)input[j + 2]) << 16) | (((uint32_t)input[j + 3]) << 24);
    }
}

// -------------------------------------------------------------
// Tabela de Lookup e Implementação Rápida do CRC32
// -------------------------------------------------------------
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfedd1772, 0x89d027e4, 0x10e3765e, 0x67e446c8, 0xfa05177b, 0x8d0227ed, 0x140b7657, 0x630c46c1,
    0xd2d74084, 0xa5d07012, 0x3cc921a8, 0x4bc0113e, 0xd5ba849d, 0xa2bdb40b, 0x3bb4e5b1, 0x4cb3d527,
    0xdbfc08f6, 0xacfb3860, 0x35f269da, 0x42f5594c, 0xdc91c97f, 0xad96f9e9, 0x329f1d53, 0x45982dc5,
    0xc5ac30a0, 0xb2ab0036, 0x2ba2518c, 0x5ca5611a, 0xc2c1f0b9, 0xb5c6c02f, 0x2ccfb195, 0x5bced103,
    0xcb77d68d, 0xbc70e61b, 0x2579b7a1, 0x527e8737, 0xcc0c17f4, 0xbd0b2762, 0x220276d8, 0x5505464e,
    0x96d98120, 0xe1d6b1b6, 0x78d7e0fc, 0x0fd0d06a, 0x91b44939, 0xe6b379af, 0x7fbd2815, 0x08baf883,
    0x98090a78, 0xef0e3ae2, 0x76076b54, 0x01005bc2, 0x9f64cd61, 0xe863fdf7, 0x716abcc5, 0x066d8c53,
    0x8b65625d, 0xfc6252cb, 0x656b0371, 0x126c33e7, 0x8c085644, 0xfb0f66d2, 0x62063768, 0x150107fe,
    0x85be4120, 0xf2b971b6, 0x6bb0200c, 0x1cb7109a, 0x82d38539, 0xf5d4b5af, 0x6cd2e415, 0x1bd5d483,
    0xdd5a0ba0, 0xaa5d3b36, 0x33546a8c, 0x44535a1a, 0xda39d5b9, 0xad3ee52f, 0x3437b495, 0x43308403,
    0xd380e22a, 0xa487d2bc, 0x3d8ea306, 0x4a899390, 0xd4e32233, 0xa3e412a5, 0x3aed431f, 0x4dca7389,
    0xc01d68ad, 0xb71a583b, 0x2e130981, 0x59143917, 0xc7706cbe, 0xb0775c28, 0x297e0d92, 0x5e793d04,
    0xcebd90bf, 0xb9baa029, 0x20b3f193, 0x57b4c105, 0xc9d21e8d, 0xbed52e1b, 0x27dc7fa1, 0x50db4f37,
    0x5768b520, 0x206f85b6, 0xb966d40c, 0xce61e49a, 0x50057139, 0x270241af, 0xbe0b1015, 0xc90c2083,
    0x59b035f2, 0x2eb70564, 0xb7be54de, 0xc0b96448, 0x5edd71eb, 0x29da417d, 0xb0d310c7, 0xc7d42051,
    0x4adfa540, 0x3dd895d6, 0xa4d1c46c, 0xd3d6f4fa, 0x4db26159, 0x3ab551cf, 0xa3bc0075, 0xd4bb30e3,
    0x44042d72, 0x33031de4, 0xaa0a4c5e, 0xdd0d7cc8, 0x4369e96b, 0x346ed9fd, 0xad678847, 0xda60b8d1,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x7161858d, 0x0666b51b, 0x9f6fe4a1, 0xe868d437, 0x760c4194, 0x010b7102, 0x980220b8, 0xef05102e,
    0x7fba98be, 0x08bdab28, 0x91b4f992, 0xe6b3c904, 0x78d75c87, 0x0fd06c11, 0x96d93da3, 0xe1de0d35,
    0x8be5359a, 0xfce2050c, 0x65eb54b6, 0x12ec6420, 0x8c88f183, 0xfb8fc115, 0x628690af, 0x1581a039,
    0x853eb5a8, 0xf239853e, 0x6b30d484, 0x1c37e412, 0x82b371b1, 0xf5b44127, 0x6cd2109d, 0x1bd5200b,
    0xc30c2084, 0xb40b1012, 0x2d0241a8, 0x5a05713e, 0xc461e49d, 0xb366d40b, 0x2a6f85b1, 0x5d68b527,
    0xcd0a98b2, 0xba0db824, 0x2304f99e, 0x5403c908, 0xca675cdb, 0xbd606c4d, 0x24693db7, 0x536e0d21,
    0xd0b97148, 0xa7bec1de, 0x3eb71064, 0x49b020f2, 0xd7d4b551, 0xa0d385c7, 0x39deb47d, 0x4ed984eb,
    0xde5ae9fc, 0xa95d796a, 0x305488d0, 0x4753b846, 0xd9372d85, 0xae301d13, 0x37394c99, 0x403e7c0f,
    0xe0dfb570, 0x97d885e6, 0x0ed1d45c, 0x79d6e4ca, 0xe7b27169, 0x90b541ff, 0x09be1045, 0x7eb920d3,
    0xee043672, 0x990306e4, 0x000a575e, 0x770d67c8, 0xe969f25a, 0x9e6ec2cc, 0x07679376, 0x7060a3e0,
    0x8a65c9ec, 0xfd62f97a, 0x646bafc0, 0x136c9f56, 0x8d080df5, 0xfa0f3d63, 0x63081d09, 0x140f2d9f,
    0x84be41de, 0xf3b97148, 0x6ab020f2, 0xf3b97148, 0x83d385c7, 0xf4d4b551, 0x6ddde4eb, 0x1adad47d,
    0x97d2d988, 0xe0d5e91e, 0x79d2b8a4, 0x0edd8832, 0x90bf1d91, 0xe7b82d07, 0x7eb17cbd, 0x09b64c2b,
    0x990951ba, 0xee0e612c, 0x77073096, 0x00000000, 0x9e6495a3, 0xe963a535, 0x706af48f, 0x076dc419,
    0xb6662d3d, 0xc1611dab, 0x58684c11, 0x2f6f7c87, 0xbf1db65d, 0xc81a86cb, 0x5113d771, 0x2614e7e7,
    0xb8bda50f, 0xcfba9599, 0x56b3c423, 0x21b4f4b5, 0xbfd06116, 0xc8d75180, 0x51de003a, 0x26d930ac,
    0xad678846, 0xda60b8d0, 0x4369e96a, 0x346ed9fc, 0xad6a8f6f, 0xda6dbf99, 0x4364ee43, 0x3463de75,
    0xa50ab56b, 0xd20d85fd, 0x4b04d447, 0x3c03e4d1, 0xa2677172, 0xd56041e4, 0x4c69105e, 0x3b6e20c8,
    0x8c080df5, 0xfb0f3d63, 0x62066cd9, 0x15015c4f, 0x8b65c9ec, 0xfd62f97a, 0x656bafc0, 0x126c9f56,
    0x82d385c7, 0xf5d4b551, 0x6cdde4eb, 0x1adad47d, 0x85be41de, 0xf3b97148, 0x6ab020f2, 0x1db71064,
    0x91b4f992, 0xe6b3c904, 0x7fba98be, 0x08bdab28, 0x96d93da3, 0xe1de0d35, 0x78d75c87, 0x0fd06c11,
    0x9f6fe4a1, 0xe868d437, 0x7161858d, 0x0666b51b, 0x980220b8, 0xef05102e, 0x760c4194, 0x010b7102,
    0xbd3af235, 0xca3db2ad, 0x53349319, 0x2433a38f, 0xba54f62c, 0xcd53c6ba, 0x545a9700, 0x235da796,
    0xb1e07b07, 0xc6e74b91, 0x5fedea2b, 0x28ebdabd, 0xb68dbf1e, 0xc18abf88, 0x5883dffd, 0x2f84ef6b,
    0xa0d3b47d, 0xd7d484eb, 0x4eddeb51, 0x39de9bc7, 0xa7be7064, 0xd0b940f2, 0x49b01148, 0x3eb721de,
    0xae08b44f, 0xd90f84d9, 0x4006d563, 0x3701e5f5, 0xa9657056, 0xde6240c0, 0x476b117a, 0x306c21ec
};

uint32_t crc32_calculate(const uint8_t* data, size_t length) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < length; i++) {
        uint8_t table_index = (uint8_t)(crc ^ data[i]);
        crc = (crc >> 8) ^ crc32_table[table_index];
    }
    return crc ^ 0xffffffff;
}
