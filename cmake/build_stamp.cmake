# L'IDENTITÉ DE CE BINAIRE, gravée dedans.
#
# ⚠ SANS ELLE, UN RAPPORT DE PLANTAGE NE SERT PRESQUE À RIEN. Une pile d'appels
# se lit avec les symboles du binaire qui l'a produite, et ceux-là ne valent que
# pour UN binaire : appliquer les symboles d'une version aux adresses d'une
# autre donne des noms de fonctions faux, ce qui est pire que pas de noms.
#
# Le rapport porte donc de quoi retrouver la bonne : la version, la révision
# exacte, et si l'arbre était propre au moment de bâtir.
#
# ⚠ RELEVÉE À LA CONFIGURATION, pas à la compilation. Un commit fait sans
# reconfigurer laisse donc une révision périmée — c'est sans conséquence pour
# une publication, qui part toujours d'une configuration fraîche, et c'est le
# prix à ne pas relancer git à chaque fichier compilé.

find_package(Git QUIET)

set(MOONCLIENT_COMMIT "inconnue")
set(MOONCLIENT_TREE "inconnu")

if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
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
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
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

message(STATUS "revision : ${MOONCLIENT_COMMIT} (arbre ${MOONCLIENT_TREE})")

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/build_stamp.h.in"
    "${CMAKE_BINARY_DIR}/generated/ro/core/build_stamp.h"
    @ONLY)
