ADD_LIBRARY(libfilter filter.cc)
TARGET_LINK_LIBRARIES(libfilter ${pkgs_LDFLAGS} -lstdc++)

ADD_EXECUTABLE(nugu_response_filter main.cc)
TARGET_LINK_LIBRARIES(nugu_response_filter ${pkgs_LDFLAGS} libnugu libfilter -lstdc++)
ADD_DEPENDENCIES(nugu_response_filter libnugu libfilter)
INSTALL(TARGETS nugu_response_filter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

IF (MSVC)
SET_TARGET_PROPERTIES(libfilter PROPERTIES 
    COMPILE_PDB_NAME "libfilter"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
ENDIF()