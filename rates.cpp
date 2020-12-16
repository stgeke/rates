#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include "omp.h"
#include <unistd.h>
#include "mpi.h"
#include "occa.hpp"

#include "gri.h"

int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);

  if(argc < 4) {
    printf("Usage: ./bw [SERIAL|CUDA|OPENCL] [number of states] [blockSize]\n");
    return 1;
  }

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  char * cwd;
  cwd = (char*) malloc(4096*sizeof(char) );
  getcwd(cwd,4096);
  std::string current_dir;
  current_dir.assign(cwd);

  occa::env::OCCA_CACHE_DIR = current_dir + "/occa/";

  std::string threadModel;
  threadModel.assign(strdup(argv[1]));

  int blockSize = std::stoi(argv[3]);
  const int N = std::stoi(argv[2])/size;

  const int deviceId = 0;
  const int platformId = 0;

  // build device
  occa::device device;
  char deviceConfig[BUFSIZ];

  if(strstr(threadModel.c_str(), "CUDA")) {
    sprintf(deviceConfig, "mode: 'CUDA', device_id: %d",deviceId);
  }else if(strstr(threadModel.c_str(),  "HIP")) {
    sprintf(deviceConfig, "mode: 'HIP', device_id: %d",deviceId);
  }else if(strstr(threadModel.c_str(),  "OPENCL")) {
    sprintf(deviceConfig, "mode: 'OpenCL', device_id: %d, platform_id: %d", deviceId, platformId);
  }else if(strstr(threadModel.c_str(),  "OPENMP")) {
    sprintf(deviceConfig, "mode: 'OpenMP' ");
  }else {
    sprintf(deviceConfig, "mode: 'Serial' ");
    omp_set_num_threads(1);
  }

  std::string deviceConfigString(deviceConfig);
  device.setup(deviceConfigString);
  if(rank == 0) std::cout << "active occa mode: " << device.mode() << "\n\n";

  const int Ntests = 10;
  const int Nblocks = (N + blockSize-1)/blockSize; 

  double *phi       = (double*) malloc(NS * N * sizeof(double));
  double *pr        = (double*) malloc(     N * sizeof(double));
  double *T         = (double*) malloc(     N * sizeof(double));
  double *rates     = (double*) malloc(NS * N * sizeof(double));
  double *rates_ref = (double*) malloc(NS * N * sizeof(double));

  const double prRef = 1.01325000e+05; // [Pa]
  const double TRef = 1000;            // K
  const double Xref = 1./NS; 
 
  double elapsedTime;
  if(N <= 1 || device.mode() == "Serial") {
    for(int i=0; i<N; i++) {
      pr[i] = 10*prRef;
      T[i]  = TRef;
      for(int j=0; j<NS; j++) phi[j + i*NS] = Xref;
    }
 
    ckwxp_(&pr[0], &T[0], &phi[0*NS], NULL, NULL, &rates_ref[0 + 0*NS]);
 
    MPI_Barrier(MPI_COMM_WORLD);
    elapsedTime = MPI_Wtime();
    for(int i=0; i<N; i++)
      ckwxp_(&pr[i], &T[i], &phi[i*NS], NULL, NULL, &rates_ref[i*NS]);
    MPI_Barrier(MPI_COMM_WORLD);
    elapsedTime += (MPI_Wtime() - elapsedTime);

    if(rank == 0) std::cout << "CPU throughput: " << (size*(double)N)/elapsedTime/1e6 << " MStates/s\n";
    if (device.mode() == "Serial") {
      MPI_Finalize();
      exit(0);
    }

  }

  occa::properties props;
  props["defines"].asObject();
  props["includes"].asArray();
  props["header"].asArray();
  props["flags"].asObject();
  //props["compiler_flags"] += " --prec-div=false --prec-sqrt=false";
  //props["compiler_flags"] += " --use_fast_math";
  props["includes"] += current_dir + "/gri.inc";
  props["okl/enabled"] = false;

  occa::kernel ratesKernel = device.buildKernel("kernel/gri.cu", "ckwxp_", props);

  for(int i=0; i<N; i++) pr[i] = 10*prRef;
  for(int i=0; i<N; i++) T[i] = TRef;
  for(int j=0; j<NS; j++) {
    for(int i=0; i<N; i++) phi[j*N + i] = Xref;
  }

  occa::memory o_phi   = device.malloc(NS*N * sizeof(double), phi);
  occa::memory o_T     = device.malloc(   N * sizeof(double), T);
  occa::memory o_pr    = device.malloc(   N * sizeof(double), pr);
  occa::memory o_rates = device.malloc(NS*N * sizeof(double));

  occa::dim outer, inner;
  outer.dims = 1;
  inner.dims = 1;
  outer[0] = Nblocks;
  inner[0] = blockSize;
  ratesKernel.setRunDims(outer, inner);

  std::cout << "Nstates: " << N << "\nblockSize: " << blockSize
            << "\nNblocks: " << Nblocks << "\n";
  std::cout << "device memory allocation: " << device.memoryAllocated()/1e6 << " MB\n";

  // warm up
  ratesKernel(N, o_pr, o_T, o_phi, o_rates); 

  device.finish();
  MPI_Barrier(MPI_COMM_WORLD);
  elapsedTime = MPI_Wtime();

  for(int i=0; i<Ntests; i++)
    ratesKernel(N, o_pr, o_T, o_phi, o_rates); 

  device.finish();
  MPI_Barrier(MPI_COMM_WORLD);
  elapsedTime += (MPI_Wtime() - elapsedTime)/Ntests;
  std::cout << "throughput: " << (size*(double)N)/elapsedTime/1e6 << " MStates/s\n";

  if(N <= 1) {
    o_rates.copyTo(phi, NS*N * sizeof(double));
    double err_inf = 0;
    for(int j=0; j<NS; j++) {
      for(int i=0; i<N; i++) {
        const double wdot_ref = rates_ref[j + i*NS];
        const double wdot = phi[j*N + i];
        const double err = fabs((wdot - wdot_ref)/wdot_ref + 1e-300);
        err_inf = std::max(err, err_inf);
      }
    }
    printf("Linf error = %g\n", err_inf);
  }

  MPI_Finalize();
  exit(0);
}
