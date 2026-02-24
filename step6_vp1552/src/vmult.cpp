extern "C" {

void vmult(const int* a, const int* b, int* c, int n) {
    #pragma HLS INTERFACE m_axi port=a bundle=gmem0
    #pragma HLS INTERFACE m_axi port=b bundle=gmem1
    #pragma HLS INTERFACE m_axi port=c bundle=gmem2
    #pragma HLS INTERFACE s_axilite port=n
    #pragma HLS INTERFACE s_axilite port=return

    for (int i = 0; i < n; i++) {
        #pragma HLS PIPELINE II=1
        c[i] = a[i] * b[i];
    }
}

}
