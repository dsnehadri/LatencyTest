#include <adf.h>
using namespace adf;

void passthrough(input_stream<int32>* in, output_stream<int32>* out) {
    for (int i = 0; i < (1<<18); ++i) {
        writeincr(out, readincr(in));
    }
}