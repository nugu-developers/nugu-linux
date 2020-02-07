ADD_EXECUTABLE(nugu_simple_text main_text.cc)
TARGET_LINK_LIBRARIES(nugu_simple_text
	${pkgs_LDFLAGS}
	-L${CMAKE_BINARY_DIR}/src -lnugu)
ADD_DEPENDENCIES(nugu_simple_text libnugu)
INSTALL(TARGETS nugu_simple_text RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
