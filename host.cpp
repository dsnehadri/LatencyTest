// host.cpp
// Minimal XRT host for passthrough

#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <thread>

#include <xrt/xrt_bo.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_graph.h>
#include <xrt/xrt_kernel.h>
#include "plio_config.h"

static uint64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << "<xclbin> [bytes] [iters]\n";
        return 2;
    }

    

    const std::string xclbin_path = argv[1];
    const size_t bytes_per_iter = (argc >= 3)? std::stoull(argv[2]) : (1u << 20);
    const int iters = (argc >= 4) ? std::stoi(argv[3]) : (10);

    if (bytes_per_iter % PLIO_BYTES != 0) {
        std::cerr << "Bytes per iteration must be a multiple of " << PLIO_BYTES << " for " << PLIO_WIDTH << "-bit kernels. Got " << bytes_per_iter << '\n';
        return 2;
    }

    const uint32_t word_count = static_cast<uint32_t>(bytes_per_iter / PLIO_BYTES);
    const size_t actual_bytes = static_cast<size_t>(word_count) * PLIO_BYTES;
    fprintf(stderr, "argc=%d bytes_per_iter=%zu word_count=%u actual_bytes=%zu\n",
        argc, bytes_per_iter, word_count, actual_bytes);
    fflush(stderr);

    try {
        // Device 0 (embedded or PCIe)

        xrt::device dev(0);
        auto uuid = dev.load_xclbin(xclbin_path);

        xrt::kernel k_mm2s(dev, uuid, "mm2s:{mm2s_0}");
        xrt::kernel k_s2mm(dev, uuid, "s2mm:{s2mm_0}");

        xrt::graph g(dev, uuid, "g");

        // allocate input/output in device DDR

        xrt::bo in_bo(dev, bytes_per_iter, k_mm2s.group_id(0));
        xrt::bo out_bo(dev, bytes_per_iter, k_s2mm.group_id(0));

        auto in_map = in_bo.map<uint8_t*>();
        auto out_map = out_bo.map<uint8_t*>();

        // initialize buffers
        // try {
            // fprintf(stderr, "resetting graph\n"); fflush(stderr);
            // g.reset();
            // fprintf(stderr, "running graph\n"); fflush(stderr);
            // g.run(-1);
            // fprintf(stderr, "graph.run() returned\n"); fflush(stderr);
        // } catch (const std::exception& e) {
        //     fprintf(stderr, "Graph error: %s\n", e.what()); fflush(stderr);
        //     return 1;
        // }
        for (size_t i = 0; i < bytes_per_iter; ++i) 
            in_map[i] = static_cast<uint8_t>(i & 0xFF);
        std::memset(out_map, 0, bytes_per_iter);

        in_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, bytes_per_iter, /*offset*/0);

        uint64_t total_kernel_ns = 0;

        for (int rep = 0; rep < iters; ++rep) {

            g.reset();
            g.run(1);

            // fprintf(stderr, "iteration %d\n", rep); fflush(stderr);

            if (rep == 0) {
                fprintf(stderr, "starting graph\n"); fflush(stderr);
            }

            xrt::run r_s2mm(k_s2mm);
            r_s2mm.set_arg(0, out_bo);
            r_s2mm.set_arg(1, nullptr);
            r_s2mm.set_arg(2, word_count);   // size

            xrt::run r_mm2s(k_mm2s);
            r_mm2s.set_arg(0, in_bo);
            r_mm2s.set_arg(1, nullptr);
            r_mm2s.set_arg(2, word_count);   // size

            r_s2mm.start();
            r_mm2s.start();

            const uint64_t t0 = now_ns();

            r_mm2s.wait();
            r_s2mm.wait();

            const uint64_t t1 = now_ns();

            total_kernel_ns += (t1 - t0);
        }

        // stop graph

        out_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, bytes_per_iter, /*offset*/0);

        // verify correctness

        size_t bad = 0;

        for (size_t i = 0; i < bytes_per_iter; ++i) {
            if (out_map[i] != in_map[i]) {
                if (bad < 16) {
                    std::cerr << "Mismatch at " << i << ": out = " << unsigned(out_map[i]) << " in = " << unsigned(in_map[i]) << "\n";
                }
                ++bad;
            }
        }

        const double avg_ns = static_cast<double>(total_kernel_ns) / iters;
        const double avg_s = avg_ns*1e-9;

        // bandwidth is end-to-end payload per iteration

        const double gbps_one_way = (static_cast<double>(actual_bytes) / avg_s) / 1e9;
        const double gbps_roundtrip = (static_cast<double>(2*actual_bytes) / avg_s) / 1e9;

        std::cout << "bytes_per_iter = " << bytes_per_iter << " iters: " << iters << "\n";        std::cout << "avg_kernel_time_s = " << std::setprecision(6) << avg_s << "\n";
        std::cout << "throughput_one_way_GB/s = " << std::setprecision(6) << gbps_one_way << "\n";
        std::cout << "throughput_roundtrip_GB/s = " << std::setprecision(6) << gbps_roundtrip << "\n";
        std::cout << "verify_bad_bytes=" << bad << "\n";

        return bad ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "XRT error: " << e.what() << "\n";
        std::cerr << "If this is a kernel/arg mismatch, dump metadata from the xclbin and verify:\n";
        std::cerr << " - CU names (IP_LAYOUT)\n";
        std::cerr << " - graph name (AIE_METADATA)\n";
        std::cerr << " - mm2s/s2mm argument order/count (embedded metadata) \n";
        return 1;
    }
}