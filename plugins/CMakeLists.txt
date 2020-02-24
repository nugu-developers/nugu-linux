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
IF(ENABLE_OPUS_PLUGIN)
	pkg_check_modules(opus REQUIRED opus)
	ADD_LIBRARY(opus SHARED opus.c)
	TARGET_COMPILE_OPTIONS(opus PRIVATE ${opus_CFLAGS})
	TARGET_LINK_LIBRARIES(opus ${pkgs_LDFLAGS} ${opus_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu -ldl)
	SET_TARGET_PROPERTIES(opus PROPERTIES PREFIX "" OUTPUT_NAME opus)
	INSTALL(TARGETS opus LIBRARY DESTINATION ${plugindir})
	ADD_DEPENDENCIES(opus libnugu)
ENDIF(ENABLE_OPUS_PLUGIN)

# PortAudio plugin
IF(ENABLE_PORTAUDIO_PLUGIN)
	pkg_check_modules(portaudio REQUIRED portaudio-2.0 alsa)
	ADD_LIBRARY(portaudio SHARED portaudio.c)
	TARGET_COMPILE_OPTIONS(portaudio PRIVATE ${portaudio_CFLAGS})
	TARGET_LINK_LIBRARIES(portaudio ${pkgs_LDFLAGS} ${portaudio_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu -ldl)
	SET_TARGET_PROPERTIES(portaudio PROPERTIES PREFIX "" OUTPUT_NAME portaudio)
	INSTALL(TARGETS portaudio LIBRARY DESTINATION ${plugindir})
	ADD_DEPENDENCIES(portaudio libnugu)
ENDIF(ENABLE_PORTAUDIO_PLUGIN)

# gstreamer plugin
IF(ENABLE_GSTREAMER_PLUGIN)
	ADD_LIBRARY(gstreamer SHARED gstreamer.c)
	TARGET_LINK_LIBRARIES(gstreamer ${pkgs_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu -ldl)
	SET_TARGET_PROPERTIES(gstreamer PROPERTIES PREFIX "" OUTPUT_NAME gstreamer)
	INSTALL(TARGETS gstreamer LIBRARY DESTINATION ${plugindir})
	ADD_DEPENDENCIES(gstreamer libnugu)
ENDIF(ENABLE_GSTREAMER_PLUGIN)
