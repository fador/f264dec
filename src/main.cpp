#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>
#include <cstdlib>

#include "f264dec.h"
#include "profiling.h"
#include "win32.h"

static void print_usage(const char *prog)
{
    std::cout << "f264dec - Fast Standalone H.264/AVC Decoder (v" << f264_get_version_string() << ")\n"
              << "Usage: " << prog << " [options] <input.264> [output.yuv]\n\n"
              << "Options:\n"
              << "  -i, --input <file>    Input H.264 Annex B bitstream file\n"
              << "  -o, --output <file>   Output decoded YUV file (optional)\n"
              << "  -r, --ref <file>      Reference YUV file for PSNR calculation\n"
              << "  -n, --frames <N>      Maximum number of frames to decode (default: all)\n"
              << "  -t, --threads <N>     Number of worker threads (default: 0 = auto)\n"
              << "  --cpuid <0|1>         Enable/disable CPUID SIMD optimizations (default: 1)\n"
              << "  -s, --silent          Suppress frame-by-frame console output\n"
              << "  -h, --help            Show this help message\n";
}

int main(int argc, char **argv)
{
    std::string infile;
    std::string outfile;
    std::string reffile;
    int max_frames = 0;
    int threads = 0;
    int cpuid = 1;
    bool silent = false;

    std::vector<std::string> pos_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            infile = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            outfile = argv[++i];
        } else if ((arg == "-r" || arg == "--ref") && i + 1 < argc) {
            reffile = argv[++i];
        } else if ((arg == "-n" || arg == "--frames") && i + 1 < argc) {
            max_frames = std::atoi(argv[++i]);
        } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            threads = std::atoi(argv[++i]);
        } else if (arg == "--cpuid" && i + 1 < argc) {
            cpuid = std::atoi(argv[++i]);
        } else if (arg == "-s" || arg == "--silent") {
            silent = true;
        } else if (!arg.empty() && arg[0] != '-') {
            pos_args.push_back(arg);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (infile.empty() && !pos_args.empty()) {
        infile = pos_args[0];
        if (outfile.empty() && pos_args.size() > 1) {
            outfile = pos_args[1];
        }
    }

    if (infile.empty()) {
        std::cerr << "Error: No input bitstream specified.\n\n";
        print_usage(argv[0]);
        return 1;
    }

    const f264_api *api = f264_api_get(8);
    f264_config *cfg = api->config_alloc();
    if (!cfg) {
        std::cerr << "Error: Failed to allocate decoder configuration.\n";
        return 1;
    }

    std::strncpy(cfg->infile, infile.c_str(), sizeof(cfg->infile) - 1);
    if (!outfile.empty()) {
        std::strncpy(cfg->outfile, outfile.c_str(), sizeof(cfg->outfile) - 1);
    }
    if (!reffile.empty()) {
        std::strncpy(cfg->reffile, reffile.c_str(), sizeof(cfg->reffile) - 1);
    }
    cfg->max_frames = max_frames;
    cfg->silent = silent ? 1 : 0;
    cfg->threads = threads;
    cfg->cpuid = cpuid;

    init_time();

    if (!silent) {
        std::cout << "f264dec H.264/AVC Decoder (v" << f264_get_version_string() << ")\n"
                  << "Input bitstream: " << cfg->infile << "\n";
        if (!outfile.empty()) {
            std::cout << "Output YUV:      " << cfg->outfile << "\n";
        }
        if (!reffile.empty()) {
            std::cout << "Reference YUV:   " << cfg->reffile << "\n";
        }
    }

    f264_decoder *dec = api->decoder_open(cfg);
    if (!dec) {
        std::cerr << "Error opening f264dec decoder\n";
        api->config_destroy(cfg);
        return 1;
    }

    int frames_decoded = 0;
    f264_picture *pic = nullptr;

    auto t_start = std::chrono::steady_clock::now();

    while (true) {
        int ret = api->decoder_decode(dec, &pic);
        if (ret == F264_EOS) {
            break;
        }
        if (ret != F264_OK) {
            std::cerr << "Decoding error at frame " << frames_decoded << " (code " << ret << ")\n";
            break;
        }
        frames_decoded++;
        if (max_frames > 0 && frames_decoded >= max_frames) {
            break;
        }
    }

    api->decoder_flush(dec, &pic);
    api->decoder_close(dec);
    api->config_destroy(cfg);

    auto t_end = std::chrono::steady_clock::now();
    double elapsed_sec = std::chrono::duration<double>(t_end - t_start).count();
    double fps = elapsed_sec > 0.0 ? (double)frames_decoded / elapsed_sec : 0.0;

    if (!silent) {
        std::cout << "\nDecoded " << frames_decoded << " frames in "
                  << elapsed_sec << " s (" << fps << " fps)\n";
    }

    f264_profile_report();

    return 0;
}
