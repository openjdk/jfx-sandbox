# WebKit dropped MSVC support. This file is only for clang-cl.

function(MSVC_ADD_COMPILE_OPTIONS)
    foreach (_option ${ARGV})
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:${_option}>)
    endforeach ()
endfunction()

if (WIN32)
    if (${WTF_CPU_X86})
        set(CMAKE_SHARED_LINKER_FLAGS "/machine:X86")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /machine:X86")
    endif ()
endif ()

# Create pdb files for debugging purposes, also for Release builds
MSVC_ADD_COMPILE_OPTIONS(/Zi /GS)

if (PORT STREQUAL "Java")
# Use AT&T syntax for inline asm work around for assembly generation compatible to clang-cl
MSVC_ADD_COMPILE_OPTIONS(/clang:-masm=att)
if (PORT STREQUAL "Java")
    # Suppress creation of pdb files for Release builds
    # FIXME: Need to re-enable the flag for Debug builds
    #add_compile_options(/Zi /GS)

else()
    # Create pdb files for debugging purposes, also for Release builds
    add_compile_options(/Zi /GS)
endif()
endif()

# Disable ICF (identical code folding) optimization,
# as it makes it unsafe to pointer-compare functions with identical definitions.
add_link_options(/DEBUG /OPT:NOICF /OPT:REF)

# We do not use exceptions
add_definitions(-D_HAS_EXCEPTIONS=0)
string(REGEX REPLACE "/EH[-a-z]+" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
MSVC_ADD_COMPILE_OPTIONS(/EHa- /EHc- /EHs- /fp:except-)

# Disable RTTI
string(REGEX REPLACE "/GR-?" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
MSVC_ADD_COMPILE_OPTIONS(/GR-)

# We have some very large object files that have to be linked
MSVC_ADD_COMPILE_OPTIONS(/analyze- /bigobj)

# Use CRT security features
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE)

# Specify the source code encoding
MSVC_ADD_COMPILE_OPTIONS(/utf-8 /validate-charset)

if (NOT ${CMAKE_GENERATOR} MATCHES "Ninja")
    MSVC_ADD_COMPILE_OPTIONS(/MP)
endif ()

# More warnings. /W4 should be specified before -Wno-* options for clang-cl.
string(REGEX REPLACE "/W3" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
WEBKIT_PREPEND_GLOBAL_COMPILER_FLAGS(/W4)

# Make sure incremental linking is turned off, as it creates unacceptably long link times.
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_EXE_LINKER_FLAGS_DEBUG ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
string(REGEX REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_SHARED_LINKER_FLAGS_DEBUG ${CMAKE_SHARED_LINKER_FLAGS_DEBUG})
string(REPLACE "/INCREMENTAL[:A-Z]+" "" CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})
add_link_options(/INCREMENTAL:NO)
if (PORT STREQUAL "Java")
# Link clang runtime builtins library
#string(REGEX MATCH "^[0-9]+" CLANG_CL_MAJOR_VERSION ${CMAKE_CXX_COMPILER_VERSION})
#cmake_path(REMOVE_FILENAME CMAKE_CXX_COMPILER OUTPUT_VARIABLE CLANG_CL_DIR)
#cmake_path(APPEND CLANG_CL_DIR "../lib/clang" ${CLANG_CL_MAJOR_VERSION} "lib/windows")
#find_library(CLANG_BUILTINS_LIBRARY clang_rt.builtins-x86_64 PATHS ${CLANG_CL_DIR} REQUIRED NO_DEFAULT_PATH)
#link_libraries(${CLANG_BUILTINS_LIBRARY})
endif()
set(HAVE_INT128_T OFF)
list(REMOVE_ITEM _WEBKIT_CONFIG_FILE_VARIABLES HAVE_INT128_T)
link_libraries(kernel32.lib bcrypt.lib ws2_32)
