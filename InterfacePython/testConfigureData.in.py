########################################################################
# This file will be configured by CMake (or pre-configured for GNU Make)
# The information here will be omnipresent throughout the testing
# environment, which will reduce the need for configuring.
########################################################################

bEnableSyncTests = True

sLibPath = "@Tasmanian_libsparsegrid_path@"

iGPUID = @Tasmanian_TESTS_GPU_ID@

bHasBlas = ("@Tasmanian_ENABLE_BLAS@" == "ON")
bHasCuBlas = ("@Tasmanian_ENABLE_CUDA@" == "ON")
bHasCuda = ("@Tasmanian_ENABLE_CUDA@" == "ON")

bUsingMSVC = ("@CMAKE_CXX_COMPILER_ID@" == "MSVC")

sGaussPattersonTableFile = "@CMAKE_CURRENT_BINARY_DIR@/GaussPattersonRule.table"