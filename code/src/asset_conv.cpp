#include "stb/stb_image_write.h"

#include "nanosvg/nanosvg.h"
#include "nanosvg/nanosvgrast.h"

#include <memory>
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <atomic>
#include <string>
#include <cstring>
#include <thread>

namespace gif643 {

const size_t    BPP         = 4;    // Bytes per pixel
const float     ORG_WIDTH   = 48.0; // Original SVG image width in px.
int             NUM_THREADS = 48;    // Default value, changed by argv. 

using PNGDataVec = std::vector<char>;
using PNGDataPtr = std::shared_ptr<PNGDataVec>;

std::mutex mut;
std::mutex mut2;
std::condition_variable data_cond;


/// \brief Wraps callbacks from stbi_image_write
//
// Provides a static method to give to stbi_write_png_to_func (rawCallback),
// which will call with a void* pointer to the instance of PNGWriter.
// The actual processing should occur in callback(...).
//
// Usage (see stbi_write_png for w,h, BPP, image_data and stride parameters): 
//
//   PNGWriter writer;
//   writer(w, h, BPP, image_data, stride); // Will ultimately 
//                                          // call callback(data, len) and
//                                          // return when it's done.
//                                          // Throws if an error occured.
//
class PNGWriter
{
private:
    PNGDataPtr png_data_;

public:
    static void rawCallback(void* context, void* data, int len)
    {
        PNGWriter* writer = (PNGWriter*)context;
        writer->callback(data, len);
    }

    /// \brief Copies the PNG data in png_data_ when compression is done and
    ///        ready to write.
    ///
    /// Meant to be called by stbi_image_write_to_func as callback.
    /// NOTE: The given data array is deleted by the caller once callback is 
    /// finished.
    /// \param data Raw pointer to uint8 array.
    /// \param len  size of the data array.
    void callback(void* data, size_t len)
    {
        char* data_raw = static_cast<char *>(data);
        png_data_ = PNGDataPtr(new PNGDataVec(&data_raw[0], &data_raw[len]));
    }

    void operator()(size_t width,
                    size_t height,
                    size_t BPP,
                    const unsigned char* image_data,
                    size_t stride)
    {
            stbi_write_func* fun = PNGWriter::rawCallback;
            int r = stbi_write_png_to_func(fun, 
                                           this,
                                           width,
                                           height,
                                           BPP,
                                           &image_data[0],
                                           stride);

            if (r == 0) {
                throw std::runtime_error("Error in write_png_to_func");
            }
    }

    /// \brief Return a shared pointer to the compressed PNG data.
    PNGDataPtr getData()
    {
        return png_data_;
    }
};

/// \brief Task definition
///
/// fname_in:  The file to process (SVG format)
/// fname_out: Where to write the result in PNG.
/// size:      The size, in pixel, of the produced image.
///
/// NOTE: Assumes the input SVG is ORG_WIDTH wide (48px) and the result will be
/// square. Does not matter if it does not fit in the resulting image, it will //// simply be cropped.


/* struct TaskDef
{
    std::atomic<std::string> fname_in;
    std::atomic<std::string> fname_out; 
    std::atomic<int> size;
};
  */


struct TaskDef
{
    std::string fname_in;
    std::string fname_out; 
    int size;

    std::string getHash()
    {
        return fname_in + std::to_string(size);
    }
}; 


/// \brief A class representing the processing of one SVG file to a PNG stream.
///
/// Not thread safe !
///
class TaskRunner
{
private:
    TaskDef task_def_;
    PNGDataPtr png_data_;
public:
    TaskRunner(const TaskDef& task_def):
        task_def_(task_def)
    {
        task_def_= task_def;
    }

