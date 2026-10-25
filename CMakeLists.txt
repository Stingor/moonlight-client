cmake_minimum_required(VERSION 3.25)

# ---------------------------------------------------------------------------
# Chaîne d'outils vcpkg
#
# Elle doit être posée AVANT project(). On la cherche dans VCPKG_ROOT, puis dans
# l'installation Visual Studio qui l'embarque : un clone frais doit configurer
# sans qu'aucune variable d'environnement n'ait été posée à la main.
# ---------------------------------------------------------------------------
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(_vcpkg_candidates
      "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      "$ENV{VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake"
      "C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
      "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake")
  foreach(_candidate IN LISTS _vcpkg_candidates)
    if(EXISTS "${_candidate}")
      set(CMAKE_TOOLCHAIN_FILE "${_candidate}" CACHE STRING "Chaîne d'outils vcpkg")
      message(STATUS "vcpkg : ${_candidate}")
      break()
    endif()
  endforeach()
  unset(_vcpkg_candidates)
endif()

# LES PORTAGES vcpkg PROPRES AU DÉPÔT, hors de Windows seulement.
#
# Celui de SDL3 rend le son au client sous Linux : le portage de vcpkg y éteint
# PipeWire et PulseAudio (voir `cmake/vcpkg-ports/linux/sdl3/portfile.cmake`).
# Sous Windows, rien ne change, et aucune dépendance ne se reconstruit.
#
# ⚠ AVANT `project()`, comme la chaîne d'outils : c'est là que vcpkg installe.
if(NOT CMAKE_HOST_WIN32 AND NOT DEFINED VCPKG_OVERLAY_PORTS)
  set(VCPKG_OVERLAY_PORTS "${CMAKE_SOURCE_DIR}/cmake/vcpkg-ports/linux")
endif()

project(moonclient VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Une DLL injectée dans un client 32 bits est bloquée en 32 bits avec lui.
# Ce dépôt n'a pas cette contrainte, et un client autonome n'a aucune raison de
# se limiter à 2 Gio d'espace d'adressage : on refuse explicitement le 32 bits
# plutôt que de le découvrir sur un plantage d'allocation de carte.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "moonclient est 64 bits uniquement (pointeur de ${CMAKE_SIZEOF_VOID_P} octets détecté).")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(MOONCLIENT_BUILD_TESTS "Construire la suite de tests" ON)
option(MOONCLIENT_WERROR "Traiter les avertissements comme des erreurs" ON)
option(MOONCLIENT_ASAN "Activer AddressSanitizer" OFF)

# ---------------------------------------------------------------------------
# Compiler PLUSIEURS fichiers à la fois
#
# MSVC n'en compile qu'UN par projet tant qu'on ne lui passe pas /MP, et rien
# ne le signale : la construction est simplement lente. `ro_render` porte plus
# de deux cents sources dans un seul projet, si bien qu'un en-tête partagé
# touché les reprenait une par une.
#
# ⚠ CE N'EST PAS LA MÊME CHOSE QUE `--parallel`, qui ne parallélise que les
# PROJETS entre eux. Les deux se cumulent, et c'est celui-ci qui compte ici :
# le gros du temps tient dans un projet unique.
#
# `/MP` sans compte laisse MSVC prendre le nombre de processeurs de la
# machine. Le fixer ici le figerait à celui de la machine qui a écrit la
# ligne.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# Les couches qui parlent à la PLATEFORME : la fenêtre, le contexte OpenGL, le
# son. Elles tirent SDL, que vcpkg doit bâtir depuis la source, et qui réclame
# les en-têtes X11 ou Wayland.
#
# Les couper est ce qui achète les sanitizers. Le runner d'intégration n'a pas
# ces en-têtes, et les lui installer ferait bâtir SDL et son mixeur à chaque
# renouvellement de cache pour vérifier des couches qui, elles, n'en dépendent
# pas : core, formats, vfs, proto, sim, net et render sont volontairement sans
# plateforme, et c'est précisément ce qui les rend vérifiables là-bas.
option(MOONCLIENT_PLATFORM_LAYERS "Construire les couches liées à SDL et à OpenGL" ON)

# ---------------------------------------------------------------------------
# Le crochet de push s'installe tout seul
#
# `.githooks/pre-push` est versionné, mais git n'y regarde que si `hooksPath`
# le lui dit — et cette configuration est LOCALE au clone. Laisser cette
# commande dans un fichier de documentation en ferait une convention, c'est-à-
# dire une règle appliquée cinq fois sur six : c'est exactement la maladie que
# le crochet soigne. On la pose donc à la configuration, que tout le monde
# lance de toute façon avant de pouvoir construire.
#
# Rien n'est écrasé : si `hooksPath` pointe déjà ailleurs, c'est un choix, et
# on le laisse.
option(MOONCLIENT_INSTALL_HOOKS "Poser core.hooksPath sur .githooks" ON)
if(MOONCLIENT_INSTALL_HOOKS AND NOT DEFINED ENV{CI} AND EXISTS "${CMAKE_SOURCE_DIR}/.git"
   AND EXISTS "${CMAKE_SOURCE_DIR}/.githooks")
  find_program(GIT_EXECUTABLE git)
  if(GIT_EXECUTABLE)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" config --get core.hooksPath
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE _hooks_path
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    if(NOT _hooks_path)
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" config core.hooksPath .githooks
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ERROR_QUIET)
      message(STATUS "crochets git : .githooks (contrôle de format avant push)")
    endif()
    unset(_hooks_path)
  endif()
endif()

include(cmake/warnings.cmake)
include(cmake/build_stamp.cmake)
include(cmake/shipping_symbols.cmake)
include(cmake/build_speed.cmake)

find_package(ZLIB REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Les couches
#
# Une bibliothèque par couche, et le graphe de dépendances est un DAG imposé par
# l'éditeur de liens : `formats` ne peut pas inclure un en-tête de `sim`, parce
# que le chemin d'inclusion ne le contient pas et que le symbole ne se résout
# pas. La couche n'est pas une convention de revue, c'est une contrainte de build.
#
#   core     ← ne connaît RIEN
#   formats  ← core
#   vfs      ← core, formats        (M1)
#   mapedit  ← core, formats        (ADR 0118)
#   proto    ← core                 (M3)
#   sim      ← core, formats        (M3)
#   net      ← core, proto, sim     (M3)
#   render   ← core, formats, vfs, mapedit (M1)
#   ui       ← core, render         (M4)
#   app      ← tout                 (M1)
# ---------------------------------------------------------------------------
add_subdirectory(libs/core)
add_subdirectory(libs/formats)
add_subdirectory(libs/vfs)
add_subdirectory(libs/mapedit)
add_subdirectory(libs/proto)
add_subdirectory(libs/sim)
add_subdirectory(libs/net)
add_subdirectory(libs/render)
# `ui` porte DEUX cibles, et seule la seconde touche à la plateforme : les
# fenêtres elles-mêmes se bâtissent et se testent ici comme sur le runner sans
# SDL. C'est son propre CMakeLists qui s'arrête à mi-chemin le cas échéant.
add_subdirectory(libs/ui)

if(MOONCLIENT_PLATFORM_LAYERS)
  add_subdirectory(libs/audio)
  add_subdirectory(app)
  # L'outil hors-jeu produit des images : il passe par le renderer OpenGL.
  if(EXISTS "${CMAKE_SOURCE_DIR}/tools/roc-cli/CMakeLists.txt")
    add_subdirectory(tools/roc-cli)
  endif()
  # Le décor de l'écran de connexion peint hors écran : la preuve d'image de
  # qui retouche sa scène.
  if(EXISTS "${CMAKE_SOURCE_DIR}/tools/backdrop-bench/CMakeLists.txt")
    add_subdirectory(tools/backdrop-bench)
  endif()
  # L'éditeur des tables source : une fenêtre ImGui, l'aperçu du jeu compris.
  if(EXISTS "${CMAKE_SOURCE_DIR}/tools/table-editor/CMakeLists.txt")
    add_subdirectory(tools/table-editor)
  endif()
endif()

# Les sources publiées (`scripts/publish_mirror.py`) ne portent ni `tools/` ni
# `tests/` : leurs cibles ne se déclarent que si leur dossier est là.
if(MOONCLIENT_BUILD_TESTS AND EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
  enable_testing()
  add_subdirectory(tests)
endif()
