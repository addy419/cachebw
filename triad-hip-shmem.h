#include "hip/hip_runtime.h"
#pragma once

#include "hip_utils.hpp"

__global__ void triad_kernel(double* d_a, double* d_b, double* d_c, size_t n,
                             size_t nreps, double* bw, double freq)
{
  const int tid = threadIdx.x;
  const int blk_sz = blockDim.x;

  const double scalar = 2.0;

  extern __shared__ double shmem[];
  double* a = &shmem[0*n];
  double* b = &shmem[1*n];
  double* c = &shmem[2*n];

  for (int i = tid; i < n; i += blk_sz) {
    a[i] = 0.0;
    b[i] = 3.0;
    c[i] = 2.0;
  }

  long long int c0 = clock64();
  for (int t = 0; t < nreps; ++t) {
    for (int i = tid; i < n; i += blk_sz) {
      a[i] += b[i] + scalar * c[i];
    }
    __syncthreads();
  }
  long long int c1 = clock64();

  double clocks = (double)(c1 - c0);
  double avg_clocks = clocks / nreps;
  double data_size = (double)n * 4.0 * sizeof(double);

  if (tid == 0) bw[blockIdx.x] = data_size / avg_clocks;
}

double cache_triad(size_t n, size_t nreps)
{
  double tot_mem_bw = 0.0;

  hipDeviceProp_t prop;
  hipGetDeviceProperties(&prop, 0);

  int num_blks = prop.multiProcessorCount * 4;
  int blk_sz = 128;

  if (4*n * 3 * sizeof(double) > prop.sharedMemPerBlock) return -1.0;

  double* a;
  double* b;
  double* c;

  HIPCHK(hipMalloc((void**)&a, sizeof(double) * n * num_blks));
  HIPCHK(hipMalloc((void**)&b, sizeof(double) * n * num_blks));
  HIPCHK(hipMalloc((void**)&c, sizeof(double) * n * num_blks));

  double* h_bw =
      (double*)malloc(sizeof(double) * num_blks);
  double* d_bw;
  HIPCHK(hipMalloc((void**)&d_bw, sizeof(double) * num_blks));

  double freq = (double)prop.clockRate/1e6;

  triad_kernel<<<num_blks, blk_sz, sizeof(double)*n*3>>>(a, b, c, n, nreps, d_bw, freq);
  HIPCHK(hipGetLastError());
  HIPCHK(hipDeviceSynchronize());

  HIPCHK(hipMemcpy(h_bw, d_bw, sizeof(double) * num_blks,
                     hipMemcpyDeviceToHost));

  for (int i = 0; i < num_blks; ++i) {
    tot_mem_bw += h_bw[i];
  }

  HIPCHK(hipFree(a));
  HIPCHK(hipFree(b));
  HIPCHK(hipFree(c));

  printf("mem bw = %f\n", tot_mem_bw);

  return tot_mem_bw;
}
