# CE QUI RACCOURCIT UNE RECONSTRUCTION, sans rien changer au programme bâti.
#
# Le coût ne tient pas dans le nombre de fichiers mais dans ce que chacun relit :
# un en-tête central touché recompile six cents fichiers, et chacun repasse par
# les mêmes en-têtes standard, par glm et, pour les essais, par Catch2.

# LES INFORMATIONS DE DÉBOGAGE DANS CHAQUE `.obj` (`/Z7`), en Debug.
#
# Avec `/Zi`, les compilations parallèles de `/MP` écrivent toutes dans un même
# `.pdb` par un service unique, et s'y attendent l'une l'autre. `/Z7` range ces
# informations dans l'objet ; l'éditeur de liens bâtit le `.pdb` du binaire comme
# avant, et le débogueur n'y voit aucune différence.
#
# ⚠ LA PUBLICATION N'EST PAS CONCERNÉE : elle garde son `/Zi` et son `.pdb`
# archivé (`shipping_symbols.cmake`). RelWithDebInfo garde aussi le sien : poser
# cette variable retire à toute configuration qu'elle ne nomme pas le format que
# CMake lui donnait.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    "$<$<CONFIG:Debug>:Embedded>$<$<CONFIG:RelWithDebInfo>:ProgramDatabase>")

# LES EN-TÊTES QUE PRESQUE TOUT LE MONDE RELIT, précompilés une fois par cible.
#
# Seulement des en-têtes qui ne portent rien du dépôt : un en-tête à nous
# précompilé recompilerait toute la cible dès qu'il change, c'est-à-dire
# exactement ce qu'on cherche à éviter.
#
# ⚠ UN FICHIER DOIT TOUJOURS INCLURE CE QU'IL EMPLOIE. L'en-tête précompilé le
# rend visible sans inclusion, mais seules les grosses cibles en ont un : un
# oubli compile ici et casse le jour où le fichier passe dans une petite.
set(MOONCLIENT_STANDARD_PCH
    <algorithm> <array> <cmath> <cstddef> <cstdint> <memory> <optional>
    <span> <string> <string_view> <utility> <vector>)

# `moonclient_precompile_headers(cible [GLM] [CATCH2])` : les en-têtes standard,
# plus glm et Catch2 pour les cibles qui en dépendent déjà.
function(moonclient_precompile_headers target)
  cmake_parse_arguments(PCH "GLM;CATCH2" "" "" ${ARGN})
  set(headers ${MOONCLIENT_STANDARD_PCH})
  if(PCH_GLM)
    list(APPEND headers <glm/glm.hpp>)
  endif()
  if(PCH_CATCH2)
    list(APPEND headers <catch2/catch_test_macros.hpp> <catch2/catch_approx.hpp>)
  endif()
  target_precompile_headers(${target} PRIVATE ${headers})
endfunction()
