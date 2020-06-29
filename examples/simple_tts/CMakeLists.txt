ADD_EXECUTABLE(nugu_simple_tts main_tts.cc)
TARGET_LINK_LIBRARIES(nugu_simple_tts
	${pkgs_LDFLAGS}
	-L${CMAKE_BINARY_DIR}/src -lnugu)
ADD_DEPENDENCIES(nugu_simple_tts libnugu)
INSTALL(TARGETS nugu_simple_tts RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
