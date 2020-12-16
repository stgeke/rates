## Setup
1) Install OCCA (available under https://github.com/libocca/occa)
2) Set OCCA_DIR in makefile
3) Run make

## Run Benchmark
```
export OCCA_DIR=/home/stefanke/occa; export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$OCCA_DIR/lib
>./rates CUDA 100000 256

active occa mode: CUDA
Nstates: 100000
blockSize: 256
Nblocks: 391
device memory allocation: 86.4 MB
throughput: 51.9102 MStates/s
```
