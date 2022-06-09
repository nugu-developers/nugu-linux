ADD_EXECUTABLE(nugu_simple_asr main_asr.cc)
TARGET_LINK_LIBRARIES(nugu_simple_asr ${pkgs_LDFLAGS} -lnugu)
ADD_DEPENDENCIES(nugu_simple_asr libnugu)
INSTALL(TARGETS nugu_simple_asr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
