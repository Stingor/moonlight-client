# ro_net — le transport : socket, découpage du flux, tampons.
#
# Cette couche ne connaît que ro_core et ro_proto. Elle transporte des OCTETS :
# elle ne décode aucun paquet et ne touche à aucun état de jeu. C'est ce qui
# permettra de la déplacer sur un fil dédié sans avoir à se demander ce qui est
# sûr d'être touché depuis où.
#
# Elle rejoindra ro_sim quand celle-ci existera, pour traduire les trames en
# événements de monde — pas avant : une dépendance déclarée d'avance ne serait
# vérifiée par rien.

add_library(ro_net STATIC
    src/achievement.cpp
    src/action.cpp
    src/character_select_return.cpp
    src/actor_guild.cpp
    src/actor_name.cpp
    src/attack_reach.cpp
    src/bank.cpp
    src/barter.cpp
    src/battleground.cpp
    src/card_album.cpp
    src/moonclient_handshake.cpp
    src/area_damage.cpp
    src/player_styles.cpp
    src/player_settings.cpp
    src/bug_report.cpp
    src/buying_store.cpp
    src/card_composition.cpp
    src/chat_author.cpp
    src/chat_room.cpp
    src/craft_session.cpp
    src/deal.cpp
    src/mail.cpp
    src/cart_choice.cpp
    src/cash_shop.cpp
    src/npc_illustration.cpp
    src/character.cpp
    src/chat_channels.cpp
    src/chat_target.cpp
    src/command_settings.cpp
    src/companion.cpp
    src/connection.cpp
    src/discord_relay.cpp
    src/frame_tally.cpp
    src/guild.cpp
    src/guild_requests.cpp
    src/inventory.cpp
    src/item_news.cpp
    src/inventory_request.cpp
    src/item_facts.cpp
    src/monster_sheet.cpp
    src/movement.cpp
    src/mvp_tracker.cpp
    src/player_bonuses.cpp
    src/player_config.cpp
    src/pvp_rank.cpp
    src/recovery.cpp
    src/refusal_text.cpp
    src/store_search.cpp
    src/repair_list.cpp
    src/snap.cpp
    src/spell_list.cpp
    src/talkbox.cpp
    src/flag_graffiti.cpp
    src/target_info.cpp
    src/title.cpp
    src/turn.cpp
    src/unit_master.cpp
    src/character_ledger.cpp
    src/quest_log.cpp
    src/character_notice.cpp
    src/class_change.cpp
    src/server_message.cpp
    src/server_message_table.cpp
    src/server_rules.cpp
    src/ui_caps.cpp
    src/vending.cpp
    src/handshake.cpp
    src/map_guidance.cpp
    src/map_name.cpp
    src/map_mode.cpp
    src/party.cpp
    src/party_requests.cpp
    src/pet_frames.cpp
    src/homunculus_frames.cpp
    src/companion_orders.cpp
    src/mercenary_frames.cpp
    src/member_statuses.cpp
    src/friends.cpp
    src/social_text.cpp
    src/session.cpp
    src/npc_dialog.cpp
    src/npc_shop.cpp
    src/skill.cpp
    src/socket.cpp
    src/skill_facts.cpp
    src/staff_requests.cpp
    src/storage_facts.cpp
    src/storage_list.cpp
    src/whisper.cpp
    src/world_notice.cpp
    src/world_skill.cpp
    src/world_arrivals.cpp
    src/world_relay.cpp
    src/warp_list.cpp
    src/warp_memo.cpp
    src/world_update.cpp
    src/zone_move.cpp)
add_library(ro::net ALIAS ro_net)

target_include_directories(ro_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ro_net
    PUBLIC ro_core ro_proto ro_sim
    PRIVATE ro_warnings)

# Winsock n'est pas dans la bibliothèque C du système : sans cette ligne, tous
# les appels réseau manquent à l'édition de liens.
if(WIN32)
    target_link_libraries(ro_net PRIVATE ws2_32)
endif()

# Les en-têtes que chaque source relit, précompilés : voir cmake/build_speed.cmake.
if(TARGET ro_net)
  moonclient_precompile_headers(ro_net)
endif()
