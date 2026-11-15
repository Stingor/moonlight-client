# CE QUI RACCOURCIT UNE RECONSTRUCTION, sans rien changer au programme bâti.
#
# Le coût ne tient pas dans le nombre de fichiers mais dans ce que chacun relit :
# un en-tête central touché recompile six cents fichiers, et chacun repasse par
# les mêmes en-têtes standard, par glm et, pour les essais, par Catch2.

# LES INFORMATIONS DE DÉBOGAGE DANS CHAQUE `.obj` (`/Z7`), jamais dans un `.pdb`
# de compilation (`/Zi`).
#
# Avec `/Zi`, les compilations parallèles de `/MP` écrivent toutes dans un même
# `.pdb` par un service unique, et s'y attendent l'une l'autre. `/Z7` range ces
# informations dans l'objet ; l'éditeur de liens bâtit le `.pdb` du binaire comme
# avant, et le débogueur n'y voit aucune différence.
#
# 🔴 LA PUBLICATION AUSSI, et pour elle ce n'est pas une affaire de vitesse. Une
# cible qui reprend l'en-tête précompilé d'une autre doit, sous `/Zi`, écrire
# dans une COPIE du `.pdb` qui l'a vu naître ; CMake refait cette copie dès que
# l'original est plus récent, c'est-à-dire dès qu'une source de la cible hôte
# est recompilée, et il écrase alors ce que les objets déjà compilés y avaient
# rangé. L'édition de liens les relie sans informations de débogage (LNK4099)
# ou s'arrête (LNK1103) : un `.pdb` de publication troué, donc un rapport de
# plantage illisible. Sous `/Z7`, il n'y a rien à copier ni à écraser.
#
# Le `.pdb` qu'on archive est celui du BINAIRE, que l'éditeur de liens bâtit
# sous `/DEBUG` (`shipping_symbols.cmake`) : il est le même dans les deux cas.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    "$<$<CONFIG:Debug,Release,RelWithDebInfo>:Embedded>")

# LES EN-TÊTES STANDARD QUE PRESQUE TOUT LE MONDE RELIT, précompilés.
#
# ⚠ `<filesystem>` EST LE PLUS CHER DE TOUS, et il est partout : sous
# `/std:c++latest` il tire `<chrono>`, `<format>` et les locales, plus d'une
# demi-seconde par source, et les en-têtes de `vfs` le portent dans leurs
# signatures. La liste se tient au rapport du compilateur (`cl /d1reportTime`),
# qui nomme ce qu'une source relit encore et ce que cela lui coûte.
#
# ⚠ UN FICHIER DOIT TOUJOURS INCLURE CE QU'IL EMPLOIE. L'en-tête précompilé le
# rend visible sans inclusion : un oubli compile ici, et casse dès qu'on bâtit
# sans lui (`CMAKE_DISABLE_PRECOMPILE_HEADERS`).
set(MOONCLIENT_STANDARD_PCH
    <algorithm> <array> <chrono> <cmath> <cstddef> <cstdint> <deque> <expected>
    <filesystem> <format> <functional> <map> <memory> <optional> <set> <span>
    <string> <string_view> <unordered_map> <utility> <variant> <vector>)

# UNE SEULE CIBLE LES PRÉCOMPILE, et les autres reprennent son fichier.
#
# Le `.pch` de cette liste pèse près de 200 Mo. Un par cible, pour cent trente
# cibles et deux configurations, remplirait le disque de chaque dossier de
# bâtisse ; et les trois quarts des cibles n'ont pas dix sources pour amortir
# la seconde et demie qu'il demande.
#
# ⚠ ELLE NE LIE AUCUNE COUCHE ET NE POSE AUCUNE DÉFINITION : ce qu'elle
# précompile vaut pour `core` comme pour `app`, et une cible qui reprend son
# fichier n'en reçoit rien d'autre que la bibliothèque standard. Les options de
# compilation, elles, doivent être les mêmes des deux côtés : toutes les cibles
# lient `ro_warnings`, et celle-ci aussi.
add_library(moonclient_std_pch STATIC "${CMAKE_CURRENT_LIST_DIR}/standard_headers.cpp")
target_link_libraries(moonclient_std_pch PRIVATE ro_warnings)
target_precompile_headers(moonclient_std_pch PRIVATE ${MOONCLIENT_STANDARD_PCH})

