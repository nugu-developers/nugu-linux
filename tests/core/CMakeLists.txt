SET(UNIT_TESTS
	test_core_nugu_timer
	test_core_focus_manager
	test_core_directive_sequencer
	test_core_session_manager
	test_core_media_player
	test_core_playstack_manager
	test_core_playsync_manager
	test_core_interaction_control_manager
	test_core_routine_manager)

# Add Compile Sources with Mock for test
SET(test_core_nugu_timer_srcs ../mock/nugu_timer_mock.c)
SET(test_core_media_player_srcs
	../mock/nugu_timer_mock.c
	../mock/media_player_mock.c
	../../src/base/nugu_player.c
	../../src/core/nugu_timer.cc
	../../src/core/media_player.cc)

FOREACH(test ${UNIT_TESTS})
	SET(SRC ${test}.cc)
	IF (${test}_srcs)
		LIST(APPEND SRC ${${test}_srcs})
	ENDIF ()
	ADD_EXECUTABLE(${test} ${SRC})
	TARGET_INCLUDE_DIRECTORIES(${test} PRIVATE
		../../src/core
		../../include
		..)
	TARGET_LINK_LIBRARIES(${test} ${COMMON_LDFLAGS} libnugu)
	ADD_DEPENDENCIES(${test} libnugu)
	ADD_TEST(${test} ${test})
	SET_PROPERTY(TEST ${test} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${PROJECT_BINARY_DIR}/src")
ENDFOREACH(test)
