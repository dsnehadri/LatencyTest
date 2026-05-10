#include <adf.h>
#include "kernels.h"
#include "../plio_config.h"
using namespace adf;

#if PLIO_WIDTH == 128
  #define PLIO_BITS plio_128_bits
#elif PLIO_WIDTH == 64
  #define PLIO_BITS plio_64_bits
#else
  #define PLIO_BITS plio_32_bits
#endif

class bench_graph : public graph {
    public:
    kernel k;
    input_plio in0;
    output_plio out0;

    bench_graph()
    : in0 (input_plio::create("DataIn1", PLIO_BITS, "data/input.txt"))
    , out0 (output_plio::create("DataOut1", PLIO_BITS, "data/output.txt"))
    {
        k = kernel::create(passthrough);
        source(k) = "aie/passthrough.cc";
        connect<>(in0.out[0], k.in[0]);
        connect<>(k.out[0], out0.in[0]);
        runtime<ratio>(k) = 0.9;
    }
};