# `moonclient_precompile_headers(cible [GLM] [CATCH2] [HEADERS en-tête…])` : la
# cible précompile POUR ELLE les en-têtes standard, plus glm, Catch2 et les
# en-têtes nommés. Une cible qui n'a besoin que des en-têtes standard n'appelle
# rien : `moonclient_share_standard_headers` lui donne le fichier commun.
#
# ⚠ UN EN-TÊTE DU DÉPÔT N'ENTRE DANS `HEADERS` QUE SI PLUS DU TIERS DES SOURCES
# DE LA CIBLE L'INCLUENT DÉJÀ. Précompilé, il recompile toute la cible dès que
# lui ou l'un des siens change ; mais chaque source coûte alors trois fois
# moins, si bien qu'au-delà du tiers ce changement ne revient pas plus cher
# qu'avant, et que tout autre changement revient trois fois moins cher. En
# deçà du tiers, il coûterait plus qu'il ne rapporte.
function(moonclient_precompile_headers target)
  cmake_parse_arguments(PCH "GLM;CATCH2" "" "HEADERS" ${ARGN})
  set(headers ${MOONCLIENT_STANDARD_PCH})
  if(PCH_GLM)
    list(APPEND headers <glm/glm.hpp>)
  endif()
  if(PCH_CATCH2)
    list(APPEND headers <catch2/catch_test_macros.hpp> <catch2/catch_approx.hpp>)
  endif()
  target_precompile_headers(${target} PRIVATE ${headers} ${PCH_HEADERS})
endfunction()

# Les cibles d'un dossier et de ses sous-dossiers.
function(_moonclient_targets_below result directory)
  get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(child IN LISTS children)
    _moonclient_targets_below(below "${child}")
    list(APPEND targets ${below})
  endforeach()
  set(${result} ${targets} PARENT_SCOPE)
endfunction()

# `moonclient_share_standard_headers()` : toute cible compilée qui n'a pas son
# propre en-tête précompilé reprend celui de `moonclient_std_pch`.
#
# S'appelle UNE fois, après la dernière `add_subdirectory` : une cible ajoutée
# demain l'obtient sans que personne ait à y penser.
#
# ⚠ UNE CIBLE QUI COMPILE AVEC D'AUTRES OPTIONS S'EN DISPENSE, par
# `DISABLE_PRECOMPILE_HEADERS`. GCC refuse un en-tête précompilé dès qu'une
# macro que la bibliothèque standard consulte n'a pas la valeur qu'elle avait à
# sa création : `-pthread`, qu'amène libsecret, définit `_REENTRANT`.
#
# ⚠ ET SOUS GCC, CE REFUS AVERTIT SANS CASSER LA BÂTISSE. Refusé, l'en-tête
# précompilé est relu comme un en-tête ordinaire : la source compile, plus
# lentement. Une cible de plus dans ce cas se lit dans l'avertissement, et se
# dispense comme la première ; elle n'arrête pas un empaquetage pour une
# affaire de vitesse.
function(moonclient_share_standard_headers)
  _moonclient_targets_below(targets "${CMAKE_SOURCE_DIR}")
  foreach(target IN LISTS targets)
    get_target_property(type ${target} TYPE)
    get_target_property(own_headers ${target} PRECOMPILE_HEADERS)
    get_target_property(dispensed ${target} DISABLE_PRECOMPILE_HEADERS)
    # Une cible d'interface ou une commande (`ro_warnings`, `build_stamp`) ne
    # compile rien.
    if(target STREQUAL "moonclient_std_pch" OR own_headers OR dispensed
       OR type MATCHES "^(INTERFACE_LIBRARY|UTILITY)$")
      continue()
    endif()
    target_precompile_headers(${target} REUSE_FROM moonclient_std_pch)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${target} PRIVATE -Wno-error=invalid-pch)
    endif()
  endforeach()
endfunction()
