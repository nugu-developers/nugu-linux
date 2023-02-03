CMAKE_MINIMUM_REQUIRED(VERSION 3.9)
PROJECT(nugu)

INCLUDE(GNUInstallDirs)
INCLUDE(FindPkgConfig)
INCLUDE(FeatureSummary)
INCLUDE(CMakePushCheckState)
INCLUDE(CheckSymbolExists)

SET(VERSION_MAJOR 1)
SET(VERSION_MINOR 7)
SET(VERSION_PATCH 4)
SET(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

MESSAGE("-- Build information:\n")
MESSAGE(" * System: ${CMAKE_SYSTEM_NAME}")
MESSAGE(" * Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
MESSAGE(" * CMAKE_GENERATOR = ${CMAKE_GENERATOR}")
MESSAGE(" * CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")
MESSAGE(" * PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}")
MESSAGE(" * CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")
MESSAGE(" * CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")

# OpenSDK/{sdk-version} ({platform}) Client/{app-version} [additional-info]
SET(NUGU_USERAGENT_FORMAT "OpenSDK/${VERSION} (Linux) Client/%s %s")

SET(NUGU_REGISTRY_URL "https://reg-http.sktnugu.com")

SET(version ${VERSION})
SET(prefix ${CMAKE_INSTALL_PREFIX})
SET(datadir ${CMAKE_INSTALL_FULL_DATAROOTDIR}/nugu)
SET(bindir "${prefix}/${CMAKE_INSTALL_BINDIR}")
SET(requires "")

IF(PLUGIN_DIR)
	SET(plugindir "${PLUGIN_DIR}")
ELSE()
	SET(plugindir "${CMAKE_INSTALL_FULL_LIBDIR}/nugu")
ENDIF()

IF(ASSET_DIR)
	SET(assetdir "${ASSET_DIR}")
ELSE()
	SET(assetdir "${datadir}")
ENDIF()

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

IF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
	SET(BUILTIN_CURL_DEFAULT "OFF")
	SET(BUILTIN_JSONCPP_DEFAULT "OFF")
	SET(PULSEAUDIO_DEFAULT "OFF")
	SET(PORTAUDIO_PLUGIN_DEFAULT "OFF")
	SET(ASAN_DEFAULT "OFF")

	ADD_DEFINITIONS(
		-DNUGU_PLUGIN_FILE_EXTENSION=".dylib"
	)
ELSE()
	SET(BUILTIN_CURL_DEFAULT "ON")
	SET(BUILTIN_JSONCPP_DEFAULT "ON")
	SET(PULSEAUDIO_DEFAULT "ON")
	SET(PORTAUDIO_PLUGIN_DEFAULT "ON")

	ADD_DEFINITIONS(
		# Run-time buffer overflow detection
		-D_FORTIFY_SOURCE=2

		-DNUGU_PLUGIN_FILE_EXTENSION=".so"
	)
ENDIF()

# Set default build options
DEFINE_FEATURE(LOG_ANSICOLOR ON "ANSI Color to log message")
DEFINE_FEATURE(LTO ${LTO_DEFAULT} "Link Time Optimization")
DEFINE_FEATURE(ASAN ${ASAN_DEFAULT} "Address Sanitizer")
DEFINE_FEATURE(VENDOR_LIBRARY ON "vendor specific library(nugu_wwd, nugu_epd)")

DEFINE_FEATURE(BUILTIN_OPUS OFF "compile built-in OPUS library")
DEFINE_FEATURE(BUILTIN_OPUS_FLOAT_API OFF "floating point to OPUS library")
DEFINE_FEATURE(BUILTIN_CURL ${BUILTIN_CURL_DEFAULT} "compile built-in curl library")
DEFINE_FEATURE(BUILTIN_JSONCPP ${BUILTIN_JSONCPP_DEFAULT} "compile built-in jsoncpp library")

DEFINE_FEATURE(DL_LINKING ON "-ldl linking")
DEFINE_FEATURE(SOCKET_LINKING OFF "-lsocket linking")
DEFINE_FEATURE(EVENTFD ON "eventfd")
DEFINE_FEATURE(PULSEAUDIO ${PULSEAUDIO_DEFAULT} "pulseaudio")

DEFINE_FEATURE(GSTREAMER_PLUGIN ON "GStreamer plugin")
DEFINE_FEATURE(OPUS_PLUGIN ON "OPUS plugin")
DEFINE_FEATURE(SPEEX_PLUGIN ON "SPEEX plugin")
DEFINE_FEATURE(PORTAUDIO_PLUGIN ${PORTAUDIO_PLUGIN_DEFAULT} "PortAudio plugin")
DEFINE_FEATURE(OPUSENC_PLUGIN OFF "OPUS encoder plugin")

DEFINE_FEATURE(LIBRARY_ONLY OFF "compile only SDK libraries")
DEFINE_FEATURE(PLUGINS_ONLY OFF "compile only plugins")
DEFINE_FEATURE(EXAMPLES_ONLY OFF "compile only examples")

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
		${PROJECT_BINARY_DIR}/curl/lib/libcurl.a
		${PROJECT_BINARY_DIR}/nghttp2/lib/libnghttp2.a)
	SET(CURL_PKGCONFIG zlib)
	SET(CURL_INCLUDE ${PROJECT_SOURCE_DIR}/externals/curl/include)
ELSE()
	SET(CURL_LIBRARY)
	SET(CURL_PKGCONFIG libcurl)
	SET(CURL_INCLUDE)
ENDIF()

IF(ENABLE_BUILTIN_JSONCPP)
	SET(JSONCPP_LIBRARY ${PROJECT_BINARY_DIR}/externals/libjsoncpp.a)
	SET(JSONCPP_PKGCONFIG)
	SET(JSONCPP_INCLUDE ${PROJECT_SOURCE_DIR}/externals/jsoncpp/include)

	INCLUDE_DIRECTORIES(${JSONCPP_INCLUDE})
ELSE()
	SET(JSONCPP_LIBRARY)
	SET(JSONCPP_PKGCONFIG jsoncpp)
	SET(JSONCPP_INCLUDE)
	SET(requires "${requires} ${JSONCPP_PKGCONFIG}")
ENDIF()

# Add vendor specific library dependency (keyword detector, end-point detector)
IF(ENABLE_VENDOR_LIBRARY)
	SET(VENDOR_PKGCONFIG nugu-kwd nugu-epd)

	ADD_DEFINITIONS(-DENABLE_VENDOR_LIBRARY)
ELSE()
	SET(VENDOR_PKGCONFIG)
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

SET(BUILD_LIBRARY ON)
SET(BUILD_PLUGINS ON)
SET(BUILD_EXAMPLES ON)

IF(ENABLE_LIBRARY_ONLY)
	SET(BUILD_LIBRARY ON)
	SET(BUILD_PLUGINS OFF)
	SET(BUILD_EXAMPLES OFF)
ENDIF()

IF(ENABLE_PLUGINS_ONLY)
	SET(BUILD_LIBRARY OFF)
	SET(BUILD_PLUGINS ON)
	SET(BUILD_EXAMPLES OFF)

	# The SPEEX plugin requires nugu-epd.
	IF(ENABLE_VENDOR_LIBRARY)
		SET(VENDOR_PKGCONFIG nugu-epd)
	ENDIF()
ENDIF()

IF(ENABLE_EXAMPLES_ONLY)
	SET(BUILD_LIBRARY OFF)
	SET(BUILD_PLUGINS OFF)
	SET(BUILD_EXAMPLES ON)
ENDIF()

IF(VENDOR_PKGCONFIG)
	pkg_check_modules(vendor_pkgs REQUIRED ${VENDOR_PKGCONFIG})
ENDIF()

IF(BUILD_LIBRARY)
	# Set NUGU SDK library default dependency
	SET(DEFAULT_LIB_DEPENDENCY
		glib-2.0 gio-2.0 gio-unix-2.0 gthread-2.0 openssl
		${CURL_PKGCONFIG} ${JSONCPP_PKGCONFIG} ${VENDOR_PKGCONFIG})

	# Sets the option so that the built library has a higher link order than
	# the library already installed on the system.
	LINK_LIBRARIES(-L${PROJECT_BINARY_DIR}/src)
ELSE()
	SET(DEFAULT_LIB_DEPENDENCY nugu)

	# Dummy target
	ADD_CUSTOM_TARGET(libnugu)
ENDIF()

# Get CFLAGS and LDFLAGS from pkg-config list
pkg_check_modules(pkgs REQUIRED ${DEFAULT_LIB_DEPENDENCY})
FOREACH(flag ${pkgs_CFLAGS})
	ADD_COMPILE_OPTIONS(${flag})
ENDFOREACH()

IF(ENABLE_GSTREAMER_PLUGIN)
	pkg_check_modules(gstreamer_pkgs REQUIRED
		gstreamer-1.0 gstreamer-app-1.0 gstreamer-pbutils-1.0)
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
		MESSAGE(" * LTO supported and enabled")
	ENDIF()
ENDIF()

IF(ENABLE_ASAN)
	# Apply AddressSanitizer
	# - https://github.com/google/sanitizers/wiki/AddressSanitizer
	ADD_COMPILE_OPTIONS(-fsanitize=address)
	LINK_LIBRARIES(-lasan)
	MESSAGE(" * Address Sanitizer enabled")
ENDIF()

# Compiler specific options (gcc, clang)
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

	LINK_LIBRARIES(
		# Link only needed libraries
		-Wl,--as-needed

		# Enforces symbol resolution at build time
		-Wl,-z,defs

		# Non-executable stack
		-Wl,-z,noexecstack

		# Debian packaging default link flags
		-Wl,-Bsymbolic-functions -Wl,-z,relro)

	# RPATH is useful only for testing without installation.
	# Please use the '-DPACKAGING' option for debian packaging.
	IF (NOT PACKAGING)
		LINK_LIBRARIES(-Wl,--rpath=${PROJECT_BINARY_DIR}/src)
	ENDIF()

	# Add __FILENAME__ macro to use path from source directory instead of full path
	IF(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
		ADD_COMPILE_OPTIONS(-fmacro-prefix-map=${PROJECT_SOURCE_DIR}/=)
	ELSE()
		IF(CMAKE_GENERATOR MATCHES "Unix Makefiles")
			SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst $(realpath ${PROJECT_SOURCE_DIR})/,,$(abspath $<))\"'")
			SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst $(realpath ${PROJECT_SOURCE_DIR})/,,$(abspath $<))\"'")
		ENDIF()
	ENDIF()
