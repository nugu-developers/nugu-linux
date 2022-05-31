SET(UNIT_TESTS
	test_clientkit_nugu_runner
	test_clientkit_http_rest
	test_clientkit_auth
	test_clientkit_capability
	test_clientkit_dialog_ux_state_aggregator
	test_clientkit_speech_recognizer_aggregator)

FOREACH(test ${UNIT_TESTS})
	ADD_EXECUTABLE(${test} ${test}.cc)
	TARGET_COMPILE_DEFINITIONS(${test} PRIVATE
		-DRUNPATH="${CMAKE_CURRENT_BINARY_DIR}")
	TARGET_INCLUDE_DIRECTORIES(${test} PRIVATE
		../../src/clientkit
		../../include)
	TARGET_LINK_LIBRARIES(${test} ${pkgs_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu -lstdc++)
	ADD_DEPENDENCIES(${test} libnugu)
	ADD_TEST(${test} ${test})
	SET_PROPERTY(TEST ${test} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src")
ENDFOREACH(test)
