/*
 * run_kernel.cpp
 * Usage: run_kernel <xclbin> <kernel_name> [n]
 *
 * Works with vadd (c = a+b) and vmult (c = a*b).
 * Both kernels share the same ABI: (int* a, int* b, int* c, int n)
 *
 * Examples:
 *   ./run_kernel vadd.xclbin  vadd  1024
 *   ./run_kernel vmult.xclbin vmult 1024
 */

#define CL_HPP_CL_1_2_DEFAULT_BUILD
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1

#include <CL/cl2.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(err, call)                                                     \
    do {                                                                     \
        call;                                                                \
        if ((err) != CL_SUCCESS) {                                           \
            fprintf(stderr, "%s:%d  " #call " failed, err=%d\n",            \
                    __FILE__, __LINE__, (err));                              \
            exit(EXIT_FAILURE);                                              \
        }                                                                    \
    } while (0)

static std::vector<char> read_file(const std::string &path) {
    std::ifstream f(path, std::ifstream::binary);
    if (!f) {
        fprintf(stderr, "ERROR: cannot open '%s'\n", path.c_str());
        exit(EXIT_FAILURE);
    }
    f.seekg(0, f.end);
    size_t n = f.tellg();
    f.seekg(0, f.beg);
    std::vector<char> buf(n);
    f.read(buf.data(), n);
    return buf;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <xclbin> <kernel_name> [n]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const std::string xclbin_path = argv[1];
    const std::string kernel_name = argv[2];
    const int n = (argc >= 4) ? atoi(argv[3]) : 1024;

    if (kernel_name != "vadd" && kernel_name != "vmult") {
        fprintf(stderr, "ERROR: kernel_name must be 'vadd' or 'vmult'\n");
        return EXIT_FAILURE;
    }

    /* ---- find Xilinx platform and device ---- */
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    cl::Platform xilinx_platform;
    bool found = false;
    for (auto &p : platforms) {
        if (p.getInfo<CL_PLATFORM_NAME>() == "Xilinx") {
            xilinx_platform = p;
            found = true;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "ERROR: Xilinx OpenCL platform not found\n");
        return EXIT_FAILURE;
    }

    std::vector<cl::Device> devices;
    xilinx_platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices);
    if (devices.empty()) {
        fprintf(stderr, "ERROR: no accelerator devices found\n");
        return EXIT_FAILURE;
    }
    cl::Device device = devices[0];
    printf("Device: %s\n", device.getInfo<CL_DEVICE_NAME>().c_str());

    /* ---- load xclbin ---- */
    printf("Loading: %s\n", xclbin_path.c_str());
    auto bin = read_file(xclbin_path);

    cl_int err;
    cl::Context context(device, nullptr, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) { fprintf(stderr, "Context failed\n"); return EXIT_FAILURE; }

    cl::Program::Binaries bins;
    bins.push_back({bin.data(), bin.size()});
    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) { fprintf(stderr, "Program failed, err=%d\n", err); return EXIT_FAILURE; }

    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
    if (err != CL_SUCCESS) { fprintf(stderr, "Queue failed\n"); return EXIT_FAILURE; }

    cl::Kernel kernel(program, kernel_name.c_str(), &err);
    if (err != CL_SUCCESS) { fprintf(stderr, "Kernel '%s' failed, err=%d\n", kernel_name.c_str(), err); return EXIT_FAILURE; }

    /* ---- allocate buffers ---- */
    size_t bytes = n * sizeof(int);
    cl::Buffer buf_a(context, CL_MEM_READ_ONLY,  bytes, nullptr, &err);
    cl::Buffer buf_b(context, CL_MEM_READ_ONLY,  bytes, nullptr, &err);
    cl::Buffer buf_c(context, CL_MEM_WRITE_ONLY, bytes, nullptr, &err);

    /* ---- map and fill inputs ---- */
    int *ptr_a = (int *)q.enqueueMapBuffer(buf_a, CL_TRUE, CL_MAP_WRITE, 0, bytes, nullptr, nullptr, &err);
    int *ptr_b = (int *)q.enqueueMapBuffer(buf_b, CL_TRUE, CL_MAP_WRITE, 0, bytes, nullptr, nullptr, &err);
    int *ptr_c = (int *)q.enqueueMapBuffer(buf_c, CL_TRUE, CL_MAP_READ,  0, bytes, nullptr, nullptr, &err);

    for (int i = 0; i < n; i++) {
        ptr_a[i] = i;   /* a = [0, 1, 2, ...] */
        ptr_b[i] = 3;   /* b = [3, 3, 3, ...] */
    }

    /* ---- run kernel ---- */
    kernel.setArg(0, buf_a);
    kernel.setArg(1, buf_b);
    kernel.setArg(2, buf_c);
    kernel.setArg(3, n);

    q.enqueueMigrateMemObjects({buf_a, buf_b}, 0 /* host -> device */);
    q.enqueueTask(kernel);
    q.enqueueMigrateMemObjects({buf_c}, CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();

    /* ---- verify ---- */
    int errors = 0;
    for (int i = 0; i < n; i++) {
        int expected = (kernel_name == "vadd") ? (ptr_a[i] + ptr_b[i])
                                               : (ptr_a[i] * ptr_b[i]);
        if (ptr_c[i] != expected) {
            fprintf(stderr, "MISMATCH at i=%d: got %d expected %d\n",
                    i, ptr_c[i], expected);
            if (++errors >= 10) break;
        }
    }

    printf("[%s] %s  n=%d  c[0:4]=[%d, %d, %d, %d]\n",
           errors ? "FAIL" : "PASS",
           kernel_name.c_str(), n,
           ptr_c[0], ptr_c[1], ptr_c[2], ptr_c[3]);

    q.enqueueUnmapMemObject(buf_a, ptr_a);
    q.enqueueUnmapMemObject(buf_b, ptr_b);
    q.enqueueUnmapMemObject(buf_c, ptr_c);
    q.finish();

    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}
