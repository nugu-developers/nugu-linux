ADD_EXECUTABLE(nugu_response_filter main.cc filter.cc)
TARGET_LINK_LIBRARIES(nugu_response_filter ${pkgs_LDFLAGS} -lnugu -lstdc++)
ADD_DEPENDENCIES(nugu_response_filter libnugu)
INSTALL(TARGETS nugu_response_filter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
