// This example from an alpha release of the Scalable HeterOgeneous Computing
// (SHOC) Benchmark Suite Alpha v1.1.4a-mic for Intel MIC architecture
// Contact: Jeffrey Vetter <vetter@ornl.gov>
//          Rezaur Rahman <rezaur.rahman@intel.com>
//
// Copyright (c) 2011-2013, UT-Battelle, LLC
// Copyright (c) 2013, Intel Corporation
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//   
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of Oak Ridge National Laboratory, nor UT-Battelle, LLC, 
//    nor the names of its contributors may be used to endorse or promote 
//    products derived from this software without specific prior written 
//    permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
// OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
// THE POSSIBILITY OF SUCH DAMAGE.

#include <iostream>
#include <string>
#include <stdio.h>
#include "omp.h"
#include "Timer.h"
#include "ResultDatabase.h"
#include "OptionParser.h"

//Based on the 5110P with 4 threads per core
#define MIC_THREADS 240
// For heterogeneous features include "offload.h"
#include "offload.h"
#ifdef __MIC__ ||__MIC2__
#include <immintrin.h>
#endif

// Memory Benchmarks Sizes
#define VECSIZE_SP 480000
#define REPS_SP 100

//float __declspec(target(mic)) testICC_read(const int reps);
float __declspec(target(mic)) testICC_read(float* data, const int reps);
//float __declspec(target(mic)) testICC_write(const int reps, const float value);
float __declspec(target(mic)) testICC_write(const size_t numElements, float* data, const int reps, const float value);


// L2 & L1 Benchmarks Sizes
#define VECSIZE_SP_L2 4864
#define REPS_SP_L2 1000000
#define VECSIZE_SP_L1 1024
#define REPS_SP_L1 1000000

// ****************************************************************************
// Function: addBenchmarkSpecOptions
//
// Purpose:
//   Add benchmark specific options parsing
//
// Arguments:
//   op: the options parser / parameter database
//
// Returns:  nothing
//
// Programmer: Alexander Heinecke
// Creation: July 23, 2010
//
// Modifications:
// ****************************************************************************
void addBenchmarkSpecOptions(OptionParser &op)
{
    // No specific options for this benchmark.
}

// ****************************************************************************
// Function: initData
//
// Purpose:
//   Randomly intialize the host data in preparation for the FP test.
//
// Arguments:
//   hostMem - uninitialized host memory
//   numFloats - length of hostMem array
//
// Returns:  nothing
//
// Programmer: Zhi Ying(zhi.ying@intel.com)
//             Jun Jin(jun.i.jin@intel.com)
//
// Modifications:
// Aug. 12, 2014 - Jeff Young - Modified to support different data type
//
// ****************************************************************************
void InitData(float *hostMem, const size_t numElements)
{
    /*const int halfNumFloats = numFloats/2;
    srand((unsigned)time(NULL));
    for (int j=0; j<halfNumFloats; ++j)
    {
        hostMem[j] = hostMem[numFloats-j-1] = (T)((rand()/ (float)RAND_MAX) *
                10.0);
    }*/
    
    #pragma ivdep
    #pragma omp parallel for shared(hostMem)
    for (int q = 0; q < numElements; q++)
    {
        hostMem[q] = 1.0;
    }
}



