# Test resources
ADD_LIBRARY(plugin_dummy SHARED plugin_dummy.c)
SET_TARGET_PROPERTIES(plugin_dummy PROPERTIES PREFIX "" OUTPUT_NAME plugin_dummy)
IF(CMAKE_SYSTEM_NAME MATCHES "MINGW" OR CMAKE_SYSTEM_NAME MATCHES "Windows")
SET_TARGET_PROPERTIES(plugin_dummy PROPERTIES SUFFIX ".dll")
ENDIF()

ADD_LIBRARY(plugin_nugu SHARED plugin_nugu.c)
SET_TARGET_PROPERTIES(plugin_nugu PROPERTIES PREFIX "" OUTPUT_NAME plugin_nugu)
ADD_DEPENDENCIES(plugin_nugu libnugu)

ADD_LIBRARY(plugin_nugu_custom SHARED plugin_nugu_custom.c)
SET_TARGET_PROPERTIES(plugin_nugu_custom PROPERTIES PREFIX "" OUTPUT_NAME plugin_nugu_custom)
ADD_DEPENDENCIES(plugin_nugu_custom libnugu)

# Unit tests
SET(UNIT_TESTS
	test_nugu_audio
	test_nugu_buffer
	test_nugu_plugin
	test_nugu_recorder
	test_nugu_decoder
	test_nugu_encoder
	test_nugu_pcm
	test_nugu_player
	test_nugu_timer
	test_nugu_event
	test_nugu_uuid
	test_nugu_directive
	test_nugu_http
	test_nugu_ringbuffer)

FOREACH(test ${UNIT_TESTS})
	ADD_EXECUTABLE(${test} ${test}.c)
	TARGET_COMPILE_DEFINITIONS(${test} PRIVATE
		-DRUNPATH="${CMAKE_CURRENT_BINARY_DIR}")
	TARGET_LINK_LIBRARIES(${test} ${pkgs_LDFLAGS} libnugu ${LDFLAGS_XXX})
	ADD_DEPENDENCIES(${test} libnugu)
	ADD_TEST(${test} ${test})
	SET_PROPERTY(TEST ${test} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${PROJECT_BINARY_DIR}/src")
ENDFOREACH(test)

ADD_SUBDIRECTORY(core)
ADD_SUBDIRECTORY(clientkit)
