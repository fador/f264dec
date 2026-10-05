#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "f264dec.h"

// Minimal portable MD5 implementation
namespace md5_detail {
    struct MD5Context {
        uint32_t state[4];
        uint32_t count[2];
        uint8_t buffer[64];
    };

    static void transform(uint32_t state[4], const uint8_t block[64]) {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
        for (int i = 0; i < 16; ++i) {
            x[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) |
                   ((uint32_t)block[i * 4 + 2] << 16) | ((uint32_t)block[i * 4 + 3] << 24);
        }
        #define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
        #define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
        #define H(x, y, z) ((x) ^ (y) ^ (z))
        #define I(x, y, z) ((y) ^ ((x) | (~z)))
        #define ROT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
        #define STEP(f, a, b, c, d, x, s, ac) { \
            (a) += f((b), (c), (d)) + (x) + (uint32_t)(ac); \
            (a) = ROT((a), (s)); \
            (a) += (b); \
        }
        STEP(F, a, b, c, d, x[0], 7, 0xd76aa478);   STEP(F, d, a, b, c, x[1], 12, 0xe8c7b756);
        STEP(F, c, d, a, b, x[2], 17, 0x242070db);  STEP(F, b, c, d, a, x[3], 22, 0xc1bdceee);
        STEP(F, a, b, c, d, x[4], 7, 0xf57c0faf);   STEP(F, d, a, b, c, x[5], 12, 0x4787c62a);
        STEP(F, c, d, a, b, x[6], 17, 0xa8304613);  STEP(F, b, c, d, a, x[7], 22, 0xfd469501);
        STEP(F, a, b, c, d, x[8], 7, 0x698098d8);   STEP(F, d, a, b, c, x[9], 12, 0x8b44f7af);
        STEP(F, c, d, a, b, x[10], 17, 0xffff5bb1); STEP(F, b, c, d, a, x[11], 22, 0x895cd7be);
        STEP(F, a, b, c, d, x[12], 7, 0x6b901122);  STEP(F, d, a, b, c, x[13], 12, 0xfd987193);
        STEP(F, c, d, a, b, x[14], 17, 0xa679438e); STEP(F, b, c, d, a, x[15], 22, 0x49b40821);

        STEP(G, a, b, c, d, x[1], 5, 0xf61e2562);   STEP(G, d, a, b, c, x[6], 9, 0xc040b340);
        STEP(G, c, d, a, b, x[11], 14, 0x265e5a51); STEP(G, b, c, d, a, x[0], 20, 0xe9b6c7aa);
        STEP(G, a, b, c, d, x[5], 5, 0xd62f105d);   STEP(G, d, a, b, c, x[10], 9, 0x02441453);
        STEP(G, c, d, a, b, x[15], 14, 0xd8a1e681); STEP(G, b, c, d, a, x[4], 20, 0xe7d3fbc8);
        STEP(G, a, b, c, d, x[9], 5, 0x21e1cde6);   STEP(G, d, a, b, c, x[14], 9, 0xc33707d6);
        STEP(G, c, d, a, b, x[3], 14, 0xf4d50d87);  STEP(G, b, c, d, a, x[8], 20, 0x455a14ed);
        STEP(G, a, b, c, d, x[13], 5, 0xa9e3e905);  STEP(G, d, a, b, c, x[2], 9, 0xfcefa3f8);
        STEP(G, c, d, a, b, x[7], 14, 0x676f02d9);  STEP(G, b, c, d, a, x[12], 20, 0x8d2a4c8a);

        STEP(H, a, b, c, d, x[5], 4, 0xfffa3942);   STEP(H, d, a, b, c, x[8], 11, 0x8771f681);
        STEP(H, c, d, a, b, x[11], 16, 0x6d9d6122); STEP(H, b, c, d, a, x[14], 23, 0xfde5380c);
        STEP(H, a, b, c, d, x[1], 4, 0xa4beea44);   STEP(H, d, a, b, c, x[4], 11, 0x4bdecfa9);
        STEP(H, c, d, a, b, x[7], 16, 0xf6bb4b60);  STEP(H, b, c, d, a, x[10], 23, 0xbebfbc70);
        STEP(H, a, b, c, d, x[13], 4, 0x289b7ec6);  STEP(H, d, a, b, c, x[0], 11, 0xeaa127fa);
        STEP(H, c, d, a, b, x[3], 16, 0xd4ef3085);  STEP(H, b, c, d, a, x[6], 23, 0x04881d05);
        STEP(H, a, b, c, d, x[9], 4, 0xd9d4d039);   STEP(H, d, a, b, c, x[12], 11, 0xe6db99e5);
        STEP(H, c, d, a, b, x[15], 16, 0x1fa27cf8); STEP(H, b, c, d, a, x[2], 23, 0xc4ac5665);

