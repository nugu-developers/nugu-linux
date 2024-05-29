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

# doesn't work timer mock in msvc cause of PDB issue.
IF(MSVC)
	LIST(REMOVE_ITEM UNIT_TESTS
		"test_core_nugu_timer"
		"test_core_media_player")
ENDIF()

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
		ADD_LIBRARY(${test}_mock ${${test}_srcs})
		SET_TARGET_PROPERTIES(${test}_mock PROPERTIES
			COMPILE_PDB_NAME "${${test}_mock}"
			COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		)
		TARGET_INCLUDE_DIRECTORIES(${test}_mock PRIVATE ..)
	ENDIF ()
	ADD_EXECUTABLE(${test} ${SRC})
	TARGET_COMPILE_DEFINITIONS(${test} PRIVATE
		-DRUNPATH="${CMAKE_CURRENT_BINARY_DIR}")
	TARGET_INCLUDE_DIRECTORIES(${test} PRIVATE
		../../src/core
		../../include
		..)
	IF (TARGET ${test}_mock)
		TARGET_LINK_LIBRARIES(${test} libnugu ${test}_mock -lstdc++)
		ADD_DEPENDENCIES(${test} libnugu ${test}_mock)
	ELSE ()
		TARGET_LINK_LIBRARIES(${test} ${pkgs_LDFLAGS} libnugu -lstdc++)
		ADD_DEPENDENCIES(${test} libnugu)
	ENDIF ()
	ADD_TEST(${test} ${test})
	SET_PROPERTY(TEST ${test} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${PROJECT_BINARY_DIR}/src")
ENDFOREACH(test)
