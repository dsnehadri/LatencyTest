#include <adf.h>
#include "../plio_config.h"
using namespace adf;

void passthrough(input_stream<int32>* in, output_stream<int32>* out) {
    for (int i = 0; i < (1<<18) / (PLIO_WIDTH / 32); ++i) {
#if PLIO_WIDTH == 128
        writeincr(out, readincr_v4<int32>(in));
#elif PLIO_WIDTH == 64
        writeincr(out, readincr_v2<int32>(in));
#else
        writeincr(out, readincr(in));
#endif
    }
}