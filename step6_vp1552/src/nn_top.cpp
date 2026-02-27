/*
 * nn_top.cpp — Vitis DDR wrapper for the hls4ml inference kernel.
 *
 * Model : 16-feature jet classifier → 5-class softmax (g q t w z)
 * in_buf  [n_samples × 16]  float32, m_axi DDR
 * out_buf [n_samples ×  5]  float32, m_axi DDR
 *
 * #include "myproject.cpp" (not .h) pulls the full hls4ml implementation into
 * this single translation unit. This prevents the multiply-defined symbol error
 * that occurs when myproject.cpp and nn_top.cpp are compiled separately —
 * both include parameters.h which defines weight arrays without static linkage.
 */
#include "myproject.cpp"

#define IN_FEATURES 16
#define N_CLASSES    5

extern "C" {

void nn_top(const float* in_buf, float* out_buf, int n_samples) {
    #pragma HLS INTERFACE m_axi port=in_buf  bundle=gmem0
    #pragma HLS INTERFACE m_axi port=out_buf bundle=gmem1
    #pragma HLS INTERFACE s_axilite port=n_samples
    #pragma HLS INTERFACE s_axilite port=return

    for (int s = 0; s < n_samples; s++) {
        #pragma HLS PIPELINE off
        input_t  x[IN_FEATURES];
        result_t y[N_CLASSES];

        for (int i = 0; i < IN_FEATURES; i++) {
            #pragma HLS UNROLL
            x[i] = (input_t)in_buf[s * IN_FEATURES + i];
        }
        myproject(x, y);
        for (int c = 0; c < N_CLASSES; c++) {
            #pragma HLS UNROLL
            out_buf[s * N_CLASSES + c] = (float)y[c];
        }
    }
}

} // extern "C"
