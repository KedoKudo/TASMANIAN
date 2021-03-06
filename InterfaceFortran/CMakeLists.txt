########################################################################
# Fortran librareis and command line tools
########################################################################

macro(Tasmanian_macro_add_libdfortran Tasmanian_shared_or_static)

if (${Tasmanian_shared_or_static} STREQUAL SHARED)
    set(Tasmanian_libfor_target_name "Tasmanian_libfortran90_shared")
else()
    set(Tasmanian_libfor_target_name "Tasmanian_libfortran90_static")
endif()

add_library(${Tasmanian_libfor_target_name} ${Tasmanian_shared_or_static} TasmanianSG.f90
                                                                          tsgC2FortranBridge.f90
                                                                          tsgC2Fortran.cpp)

target_link_libraries(${Tasmanian_libfor_target_name} Tasmanian_addons)
set_target_properties(${Tasmanian_libfor_target_name} PROPERTIES OUTPUT_NAME "tasmanianfortran90"
                                                                 INSTALL_RPATH "${Tasmanian_final_install_path}/lib"
                                                                 SOVERSION ${Tasmanian_VERSION_MAJOR}
                                                                 VERSION   ${PROJECT_VERSION})

target_include_directories(${Tasmanian_libfor_target_name} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/>)

if (Tasmanian_ENABLE_MPI)
    # see the comments in the Addons/CMakeLists.txt
    target_link_libraries(${Tasmanian_libfor_target_name} MPI::MPI_Fortran)
endif()

install(TARGETS ${Tasmanian_libfor_target_name}
        EXPORT "${Tasmanian_export_name}"
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib")

unset(Tasmanian_libfor_target_name)
endmacro()

########################################################################
# add the fortran tester and examples executables
########################################################################
add_executable(Tasmanian_fortester fortester.f90)

set_target_properties(Tasmanian_fortester  PROPERTIES OUTPUT_NAME "fortester" LINKER_LANGUAGE Fortran)

########################################################################
# add the libraries
########################################################################
foreach(_tsglibtype ${Tasmanian_libs_type})
    string(TOUPPER ${_tsglibtype} Tasmanian_ltype)
    Tasmanian_macro_add_libdfortran(${Tasmanian_ltype})

    target_link_libraries(Tasmanian_libfortran90_${_tsglibtype} Tasmanian_libdream_${_tsglibtype})
endforeach()
unset(_tsglibtype)
unset(Tasmanian_ltype)

target_link_libraries(Tasmanian_fortester  Tasmanian_libfortran90_${Tasmanian_lib_default})

if (Tasmanian_ENABLE_MPI)
    add_executable(Tasmanian_mpif90    mpitester.f90)
    set_target_properties(Tasmanian_mpif90     PROPERTIES OUTPUT_NAME "mpitester" LINKER_LANGUAGE Fortran)
    target_link_libraries(Tasmanian_mpif90     Tasmanian_libfortran90_${Tasmanian_lib_default})
    add_test(MPIFortranGridIO ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 3 ${MPIEXEC_PREFLAGS} ${CMAKE_CURRENT_BINARY_DIR}/mpitester ${MPIEXEC_POSTFLAGS})
    set_tests_properties(MPIFortranGridIO PROPERTIES RUN_SERIAL ON)
    list(APPEND Tasmanian_fortranmpi_test "MPIFortranGridIO")
endif()

########################################################################
# Windows specific support (DLL export/import directives and names)
########################################################################
if ((${CMAKE_SYSTEM_NAME} STREQUAL "Windows") AND (TARGET Tasmanian_libfortran90_static))
    set_target_properties(Tasmanian_libfortran90_static PROPERTIES OUTPUT_NAME "tasmanianfortran90_static")
endif()


########################################################################
# Testing
########################################################################
add_test(Fortran90 fortester)
if (Tasmanian_TESTS_OMP_NUM_THREADS GREATER 0)
    set_tests_properties(Fortran90 ${Tasmanian_fortranmpi_test}
        PROPERTIES
        PROCESSORS "${Tasmanian_TESTS_OMP_NUM_THREADS}"
        ENVIRONMENT "OMP_NUM_THREADS=${Tasmanian_TESTS_OMP_NUM_THREADS}")
endif()
unset(Tasmanian_fortranmpi_test)


########################################################################
# Installation
########################################################################
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tasmaniansg.mod"
        DESTINATION include
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
# install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Examples/example_sparse_grids.f90"
#         DESTINATION "share/Tasmanian/examples/"
#         PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
