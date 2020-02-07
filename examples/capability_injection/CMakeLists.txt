FILE(GLOB_RECURSE SRC *.cc)
ADD_EXECUTABLE(nugu_capability_injection ${SRC})
TARGET_LINK_LIBRARIES(nugu_capability_injection
	${pkgs_LDFLAGS}
	-L${CMAKE_BINARY_DIR}/src -lnugu)
ADD_DEPENDENCIES(nugu_capability_injection libnugu)
INSTALL(TARGETS nugu_capability_injection RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
