#pragma once

#include <sycl/sycl.hpp>

#ifdef __SYCL_DEVICE_ONLY__
extern SYCL_EXTERNAL ulong __attribute__((overloadable))
intel_get_cycle_counter(void);
#endif

double cache_triad(size_t n, size_t nreps) {
  double tot_mem_bw = 0.0;

  try {
    sycl::queue gpuQueue{sycl::gpu_selector_v};
    sycl::device device = gpuQueue.get_device();

    // Print device
    std::cout << "Running on " << device.get_info<sycl::info::device::name>()
              << "\n";

    int num_blocks =
        device.get_info<sycl::info::device::max_compute_units>() * 1;
    int block_size = 1024;

    // if we don't have enough GPU memory, don't run
    long long int max_mem =
        device.get_info<sycl::info::device::global_mem_size>();

    if (num_blocks * n * 3 * sizeof(double) > max_mem) {
      fprintf(stderr, "error: out of GPU global memory\n");
      return -1.0;
    }

    double *d_a =
        sycl::malloc_device<double>(sizeof(double) * n * num_blocks, gpuQueue);
    double *d_b =
        sycl::malloc_device<double>(sizeof(double) * n * num_blocks, gpuQueue);
    double *d_c =
        sycl::malloc_device<double>(sizeof(double) * n * num_blocks, gpuQueue);

    double *h_bw = (double *)malloc(sizeof(double) * num_blocks);
    double *d_bw =
        sycl::malloc_device<double>(sizeof(double) * num_blocks, gpuQueue);

    // FIXME: not sure how this will affect the result with dynamic clock rate
    double freq =
        (double)device.get_info<sycl::info::device::max_clock_frequency>() /
        1e3;

    gpuQueue.submit([&](sycl::handler &cgh) {
      const sycl::range<3> gridDim(num_blocks, 1, 1);
      const sycl::range<3> blockDim(block_size, 1, 1);
      cgh.parallel_for(
          sycl::nd_range<3>(gridDim * blockDim, blockDim), [=](sycl::nd_item<3> item) {
            const int thread_idx = item.get_local_id(0);
            const int block_idx = item.get_group(0);
            const int block_dimx = item.get_local_range(0);
            const double scalar = 2.0;

            double *a = d_a + block_idx * n;
            double *b = d_b + block_idx * n;
            double *c = d_c + block_idx * n;

            // This should place a,b,c in cache
            for (int i = thread_idx; i < n; i += block_dimx) {
              a[i] = 0.0;
              b[i] = 3.0;
              c[i] = 2.0;
            }

            ulong c0, c1;
#ifdef __SYCL_DEVICE_ONLY__
            c0 = intel_get_cycle_counter();
#endif

            for (int t = 0; t < nreps; ++t) {
              for (int i = thread_idx; i < n; i += block_dimx) {
                a[i] += b[i] + scalar * c[i];
              }
              item.barrier(); // Can it be more specific?
            }

#ifdef __SYCL_DEVICE_ONLY__
            c1 = intel_get_cycle_counter();
#endif
            double seconds = (((double)(c1 - c0)) / freq) / 1e9;
            double avg_seconds = seconds / nreps;
            double data_size = (double)n * 4.0 * sizeof(double) / 1e9;

            if (thread_idx == 0) {
              d_bw[block_idx] = data_size / avg_seconds;
            }
          });
    }).wait_and_throw();


    gpuQueue.memcpy(h_bw, d_bw, sizeof(double) * num_blocks).wait_and_throw();

    // Sum the memory bw per SM to get the aggregate memory bandwidth
    for (int i = 0; i < num_blocks; ++i) {
      tot_mem_bw += h_bw[i];
    }

    sycl::free(d_a, gpuQueue);
    sycl::free(d_b, gpuQueue);
    sycl::free(d_c, gpuQueue);

    return tot_mem_bw;
  } catch (sycl::exception &e) {
    /* handle SYCL exception */
    std::cerr << e.what() << std::endl;
    return -1.0;
  }
}
