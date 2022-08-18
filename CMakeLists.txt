CMAKE_MINIMUM_REQUIRED(VERSION 3.9)
PROJECT(nugu)

INCLUDE(GNUInstallDirs)
INCLUDE(FindPkgConfig)
INCLUDE(FeatureSummary)
INCLUDE(CMakePushCheckState)
INCLUDE(CheckSymbolExists)

SET(VERSION_MAJOR 1)
SET(VERSION_MINOR 7)
SET(VERSION_PATCH 2)
SET(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

# OpenSDK/{sdk-version} ({platform}) Client/{app-version} [additional-info]
SET(NUGU_USERAGENT_FORMAT "OpenSDK/${VERSION} (Linux) Client/%s %s")

SET(NUGU_REGISTRY_URL "https://reg-http.sktnugu.com")

SET(version ${VERSION})
SET(prefix ${CMAKE_INSTALL_PREFIX})
SET(datadir ${CMAKE_INSTALL_FULL_DATAROOTDIR}/nugu)
SET(bindir "${prefix}/${CMAKE_INSTALL_BINDIR}")
SET(plugindir "${CMAKE_INSTALL_FULL_LIBDIR}/nugu")

# Generate compile_commands.json
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ standard: c++11 (not gnu++11)
SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_EXTENSIONS OFF)

# C standard: gnu99 (not c99)
SET(CMAKE_C_STANDARD 99)
SET(CMAKE_C_EXTENSIONS ON)

# build type: debug (default)
IF (NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Debug)
ENDIF()

# Macro for feature definition (Get all options: cmake -LAH)
MACRO(DEFINE_FEATURE id default description)
	OPTION(ENABLE_${id} "Enable - ${description} (Default: ${default})" ${default})
	add_feature_info(FEATURE_${id} ENABLE_${id} "${description} (Default: ${default})")
ENDMACRO(DEFINE_FEATURE)

IF (PACKAGING)
	SET(ASAN_DEFAULT "OFF")
	SET(LTO_DEFAULT "ON")
ELSE()
	IF ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		SET(ASAN_DEFAULT "ON")
		SET(LTO_DEFAULT "OFF")
	ELSE()
		SET(ASAN_DEFAULT "OFF")
		SET(LTO_DEFAULT "ON")
	ENDIF()
ENDIF()

# Set default build options
DEFINE_FEATURE(LOG_ANSICOLOR ON "ANSI Color to log message")
DEFINE_FEATURE(LTO ${LTO_DEFAULT} "Link Time Optimization")
DEFINE_FEATURE(ASAN ${ASAN_DEFAULT} "Address Sanitizer")
DEFINE_FEATURE(VENDOR_LIBRARY ON "vendor specific library(nugu_wwd, nugu_epd)")
DEFINE_FEATURE(OPUS_LIBRARY OFF "compile built-in OPUS library")
DEFINE_FEATURE(OPUS_LIBRARY_FLOAT_API OFF "floating point to OPUS library")
DEFINE_FEATURE(BUILTIN_CURL ON "compile built-in curl library")

DEFINE_FEATURE(DL_LINKING ON "-ldl linking")
DEFINE_FEATURE(SOCKET_LINKING OFF "-lsocket linking")
DEFINE_FEATURE(EVENTFD ON "eventfd")
DEFINE_FEATURE(PULSEAUDIO ON "pulseaudio")

DEFINE_FEATURE(GSTREAMER_PLUGIN ON "GStreamer plugin")
DEFINE_FEATURE(OPUS_PLUGIN ON "OPUS plugin")
DEFINE_FEATURE(SPEEX_PLUGIN ON "SPEEX plugin")
DEFINE_FEATURE(PORTAUDIO_PLUGIN ON "PortAudio plugin")
DEFINE_FEATURE(OPUSENC_PLUGIN OFF "OPUS encoder plugin")

