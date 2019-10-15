SET(UNIT_TESTS
	test-nugu-buffer
	test-nugu-plugin
	test-nugu-recorder
	test-nugu-decoder
	test-nugu-pcm
	test-nugu-player
	test-nugu-timer
	test-nugu-event
	test-nugu-directive
	test-nugu-ringbuffer)

FOREACH(test ${UNIT_TESTS})
	ADD_EXECUTABLE(${test} ${test}.c)
	TARGET_LINK_LIBRARIES(${test} ${pkgs_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu)
	ADD_DEPENDENCIES(${test} libnugu)
	ADD_TEST(${test} ${test})
	SET_PROPERTY(TEST ${test} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src")
ENDFOREACH(test)