    void operator()()
    {
        const std::string&  fname_in    = task_def_.fname_in;
        const std::string&  fname_out   = task_def_.fname_out;
        const size_t&       width       = task_def_.size; 
        const size_t&       height      = task_def_.size; 
        const size_t        stride      = width * BPP;
        const size_t        image_size  = height * stride;
        const float&        scale       = float(width) / ORG_WIDTH;

        std::cerr << "Running for "
                  << fname_in 
                  << "..." 
                  << std::endl;

        NSVGimage*          image_in        = nullptr;
        NSVGrasterizer*     rast            = nullptr;

        try {
            // Read the file ...
            std::lock_guard<std::mutex> lock(mut2);
            image_in = nsvgParseFromFile(fname_in.c_str(), "px", 0);
            if (image_in == nullptr) {
                std::string msg = "Cannot parse '" + fname_in + "'.";
                throw std::runtime_error(msg.c_str());
            }

            // Raster it ...
            std::vector<unsigned char> image_data(image_size, 0);
            rast = nsvgCreateRasterizer();
            nsvgRasterize(rast,
                          image_in,
                          0,
                          0,
                          scale,
                          &image_data[0],
                          width,
                          height,
                          stride); 

            // Compress it ...
            PNGWriter writer;
            writer(width, height, BPP, &image_data[0], stride);
            png_data_ = writer.getData();
            // Write it out ...
            std::ofstream file_out(fname_out, std::ofstream::binary);
            auto data = writer.getData();
            file_out.write(&(data->front()), data->size());
            
        } catch (std::runtime_error e) {
            std::cerr << "Exception while processing "
                      << fname_in
                      << ": "
                      << e.what()
                      << std::endl;
        }
        
        // Bring down ...
        nsvgDelete(image_in);
        nsvgDeleteRasterizer(rast);

        std::cerr << std::endl 
                  << "Done for "
                  << fname_in 
                  << "." 
                  << std::endl;
    }
    PNGDataPtr getData()
    {
        return png_data_;
    }
};

/// \brief A class that organizes the processing of SVG assets in PNG files.
///
/// Receives task definition as input and processes them, resulting in PNG 
/// files being written on disk.
///
/// Two main methods are offered: 
///  - parseAndRun(...): Parses a task definition string and immediately
///    processes the request. Blocking call.
///  - parseAndQueue(...): Parses a task definition string and put it at the
///    back of a queue for future processing. Returns immediately. If the 
///    definition is valid it will be processed in the future.
///
/// TODO: Process assets in a thread pool.
/// TODO: Cache the PNG result in memory if the same requests arrives again.
///
class Processor
{
private:
    // The tasks to run queue (FIFO).
    std::queue<TaskDef> task_queue_;

    // The cache hash map (TODO). Note that we use the string definition as the // key.
    using PNGHashMap = std::unordered_map<std::string, PNGDataPtr>;
    PNGHashMap png_cache_;

    bool should_run_;           // Used to signal the end of the processor to
                                // threads.

    std::vector<std::thread> queue_threads_;

public:
    /// \brief Default constructor.
    ///
    /// Creates background threads that monitors and processes the task queue.
    /// These threads are joined and stopped at the destruction of the instance.
    /// 
    /// \param n_threads: Number of threads (default: NUM_THREADS)
    Processor(int n_threads = NUM_THREADS):
        should_run_(true)
    {
        if (n_threads <= 0) {
            std::cerr << "Warning, incorrect number of threads ("
                      << n_threads
                      << "), setting to "
                      << NUM_THREADS
                      << std::endl;
            n_threads = NUM_THREADS;
        }

        for (int i = 0; i < n_threads; ++i) {
            queue_threads_.push_back(
                std::thread(&Processor::processQueue, this)
            );
        }
    }

    ~Processor()
    {
        should_run_ = false;
        int i =0;
        for (auto& qthread: queue_threads_) {
            data_cond.notify_all();
            qthread.join();
            std::cerr << "Thread " << i++ << " joined." << std::endl;
        }
        
    }

    /// \brief Parse a task definition string and fills the references TaskDef
    ///        structure. Returns true if it's a success, false if a failure 
    ///        occured and the structure is not valid.
    bool parse(const std::string& line_org, TaskDef& def)
    {
        std::string line = line_org;
        std::vector<std::string> tokens;
            size_t pos;
            while ((pos = line.find(";")) != std::string::npos) {
                tokens.push_back(line.substr(0, pos));
                line.erase(0, pos + 1);
            }
            tokens.push_back(line);

            if (tokens.size() < 3) {
                std::cerr << "Error: Wrong line format: "
                        << line_org
                        << " (size: " << line_org.size() << ")."
                        << std::endl;
                return false;
            }

            const std::string& fname_in     = tokens[0];
            const std::string& fname_out    = tokens[1];
            const std::string& width_str    = tokens[2]; 

            int width = std::atoi(width_str.c_str());

            def = {
                fname_in,
                fname_out,
                width
            };

            return true;
    }

    /// \brief Tries to parse the given task definition and run it.
    ///
    /// The parsing method will output error messages if it is not valid. 
    /// Nothing occurs if it's the case.
    void parseAndRun(const std::string& line_org)
    {
        TaskDef def;
        if (parse(line_org, def)) {
            TaskRunner runner(def);
            runner();
        }
    }

