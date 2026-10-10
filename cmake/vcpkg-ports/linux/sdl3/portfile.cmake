# SDL3 POUR LE CLIENT SOUS LINUX.
#
# Le portage de vcpkg éteint le son de SDL sous Linux : PipeWire et PulseAudio y
# sont forcés à OFF, ALSA ne s'obtient que par une fonction qui reconstruit la
# bibliothèque ALSA sans les greffons de la distribution. Un joueur qui
# construit le client sur sa machine n'aurait donc pas de son.
#
# Celui-ci laisse SDL faire ce qu'il fait compilé par une distribution : il
# essaie PipeWire, puis PulseAudio, puis ALSA, et prend le premier qui répond.
# Chacun est CHARGÉ À L'EXÉCUTION (`SDL_*_SHARED`, le défaut de SDL) : une
# machine sans PipeWire démarre quand même, et prend le suivant. Il en va de
# même de Wayland, X11, libdecor (la barre de titre sous GNOME), D-Bus et IBus.
#
# ⚠ IL NE S'APPLIQUE QU'HORS DE WINDOWS : `CMakeLists.txt` ne le pose que là.
# Sous Windows, le portage de vcpkg sert tel quel.
#
# Les en-têtes de chaque pilote viennent de la distribution : voir le README.

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO libsdl-org/SDL
    REF "release-${VERSION}"
    SHA512 d815b28f72d60e4d83e31d878bcf2dd52089353637a4ffd0fd094deaafaaa5b5ee797caad9eeb952f6c28a2bbe26253092c294ce1aefbeb92d85469f35229ed1
    HEAD_REF main
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" SDL_STATIC)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" SDL_SHARED)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSDL_STATIC=${SDL_STATIC}
        -DSDL_SHARED=${SDL_SHARED}
        # Le son : les trois, chargés à l'exécution.
        -DSDL_PIPEWIRE=ON
        -DSDL_PULSEAUDIO=ON
        -DSDL_ALSA=ON
        # La fenêtre et ce qui l'entoure. libdecor donne sa barre de titre à la
        # fenêtre sous GNOME en Wayland, qui n'en dessine aucune lui-même.
        -DSDL_WAYLAND=ON
        -DSDL_WAYLAND_LIBDECOR=ON
        -DSDL_X11=ON
        -DSDL_DBUS=ON
        -DSDL_IBUS=ON
        -DSDL_LIBUDEV=ON
        # Ce dont le client n'a pas l'usage.
        -DSDL_JACK=OFF
        -DSDL_SNDIO=OFF
        -DSDL_KMSDRM=OFF
        -DSDL_TESTS=OFF
        -DSDL_TEST_LIBRARY=OFF
        -DSDL_INSTALL_CMAKEDIR_ROOT=share/${PORT}
        # Une révision nommée : SDL n'a pas à interroger git.
        -DSDL_REVISION=vcpkg
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup()
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