ELSE()
	ADD_COMPILE_OPTIONS(-fmacro-prefix-map=${PROJECT_SOURCE_DIR}/=)

	IF (NOT PACKAGING)
		LINK_LIBRARIES(-Wl,-rpath,${PROJECT_BINARY_DIR}/src)
	ENDIF()
ENDIF()

IF(ENABLE_LOG_ANSICOLOR)
	# Turn on the colorful log message
	ADD_DEFINITIONS(-DNUGU_LOG_USE_ANSICOLOR)
ENDIF()

# Set asset file path
ADD_DEFINITIONS(-DNUGU_ASSET_PATH="${assetdir}")

# Global definitions
ADD_DEFINITIONS(
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

MESSAGE("")
feature_summary(WHAT ALL)

# External dependency modules - nghttp2, curl, jsoncpp
IF(BUILD_LIBRARY)
	# Global include directories
	INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

	# Install header files
	CONFIGURE_FILE(nugu.h.in ${PROJECT_SOURCE_DIR}/include/nugu.h @ONLY)
	INSTALL(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nugu/)

	# Install pkgconfig
	CONFIGURE_FILE(nugu.pc.in ${PROJECT_BINARY_DIR}/nugu.pc @ONLY)
	INSTALL(FILES ${PROJECT_BINARY_DIR}/nugu.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig/)

	# Install asset files
	INSTALL(DIRECTORY assets/model DESTINATION ${assetdir} COMPONENT libnugu)

	ENABLE_TESTING()

	ADD_SUBDIRECTORY(externals)

	# NUGU SDK
	ADD_SUBDIRECTORY(src)

	# Unit tests
	ADD_SUBDIRECTORY(tests)
ENDIF()

# HAL Plugins
IF(BUILD_PLUGINS)
	ADD_SUBDIRECTORY(plugins)
ENDIF()

# Examples
IF(BUILD_EXAMPLES)
	ADD_SUBDIRECTORY(examples)
ENDIF()
