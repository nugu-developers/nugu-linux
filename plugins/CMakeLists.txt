SET(PLUGINS
	filedump
	dummy)

FOREACH(plugin ${PLUGINS})
	ADD_LIBRARY(${plugin} SHARED ${plugin}.c)
	TARGET_LINK_LIBRARIES(${plugin} ${pkgs_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu -ldl)
	SET_TARGET_PROPERTIES(${plugin} PROPERTIES PREFIX "" OUTPUT_NAME ${plugin})
	INSTALL(TARGETS ${plugin} LIBRARY DESTINATION ${plugindir})
	ADD_DEPENDENCIES(${plugin} libnugu)
ENDFOREACH(plugin)

# OPUS plugin
pkg_check_modules(opus REQUIRED opus)
ADD_LIBRARY(opus SHARED opus.c)
TARGET_COMPILE_OPTIONS(opus PRIVATE ${opus_CFLAGS})
TARGET_LINK_LIBRARIES(opus ${pkgs_LDFLAGS} ${opus_LDFLAGS}
	-L${CMAKE_BINARY_DIR}/src -lnugu -ldl)
SET_TARGET_PROPERTIES(opus PROPERTIES PREFIX "" OUTPUT_NAME opus)
INSTALL(TARGETS opus LIBRARY DESTINATION ${plugindir})
ADD_DEPENDENCIES(opus libnugu)

# portaudio plugin
pkg_check_modules(portaudio REQUIRED portaudio-2.0)
ADD_LIBRARY(portaudio SHARED portaudio.c)
TARGET_COMPILE_OPTIONS(portaudio PRIVATE ${portaudio_CFLAGS})
TARGET_LINK_LIBRARIES(portaudio ${pkgs_LDFLAGS} ${portaudio_LDFLAGS}
	-L${CMAKE_BINARY_DIR}/src -lnugu -ldl)
SET_TARGET_PROPERTIES(portaudio PROPERTIES PREFIX "" OUTPUT_NAME portaudio)
INSTALL(TARGETS portaudio LIBRARY DESTINATION ${plugindir})
ADD_DEPENDENCIES(portaudio libnugu)

# gstreamer plugin
pkg_check_modules(gstreamer REQUIRED gstreamer-1.0 gstreamer-pbutils-1.0)
ADD_LIBRARY(gstreamer SHARED gstreamer.c)
TARGET_COMPILE_OPTIONS(gstreamer PRIVATE ${gstreamer_CFLAGS})
TARGET_LINK_LIBRARIES(gstreamer ${pkgs_LDFLAGS} ${gstreamer_LDFLAGS}
	-L${CMAKE_BINARY_DIR}/src -lnugu -ldl)
SET_TARGET_PROPERTIES(gstreamer PROPERTIES PREFIX "" OUTPUT_NAME gstreamer)
INSTALL(TARGETS gstreamer LIBRARY DESTINATION ${plugindir})
ADD_DEPENDENCIES(gstreamer libnugu)
