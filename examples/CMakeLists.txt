ADD_SUBDIRECTORY(standalone)
ADD_SUBDIRECTORY(oob_setup)

INSTALL(PROGRAMS nugusdk_start_sample.sh DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME nugusdk_start_sample)
