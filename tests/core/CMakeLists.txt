SET(UNIT_TESTS
	test_core_nugu_timer
	test_core_focus_manager
	test_core_directive_sequencer
	test_core_session_manager
	test_core_playstack_manager
	test_core_interaction_control_manager)

# Add Compile Sources with Mock for test
SET(test_core_nugu_timer_srcs
	test_core_nugu_timer_mock.cc ../../src/core/nugu_timer.cc)
SET(test_core_focus_manager_srcs ../../src/core/focus_manager.cc)
SET(test_core_directive_sequencer_srcs ../../src/core/directive_sequencer.cc)
SET(test_core_session_manager_srcs ../../src/core/session_manager.cc)
SET(test_core_playstack_manager_srcs ../../src/core/playstack_manager.cc)
SET(test_core_interaction_control_manager_srcs ../../src/core/interaction_control_manager.cc)

FOREACH(test ${UNIT_TESTS})
	SET(SRC ${test}.cc)
	IF (${test}_srcs)
		LIST(APPEND SRC ${${test}_srcs})
	ENDIF ()
	ADD_EXECUTABLE(${test} ${SRC})
	TARGET_COMPILE_DEFINITIONS(${test} PRIVATE
		-DRUNPATH="${CMAKE_CURRENT_BINARY_DIR}")
	TARGET_INCLUDE_DIRECTORIES(${test} PRIVATE
		../../src/core
		../../include)
	TARGET_LINK_LIBRARIES(${test} ${pkgs_LDFLAGS}
		-L${CMAKE_BINARY_DIR}/src -lnugu -lstdc++)
	ADD_DEPENDENCIES(${test} libnugu)
	ADD_TEST(${test} ${test})
	SET_PROPERTY(TEST ${test} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src")
ENDFOREACH(test)
