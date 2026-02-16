#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
// #include <CL/cl.hpp>
#include <CL/cl2.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <iomanip>
#include <stdexcept>

// ====================== USER-CONFIGURABLE SECTION ======================

constexpr uint32_t MAX_PIXEL_CLUSTERS = 326400;
constexpr uint32_t MAX_STRIP_CLUSTERS = 307200;
constexpr uint16_t CLUSTER_SEG_SIZE = 256;
constexpr uint8_t  NUM_PIXEL_WORDS  = 10;
constexpr uint8_t  NUM_STRIP_WORDS  = 9;
constexpr uint8_t  NUM_PIXEL_FIELDS = 17;
constexpr uint8_t  NUM_STRIP_FIELDS = 11;
constexpr uint8_t  NUM_PIXEL_ROWS   = 22;
constexpr uint8_t  NUM_STRIP_ROWS   = 14;
constexpr uint32_t PIXEL_BLOCK_BUF_SIZE	    = (NUM_PIXEL_WORDS*MAX_PIXEL_CLUSTERS/1024+1)*1024;		// align to 1024*sizeof(uint32_t)=4096
constexpr uint32_t STRIP_BLOCK_BUF_SIZE	    = (NUM_STRIP_WORDS*MAX_STRIP_CLUSTERS/1024+1)*1024;
constexpr uint32_t PIXEL_CONTAINER_BUF_SIZE = ((NUM_PIXEL_ROWS*MAX_PIXEL_CLUSTERS+16)/1024+1)*1024;	// +16: for cluster number word
constexpr uint32_t STRIP_CONTAINER_BUF_SIZE = ((NUM_STRIP_ROWS*MAX_STRIP_CLUSTERS+16)/1024+1)*1024;
  
// Optimize these, 
  constexpr unsigned int MAX_NUM_INPUTCLUSTERS = 409600;
  constexpr unsigned int NUM_MAXINPUT_PIXEL_ROW = 3;
  constexpr unsigned int NUM_MAXINPUT_STRIP_ROW = 2;
  constexpr unsigned long PIXEL_CONTAINER_INPUT_BUF_SIZE = (NUM_MAXINPUT_PIXEL_ROW*MAX_NUM_INPUTCLUSTERS + 4096);
  constexpr unsigned long STRIP_CONTAINER_INPUT_BUF_SIZE = (NUM_MAXINPUT_STRIP_ROW*MAX_NUM_INPUTCLUSTERS + 4096);


// Kernel names – fill these with your actual names.
static const std::string PIXEL_START_KERNEL_NAME = "configurableLengthWideLoader:{pixelLoader}";
static const std::string PIXEL_END_KERNEL_NAME   = "EDMWriter:{PixelEDMWriter}";
static const std::string STRIP_START_KERNEL_NAME = "configurableLengthWideLoader:{stripLoader}";
static const std::string STRIP_END_KERNEL_NAME   = "EDMWriter:{StripEDMWriter}";

// ======================================================================

static std::vector<uint64_t> read_hex_file64(const std::string &path)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    std::vector<uint64_t> data;
    std::string line;
    std::size_t line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;

        // Trim whitespace from both ends
        auto is_space = [](unsigned char c) { return std::isspace(c); };
        while (!line.empty() && is_space(line.front())) line.erase(line.begin());
        while (!line.empty() && is_space(line.back()))  line.pop_back();

        // Skip completely empty lines
        if (line.empty()) continue;

        // Expect a pure hex string per line (like: ab00000000030000)
        // Typical width is 16 hex chars for a 64-bit word, but we allow shorter
        // and rely on stoull to handle leading zeros.
        for (char c : line) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                throw std::runtime_error(
                    "Non-hex character in file " + path +
                    " at line " + std::to_string(line_no) +
                    ": \"" + line + "\""
                );
            }
        }

        try {
            uint64_t value = std::stoull(line, nullptr, 16);
            data.push_back(value);
        } catch (const std::exception &e) {
            throw std::runtime_error(
                "Failed to parse hex word in file " + path +
                " at line " + std::to_string(line_no) +
                ": \"" + line + "\""
            );
        }
    }

    return data;
}


