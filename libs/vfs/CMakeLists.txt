# ro_vfs — le système de fichiers virtuel du client : disque d'abord, puis les
# archives dans l'ordre que l'appelant donne.
#
# C'est ici, et NULLE PART ailleurs, que vit la frontière d'encodage. Au-dessus,
# tout est en UTF-8 ; en dessous, `formats` n'indexe que des octets bruts et
# ignore qu'un encodage existe.

# Les tables exportées du serveur, sous data\yaml_files\, sont du YAML.
find_package(yaml-cpp CONFIG REQUIRED)

add_library(ro_vfs STATIC
    src/achievement_catalog.cpp
    src/achievement_database.cpp
    src/achievement_titles.cpp
    src/achievement_words.cpp
    src/battle_words.cpp
    src/card_illustrations.cpp
    src/card_item_list.cpp
    src/cash_shop_words.cpp
    src/chat_room_words.cpp
    src/craft_words.cpp
    src/trade_words.cpp
    src/whisper_words.cpp
    src/encoding.cpp
    src/npc_illustration_paths.cpp
    src/item_link.cpp
    src/item_name.cpp
    src/item_info_text.cpp
    src/item_properties.cpp
    src/item_stacking.cpp
    src/item_table.cpp
    src/indoor_maps.cpp
    src/item_words.cpp
    src/job_names.cpp
    src/map_display_names.cpp
    src/map_music_table.cpp
    src/message_word.cpp
    src/navi_library.cpp
    src/package_items.cpp
    src/pet_tables.cpp
    src/pet_talk_table.cpp
    src/monster_talk_table.cpp
    src/random_option_names.cpp
    src/banned_word_list.cpp
    src/books.cpp
    src/monster_list.cpp
    src/server_db.cpp
    src/server_db_rates.cpp
    src/server_db_search.cpp
    src/monster_size_effects.cpp
    src/monster_words.cpp
    src/res_name_table.cpp
    src/skill_info.cpp
    src/skill_layout.cpp
    src/skill_tree_rules.cpp
    src/skill_words.cpp
    src/status_table.cpp
    src/talk_lines.cpp
    src/talkbox_words.cpp
    src/town_info.cpp
    src/world_view.cpp
    src/warp_agents.cpp
    src/warp_list_words.cpp
    src/warp_memo_words.cpp
    src/vfs.cpp)
add_library(ro::vfs ALIAS ro_vfs)

target_include_directories(ro_vfs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ro_vfs
    PUBLIC ro_core ro_formats
    PRIVATE yaml-cpp::yaml-cpp ro_warnings)

# Sous Windows le transcodage CP949 passe par l'API Win32 ; ailleurs, par iconv,
# qui fait partie de la libc sur glibc et demande une bibliothèque à part sinon.
if(NOT WIN32)
  find_package(Iconv)
  if(Iconv_FOUND AND NOT Iconv_IS_BUILT_IN)
    target_link_libraries(ro_vfs PRIVATE Iconv::Iconv)
  endif()
endif()
