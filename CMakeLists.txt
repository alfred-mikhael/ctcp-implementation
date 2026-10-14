check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/modular-transport-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME ctcpTest
    SOURCE_FILES MTCTCP.cc
                 CTCPTXApp.cc
                 CTCPRXApp.cc
                 CTCPTXNet.cc
                 CTCPRXNet.cc
                 CTCPDispatcher.cc
                 CTCPEventProcs.cc
                 CTCPHeaders.cc
                 gf256.cc
    HEADER_FILES MTCTCP.h
                 CTCPTXApp.h
                 CTCPRXApp.h
                 CTCPTXNet.h
                 CTCPRXNet.h
                 CTCPEvents.h
                 CTCPInstructions.h
                 CTCPEventProcs.h
                 CTCPDispatcher.h
                 CTCPContext.h
                 CTCPStructs.h
                 CTCPContext.h

                
    LIBRARIES_TO_LINK ${libinternet} ${libmodular-transport}
    TEST_SOURCES ${examples_as_tests_sources}
)
