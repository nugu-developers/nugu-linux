ADD_EXECUTABLE(nugu_simple_play main_play.cc)
TARGET_LINK_LIBRARIES(nugu_simple_play ${pkgs_LDFLAGS} -lnugu)
ADD_DEPENDENCIES(nugu_simple_play libnugu)
INSTALL(TARGETS nugu_simple_play RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
