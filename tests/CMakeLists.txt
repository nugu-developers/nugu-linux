# Test resources
ADD_LIBRARY(plugin_dummy.so SHARED plugin_dummy.c)
SET_TARGET_PROPERTIES(plugin_dummy.so PROPERTIES PREFIX "" OUTPUT_NAME plugin_dummy)

ADD_LIBRARY(plugin_nugu.so SHARED plugin_nugu.c)
SET_TARGET_PROPERTIES(plugin_nugu.so PROPERTIES PREFIX "" OUTPUT_NAME plugin_nugu)
ADD_DEPENDENCIES(plugin_nugu.so libnugu)

ADD_LIBRARY(plugin_nugu_custom.so SHARED plugin_nugu_custom.c)
SET_TARGET_PROPERTIES(plugin_nugu_custom.so PROPERTIES PREFIX "" OUTPUT_NAME plugin_nugu_custom)
ADD_DEPENDENCIES(plugin_nugu_custom.so libnugu)

# Unit tests
SET(UNIT_TESTS
	test_nugu_buffer
	test_nugu_plugin
	test_nugu_recorder
	test_nugu_decoder
	test_nugu_pcm
	test_nugu_player
	test_nugu_timer
	test_nugu_event
	test_nugu_uuid
	test_nugu_directive
	test_nugu_ringbuffer)

FOREACH(test ${UNIT_TESTS})
	ADD_EXECUTABLE(${test} ${test}.c)
	TARGET_COMPILE_DEFINITIONS(${test} PRIVATE
		-DRUNPATH="${CMAKE_CURRENT_BINARY_DIR}")
	TARGET_LINK_LIBRARIES(${test} ${pkgs_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu -lstdc++)
	ADD_DEPENDENCIES(${test} libnugu)
	ADD_TEST(${test} ${test})
	SET_PROPERTY(TEST ${test} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src")
ENDFOREACH(test)

ADD_SUBDIRECTORY(core)
