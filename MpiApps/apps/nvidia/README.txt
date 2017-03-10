Interfacing Intel InfiniBand with NVIDIA GPUs

-----------------------------------------------------------------------------
Introduction
-----------------------------------------------------------------------------

This directory provides information on how to setup and test that two 
drivers -- the Intel InfiniBand driver (qib) and the NVIDIA GPU driver -- 
can share pinned memory pages for optimal performance.

If you are running on any Linux distribution supported by this Intel
InfiniBand software release, then you must use NVIDIA CUDA 4.0 or newer to
have a supported solution from NVIDIA.  To get optimal performance with the
qib InfiniBand driver in this software release in applications which CUDA 4.x,
all you need to do is to set the environment variable:
   CUDA_NIC_INTEROP=1
and to propagate that variable to the compute nodes running the application.
   
---------------------------------------
CONTENTS

- README.txt       - This file                     
                     
- mpi_pinned.c     - A small test program that can verify if the
                     installation was successful.
- Makefile         - File to enable easy build of two test programs
                     from mpi_pinned.c

---------------------------------------

Note:  

In previous QLogic IFS releases, NVIDIA support required a patch to the CUDA
3.1 or 3.2 driver.  This patch is not needed when using IFS 7.1 and CUDA 4.0
or higher, so the patch is no longer included.

------------------------------------------------
CUDA Toolkit and mpi_pinned.c test program 
------------------------------------------------

To build the test programs, use the following steps:

1. Download and install the CUDA Toolkit for your Linux distribution.

   - Please make sure your LD_LIBRARY_PATH includes /usr/local/cuda/lib64
   - Please make sure your PATH includes /usr/local/cuda/bin

2. From the /usr/src/opa/mpi_apps directory, choose your mpi as you
   would normally, then type:

   # make NVIDIA

3. Copy the resulting binaries to all nodes using the fast-fabric tools or
   with scp.

4. From the same directory, type:

   # ./run_nvidia

