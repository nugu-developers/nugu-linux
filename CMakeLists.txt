CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT(nugu)

INCLUDE(GNUInstallDirs)
INCLUDE(FindPkgConfig)

SET(VERSION_MAJOR 0)
SET(VERSION_MINOR 6)
SET(VERSION_PATCH 0)
SET(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

SET(NUGU_USERAGENT "OpenSDK/1.0.0 (Linux) Client/${VERSION}")
SET(NUGU_REGISTRY_URL "https://reg-http.sktnugu.com")

SET(version ${VERSION})
SET(prefix ${CMAKE_INSTALL_PREFIX})
SET(bindir "${prefix}/${CMAKE_INSTALL_BINDIR}")
SET(plugindir "${CMAKE_INSTALL_FULL_LIBDIR}/nugu")

# Set default build options
OPTION(ENABLE_GSTREAMER_PLUGIN "Enable the GStreamer plugin" ON)
OPTION(ENABLE_OPUS_PLUGIN "Enable the OPUS plugin" ON)
OPTION(ENABLE_PORTAUDIO_PLUGIN "Enable the PortAudio plugin" ON)
OPTION(ENABLE_VENDOR_LIBRARY "Enable the vendor specific library" ON)
OPTION(ENABLE_OPUS_LIBRARY "Enable to embed OPUS library" OFF)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# Set NUGU SDK library default dependency
SET(DEFAULT_LIB_DEPENDENCY
	glib-2.0 gio-2.0 gio-unix-2.0 gthread-2.0
	openssl
	zlib)
SET(FORCE_LIB_DEPENDENCY)

# Add vendor specific library dependency (keyword detector, end-point detector)
IF(ENABLE_VENDOR_LIBRARY)
	ADD_DEFINITIONS(-DENABLE_VENDOR_LIBRARY)
	LIST(APPEND DEFAULT_LIB_DEPENDENCY nugu-kwd nugu-epd)
ENDIF()

# Gstreamer does not support dynamic loading/unloading of their libraries.
# Therefore, add the library dependency to NUGU SDK instead of NUGU Plugin.
IF(ENABLE_GSTREAMER_PLUGIN)
	LIST(APPEND FORCE_LIB_DEPENDENCY gstreamer-1.0 gstreamer-pbutils-1.0)
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

IF (NOT PACKAGING)
	IF ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND "${CMAKE_LIBRARY_ARCHITECTURE}" MATCHES "x86")
		MESSAGE("Enable AddressSanitizer")
		# Apply AddressSanitizer
		# - https://github.com/google/sanitizers/wiki/AddressSanitizer
		ADD_COMPILE_OPTIONS(-fsanitize=address)
		LINK_LIBRARIES(-lasan)
	ENDIF()
ENDIF()

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
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

		# RPATH is useful only for testing without installation.
		# Please use the '-DPACKAGING' option for debian packaging.
		IF (NOT PACKAGING)
			LINK_LIBRARIES(-Wl,--rpath=${CMAKE_BINARY_DIR}/src)
		ENDIF()
	ENDIF()
ENDIF()

# Add __FILENAME__ macro to use path from source directory instead of full path
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst $(realpath ${CMAKE_SOURCE_DIR})/,,$(abspath $<))\"'")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst $(realpath ${CMAKE_SOURCE_DIR})/,,$(abspath $<))\"'")

# Global definitions
ADD_DEFINITIONS(
	# Run-time buffer overflow detection
	-D_FORTIFY_SOURCE=2

	# Turn on the colorful log message
	-DCONFIG_LOG_ANSICOLOR

	# Environment variable names
	# - Please comment out the following definition to avoid overwriting
	#   the value by setting an environment variable. (e.g. Production)
	-DNUGU_ENV_LOG="NUGU_LOG"
	-DNUGU_ENV_LOG_MODULE="NUGU_LOG_MODULE"
	-DNUGU_ENV_NETWORK_REGISTRY_SERVER="NUGU_REGISTRY_SERVER"
	-DNUGU_ENV_NETWORK_USERAGENT="NUGU_USERAGENT"
	-DNUGU_ENV_NETWORK_USE_EVENTS="NUGU_NETWORK_USE_EVENTS"
	-DNUGU_ENV_NETWORK_USE_V2="NUGU_NETWORK_USE_V2"
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

ENABLE_TESTING()

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
