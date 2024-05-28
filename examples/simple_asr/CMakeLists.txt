ADD_EXECUTABLE(nugu_simple_asr main_asr.cc)
TARGET_LINK_LIBRARIES(nugu_simple_asr ${COMMON_LDFLAGS} libnugu)
ADD_DEPENDENCIES(nugu_simple_asr libnugu)
INSTALL(TARGETS nugu_simple_asr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
