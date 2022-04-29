########### install files ###############
MESSAGE( STATUS "KDE_INSTALL_DOCBUNDLEDIR = ${KDE_INSTALL_DOCBUNDLEDIR}.")
kdoctools_create_handbook(index.docbook INSTALL_DESTINATION ${KDE_INSTALL_DOCBUNDLEDIR}/en SUBDIR dcontrol/kwinvirtualkeyboard)