// ****************************************************************************
// Function: runBenchmark
//
// Purpose:
//
// Arguments:
//  resultDB: the benchmark stores its results in this ResultDatabase
//  op: the options parser / parameter database
//
// Returns:  nothing
//
// Programmer: Alexander Heinecke
// Creation: July 23, 2010
//
// Modifications:
// Dec. 12, 2012 - Kyle Spafford - Updates and SHOC coding style conformance.
// Aug. 12, 2014 - Jeff Young - Removed intrinsics code and eversion variable 
// Aug. 25, 2014 - Jeff Young - Updated bandwidth calculation and added result
//    variables for compatibility with shocdriver.
//         
// ****************************************************************************
void RunBenchmark(OptionParser &op, ResultDatabase &resultDB)
{
    const bool verbose = op.getOptionBool("verbose");
    const unsigned int passes = op.getOptionInt("passes");

    char sizeStr[256];

    double t = 0.0f;
    double startTime;
    unsigned int w;
    __declspec(target(mic)) static unsigned int reps;
    double nbytes;
    float input = 1.0;

    int numThreads = MIC_THREADS;
    double dThreads = static_cast<double>(numThreads);
    
    double bdwth;

    //Initialize the data array for each test
    size_t numElements;
    numElements = VECSIZE_SP*MIC_THREADS;

    static __declspec(target(mic)) float *hostMem;
    hostMem = (float*)_mm_malloc(sizeof(float)*numElements, 64);
    
    float res = 0.0;
    
    w = VECSIZE_SP;
    reps = REPS_SP;
    
    //Number of bytes read/written is currently constant
    nbytes = ((double)w)*((double)reps)*((double)sizeof(float))*dThreads;
    
    if(verbose)
      cout<< "Test Parameters:\n VECSIZE_SP = "<<w<<", REPS_SP = "<<reps<<", MIC_THREADS = "<<MIC_THREADS<<", nbytes/test = "<< nbytes <<" B"<<endl<<endl;
    
    //Total number of bytes transferred across all reps of each test
    sprintf(sizeStr, "%4f kB", nbytes/(1024.0));

    for (int p = 0; p < passes; p++)
    {
        cout << "Running benchmarks, pass: " << p << "\n";

        // Test Memory

        // ========= Test Read - ICC Code =============
        //Reinitialize input array for each test and each pass
        InitData(hostMem,numElements);  
        //Transfer data to device before timed section and don't free the allocation
        #pragma offload target(mic) in(hostMem:length(numElements) free_if(0))
        {}

        int testICC_readTimerHandle = Timer::Start();

        //Specify that hostMem was already transferred
        #pragma offload target (mic) nocopy(hostMem)
        res = testICC_read(hostMem, reps);
    
        t = Timer::Stop(testICC_readTimerHandle, "testICC_read");

        // Add Result - while this is not strictly a coalesced read, this value matches up with the gmem_writebw result for SHOC
        bdwth = ((double)nbytes) / (t*1.e9);
        resultDB.AddResult("readGlobalMemoryCoalesced", sizeStr, "GB/s", bdwth);

        // ========= Test Write - ICC Code =============
        //Reinitialize input array for each test and each pass
        InitData(hostMem,numElements);  
        //Transfer data to device before timed section and don't free the allocation
        #pragma offload target(mic) in(hostMem:length(numElements) free_if(0))        
        {}
        
        int testICC_writeTimerHandle = Timer::Start();
        #pragma offload target (mic) in(numElements) nocopy(hostMem)
        res = testICC_write(numElements, hostMem, reps, input);
        t = Timer::Stop(testICC_writeTimerHandle, "testICC_write");
        

        // Add Result - while this is not strictly a coalesced write, this value matches up with the gmem_writebw result for SHOC
        bdwth = ((double)nbytes) / (t*1.e9);
        resultDB.AddResult("writeGlobalMemoryCoalesced", sizeStr, "GB/s",
                bdwth);

        //Add bogus results for the other 5 DeviceMemory metrics to preserve compatibility with CUDA/OpenCL implementations.
        //These results should not be reported by shocdriver 
        resultDB.AddResult("readGlobalMemoryUnit", sizeStr, "GB/s", FLT_MAX);
        resultDB.AddResult("writeGlobalMemoryUnit", sizeStr, "GB/s", FLT_MAX);
        resultDB.AddResult("readLocalMemory", sizeStr, "GB/s", FLT_MAX);
        resultDB.AddResult("writeLocalMemory", sizeStr, "GB/s", FLT_MAX);
        resultDB.AddResult("TextureRepeatedRandomAccess", sizeStr, "GB/s", FLT_MAX);
    }
    
    // Free memory allocated on the mic
    #pragma offload target(mic) \
    in(hostMem:length(numElements) alloc_if(0)  )
    {
    }
    
    _mm_free(hostMem);
}
// ****************************************************************************
// Function: testICC_read
//
// Purpose: RUns the 
//
// Arguments:
//
// Returns:  nothing
//
// Programmer: Alexander Heinecke
// Creation: July 23, 2010
//
// Modifications:
// Aug. 12, 2014 - Jeff Young - Removed eversion variable
//
// ****************************************************************************


float __declspec(target(mic)) testICC_read(float* data, const int reps)
{
#ifdef __MIC__ || __MIC2__

    __declspec(aligned(64))float res = 0.0;

    #pragma omp parallel shared(res)
    {
        __declspec(aligned(64))float b = 0.0;
        int offset = VECSIZE_SP * omp_get_thread_num();

        for (int m = 0; m < reps; m++)
        {
            #pragma vector aligned  //arrays can be aligned for perf optimization
            #pragma ivdep           //ignore vector dependencies
            for (int q = offset; q < offset+VECSIZE_SP; q++)
            {
                b += data[q];
            }
            b += 1.0;
        }
        #pragma omp critical //Each thread will update the result sum with their contribution
        {
            res += b;
        }
    }
    return res;
#else
    return 0.0;
#endif
}


float __declspec(target(mic)) testICC_write(const size_t numElements, float* data, const int reps, const float value)
{
#ifdef __MIC__ || __MIC2__


    __declspec(aligned(64))float res = 0.0;

    #pragma omp parallel shared(res)
    {
        int offset = VECSIZE_SP * omp_get_thread_num();
        __declspec(aligned(64))float writeData = value + 
            static_cast<float>(omp_get_thread_num());

        for (int m = 0; m < reps; m++)
        {
            #pragma vector aligned
            #pragma ivdep
            for (int q = offset; q < offset+VECSIZE_SP; q++)
            {
                data[q] += writeData;
            }
            writeData += 1.0;
        }
    }

    // Sum something in data, avoid compiler optimizations
    res = data[0] + data[numElements-1];
    
    return res;
#else
    return 0.0;
#endif
}