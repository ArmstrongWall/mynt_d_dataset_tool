#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mynteye_depth" for configuration "Debug"
set_property(TARGET mynteye_depth APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(mynteye_depth PROPERTIES
  IMPORTED_LOCATION_DEBUG "/usr/local/lib/libmynteye_depthd.so.1.6.0"
  IMPORTED_SONAME_DEBUG "libmynteye_depthd.so.1"
  )

list(APPEND _IMPORT_CHECK_TARGETS mynteye_depth )
list(APPEND _IMPORT_CHECK_FILES_FOR_mynteye_depth "/usr/local/lib/libmynteye_depthd.so.1.6.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
