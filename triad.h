#pragma once

#if SHMEM==1
#include "triad-shmem.h"
#elif defined(GPU)
#include "triad-gpu.h"
#elif defined(SYCL_USM)
#include "triad-sycl-usm.hpp"
#else
#include "triad-cpu.h"
#endif