    /// \brief Parse the task definition and put it in the processing queue.
    ///
    /// If the definition is invalid, error messages are sent to stderr and 
    /// nothing is queued.
    void parseAndQueue(const std::string& line_org)
    {
        std::queue<TaskDef> queue;
        TaskDef def;
        std::lock_guard<std::mutex> lock(mut);
        if (parse(line_org, def)) {
            std::cerr << "Queueing task '" << line_org << "'." << std::endl;
            task_queue_.push(def);
            data_cond.notify_one();
        }
    }

    /// \brief Returns if the internal queue is empty (true) or not.
    bool queueEmpty()
    {
        return task_queue_.empty();
    }

private:
    /// \brief Queue processing thread function.
    void processQueue()
    {
        while (should_run_) {
            std::unique_lock<std::mutex> lock(mut);
            data_cond.wait(lock, [this]{return !task_queue_.empty() || !should_run_;});
            
            
            if (!should_run_) {
                return;
            }
            TaskDef task_def = task_queue_.front();

            
            task_queue_.pop();
            lock.unlock();
            
            
            auto hashKey = task_def.getHash();
            auto dataIterator = png_cache_.find(hashKey);

            if (dataIterator != png_cache_.end()) {
                std::cerr << "Cache hit " <<std::endl;
                auto png_data = png_cache_[hashKey];
                std::ofstream file_out(task_def.fname_out+"copy", std::ofstream::binary);
            }
            else{
                TaskRunner runner(task_def);
                runner();
                png_cache_[hashKey] = runner.getData();
                std::cerr << png_cache_.size() << std::endl;
            }
        }
    }
};

}

int main(int argc, char** argv)
{
    using namespace gif643;
    bool cache = false;
    std::ifstream file_in; 
    
    if (argc >= 2 ) {
        for (size_t i = 1; i < argc; i++){ 
            if (!strcmp(argv[i], "-f"))
            { 
                file_in.open(argv[i+1]);
                if (file_in.is_open()) {
                    std::cin.rdbuf(file_in.rdbuf());
                    std::cerr << "Using " << argv[i+1] << "..." << std::endl;
                } else {
                    std::cerr   << "Error: Cannot open '"
                                << argv[i+1] 
                                << "', using stdin (press CTRL-D for EOF)." 
                                << std::endl;
                };
            }
            if (!strcmp(argv[i], "-t"))
            {
                NUM_THREADS = atoi(argv[i+1]);
                std::cerr << "Using " << NUM_THREADS << " threads..." << std::endl;
            }
            if (!strcmp(argv[i], "-c"))
            {
                cache = true;
                std::cerr << "Using cache..." << std::endl;
            }
        }
        
        
    } else {
        std::cerr << "Using stdin (press CTRL-D for EOF)." << std::endl;
    }

    // TODO: change the number of threads from args.
    Processor proc;
    std::vector<std::string> doublons;
    while (!std::cin.eof()) {

        std::string line, line_org;

        std::getline(std::cin, line);
        if (!line.empty()) {
            doublons.push_back(line);
            proc.parseAndQueue(line);
        }
    }
    std::cerr << "doublons size" << doublons.size() << std::endl;
    if(cache){
        for (size_t i = 0; i < doublons.size(); i++)
        {
            std::cerr << "double" << doublons[i] << std::endl;
            proc.parseAndQueue(doublons[i]);
        }
        
    } 
    
    
    if (file_in.is_open()) {
        file_in.close();
    }

    // Wait until the processor queue's has tasks to do.
    while (!proc.queueEmpty()) {};
}
/*
                    auto hashKey = task_def.get_hash_key();
                    auto dataIterator = png_cache_.find(hashKey);
                    if (dataIterator == png_cache_.end() || !std::get<PNGDataPtr>(*dataIterator))
                    {
                        png_cache_.emplace(hashKey, PNGDataPtr{});
                        hashMutex.unlock();
                        
                        TaskRunner runner(task_def);
                        runner();
                           
                        PNGDataPtr pngData = runner.getData();
                        hashMutex.lock();
                        std::cout << "inserting " << task_def.fname_out << std::endl;
                        png_cache_[hashKey] = pngData;
                        hashMutex.unlock();
                    }
                    else
                    {
                        std::cout << "No need to compute" << std::endl;

                        std::cout << "getting for " << task_def.fname_out << std::endl;
                        PNGDataPtr pngData = std::get<PNGDataPtr>(*dataIterator);
                        
                        // Write it out ...
                        std::ofstream file_out(task_def.fname_out, std::ofstream::binary);
                        
                        std::cout << "got" << std::endl;
                        hashMutex.unlock();
                        file_out.write(&(pngData->front()), pngData->size());
                        std::cout << "file written" << std::endl;
                    }
*/