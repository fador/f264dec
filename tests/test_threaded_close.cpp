// Closes the decoder while the frame pipeline still has decode jobs in
// flight, the way a player resets the decoder to seek. The jobs must be
// waited for before their slices, buffers and slice-group maps are freed;
// otherwise the workers crash or corrupt the heap.
#include "f264dec.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    std::string stream_path = "tests/streams/high_hier_b.264";
    if (argc >= 2) {
        stream_path = argv[1];
    }
    std::ifstream file(stream_path, std::ios::binary);
    if (!file) {
        std::fprintf(stderr, "cannot open %s\n", stream_path.c_str());
        return 1;
    }
    const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());

    const int kIterations = 40;
    for (int iteration = 0; iteration < kIterations; ++iteration) {
        const f264_api *api = f264_api_get(8);
        f264_config *cfg = api->config_alloc();
        cfg->memory_input = 1;
        cfg->silent = 1;
        cfg->threads = 2;  // enable the frame pipeline
        f264_decoder *dec = api->decoder_open(cfg);
        if (!dec) {
            std::fprintf(stderr, "decoder_open failed on iteration %d\n", iteration);
            api->config_destroy(cfg);
            return 1;
        }
        api->decoder_push(dec, data.data(), data.size());
        // Decode a few access units without draining the pictures, then close
        // while the workers are still running.
        for (int i = 0; i < 12; ++i) {
            const int result = api->decoder_decode(dec, nullptr);
            if (result == F264_EOS) {
                break;
            }
        }
        api->decoder_close(dec);
        api->config_destroy(cfg);
    }

    std::printf("threaded close: %d iterations ok\n", kIterations);
    return 0;
}
