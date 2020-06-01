SET(UNIT_TESTS test-core-nugu-timer)

# Add Compile Sources with Mock for test
SET(test-core-nugu-timer-srcs test-core-nugu-timer-mock.cc
								../../src/core/nugu_timer.cc)

FOREACH(test ${UNIT_TESTS})
	SET(SRC ${test}.cc)
	IF (${test}-srcs)
		LIST(APPEND SRC ${${test}-srcs})
	ENDIF ()
	ADD_EXECUTABLE(${test} ${SRC})
	TARGET_COMPILE_DEFINITIONS(${test} PRIVATE
		-DRUNPATH="${CMAKE_CURRENT_BINARY_DIR}")
	TARGET_INCLUDE_DIRECTORIES(${test} PRIVATE
		../../src/core
		../../include)
	TARGET_LINK_LIBRARIES(${test} ${pkgs_LDFLAGS})
	ADD_TEST(${test} ${test})
	SET_PROPERTY(TEST ${test} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src")
ENDFOREACH(test)
