set (CMAKE_BUILD_TYPE RELEASE CACHE STRING "")

set (Trilinos_ENABLE_STK ON CACHE BOOL "")
set (Trilinos_ENABLE_SEACAS ON CACHE BOOL "")
set (Trilinos_ENABLE_TESTS ON CACHE BOOL "")
set (Trilinos_ENABLE_ALL_OPTIONAL_PACKAGES OFF CACHE BOOL "")

set (TPL_ENABLE_MPI ON CACHE BOOL "")
set (TPL_ENABLE_Matio OFF CACHE BOOL "")
set (TPL_ENABLE_BLAS ON CACHE BOOL "")
set (TPL_ENABLE_LAPACK ON CACHE BOOL "")
set (TPL_ENABLE_Boost ON CACHE BOOL "")
set (TPL_ENABLE_HDF5 ON CACHE BOOL "")
set (TPL_ENABLE_Netcdf ON CACHE BOOL "")

set (CMAKE_C_COMPILER "$ENV{MPICC}" CACHE FILEPATH "")
set (CMAKE_CXX_COMPILER "$ENV{MPICXX}" CACHE FILEPATH "")
set (CMAKE_Fortran_COMPILER "$ENV{MPIF90}" CACHE FILEPATH "")

set (MPI_BASE_DIR "$ENV{SEMS_OPENMPI_ROOT}" CACHE PATH "")

set (Boost_INCLUDE_DIRS "$ENV{SEMS_BOOST_INCLUDE_PATH}" CACHE PATH "")
set (Boost_LIBRARY_DIRS "$ENV{SEMS_BOOST_LIBRARY_PATH}" CACHE PATH "")

set (HDF5_INCLUDE_DIRS "$ENV{SEMS_HDF5_INCLUDE_PATH}" CACHE PATH "")
set (HDF5_LIBRARY_DIRS "$ENV{SEMS_HDF5_LIBRARY_PATH}" CACHE PATH "")

set (Netcdf_INCLUDE_DIRS "$ENV{SEMS_NETCDF_INCLUDE_PATH}" CACHE PATH "")
set (Netcdf_LIBRARY_DIRS "$ENV{SEMS_NETCDF_LIBRARY_PATH}" CACHE PATH "")

set (Trilinos_ENABLE_CONFIGURE_TIMING ON CACHE BOOL "")
set (Trilinos_SHOW_DEPRECATED_WARNINGS OFF CACHE BOOL "")
set (CMAKE_CXX_FLAGS "-Wall -Wno-clobbered -Wno-vla -Wno-pragmas -Wno-unknown-pragmas -Wno-unused-local-typedefs -Wno-literal-suffix -Wno-deprecated-declarations -Wno-misleading-indentation -Wno-int-in-bool-context -Wno-maybe-uninitialized -Wno-nonnull-compare -Wno-address -Wno-inline -DTRILINOS_HIDE_DEPRECATED_HEADER_WARNINGS -Werror" CACHE STRING "Warnings as errors settings")
set (STK_DISABLE_MPI_NEIGHBOR_COMM ON CACHE BOOL "")
set (MPI_EXEC_PRE_NUMPROCS_FLAGS "--bind-to;none" CACHE STRING "")

