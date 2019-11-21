# HTTP/2 module using curl and nghttp2 libraries
ADD_LIBRARY(objhttp2 OBJECT
	network/http2/multipart_parser.c
	network/http2/http2_request.c
	network/http2/http2_network.c
	network/http2/v1_event.c
	network/http2/v1_event_attachment.c
	network/http2/v1_directives.cc
	network/http2/v1_ping.c
	network/http2/gateway_registry.cc
	network/http2/http2_manage.c)
TARGET_INCLUDE_DIRECTORIES(objhttp2 PRIVATE
	${CMAKE_BINARY_DIR}/nghttp2/include
	${CMAKE_BINARY_DIR}/curl/include)
ADD_DEPENDENCIES(objhttp2 CURL)

# nugu library
FILE(GLOB SRCS *.c)

ADD_LIBRARY(libnugu SHARED
	$<TARGET_OBJECTS:objservice>
	$<TARGET_OBJECTS:objhttp2>
	$<TARGET_OBJECTS:objjsoncpp>
	$<TARGET_OBJECTS:objinterface>
	${SRCS})
TARGET_LINK_LIBRARIES(libnugu PUBLIC
	${CMAKE_BINARY_DIR}/curl/lib/libcurl.a
	${CMAKE_BINARY_DIR}/nghttp2/lib/libnghttp2.a
	${pkgs_LDFLAGS} -ldl)
TARGET_INCLUDE_DIRECTORIES(libnugu PRIVATE
	network/http2
	${CMAKE_BINARY_DIR}/curl/include)

TARGET_COMPILE_DEFINITIONS(libnugu PRIVATE
	# Only symbols that use EXPORT_API are visible, and all other symbols are hidden.
	-DEXPORT_API=__attribute__\(\(visibility\(\"default\"\)\)\)

	# Convenient macro to avoid unused parameter warnings.
	-D__UNUSED__=__attribute\(\(unused\)\))

TARGET_COMPILE_OPTIONS(libnugu PRIVATE
	# Set default visibility to hidden to reduce symbol count
	-fvisibility=hidden

	# Turn on extra warnings
	-Wmissing-prototypes
	-Waggregate-return
	-Wmissing-declarations
	-Wswitch-default)

SET_TARGET_PROPERTIES(libnugu
	PROPERTIES VERSION ${VERSION} SOVERSION ${VERSION_MAJOR} OUTPUT_NAME nugu)

INSTALL(TARGETS libnugu LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
