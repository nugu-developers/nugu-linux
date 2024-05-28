ADD_EXECUTABLE(nugu_simple_text main_text.cc)
TARGET_LINK_LIBRARIES(nugu_simple_text ${COMMON_LDFLAGS} libnugu)
ADD_DEPENDENCIES(nugu_simple_text libnugu)
INSTALL(TARGETS nugu_simple_text RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
