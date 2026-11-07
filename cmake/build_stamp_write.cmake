# ÉCRIT L'EN-TÊTE D'IDENTITÉ, à partir de git. Inclus à la configuration, puis
# lancé en script (`cmake -P`) au début de chaque bâtisse : voir
# `cmake/build_stamp.cmake`.
#
# ⚠ L'en-tête n'est réécrit que si son texte change : sans nouveau commit, rien
# ne se recompile.
#
# Attend : MOONCLIENT_STAMP_SOURCE_DIR, MOONCLIENT_STAMP_BINARY_DIR,
# PROJECT_VERSION et GIT_EXECUTABLE (vide sans git).

set(MOONCLIENT_COMMIT "inconnue")
set(MOONCLIENT_TREE "inconnu")

if(GIT_EXECUTABLE AND EXISTS "${MOONCLIENT_STAMP_SOURCE_DIR}/.git")
  execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
      WORKING_DIRECTORY "${MOONCLIENT_STAMP_SOURCE_DIR}"
      OUTPUT_VARIABLE MOONCLIENT_COMMIT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)

  # UN ARBRE SALE SE DIT. Un binaire bâti sur des retouches non validées ne
  # correspond à aucune révision : le taire ferait chercher la panne dans un
  # code qui n'est pas celui qui a planté. Les carnets du joueur, jamais
  # compilés, n'en sont pas : même liste que NOTE_FILES de
  # `scripts/build_identity.py`.
  execute_process(
      COMMAND "${GIT_EXECUTABLE}" status --porcelain -- .
              ":(exclude)TODO.txt" ":(exclude)FIXME.txt"
      WORKING_DIRECTORY "${MOONCLIENT_STAMP_SOURCE_DIR}"
      OUTPUT_VARIABLE _dirty
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
  if(_dirty STREQUAL "")
    set(MOONCLIENT_TREE "propre")
  else()
    set(MOONCLIENT_TREE "MODIFIE")
  endif()
  unset(_dirty)
endif()

configure_file(
    "${MOONCLIENT_STAMP_SOURCE_DIR}/cmake/build_stamp.h.in"
    "${MOONCLIENT_STAMP_BINARY_DIR}/generated/ro/core/build_stamp.h"
    @ONLY)
