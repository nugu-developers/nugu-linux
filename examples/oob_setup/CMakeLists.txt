INSTALL(PROGRAMS nugu_oob_server.py DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME nugu_oob_server)
INSTALL(FILES nugu_oob.service DESTINATION /lib/systemd/system)