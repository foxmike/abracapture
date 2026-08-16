include(GNUInstallDirs)

install(TARGETS abracapture
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/abracapture.desktop"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
)

install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/abracapture.metainfo.xml"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo"
)
