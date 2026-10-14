# ro_formats — un parseur par format de données client.
#
# Cette couche ne connaît QUE ro_core. Pas de système de fichiers (elle lit une
# ByteSource), pas d'état global, pas d'exception : toutes les fonctions
# faillibles rendent un Result. C'est ce qui la rend testable sur des archives
# synthétiques et exploitable par un fuzzer.
#
# zlib est liée en PRIVATE : elle n'apparaît dans aucun en-tête public, donc
# aucune couche supérieure n'en hérite. `stb_image` de même, et il porte en plus
# le SEUL point d'implémentation du binaire — voir `src/stb_image_impl.cpp`.

find_path(STB_INCLUDE_DIRS "stb_image.h" REQUIRED)

add_library(ro_formats STATIC
    src/act.cpp
    src/assist_address.cpp
    src/bgm_table.cpp
    src/talk_lines.cpp
    src/bmp.cpp
    src/bson.cpp
    src/emblem.cpp
    src/gat.cpp
    src/gif.cpp
    src/gif_transparent.cpp
    src/gnd.cpp
    src/gr2_arith.cpp
    src/gr2_file.cpp
    src/gr2_image_bands.cpp
    src/gr2_image_codec.cpp
    src/gr2_model.cpp
    src/gr2_objects.cpp
    src/gr2_section_codec.cpp
    src/jpeg.cpp
    src/png.cpp
    src/lua_source.cpp
    src/lua_table.cpp
    src/lua_calls.cpp
    src/monster_skill_info.cpp
    src/named_effects.cpp
    src/navi_tables.cpp
    src/grf.cpp
    src/grf_merge.cpp
    src/grf_writer.cpp
    src/image.cpp
    src/palette.cpp
    src/rsm.cpp
    src/rsw.cpp
    src/spr.cpp
    src/stb_image_impl.cpp
    src/str.cpp
    src/rathena_db_text.cpp
    src/strict_text.cpp
    src/tga.cpp
    src/thor.cpp
    src/water_field.cpp
    src/web_picture.cpp)
add_library(ro::formats ALIAS ro_formats)

target_include_directories(ro_formats PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# En SYSTEM : `stb_image` déploie des milliers de lignes de C qui ne sont pas
# les nôtres, et `/WX` ferait de ses avertissements des erreurs.
target_include_directories(ro_formats SYSTEM PRIVATE ${STB_INCLUDE_DIRS})

target_link_libraries(ro_formats
    PUBLIC ro_core
    PRIVATE ZLIB::ZLIB ro_warnings)