        STEP(I, a, b, c, d, x[0], 6, 0xf4292244);   STEP(I, d, a, b, c, x[7], 10, 0x432aff97);
        STEP(I, c, d, a, b, x[14], 15, 0xab9423a7); STEP(I, b, c, d, a, x[5], 21, 0xfc93a039);
        STEP(I, a, b, c, d, x[12], 6, 0x655b59c3);  STEP(I, d, a, b, c, x[3], 10, 0x8f0ccc92);
        STEP(I, c, d, a, b, x[10], 15, 0xffeff47d); STEP(I, b, c, d, a, x[1], 21, 0x85845dd1);
        STEP(I, a, b, c, d, x[8], 6, 0x6fa87e4f);   STEP(I, d, a, b, c, x[15], 10, 0xfe2ce6e0);
        STEP(I, c, d, a, b, x[6], 15, 0xa3014314);  STEP(I, b, c, d, a, x[13], 21, 0x4e0811a1);
        STEP(I, a, b, c, d, x[4], 6, 0xf7537e82);   STEP(I, d, a, b, c, x[11], 10, 0xbd3af235);
        STEP(I, c, d, a, b, x[2], 15, 0x2ad7d2bb);  STEP(I, b, c, d, a, x[9], 21, 0xeb86d391);
        #undef F
        #undef G
        #undef H
        #undef I
        #undef ROT
        #undef STEP
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    }

    static void init(MD5Context *ctx) {
        ctx->count[0] = ctx->count[1] = 0;
        ctx->state[0] = 0x67452301;
        ctx->state[1] = 0xefcdab89;
        ctx->state[2] = 0x98badcfe;
        ctx->state[3] = 0x10325476;
    }

    static void update(MD5Context *ctx, const uint8_t *input, size_t inputLen) {
        uint32_t i, index, partLen;
        index = (uint32_t)((ctx->count[0] >> 3) & 0x3F);
        if ((ctx->count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3))
            ctx->count[1]++;
        ctx->count[1] += ((uint32_t)inputLen >> 29);
        partLen = 64 - index;
        if (inputLen >= partLen) {
            memcpy(&ctx->buffer[index], input, partLen);
            transform(ctx->state, ctx->buffer);
            for (i = partLen; i + 63 < inputLen; i += 64)
                transform(ctx->state, &input[i]);
            index = 0;
        } else {
            i = 0;
        }
        memcpy(&ctx->buffer[index], &input[i], inputLen - i);
    }

    static void final(uint8_t digest[16], MD5Context *ctx) {
        uint8_t bits[8];
        for (int i = 0; i < 8; ++i)
            bits[i] = (uint8_t)((ctx->count[i >= 4 ? 1 : 0] >> ((i % 4) * 8)) & 0xFF);
        uint32_t index = (uint32_t)((ctx->count[0] >> 3) & 0x3f);
        uint32_t padLen = (index < 56) ? (56 - index) : (120 - index);
        static const uint8_t PADDING[64] = { 0x80 };
        update(ctx, PADDING, padLen);
        update(ctx, bits, 8);
        for (int i = 0; i < 16; ++i)
            digest[i] = (uint8_t)((ctx->state[i / 4] >> ((i % 4) * 8)) & 0xFF);
    }

    static std::string hex(const uint8_t digest[16]) {
        std::ostringstream ss;
        for (int i = 0; i < 16; ++i)
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
        return ss.str();
    }
}