DEFINE_FEATURE(EXAMPLES_OOB_SETUP ON "Examples - OOB setup")
DEFINE_FEATURE(EXAMPLES_STANDALONE ON "Examples - standalone")
DEFINE_FEATURE(EXAMPLES_SIMPLE_TEXT ON "Examples - simple-text")
DEFINE_FEATURE(EXAMPLES_SIMPLE_ASR ON "Examples - simple-asr")
DEFINE_FEATURE(EXAMPLES_SIMPLE_PLAY ON "Examples - simple-play")
DEFINE_FEATURE(EXAMPLES_SIMPLE_TTS ON "Examples - simple-tts")
DEFINE_FEATURE(EXAMPLES_PROFILING ON "Examples - profiling")
DEFINE_FEATURE(EXAMPLES_CAP_INJECTION ON "Examples - capability-injection")
DEFINE_FEATURE(EXAMPLES_RESP_FILTER ON "Examples - response-filter")

# API check
CMAKE_PUSH_CHECK_STATE(RESET)
SET(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
SET(CMAKE_REQUIRED_LIBRARIES "pthread")
CHECK_SYMBOL_EXISTS(pthread_setname_np "pthread.h" HAVE_PTHREAD_SETNAME_NP)
IF(ENABLE_EVENTFD)
	CHECK_SYMBOL_EXISTS(eventfd "sys/eventfd.h" HAVE_EVENTFD)
ENDIF()
CHECK_SYMBOL_EXISTS(syscall "unistd.h" HAVE_SYSCALL)
CMAKE_POP_CHECK_STATE()

IF(HAVE_PTHREAD_SETNAME_NP)
	ADD_DEFINITIONS(-DHAVE_PTHREAD_SETNAME_NP)
ENDIF()
IF(HAVE_EVENTFD)
	ADD_DEFINITIONS(-DHAVE_EVENTFD)
ENDIF()
IF(HAVE_SYSCALL)
	ADD_DEFINITIONS(-DHAVE_SYSCALL)
ENDIF()

IF(ENABLE_BUILTIN_CURL)
	SET(CURL_LIBRARY
		${CMAKE_BINARY_DIR}/curl/lib/libcurl.a
		${CMAKE_BINARY_DIR}/nghttp2/lib/libnghttp2.a)
	SET(CURL_PKGCONFIG)
	SET(CURL_INCLUDE ${CMAKE_SOURCE_DIR}/externals/curl/include)
ELSE()
	SET(CURL_LIBRARY)
	SET(CURL_PKGCONFIG libcurl)
	SET(CURL_INCLUDE)
ENDIF()

# Set NUGU SDK library default dependency
SET(DEFAULT_LIB_DEPENDENCY
	glib-2.0 gio-2.0 gio-unix-2.0 gthread-2.0
	openssl
	zlib ${CURL_PKGCONFIG})
SET(FORCE_LIB_DEPENDENCY)

# Add vendor specific library dependency (keyword detector, end-point detector)
IF(ENABLE_VENDOR_LIBRARY)
	ADD_DEFINITIONS(-DENABLE_VENDOR_LIBRARY)
	LIST(APPEND DEFAULT_LIB_DEPENDENCY nugu-kwd nugu-epd)
ENDIF()

# Gstreamer does not support dynamic loading/unloading of their libraries.
# Therefore, add the library dependency to NUGU SDK instead of NUGU Plugin.
IF(ENABLE_GSTREAMER_PLUGIN)
	LIST(APPEND FORCE_LIB_DEPENDENCY gstreamer-1.0 gstreamer-app-1.0 gstreamer-pbutils-1.0 gstreamer-app-1.0)
ENDIF()

IF(ENABLE_PULSEAUDIO)
	ADD_DEFINITIONS(-DENABLE_PULSEAUDIO)
ENDIF()

IF(ENABLE_DL_LINKING)
	SET(LDFLAG_DL "-ldl")
ELSE()
	SET(LDFLAG_DL "")
ENDIF()

IF(ENABLE_SOCKET_LINKING)
	SET(LDFLAG_SOCKET "-lsocket")
ELSE()
	SET(LDFLAG_SOCKET "")
ENDIF()

# Get CFLAGS and LDFLAGS from pkg-config list
pkg_check_modules(pkgs REQUIRED ${DEFAULT_LIB_DEPENDENCY})
FOREACH(flag ${pkgs_CFLAGS})
	ADD_COMPILE_OPTIONS(${flag})
ENDFOREACH()

IF(FORCE_LIB_DEPENDENCY)
	pkg_check_modules(force_pkgs REQUIRED ${FORCE_LIB_DEPENDENCY})
	FOREACH(flag ${force_pkgs_CFLAGS})
		ADD_COMPILE_OPTIONS(${flag})
	ENDFOREACH(flag)
ENDIF()

# Common compile options
ADD_COMPILE_OPTIONS(
	-g -Os
	-fPIC

	# Improved version of “-fstack-protector” since gcc 4.9
	-fstack-protector-strong

	# Store the stack frame pointer in a register.
	-fno-omit-frame-pointer

	# Eliminate unused code and data (with --gc-sections link option)
	-fdata-sections -ffunction-sections

	# Increase reliability of backtraces
	-fasynchronous-unwind-tables

	# Turn on all warnings and treat all warnings as errors.
	-Werror -Wall -Wextra

	# Additional warnings
	-Wunreachable-code -Wpointer-arith
	-Wformat=2 -Wno-format-nonliteral

	# Not warnings
	-Wno-missing-field-initializers
	-Wno-unused-parameter)

IF (ENABLE_LTO)
	INCLUDE(CheckIPOSupported)
	check_ipo_supported(RESULT IPO_SUPPORTED)
	IF(IPO_SUPPORTED)
		SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
		MESSAGE("LTO supported and enabled")
	ENDIF()
ENDIF()

IF(ENABLE_ASAN)
	# Apply AddressSanitizer
	# - https://github.com/google/sanitizers/wiki/AddressSanitizer
	ADD_COMPILE_OPTIONS(-fsanitize=address)
	LINK_LIBRARIES(-lasan)
	MESSAGE("Address Sanitizer enabled")
ENDIF()

# Sets the option so that the built library has a higher link order than
# the library already installed on the system.
LINK_LIBRARIES(-L${CMAKE_BINARY_DIR}/src)

IF (CMAKE_COMPILER_IS_GNUCC)
	# Enable a new static analysis pass and associated warnings. (Introduced gcc v10.1)
	# This option will not work if LTO is enabled.
	IF (CMAKE_C_COMPILER_VERSION VERSION_GREATER 10.1)
		# It is recommended to use only C language.
		STRING(APPEND CMAKE_C_FLAGS " -fanalyzer")

		# Not errors
		STRING(APPEND CMAKE_C_FLAGS " -Wno-analyzer-possible-null-dereference")
	ENDIF()

	ADD_COMPILE_OPTIONS(
		# Non-executable stack
		-Wa,--execstack

		# Eliminate unused code and data (with --gc-sections link option)
		-Wl,--gc-sections)

	IF (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		# Link with force dependency libraries
		LINK_LIBRARIES(-Wl,--no-as-needed ${force_pkgs_LDFLAGS})

		# Link only needed libraries
		LINK_LIBRARIES(-Wl,--as-needed)

		# Enforces symbol resolution at build time
		LINK_LIBRARIES(-Wl,-z,defs)

		# Non-executable stack
		LINK_LIBRARIES(-Wl,-z,noexecstack)

		# Debian packaging default link flags
		LINK_LIBRARIES(-Wl,-Bsymbolic-functions -Wl,-z,relro)

		# RPATH is useful only for testing without installation.
		# Please use the '-DPACKAGING' option for debian packaging.
		IF (NOT PACKAGING)
			LINK_LIBRARIES(-Wl,--rpath=${CMAKE_BINARY_DIR}/src)
		ENDIF()
	ENDIF()
ENDIF()

# Add __FILENAME__ macro to use path from source directory instead of full path
IF(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
	ADD_COMPILE_OPTIONS(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)
ELSE()
	MESSAGE("CMAKE_GENERATOR = ${CMAKE_GENERATOR}")
	IF(CMAKE_GENERATOR MATCHES "Unix Makefiles")
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst $(realpath ${CMAKE_SOURCE_DIR})/,,$(abspath $<))\"'")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst $(realpath ${CMAKE_SOURCE_DIR})/,,$(abspath $<))\"'")
	ENDIF()
