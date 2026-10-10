# Avertissements : posés AVANT la première ligne de code, jamais après.
#
# Un code compilé avertissements éteints laisse passer, par exemple, un
# commentaire `//` terminé par un antislash, qui avale la ligne suivante sans
# rien dire. MSVC le signale (C4010), mais seulement si ses avertissements sont
# allumés.
#
# Cible INTERFACE : chaque cible du projet la lie en PRIVATE. Les en-têtes tiers
# sont exclus (/external, -isystem via vcpkg) pour qu'un avertissement d'une
# dépendance ne casse pas le build.

add_library(ro_warnings INTERFACE)

if(MSVC)
  target_compile_options(ro_warnings INTERFACE
      /W4
      /permissive-        # conformité stricte
      /utf-8              # les sources portent des commentaires accentués
      /Zc:__cplusplus     # sinon __cplusplus ment et vaut 199711L
      /Zc:preprocessor    # préprocesseur conforme
      /external:anglebrackets /external:W0
      /w14242 /w14254 /w14263 /w14265 /w14287 /w14296 /w14545 /w14546
      /w14547 /w14549 /w14555 /w14619 /w14640 /w14826 /w14905 /w14906 /w14928
      $<$<BOOL:${MOONCLIENT_WERROR}>:/WX>)
  # C4324 : padding d'alignement, attendu sur les structures de paquets.
  target_compile_options(ro_warnings INTERFACE /wd4324)
else()
  target_compile_options(ro_warnings INTERFACE
      -Wall -Wextra -Wpedantic
      -Wshadow
      -Wnon-virtual-dtor
      -Wold-style-cast
      -Wcast-align
      -Wunused
      -Woverloaded-virtual
      -Wconversion
      -Wsign-conversion
      -Wdouble-promotion
      -Wformat=2
      -Wimplicit-fallthrough
      $<$<BOOL:${MOONCLIENT_WERROR}>:-Werror>)

  # ⚠ -Wmissing-field-initializers EST ÉTEINT SUR MESURE, et non par confort.
  #
  # Ce que g++ signale, ce sont des agrégats construits par désignateurs
  # partiels — `RswBuildOptions{.major = 3}`. La question qui décide est :
  # que devient un membre omis ? Depuis C++14, il prend son initialisateur par
  # défaut s'il en a un, et n'est mis à zéro que s'il n'en a aucun.
  #
  # Les dix structures que g++ nomme ont été relevées membre par membre :
  # AUCUNE ne porte de membre scalaire sans initialisateur par défaut. Les
  # omissions prennent donc toutes la valeur écrite dans la déclaration, et
  # l'avertissement ne désigne aucun risque — il désigne un style.
  #
  # ⚠ CE QUI LE RENDRAIT UTILE, c'est justement ce que la règle du memset
  # interdit ailleurs : un membre dont le défaut voulu n'est pas zéro et n'est
  # pas écrit. Un agrégat nouveau qui en porterait un ne serait plus couvert
  # par cette mesure.
  target_compile_options(ro_warnings INTERFACE -Wno-missing-field-initializers)

  # ⚠ TROIS AVERTISSEMENTS DE L'OPTIMISEUR DE g++ RESTENT AFFICHÉS, MAIS NE
  # CASSENT PAS LA CONSTRUCTION. Ils naissent en -O2, après inlining, et
  # n'existent donc pas dans une construction Debug.
  #
  # Ceux que la première construction Release sous g++ 14 a levés ont été
  # relevés un par un, et AUCUN ne désigne un défaut :
  #  - `free-nonheap-object` et `stringop-overflow` : onze, tous dans la
  #    réallocation d'un `std::vector<uint8_t>` que `ByteWriter` remplit par
  #    `push_back`. Il n'y a là que de la mémoire du tas, tenue par le vecteur.
  #  - `maybe-uninitialized` : cinq, tous sur un `std::optional`. Quatre sont
  #    gardés par un `has_value()` dans la même expression — ou dans son
  #    `operator==`, qui le vérifie lui-même. Le cinquième, dans `LeftClick`,
  #    l'est par un invariant écrit dans un autre fichier, que l'optimiseur ne
  #    voit pas : `GroundUnitClickOf` ne rend `kAttack` que pour une unité
  #    présente.
  #
  # ⚠ CE QUI LES RENDRAIT UTILES, c'est un défaut qu'ils seraient seuls à voir.
  # Un nouvel avertissement de ces trois familles se lit donc toujours, au lieu
  # de s'ignorer sous prétexte qu'il ne bloque pas.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ro_warnings INTERFACE
        -Wno-error=free-nonheap-object
        -Wno-error=stringop-overflow
        -Wno-error=maybe-uninitialized)
  endif()
endif()

if(MOONCLIENT_ASAN)
  if(MSVC)
    target_compile_options(ro_warnings INTERFACE /fsanitize=address)
  else()
    target_compile_options(ro_warnings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ro_warnings INTERFACE -fsanitize=address,undefined)
  endif()
endif()
