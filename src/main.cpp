#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>
#include <cstdlib>

#include "h264decoder.h"
#include "profiling.h"
#include "win32.h"

static void print_usage(const char *prog)
{
    std::cout << "f264dec - Fast Standalone H.264/AVC Decoder\n"
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

    InputParameters inp{};
    std::strncpy(inp.infile, infile.c_str(), sizeof(inp.infile) - 1);
    if (!outfile.empty()) {
        std::strncpy(inp.outfile, outfile.c_str(), sizeof(inp.outfile) - 1);
    }
    if (!reffile.empty()) {
        std::strncpy(inp.reffile, reffile.c_str(), sizeof(inp.reffile) - 1);
    }
    inp.FileFormat = PAR_OF_ANNEXB;
    inp.iDecFrmNum = max_frames;
    inp.silent = silent ? 1 : 0;
    inp.threads = threads;
    inp.cpuid = cpuid;

    init_time();

    if (!silent) {
        std::cout << "f264dec H.264/AVC Decoder\n"
                  << "Input bitstream: " << inp.infile << "\n";
        if (!outfile.empty()) {
            std::cout << "Output YUV:      " << inp.outfile << "\n";
        }
        if (!reffile.empty()) {
            std::cout << "Reference YUV:   " << inp.reffile << "\n";
        }
    }

    int ret = OpenDecoder(&inp);
    if (ret != DEC_OPEN_NOERR) {
        std::cerr << "Error opening decoder (code 0x" << std::hex << ret << ")\n";
        return 1;
    }

    int frames_decoded = 0;
    DecodedPicList *pic_list = nullptr;

    auto t_start = std::chrono::steady_clock::now();

    while (true) {
        ret = DecodeOneFrame(&pic_list);
        if (ret == DEC_EOS) {
            break;
        }
        if (ret != DEC_SUCCEED) {
            std::cerr << "Decoding error at frame " << frames_decoded << " (code 0x" << std::hex << ret << ")\n";
            break;
        }
        frames_decoded++;
        if (max_frames > 0 && frames_decoded >= max_frames) {
            break;
        }
    }

    FinitDecoder(&pic_list);
    CloseDecoder();

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
