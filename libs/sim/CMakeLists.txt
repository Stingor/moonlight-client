# ro_sim — le monde : acteurs, cases, déplacements.
#
# Cette couche ne connaît que ro_core et ro_formats. Elle ne sait RIEN du
# protocole : c'est ro_net qui traduit les trames reçues en changements de
# monde, et cette frontière est ce qui permet de rejouer une session sans
# socket, ou d'en fabriquer une à la main dans un essai.
#
# Elle ne connaît pas non plus le rendu. Le renderer reçoit une description de
# données produite par app, jamais le monde lui-même.

add_library(ro_sim STATIC
    src/achievement_book.cpp
    src/barter.cpp
    src/battlefield.cpp
    src/entry_queue.cpp
    src/memorial_dungeon.cpp
    src/market.cpp
    src/long_walk.cpp
    src/buying_store.cpp
    src/buying_store_checks.cpp
    src/card_album.cpp
    src/card_composition.cpp
    src/cart_choice.cpp
    src/cash_shop.cpp
    src/chat_mention.cpp
    src/chat_room.cpp
    src/damage_meter.cpp
    src/damage_meter_feed.cpp
    src/meter_tally.cpp
    src/deal.cpp
    src/mailbox.cpp
    src/monster_sheet.cpp
    src/mvp_tracker.cpp
    src/quest_log.cpp
    src/motion_events.cpp
    src/movement.cpp
    src/navi_route.cpp
    src/navi_guide.cpp
    src/navi_search.cpp
    src/grimoire.cpp
    src/guild.cpp
    src/map_viewpoints.cpp
    src/navigation_request.cpp
    src/inventory.cpp
    src/item_facts.cpp
    src/item_news.cpp
    src/broadcast_balloon.cpp
    src/packed_position.cpp
    src/party_roster.cpp
    src/party_leave.cpp
    src/friend_list.cpp
    src/member_statuses.cpp
    src/pet_egg.cpp
    src/pet.cpp
    src/pet_talk.cpp
    src/monster_talk.cpp
    src/homunculus.cpp
    src/mercenary.cpp
    src/companion_ai.cpp
    src/companion_sight.cpp
    src/companion_server_rules.cpp
    src/companion_skills.cpp
    src/companion_tactics.cpp
    src/companion_brain_skills.cpp
    src/companion_brain_targets.cpp
    src/companion_brain_rules.cpp
    src/companion_brain_conditions.cpp
    src/companion_statuses.cpp
    src/companion_rules.cpp
    src/companion_players.cpp
    src/companion_brain_players.cpp
    src/companion_brain_memory.cpp
    src/death_rules.cpp
    src/equip_compare.cpp
    src/ground_skill_log.cpp
    src/pending_action.cpp
    src/pickup_intent.cpp
    src/pvp_rank.cpp
    src/quest_markers.cpp
    src/server_clock.cpp
    src/npc_dialog.cpp
    src/player_status.cpp
    src/shortcut_bar.cpp
    src/sight_purge.cpp
    src/skill_banner.cpp
    src/skill_side_effects.cpp
    src/party_booking.cpp
    src/skill_book.cpp
    src/skill_tree.cpp
    src/skill_use_block.cpp
    src/status_effects.cpp
    src/strike_log.cpp
    src/skill_facts.cpp
    src/skill_unit_look.cpp
    src/storage_facts.cpp
    src/store_search.cpp
    src/style.cpp
    src/stat_presets.cpp
    src/stat_raise_run.cpp
    src/style_code.cpp
    src/summon_master.cpp
    src/title_book.cpp
    src/vending.cpp
    src/walk_map.cpp
    src/walk_field.cpp
    src/walk_path.cpp
    src/warp_menu.cpp
    src/whisper.cpp
    src/whisper_blocks.cpp
    src/banned_words.cpp
    src/adoption.cpp
    src/actor_traces.cpp
    src/world.cpp
    src/world_bearers.cpp
    src/world_combo.cpp
    src/world_blade_stop.cpp
    src/world_glide.cpp
    src/world_holdings.cpp
    src/world_devotion.cpp
    src/world_effect_list.cpp
    src/world_facing.cpp
    src/world_spirit_charm.cpp
    src/world_npc_dialog.cpp
    src/npc_illustration.cpp
    src/progress_bar.cpp
    src/npc_shop.cpp
    src/npc_shop_cart.cpp
    src/world_pvp_rings.cpp
    src/world_skill_unit_speech.cpp
    src/world_support.cpp)
add_library(ro::sim ALIAS ro_sim)

target_include_directories(ro_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ro_sim
    PUBLIC ro_core ro_formats
    PRIVATE ro_warnings)

# Les en-têtes que chaque source relit, précompilés : voir cmake/build_speed.cmake.
if(TARGET ro_sim)
  moonclient_precompile_headers(ro_sim)
endif()