// Load xclbin into a binary for OpenCL
static std::vector<unsigned char> load_xclbin(const std::string &xclbin_path)
{
    std::ifstream stream(xclbin_path, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open xclbin file: " + xclbin_path);
    }

    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(size);
    if (!stream.read(reinterpret_cast<char *>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read xclbin file: " + xclbin_path);
    }
    return buffer;
}

// Pick a Xilinx device from the available OpenCL platforms/devices
static cl::Device get_xilinx_device()
{
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    for (const auto &platform : platforms) {
        std::string pname = platform.getInfo<CL_PLATFORM_NAME>();
        if (pname.find("Xilinx") == std::string::npos)
            continue;

        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices);
        if (!devices.empty()) {
            return devices[0];
        }
    }

    throw std::runtime_error("No Xilinx platform / accelerator device found.");
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <xclbin> <pixel_input_txt> <strip_input_txt>\n";
        return 1;
    }

    const std::string xclbin_path       = argv[1];
    const std::string pixel_input_path  = argv[2];
    const std::string strip_input_path  = argv[3];

    try {
        // ================== Read input files ==================
        std::vector<uint64_t> pixelInput = read_hex_file64(pixel_input_path);
        std::vector<uint64_t> stripInput = read_hex_file64(strip_input_path);


        std::cout<<"Reading done"<<std::endl;

        const cl_ulong pixelInputSize = static_cast<cl_ulong>(pixelInput.size());
        const cl_ulong stripInputSize = static_cast<cl_ulong>(stripInput.size());

        if (pixelInput.size() > PIXEL_CONTAINER_INPUT_BUF_SIZE) {
            throw std::runtime_error("Pixel input size exceeds PIXEL_CONTAINER_INPUT_BUF_SIZE");
        }
        if (stripInput.size() > STRIP_CONTAINER_INPUT_BUF_SIZE) {
            throw std::runtime_error("Strip input size exceeds STRIP_CONTAINER_INPUT_BUF_SIZE");
        }

        // ================== OpenCL / Xilinx device setup ==================
        cl::Device device = get_xilinx_device();
        cl::Context context(device);
        std::cout<<"Found device"<<std::endl;

        cl_int err = CL_SUCCESS;
        cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to create command queue, error " + std::to_string(err));
        }

        // ================== Load xclbin and build program ==================
        std::vector<unsigned char> xclbin = load_xclbin(xclbin_path);

        // On this system, cl::Program::Binaries is std::vector<std::vector<unsigned char>>.
        // Each element is the binary for one device.
        // We have exactly one device, so just push the whole xclbin vector.
        cl::Program::Binaries binaries;
        binaries.push_back(xclbin);

        cl::Program program(context, {device}, binaries, nullptr, &err);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to create program from xclbin, error " + std::to_string(err));
        }
        std::cout<<"xclbin program done"<<std::endl;


        // ================== Create kernels ==================
        cl::Kernel pixelStartKernel(program, PIXEL_START_KERNEL_NAME.c_str(), &err);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to create kernel: " + PIXEL_START_KERNEL_NAME);
        }

        cl::Kernel pixelEndKernel(program, PIXEL_END_KERNEL_NAME.c_str(), &err);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to create kernel: " + PIXEL_END_KERNEL_NAME);
        }

        cl::Kernel stripStartKernel(program, STRIP_START_KERNEL_NAME.c_str(), &err);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to create kernel: " + STRIP_START_KERNEL_NAME);
        }

        cl::Kernel stripEndKernel(program, STRIP_END_KERNEL_NAME.c_str(), &err);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to create kernel: " + STRIP_END_KERNEL_NAME);
        }
        std::cout<<"kernels done"<<std::endl;

        // ================== Create buffers ==================
        cl::Buffer pixelInputBuffer(
            context,
            CL_MEM_READ_ONLY,
            PIXEL_CONTAINER_INPUT_BUF_SIZE * sizeof(uint64_t),
            nullptr,
            &err);
        if (err != CL_SUCCESS) throw std::runtime_error("Failed to create pixel input buffer");

        cl::Buffer stripInputBuffer(
            context,
            CL_MEM_READ_ONLY,
            STRIP_CONTAINER_INPUT_BUF_SIZE * sizeof(uint64_t),
            nullptr,
            &err);
        if (err != CL_SUCCESS) throw std::runtime_error("Failed to create strip input buffer");

        cl::Buffer pixelOutputBuffer(
            context,
            CL_MEM_READ_WRITE,
            PIXEL_CONTAINER_BUF_SIZE * sizeof(uint32_t),
            nullptr,
            &err);
        if (err != CL_SUCCESS) throw std::runtime_error("Failed to create pixel output buffer");

        cl::Buffer stripOutputBuffer(
            context,
            CL_MEM_READ_WRITE,
            STRIP_CONTAINER_BUF_SIZE * sizeof(uint32_t),
            nullptr,
            &err);
        if (err != CL_SUCCESS) throw std::runtime_error("Failed to create strip output buffer");

        std::cout<<"buffers done"<<std::endl;

        // Host-side output containers (not written to disk)
        std::vector<uint32_t> pixelOutput(PIXEL_CONTAINER_BUF_SIZE, 0);
        std::vector<uint32_t> stripOutput(STRIP_CONTAINER_BUF_SIZE, 0);

        // ================== Set kernel arguments ==================
        // As per your note: only these args are needed, others correspond to internal streaming.
        // pixelStartCluster(args): arg0 = input buffer, arg2 = input size
        err  = pixelStartKernel.setArg(0, pixelInputBuffer);
        err |= pixelStartKernel.setArg(2, pixelInputSize);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to set args for pixelStartKernel");
        }

        // pixelEndCluster(args): arg2 = pixel output buffer
        err  = pixelEndKernel.setArg(2, pixelOutputBuffer);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to set args for pixelEndKernel");
        }

        // stripStartCluster(args): arg0 = input buffer, arg2 = input size
        err  = stripStartKernel.setArg(0, stripInputBuffer);
        err |= stripStartKernel.setArg(2, stripInputSize);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to set args for stripStartKernel");
        }

        // stripEndCluster(args): arg2 = strip output buffer
        err  = stripEndKernel.setArg(2, stripOutputBuffer);
        if (err != CL_SUCCESS) {
            throw std::runtime_error("Failed to set args for stripEndKernel");
        }
        std::cout<<"args done"<<std::endl;

        // ================== Write input buffers ==================
        cl::Event evt_write_pixel_input;
        cl::Event evt_write_strip_input;

        err = queue.enqueueWriteBuffer(
            pixelInputBuffer,
            CL_FALSE,                // non-blocking
            0,
            pixelInput.size() * sizeof(uint64_t),
            pixelInput.data(),
            nullptr,
            &evt_write_pixel_input);
        if (err != CL_SUCCESS) throw std::runtime_error("enqueueWriteBuffer pixelInput failed");

        err = queue.enqueueWriteBuffer(
            stripInputBuffer,
            CL_FALSE,
            0,
            stripInput.size() * sizeof(uint64_t),
            stripInput.data(),
            nullptr,
            &evt_write_strip_input);
        if (err != CL_SUCCESS) throw std::runtime_error("enqueueWriteBuffer stripInput failed");

        std::vector<cl::Event> wait_pixel_input{evt_write_pixel_input};
        std::vector<cl::Event> wait_strip_input{evt_write_strip_input};

        // ================== Enqueue kernels ==================
        cl::Event evt_pixel_start;
        cl::Event evt_pixel_end;
        cl::Event evt_strip_start;
        cl::Event evt_strip_end;

        // Pixel start depends on pixel input write
        err = queue.enqueueTask(pixelStartKernel, &wait_pixel_input, &evt_pixel_start);
        if (err != CL_SUCCESS) throw std::runtime_error("enqueueTask pixelStartKernel failed");

        // Pixel end depends on pixel start
        {
            err = queue.enqueueTask(pixelEndKernel, nullptr, &evt_pixel_end);
            if (err != CL_SUCCESS) throw std::runtime_error("enqueueTask pixelEndKernel failed");
        }

        // Strip start depends on strip input write
        err = queue.enqueueTask(stripStartKernel, &wait_strip_input, &evt_strip_start);
        if (err != CL_SUCCESS) throw std::runtime_error("enqueueTask stripStartKernel failed");

        // Strip end depends on strip start
        {
            err = queue.enqueueTask(stripEndKernel, nullptr, &evt_strip_end);
            if (err != CL_SUCCESS) throw std::runtime_error("enqueueTask stripEndKernel failed");
        }

        // ================== Read output buffers ==================
        cl::Event evt_pixel_output_read;
        cl::Event evt_strip_output_read;

        {
            std::vector<cl::Event> wait_pixel_done{evt_pixel_end};
            err = queue.enqueueReadBuffer(
                pixelOutputBuffer,
                CL_FALSE,
                0,
                pixelOutput.size() * sizeof(uint32_t),
                pixelOutput.data(),
                &wait_pixel_done,
                &evt_pixel_output_read);
            if (err != CL_SUCCESS) throw std::runtime_error("enqueueReadBuffer pixelOutput failed");
        }

        {
            std::vector<cl::Event> wait_strip_done{evt_strip_end};
            err = queue.enqueueReadBuffer(
                stripOutputBuffer,
                CL_FALSE,
                0,
                stripOutput.size() * sizeof(uint32_t),
                stripOutput.data(),
                &wait_strip_done,
                &evt_strip_output_read);
            if (err != CL_SUCCESS) throw std::runtime_error("enqueueReadBuffer stripOutput failed");
        }
        std::cout<<"waiting"<<std::endl;

        // Ensure all operations are done
        queue.finish();

        // ================== Timing summary (for 1 event) ==================
        const double inv_ms = 1.0e-6;

        // command profiling in ns
        auto pixel_input_time  = evt_write_pixel_input.getProfilingInfo<CL_PROFILING_COMMAND_END>()
                               - evt_write_pixel_input.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        auto strip_input_time  = evt_write_strip_input.getProfilingInfo<CL_PROFILING_COMMAND_END>()
                               - evt_write_strip_input.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        auto pixel_pipeline_time = evt_pixel_end.getProfilingInfo<CL_PROFILING_COMMAND_END>()
                                 - evt_pixel_start.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        auto strip_pipeline_time = evt_strip_end.getProfilingInfo<CL_PROFILING_COMMAND_END>()
                                 - evt_strip_start.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        auto pixel_output_time = evt_pixel_output_read.getProfilingInfo<CL_PROFILING_COMMAND_END>()
                               - evt_pixel_output_read.getProfilingInfo<CL_PROFILING_COMMAND_START>();
        auto strip_output_time = evt_strip_output_read.getProfilingInfo<CL_PROFILING_COMMAND_END>()
                               - evt_strip_output_read.getProfilingInfo<CL_PROFILING_COMMAND_START>();

        std::cout << "Finalizing F110StreamIntegration standalone host\n";
        std::cout << "Number of events: 1\n";

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Pixel input ave time:  " << pixel_input_time  * inv_ms << " ms\n";
        std::cout << "Strip input ave time:  " << strip_input_time  * inv_ms << " ms\n";
        std::cout << "Pixel pipeline ave time: " << pixel_pipeline_time * inv_ms << " ms\n";
        std::cout << "Strip pipeline ave time: " << strip_pipeline_time * inv_ms << " ms\n";
        std::cout << "Pixel output ave time: " << pixel_output_time * inv_ms << " ms\n";
        std::cout << "Strip output ave time: " << strip_output_time * inv_ms << " ms\n";

        // pixelOutput and stripOutput remain in CPU memory here; you can inspect them in a debugger
        // or later extend this code to dump them to files.

    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

