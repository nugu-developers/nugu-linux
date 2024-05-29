ADD_LIBRARY(libpermission permission_agent.cc)
TARGET_LINK_LIBRARIES(libpermission ${pkgs_LDFLAGS} libnugu -lstdc++)
ADD_DEPENDENCIES(libpermission libnugu)

ADD_EXECUTABLE(nugu_capability_injection main_capability_injection.cc)
TARGET_LINK_LIBRARIES(nugu_capability_injection ${pkgs_LDFLAGS} libpermission -lstdc++)
ADD_DEPENDENCIES(nugu_capability_injection libpermission)
INSTALL(TARGETS nugu_capability_injection RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

IF (MSVC)
TARGET_COMPILE_DEFINITIONS(libpermission PRIVATE EXAMPLE_LIBRARY_BUILD=1)
SET_TARGET_PROPERTIES(libpermission PROPERTIES 
    COMPILE_PDB_NAME "libpermission"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
ENDIF()