ENDIF()

IF(ENABLE_LOG_ANSICOLOR)
	# Turn on the colorful log message
	ADD_DEFINITIONS(-DNUGU_LOG_USE_ANSICOLOR)
ENDIF()

# Set asset file path
ADD_DEFINITIONS(-DNUGU_ASSET_PATH="${datadir}")

# Global definitions
ADD_DEFINITIONS(
	# Run-time buffer overflow detection
	-D_FORTIFY_SOURCE=2

	# Environment variable names
	# - Please comment out the following definition to avoid overwriting
	#   the value by setting an environment variable. (e.g. Production)
	-DNUGU_ENV_LOG="NUGU_LOG"
	-DNUGU_ENV_LOG_LEVEL="NUGU_LOG_LEVEL"
	-DNUGU_ENV_LOG_MODULE="NUGU_LOG_MODULE"
	-DNUGU_ENV_LOG_PROTOCOL_LINE_LIMIT="NUGU_LOG_PROTOCOL_LINE_LIMIT"
	-DNUGU_ENV_NETWORK_REGISTRY_SERVER="NUGU_REGISTRY_SERVER"
	-DNUGU_ENV_NETWORK_USERAGENT="NUGU_USERAGENT"
	-DNUGU_ENV_NETWORK_USE_V1="NUGU_NETWORK_USE_V1"
	-DNUGU_ENV_DUMP_PATH_PCM="NUGU_DUMP_PATH_PCM"
	-DNUGU_ENV_DUMP_PATH_RECORDER="NUGU_DUMP_PATH_RECORDER"
	-DNUGU_ENV_DUMP_PATH_DECODER="NUGU_DUMP_PATH_DECODER"
	-DNUGU_ENV_DUMP_PATH_ENCODER="NUGU_DUMP_PATH_ENCODER"
	-DNUGU_ENV_RECORDING_FROM_FILE="NUGU_RECORDING_FROM_FILE"
	-DNUGU_ENV_DEFAULT_PCM_DRIVER="NUGU_DEFAULT_PCM_DRIVER"
	-DNUGU_ENV_PLUGIN_PATH="NUGU_PLUGIN_PATH"
)

# Global include directories
INCLUDE_DIRECTORIES(
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/externals/jsoncpp/include
	${CMAKE_SOURCE_DIR}/include)

# Install header files
CONFIGURE_FILE(nugu.h.in ${CMAKE_SOURCE_DIR}/include/nugu.h @ONLY)
INSTALL(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nugu/)

# Install pkgconfig
CONFIGURE_FILE(nugu.pc.in ${CMAKE_BINARY_DIR}/nugu.pc @ONLY)
INSTALL(FILES ${CMAKE_BINARY_DIR}/nugu.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig/)

# Install asset files
INSTALL(DIRECTORY assets/model DESTINATION ${datadir})

ENABLE_TESTING()

feature_summary(WHAT ALL)

# External dependency modules - nghttp2, curl, jsoncpp
ADD_SUBDIRECTORY(externals)

# NUGU SDK
ADD_SUBDIRECTORY(src)

# Unit tests
ADD_SUBDIRECTORY(tests)

# HAL Plugins
ADD_SUBDIRECTORY(plugins)

# Examples
ADD_SUBDIRECTORY(examples)
