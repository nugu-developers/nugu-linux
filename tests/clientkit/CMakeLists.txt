SET(UNIT_TESTS
	test_clientkit_capability)

# Add Compile Sources with Mock for test
SET(test_clientkit_capability_srcs ../../src/clientkit/capability.cc)

FOREACH(test ${UNIT_TESTS})
	SET(SRC ${test}.cc)
	IF (${test}_srcs)
		LIST(APPEND SRC ${${test}_srcs})
	ENDIF ()
	ADD_EXECUTABLE(${test} ${SRC})
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
