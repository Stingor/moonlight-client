# ro_mapedit — l'édition des cartes : le document d'une carte, ses opérations,
# leur vérification et leur application sur les données lues, l'historique et
# la sélection de l'éditeur. Voir docs/adr/0118.
#
# PURE : ni OpenGL, ni monde, ni système de fichiers. Elle se vérifie sans
# fenêtre, et `render` l'applique au chargement d'une carte.

find_package(yaml-cpp CONFIG REQUIRED)

add_library(ro_mapedit STATIC
    src/edit_history.cpp
    src/ground_edit.cpp
    src/ground_edit_yaml.cpp
    src/map_edit.cpp
    src/map_edit_header.cpp
    src/map_edit_sprite_yaml.cpp
    src/map_edit_yaml.cpp
    src/map_sprite.cpp
    src/sound_edit.cpp
    src/sound_edit_yaml.cpp)
add_library(ro::mapedit ALIAS ro_mapedit)

target_include_directories(ro_mapedit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ro_mapedit
    PUBLIC ro_core ro_formats
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)
