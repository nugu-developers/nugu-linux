ADD_LIBRARY(dummy SHARED dummy.c)
TARGET_LINK_LIBRARIES(dummy -lnugu)
SET_TARGET_PROPERTIES(dummy PROPERTIES PREFIX "" OUTPUT_NAME dummy)
INSTALL(TARGETS dummy LIBRARY DESTINATION ${plugindir})
ADD_DEPENDENCIES(dummy libnugu)

ADD_LIBRARY(filedump SHARED filedump.c)
TARGET_LINK_LIBRARIES(filedump ${pkgs_LDFLAGS} -lnugu)
SET_TARGET_PROPERTIES(filedump PROPERTIES PREFIX "" OUTPUT_NAME filedump)
INSTALL(TARGETS filedump LIBRARY DESTINATION ${plugindir} COMPONENT libnugu)
ADD_DEPENDENCIES(filedump libnugu)

# OPUS plugin
IF(ENABLE_OPUS_PLUGIN)
	ADD_LIBRARY(opus SHARED opus.c)

	IF(ENABLE_BUILTIN_OPUS)
		TARGET_INCLUDE_DIRECTORIES(opus PRIVATE
			${PROJECT_BINARY_DIR}/opus/include/opus)
		TARGET_LINK_LIBRARIES(opus ${pkgs_LDFLAGS}
			${PROJECT_BINARY_DIR}/opus/lib/libopus.a
			-lnugu -lm)
		ADD_DEPENDENCIES(opus OPUSLIB)
	ELSE()
		pkg_check_modules(opus REQUIRED opus)
		TARGET_COMPILE_OPTIONS(opus PRIVATE ${opus_CFLAGS})
		TARGET_LINK_LIBRARIES(opus ${pkgs_LDFLAGS} ${opus_LDFLAGS} -lnugu)
	ENDIF()

	SET_TARGET_PROPERTIES(opus PROPERTIES PREFIX "" OUTPUT_NAME opus)
	INSTALL(TARGETS opus LIBRARY DESTINATION ${plugindir} COMPONENT libnugu)
	ADD_DEPENDENCIES(opus libnugu)
ENDIF(ENABLE_OPUS_PLUGIN)

# OPUS encoder plugin
IF(ENABLE_OPUSENC_PLUGIN)
	ADD_LIBRARY(opusenc SHARED opus_encoder.c)
	pkg_check_modules(opusenc REQUIRED opus ogg)
	TARGET_COMPILE_OPTIONS(opusenc PRIVATE ${opusenc_CFLAGS})
	TARGET_LINK_LIBRARIES(opusenc ${pkgs_LDFLAGS} ${opusenc_LDFLAGS} -lnugu)
	SET_TARGET_PROPERTIES(opusenc PROPERTIES PREFIX "" OUTPUT_NAME opusenc)
	INSTALL(TARGETS opusenc LIBRARY DESTINATION ${plugindir} COMPONENT libnugu)
	ADD_DEPENDENCIES(opusenc libnugu)
ENDIF(ENABLE_OPUSENC_PLUGIN)

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
		TARGET_LINK_LIBRARIES(${plugin} ${pkgs_LDFLAGS} ${portaudio_LDFLAGS} -lnugu)
		SET_TARGET_PROPERTIES(${plugin} PROPERTIES PREFIX "" OUTPUT_NAME ${plugin})
		INSTALL(TARGETS ${plugin} LIBRARY DESTINATION ${plugindir} COMPONENT libnugu)
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
		TARGET_LINK_LIBRARIES(${plugin}
			${pkgs_LDFLAGS}
			${gstreamer_pkgs_LDFLAGS}
			-lnugu)
		FOREACH(flag ${gstreamer_pkgs_CFLAGS})
			TARGET_COMPILE_OPTIONS(${plugin} PRIVATE ${flag})
		ENDFOREACH(flag)

		SET_TARGET_PROPERTIES(${plugin} PROPERTIES PREFIX "" OUTPUT_NAME ${plugin})
		INSTALL(TARGETS ${plugin} LIBRARY DESTINATION ${plugindir} COMPONENT libnugu)
		ADD_DEPENDENCIES(${plugin} libnugu)
	ENDFOREACH(plugin)
ENDIF(ENABLE_GSTREAMER_PLUGIN)

# Speex plugin
IF(ENABLE_SPEEX_PLUGIN AND ENABLE_VENDOR_LIBRARY)
	ADD_LIBRARY(speex SHARED speex.c)
	FOREACH(flag ${vendor_pkgs_CFLAGS})
		TARGET_COMPILE_OPTIONS(speex PRIVATE ${flag})
	ENDFOREACH(flag)
	TARGET_LINK_LIBRARIES(speex ${pkgs_LDFLAGS} ${vendor_pkgs_LDFLAGS} -lnugu)
	SET_TARGET_PROPERTIES(speex PROPERTIES PREFIX "" OUTPUT_NAME speex)
	INSTALL(TARGETS speex LIBRARY DESTINATION ${plugindir} COMPONENT libnugu)
	ADD_DEPENDENCIES(speex libnugu)
ENDIF()
