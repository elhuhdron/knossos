
# provides an imported target for the szip library

#find_library(SZIP_LIB NAMES libszip szip sz)
find_library(SZIP_LIB NAMES szip sz)
find_path(SZIP_INCLUDE szlib.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SZIP
    REQUIRED_VARS SZIP_LIB SZIP_INCLUDE
)

if(SZIP_FOUND)
    add_library(szip::szip UNKNOWN IMPORTED)
    set_target_properties(szip::szip PROPERTIES
        IMPORTED_LOCATION ${SZIP_LIB}
        INTERFACE_INCLUDE_DIRECTORIES ${SZIP_INCLUDE}
    )
else(SZIP_FOUND)
    message (FATAL_ERROR "SZip was NOT found and is Required by this project")
endif(SZIP_FOUND)
