# L'IDENTITÉ DE CE BINAIRE, gravée dedans.
#
# ⚠ SANS ELLE, UN RAPPORT DE PLANTAGE NE SERT PRESQUE À RIEN. Une pile d'appels
# se lit avec les symboles du binaire qui l'a produite, et ceux-là ne valent que
# pour UN binaire : appliquer les symboles d'une version aux adresses d'une
# autre donne des noms de fonctions faux, ce qui est pire que pas de noms.
#
# Le rapport porte donc de quoi retrouver la bonne : la version, la révision
# exacte, et si l'arbre était propre au moment de bâtir. Le titre de la fenêtre
# affiche cette même révision.
#
# ⚠ RELEVÉE À CHAQUE BÂTISSE, par la cible `build_stamp` dont `ro_core` dépend :
# un commit fait sans reconfigurer ne laisse pas de révision périmée. Le relevé
# demande une fraction de seconde à git ; un commit relie ensuite ce qui lie
# `ro_core`, dont le journal et le rapport de plantage portent le tampon.

find_package(Git QUIET)

set(MOONCLIENT_STAMP_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
set(MOONCLIENT_STAMP_BINARY_DIR "${CMAKE_BINARY_DIR}")
include("${CMAKE_SOURCE_DIR}/cmake/build_stamp_write.cmake")

message(STATUS "revision : ${MOONCLIENT_COMMIT} (arbre ${MOONCLIENT_TREE})")

add_custom_target(build_stamp
    COMMAND "${CMAKE_COMMAND}"
            "-DMOONCLIENT_STAMP_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DMOONCLIENT_STAMP_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DPROJECT_VERSION=${PROJECT_VERSION}"
            "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
            -P "${CMAKE_SOURCE_DIR}/cmake/build_stamp_write.cmake"
    BYPRODUCTS "${CMAKE_BINARY_DIR}/generated/ro/core/build_stamp.h"
    VERBATIM)
