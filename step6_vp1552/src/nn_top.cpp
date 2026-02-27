/*
 * nn_top.cpp — Vitis DDR wrapper around the hls4ml inference kernel.
 *
 * IMPORTANT: only include myproject.h here — NOT parameters.h.
 * parameters.h defines weight arrays (w2, b2, ...) as non-static globals.
 * myproject.cpp also includes parameters.h. If both translation units are
 * passed to v++, LLVM sees multiply-defined symbols and fails to link.
 * myproject.h gives us the prototype + defines.h (input_t, result_t) — 
 * that is all this wrapper needs.
 *
 * Interface:
 *   in_buf  [n_samples * 16]  float32, m_axi, DDR
 *   out_buf [n_samples * 5] float32, m_axi, DDR
 *   n_samples                       s_axilite scalar
 */
#include "myproject.h"   // gives defines.h (input_t, result_t) + prototype

#define IN_FEATURES  16
#define N_CLASSES    5

extern "C" {

void nn_top(
    const float* in_buf,
    float*       out_buf,
    int          n_samples
) {
    #pragma HLS INTERFACE m_axi port=in_buf  bundle=gmem0 depth=IN_FEATURES
    #pragma HLS INTERFACE m_axi port=out_buf bundle=gmem1 depth=N_CLASSES
    #pragma HLS INTERFACE s_axilite port=n_samples
    #pragma HLS INTERFACE s_axilite port=return

    for (int s = 0; s < n_samples; s++) {
        #pragma HLS PIPELINE off

        input_t  in_sample[IN_FEATURES];
        result_t out_sample[N_CLASSES];

        for (int i = 0; i < IN_FEATURES; i++) {
            #pragma HLS UNROLL
            in_sample[i] = (input_t)in_buf[s * IN_FEATURES + i];
        }

        myproject(in_sample, out_sample);

        for (int c = 0; c < N_CLASSES; c++) {
            #pragma HLS UNROLL
            out_buf[s * N_CLASSES + c] = (float)out_sample[c];
        }
    }
}

} // extern "C"
