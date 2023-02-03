
ADD_SUBDIRECTORY(base)
ADD_SUBDIRECTORY(core)
ADD_SUBDIRECTORY(capability)
ADD_SUBDIRECTORY(clientkit)

# NUGU SDK Library
ADD_LIBRARY(libnugu SHARED
	$<TARGET_OBJECTS:objhttp2>
	$<TARGET_OBJECTS:objbase>
	$<TARGET_OBJECTS:objcore>
	$<TARGET_OBJECTS:objcapability>
	$<TARGET_OBJECTS:objclientkit>)

TARGET_LINK_LIBRARIES(libnugu PUBLIC
	${CURL_LIBRARY} ${JSONCPP_LIBRARY}
	${pkgs_LDFLAGS} ${LDFLAG_SOCKET} ${LDFLAG_DL})

# Gstreamer does not support dynamic loading/unloading of their libraries.
# Therefore, add the library dependency to NUGU SDK instead of NUGU Plugin.
FOREACH(flag ${gstreamer_pkgs_CFLAGS})
    TARGET_COMPILE_OPTIONS(libnugu PRIVATE ${flag})
ENDFOREACH(flag)

IF(CMAKE_COMPILER_IS_GNUCC)
    TARGET_LINK_LIBRARIES(libnugu PUBLIC -Wl,--no-as-needed ${gstreamer_pkgs_LDFLAGS})
ELSE()
    TARGET_LINK_LIBRARIES(libnugu PUBLIC ${gstreamer_pkgs_LDFLAGS})
ENDIF()

SET_TARGET_PROPERTIES(libnugu
	PROPERTIES VERSION ${VERSION} SOVERSION ${VERSION_MAJOR} OUTPUT_NAME nugu)

INSTALL(TARGETS libnugu LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	COMPONENT libnugu)
