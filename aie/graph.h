#include <adf.h>
#include "kernels.h"
using namespace adf;

class bench_graph : public graph {
    public:
    kernel k;
    input_plio in0;
    output_plio out0;

    bench_graph()
    : in0 (input_plio::create("DataIn1", plio_128_bits, "data/input.txt"))
    , out0 (output_plio::create("DataOut1", plio_128_bits, "data/output.txt"))
    {
        k = kernel::create(passthrough);
        source(k) = "aie/passthrough.cc";
        connect<>(in0.out[0], k.in[0]);
        connect<>(k.out[0], out0.in[0]);
        runtime<ratio>(k) = 0.9;
    }
};