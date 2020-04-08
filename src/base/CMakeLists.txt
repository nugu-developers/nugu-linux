# HTTP/2 module using curl and nghttp2 libraries
ADD_LIBRARY(objhttp2 OBJECT
	network/http2/threadsync.c
	network/http2/multipart_parser.c
	network/http2/http2_request.c
	network/http2/http2_network.c
	network/http2/directives_parser.cc
	network/http2/v1_event.c
	network/http2/v1_event_attachment.c
	network/http2/v1_directives.c
	network/http2/v1_ping.c
	network/http2/v1_policies.cc
	network/http2/v2_events.c
	network/dg_registry.c
	network/dg_server.c)
TARGET_INCLUDE_DIRECTORIES(objhttp2 PRIVATE
	network
	${CMAKE_SOURCE_DIR}/externals/curl/include)
ADD_DEPENDENCIES(objhttp2 CURL)

# NUGU Base
FILE(GLOB SRCS *.c)

ADD_LIBRARY(objbase OBJECT ${SRCS})
TARGET_INCLUDE_DIRECTORIES(objbase PRIVATE
	${CMAKE_SOURCE_DIR}/externals/curl/include)
TARGET_COMPILE_DEFINITIONS(objbase PRIVATE
	# Only symbols that use EXPORT_API are visible, and all other symbols are hidden.
	-DEXPORT_API=__attribute__\(\(visibility\(\"default\"\)\)\)

	# Convenient macro to avoid unused parameter warnings.
	-D__UNUSED__=__attribute\(\(unused\)\))
TARGET_COMPILE_OPTIONS(objbase PRIVATE
	# Set default visibility to hidden to reduce symbol count
	-fvisibility=hidden

	# Turn on extra warnings
	-Wmissing-prototypes
	-Waggregate-return
	-Wmissing-declarations
	-Wswitch-default)
ADD_DEPENDENCIES(objbase CURL)
