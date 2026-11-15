# LES SYMBOLES D'UNE CONSTRUCTION DE PUBLICATION, et pourquoi ils ne sont pas
# facultatifs.
#
# ⚠ UN RAPPORT DE PLANTAGE SANS SYMBOLES NE SE LIT PAS. La pile d'un joueur
# arrive en adresses nues ; il faut le `.pdb` du binaire EXACT qui a planté pour
# les traduire en noms de fonctions et en numéros de ligne. Ce fichier n'existe
# qu'au moment de bâtir — s'il n'est pas gardé alors, il est perdu, et aucun
# travail ultérieur ne le reconstitue.
#
# C'est la seule pièce de tout le dispositif de traçabilité qui ne se rattrape
# PAS après coup : un vidage reçu dans six mois sans son `.pdb` est un fichier
# qu'on jette.
#
# ⚠ LE `.pdb` NE SE LIVRE PAS AU JOUEUR. Il porte les noms de toutes les
# fonctions et la disposition des structures : c'est le plan du programme. On
# l'ARCHIVE, indexé par révision — voir `scripts/archive_symbols.py`.

if(MSVC)
  # LES INFORMATIONS DE DÉBOGAGE en publication : le code reste optimisé, et
  # chaque objet porte les siennes (`/Z7`).
  #
  # Sans elles, la configuration Release de CMake passe `/O2 /DNDEBUG` et RIEN
  # d'autre : le binaire est muet, et un rapport ne peut nommer que des
  # adresses. Leur format se pose par `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT`,
  # avec celui des autres configurations : voir `build_speed.cmake`, qui dit
  # pourquoi ce n'est pas `/Zi`.

  # `/DEBUG` demande l'édition des liens avec le `.pdb`. Les deux `/OPT` la
  # suivent parce que `/DEBUG` les désactive par défaut : sans elles, le binaire
  # publié garderait le code mort et les chaînes dupliquées, c'est-à-dire qu'il
  # serait plus gros SANS être plus lisible.
  add_link_options(
      $<$<CONFIG:Release>:/DEBUG>
      $<$<CONFIG:Release>:/OPT:REF>
      $<$<CONFIG:Release>:/OPT:ICF>)
endif()
