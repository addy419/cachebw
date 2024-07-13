#include <stdio.h>
#include <stdlib.h>

#include "triad.hpp"

int main(int argc, char** argv)
{
  if (argc < 3) {
    fprintf(
        stderr,
        "error: need 2 input args; (1) Size of data per thread(CPU)/SM(GPU) in KiB, (2) Number of reps\n");
    return 1;
  }

  int kbytes = atoi(argv[1]);
  int nreps = atoi(argv[2]);

  size_t n = ((size_t)kbytes * 1024) / (3 * sizeof(double));

  double tot_mem_bw = cache_triad(n, nreps);

  printf("n %8zu, reps = %8d, bytes %12zu, bandwidth = %12f\n", n, nreps,
         n * 3 * sizeof(double), tot_mem_bw);

  return 0;
}
