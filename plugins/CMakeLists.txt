SET(PLUGINS
	filedump
	dummy)

FOREACH(plugin ${PLUGINS})
	ADD_LIBRARY(${plugin} SHARED ${plugin}.c)
	TARGET_LINK_LIBRARIES(${plugin} ${pkgs_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu)
	SET_TARGET_PROPERTIES(${plugin} PROPERTIES PREFIX "" OUTPUT_NAME ${plugin})
	INSTALL(TARGETS ${plugin} LIBRARY DESTINATION ${plugindir})
	ADD_DEPENDENCIES(${plugin} libnugu)
ENDFOREACH(plugin)

# OPUS plugin
IF(ENABLE_OPUS_PLUGIN)
	ADD_LIBRARY(opus SHARED opus.c)

	IF(ENABLE_OPUS_LIBRARY)
		TARGET_INCLUDE_DIRECTORIES(opus PRIVATE
			${CMAKE_BINARY_DIR}/opus/include/opus)
		TARGET_LINK_LIBRARIES(opus ${pkgs_LDFLAGS}
			${CMAKE_BINARY_DIR}/opus/lib/libopus.a
			-L${CMAKE_BINARY_DIR}/src -lnugu -lm)
		ADD_DEPENDENCIES(opus OPUSLIB)
	ELSE()
		pkg_check_modules(opus REQUIRED opus)
		TARGET_COMPILE_OPTIONS(opus PRIVATE ${opus_CFLAGS})
		TARGET_LINK_LIBRARIES(opus ${pkgs_LDFLAGS} ${opus_LDFLAGS}
			-L${CMAKE_BINARY_DIR}/src -lnugu)
	ENDIF()

	SET_TARGET_PROPERTIES(opus PROPERTIES PREFIX "" OUTPUT_NAME opus)
	INSTALL(TARGETS opus LIBRARY DESTINATION ${plugindir})
	ADD_DEPENDENCIES(opus libnugu)
ENDIF(ENABLE_OPUS_PLUGIN)

# PortAudio plugin
IF(ENABLE_PORTAUDIO_PLUGIN)
	pkg_check_modules(portaudio REQUIRED portaudio-2.0 alsa)
	SET(PORTAUDIO_PLUGINS
		portaudio
		portaudio_recorder
		portaudio_pcm_sync
		portaudio_pcm_async)

	FOREACH(plugin ${PORTAUDIO_PLUGINS})
		ADD_LIBRARY(${plugin} SHARED ${plugin}.c)
		TARGET_COMPILE_OPTIONS(${plugin} PRIVATE ${portaudio_CFLAGS})
		TARGET_LINK_LIBRARIES(${plugin} ${pkgs_LDFLAGS} ${portaudio_LDFLAGS}
			-L${CMAKE_BINARY_DIR}/src -lnugu)
		SET_TARGET_PROPERTIES(${plugin} PROPERTIES PREFIX "" OUTPUT_NAME ${plugin})
		INSTALL(TARGETS ${plugin} LIBRARY DESTINATION ${plugindir})
		ADD_DEPENDENCIES(${plugin} libnugu)
	ENDFOREACH(plugin)
ENDIF(ENABLE_PORTAUDIO_PLUGIN)

# gstreamer plugin
IF(ENABLE_GSTREAMER_PLUGIN)
	SET(GSTREAMER_PLUGINS
		gstreamer
		gstreamer_recorder
		gstreamer_pcm)

	FOREACH(plugin ${GSTREAMER_PLUGINS})
		ADD_LIBRARY(${plugin} SHARED ${plugin}.c)
		TARGET_LINK_LIBRARIES(${plugin} ${pkgs_LDFLAGS}
			-L${CMAKE_BINARY_DIR}/src -lnugu)
		SET_TARGET_PROPERTIES(${plugin} PROPERTIES PREFIX "" OUTPUT_NAME ${plugin})
		INSTALL(TARGETS ${plugin} LIBRARY DESTINATION ${plugindir})
		ADD_DEPENDENCIES(${plugin} libnugu)
	ENDFOREACH(plugin)
ENDIF(ENABLE_GSTREAMER_PLUGIN)

# Speex plugin
IF(ENABLE_SPEEX_PLUGIN AND ENABLE_VENDOR_LIBRARY)
	ADD_LIBRARY(speex SHARED speex.c)
	TARGET_LINK_LIBRARIES(speex ${pkgs_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu)

	SET_TARGET_PROPERTIES(speex PROPERTIES PREFIX "" OUTPUT_NAME speex)
	INSTALL(TARGETS speex LIBRARY DESTINATION ${plugindir})
	ADD_DEPENDENCIES(speex libnugu)
ENDIF()
