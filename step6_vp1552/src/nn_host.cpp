/*
 * nn_host.cpp — XRT host for the hls4ml nn_top kernel on felix-155.
 *
 * Cross-compiled for aarch64, packed into the SD card image.
 * Usage:  /boot/nn_host /boot/nn_top.xclbin [n_samples]
 *
 * 8 normalised jet-tagging samples are embedded as a C array.
 * Classes: g=gluon  q=quark  t=top  w=W-boson  z=Z-boson
 */
#define CL_HPP_CL_1_2_DEFAULT_BUILD
#define CL_HPP_TARGET_OPENCL_VERSION  120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1

#include <CL/cl2.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define IN_FEATURES 16
#define N_CLASSES    5
#define N_DEMO       8

static const float SAMPLES[N_DEMO][IN_FEATURES] = {
    {-0.120f,  0.406f, -1.041f, -0.825f, -0.755f, -0.581f,  1.987f,  1.538f,  1.987f,  0.631f,  0.384f, -0.201f,  1.060f,  0.409f, -1.020f, -0.180f},
    { 0.309f,  0.227f, -1.156f, -0.846f, -1.035f, -0.620f,  0.183f,  0.596f,  0.183f,  1.702f,  2.041f,  2.423f,  1.524f,  1.867f, -1.232f, -1.195f},
    {-1.266f,  0.668f,  1.409f,  1.401f,  1.578f,  1.293f,  0.124f,  0.027f,  0.124f,  0.329f,  0.521f,  0.733f,  0.552f,  0.582f,  1.309f,  1.757f},
    {-1.065f,  0.682f,  2.174f,  2.234f,  2.051f,  2.352f, -0.133f,  0.228f, -0.133f,  0.907f,  1.111f,  1.613f,  0.912f,  1.396f,  1.813f,  0.973f},
    {-1.521f,  0.877f,  2.424f,  2.769f,  1.995f,  1.781f, -0.358f, -0.305f, -0.358f,  0.524f,  1.055f,  1.124f,  0.423f,  0.368f,  1.758f,  1.527f},
    {-0.255f,  0.353f,  0.416f, -0.027f,  0.582f,  0.172f,  0.152f,  0.234f,  0.152f,  0.778f,  0.678f,  0.886f,  0.497f,  0.466f, -0.104f,  0.558f},
    { 0.489f, -0.565f, -0.417f, -0.474f,  0.158f, -0.196f,  1.579f,  0.535f,  1.579f, -0.388f, -0.657f, -0.567f,  0.343f,  0.143f, -0.278f,  0.881f},
    {-0.991f,  0.699f, -0.331f, -0.559f,  0.117f, -0.121f,  1.065f,  1.585f,  1.065f,  1.143f,  0.756f,  0.570f,  1.127f,  1.112f, -0.218f,  0.881f},
};
static const int         GT[N_DEMO]          = {1, 1, 2, 2, 2, 2, 1, 0};
static const char* const CLASS_NAMES[N_CLASSES] = {"g", "q", "t", "w", "z"};

static std::vector<char> read_file(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) { fprintf(stderr, "ERROR: cannot open %s\n", p.c_str()); exit(1); }
    f.seekg(0, f.end); size_t n = f.tellg(); f.seekg(0, f.beg);
    std::vector<char> buf(n); f.read(buf.data(), n); return buf;
}

int main(int argc, char* argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <nn_top.xclbin> [n]\n", argv[0]); return 1; }
    const std::string xclbin = argv[1];
    const int n = (argc >= 3) ? atoi(argv[2]) : N_DEMO;

    std::vector<cl::Platform> plts; cl::Platform::get(&plts);
    cl::Platform xplt;
    for (auto& p : plts) if (p.getInfo<CL_PLATFORM_NAME>() == "Xilinx") { xplt = p; break; }
    std::vector<cl::Device> devs; xplt.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devs);
    if (devs.empty()) { fprintf(stderr, "ERROR: no accelerator\n"); return 1; }
    cl::Device dev = devs[0];
    printf("Device : %s\n", dev.getInfo<CL_DEVICE_NAME>().c_str());

    auto bin = read_file(xclbin);
    cl_int err;
    cl::Context      ctx(dev, nullptr, nullptr, nullptr, &err);
    cl::Program::Binaries b; b.push_back({bin.data(), bin.size()});
    cl::Program      prog(ctx, {dev}, b, nullptr, &err);
    cl::CommandQueue q(ctx, dev, CL_QUEUE_PROFILING_ENABLE, &err);
    cl::Kernel       krnl(prog, "nn_top", &err);
    if (err) { fprintf(stderr, "Kernel load failed %d\n", err); return 1; }

    size_t in_sz  = (size_t)n * IN_FEATURES * sizeof(float);
    size_t out_sz = (size_t)n * N_CLASSES   * sizeof(float);
    cl::Buffer buf_in (ctx, CL_MEM_READ_ONLY  | CL_MEM_ALLOC_HOST_PTR, in_sz,  nullptr, &err);
    cl::Buffer buf_out(ctx, CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR, out_sz, nullptr, &err);

    float* in_ptr = (float*)q.enqueueMapBuffer(buf_in, CL_TRUE, CL_MAP_WRITE, 0, in_sz, nullptr, nullptr, &err);
    for (int s = 0; s < n; s++)
        memcpy(in_ptr + s * IN_FEATURES, SAMPLES[s % N_DEMO], IN_FEATURES * sizeof(float));
    q.enqueueUnmapMemObject(buf_in, in_ptr);

    krnl.setArg(0, buf_in); krnl.setArg(1, buf_out); krnl.setArg(2, n);
    cl::Event ev;
    q.enqueueMigrateMemObjects({buf_in},  0);
    q.enqueueTask(krnl, nullptr, &ev);
    q.enqueueMigrateMemObjects({buf_out}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();

    cl_ulong ts, te;
    ev.getProfilingInfo(CL_PROFILING_COMMAND_START, &ts);
    ev.getProfilingInfo(CL_PROFILING_COMMAND_END,   &te);
    double ms = (te - ts) * 1e-6;

    float* out_ptr = (float*)q.enqueueMapBuffer(buf_out, CL_TRUE, CL_MAP_READ, 0, out_sz, nullptr, nullptr, &err);
    int ok = 0;
    printf("\n%-6s  %-4s  %-4s  Scores\n", "Sample", "Pred", "GT");
    printf("%s\n", std::string(50, '-').c_str());
    for (int s = 0; s < n; s++) {
        const float* sc = out_ptr + s * N_CLASSES;
        int pred = 0;
        for (int c = 1; c < N_CLASSES; c++) if (sc[c] > sc[pred]) pred = c;
        int gt = GT[s % N_DEMO];
        ok += (pred == gt);
        printf("%-6d  %-4s  %-4s  ", s, CLASS_NAMES[pred], CLASS_NAMES[gt]);
        for (int c = 0; c < N_CLASSES; c++) printf("%.3f ", sc[c]);
        printf("%s\n", pred == gt ? "[OK]" : "[X]");
    }
    printf("%s\n", std::string(50, '-').c_str());
    printf("Accuracy : %d/%d = %.0f%%    Kernel : %.2f ms (%.3f ms/sample)\n",
           ok, n, 100.0*ok/n, ms, ms/n);

    q.enqueueUnmapMemObject(buf_out, out_ptr);
    q.finish();
    return 0;
}