static void hash_picture(md5_detail::MD5Context *ctx, const f264_picture *pic) {
    int bytes_per_sample = (pic->bit_depth > 8) ? 2 : 1;
    // Luma plane
    for (int y = 0; y < pic->height; ++y) {
        md5_detail::update(ctx, (const uint8_t*)pic->y + (size_t)y * pic->stride, (size_t)pic->width * bytes_per_sample);
    }
    // Chroma U plane
    if (pic->u && pic->width_c > 0 && pic->height_c > 0) {
        for (int y = 0; y < pic->height_c; ++y) {
            md5_detail::update(ctx, (const uint8_t*)pic->u + (size_t)y * pic->stride_c, (size_t)pic->width_c * bytes_per_sample);
        }
    }
    // Chroma V plane
    if (pic->v && pic->width_c > 0 && pic->height_c > 0) {
        for (int y = 0; y < pic->height_c; ++y) {
            md5_detail::update(ctx, (const uint8_t*)pic->v + (size_t)y * pic->stride_c, (size_t)pic->width_c * bytes_per_sample);
        }
    }
}

int main(int argc, char **argv) {
    std::cout << "test_memory_api starting..." << std::endl;
    std::string stream_path = "tests/streams/base_cavlc_slices.264";
    std::string md5_path = "tests/streams/base_cavlc_slices.md5";
    int expected_bit_depth = 0;
    if (argc >= 2) stream_path = argv[1];
    if (argc >= 3) md5_path = argv[2];
    if (argc >= 4) expected_bit_depth = std::atoi(argv[3]);

    std::cout << "Opening stream: " << stream_path << std::endl;
    std::ifstream sf(stream_path, std::ios::binary);
    if (!sf) {
        std::cerr << "Failed to open stream file: " << stream_path << std::endl;
        return 1;
    }
    std::vector<uint8_t> stream_data((std::istreambuf_iterator<char>(sf)), std::istreambuf_iterator<char>());
    std::cout << "Read " << stream_data.size() << " bytes." << std::endl;

    std::string expected_md5;
    std::ifstream mf(md5_path);
    if (mf) {
        mf >> expected_md5;
    }

    const f264_api *api = f264_api_get(8);
    f264_config *cfg = api->config_alloc();
    cfg->memory_input = 1;
    cfg->silent = 1;
    cfg->threads = 1;
    cfg->outfile[0] = '\0'; // IN-MEMORY ONLY! No file output!

    // Test config_parse for deblock_enable and file_format
    if (!api->config_parse(cfg, "deblock_enable", "1") || cfg->deblock_enable != 1) {
        std::cerr << "FAIL: config_parse deblock_enable failed\n";
        return 1;
    }
    if (!api->config_parse(cfg, "file_format", "0") || cfg->file_format != 0) {
        std::cerr << "FAIL: config_parse file_format failed\n";
        return 1;
    }

    std::cout << "Calling decoder_open..." << std::endl;
    f264_decoder *dec = api->decoder_open(cfg);
    if (!dec) {
        std::cerr << "Failed to open in-memory decoder\n";
        api->config_destroy(cfg);
        return 1;
    }
    std::cout << "Decoder opened successfully." << std::endl;

    // Test single-instance constraint: second open should return NULL
    f264_decoder *dec2 = api->decoder_open(cfg);
    if (dec2 != nullptr) {
        std::cerr << "FAIL: second decoder_open should have returned NULL\n";
        api->decoder_close(dec2);
        return 1;
    }
    std::cout << "Verified single-instance guard successfully." << std::endl;

    md5_detail::MD5Context md5_ctx;
    md5_detail::init(&md5_ctx);

    size_t chunk_size = 2048;
    std::cout << "Pushing " << stream_data.size() << " bytes in " << chunk_size << "-byte chunks..." << std::endl;
    for (size_t off = 0; off < stream_data.size(); off += chunk_size) {
        size_t n = std::min(chunk_size, stream_data.size() - off);
        api->decoder_push(dec, stream_data.data() + off, n);
    }
    std::cout << "All chunks pushed." << std::endl;

    int frames_received = 0;
    int reported_bit_depth = 0;
    f264_picture *pic = nullptr;

    while (true) {
        int ret = api->decoder_decode(dec, &pic);
        if (ret == F264_EOS) {
            break;
        }
        if (ret != F264_OK) {
            std::cerr << "Decode error code " << ret << std::endl;
            break;
        }
        if (pic) {
            if (reported_bit_depth == 0) {
                reported_bit_depth = pic->bit_depth;
            }
            hash_picture(&md5_ctx, pic);
            frames_received++;
        }
    }

    std::cout << "Flushing remaining pictures..." << std::endl;
    while (api->decoder_flush(dec, &pic) == F264_OK && pic) {
        if (reported_bit_depth == 0) {
            reported_bit_depth = pic->bit_depth;
        }
        hash_picture(&md5_ctx, pic);
        frames_received++;
    }

    api->decoder_close(dec);
    api->config_destroy(cfg);

    uint8_t digest[16];
    md5_detail::final(digest, &md5_ctx);
    std::string actual_md5 = md5_detail::hex(digest);

    std::cout << "Stream: " << stream_path << std::endl;
    std::cout << "Frames decoded in-memory: " << frames_received << std::endl;
    std::cout << "Reported bit depth: " << reported_bit_depth << std::endl;
    std::cout << "Actual MD5:   " << actual_md5 << std::endl;
    std::cout << "Expected MD5: " << expected_md5 << std::endl;

    if (!expected_md5.empty() && actual_md5 != expected_md5) {
        std::cerr << "FAIL: MD5 mismatch!" << std::endl;
        return 1;
    }

    if (expected_bit_depth != 0 && reported_bit_depth != expected_bit_depth) {
        std::cerr << "FAIL: expected bit depth " << expected_bit_depth << ", got "
                  << reported_bit_depth << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: In-memory push and drain verified bit-exact!" << std::endl;

    // --- Robustness Tests ---
    std::cout << "\nRunning robustness tests..." << std::endl;

    // 1. Error callback and non-existent input file
    struct ErrorCollector {
        int call_count = 0;
        int last_code = 0;
        std::string last_msg;
    } err_collector;

    auto err_cb = [](void *user_data, int code, const char *msg) {
        auto *collector = static_cast<ErrorCollector *>(user_data);
        collector->call_count++;
        collector->last_code = code;
        collector->last_msg = msg ? msg : "";
    };

    f264_config *bad_cfg = api->config_alloc();
    bad_cfg->memory_input = 0;
    std::strncpy(bad_cfg->infile, "non_existent_stream_file_987654.264", sizeof(bad_cfg->infile) - 1);
    bad_cfg->silent = 1;
    bad_cfg->error_cb = err_cb;
    bad_cfg->error_cb_user_data = &err_collector;

    f264_decoder *bad_dec = api->decoder_open(bad_cfg);
    if (bad_dec != nullptr) {
        std::cerr << "FAIL: decoder_open with non-existent file should return NULL!\n";
        api->decoder_close(bad_dec);
        api->config_destroy(bad_cfg);
        return 1;
    }
    if (err_collector.call_count == 0) {
        std::cerr << "FAIL: error_cb was not called on file open error!\n";
        api->config_destroy(bad_cfg);
        return 1;
    }
    std::cout << "Verified non-existent file handling: error_cb invoked with code " 
              << err_collector.last_code << " (\"" << err_collector.last_msg << "\")" << std::endl;
    api->config_destroy(bad_cfg);

    // 2. Verify decoder can be opened cleanly immediately after a failed open
    f264_config *recov_cfg = api->config_alloc();
    recov_cfg->memory_input = 1;
    recov_cfg->silent = 1;
    f264_decoder *recov_dec = api->decoder_open(recov_cfg);
    if (!recov_dec) {
        std::cerr << "FAIL: Failed to open decoder after previous failed open (state not cleanly reset)\n";
        api->config_destroy(recov_cfg);
        return 1;
    }
    std::cout << "Verified decoder open recovery after failed open." << std::endl;

    // 3. Corrupt bitstream pushed to in-memory decoder: must not crash or exit
    uint8_t garbage[64] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
    api->decoder_push(recov_dec, garbage, sizeof(garbage));
    f264_picture *dummy_pic = nullptr;
    int corrupt_ret = api->decoder_decode(recov_dec, &dummy_pic);
    std::cout << "Verified corrupt stream decode: returned code " << corrupt_ret << " (graceful handling, no crash or exit)" << std::endl;

    api->decoder_close(recov_dec);
    api->config_destroy(recov_cfg);

    std::cout << "ALL ROBUSTNESS TESTS PASSED!" << std::endl;
    return 0;
}
