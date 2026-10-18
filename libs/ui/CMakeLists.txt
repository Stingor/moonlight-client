# ro_ui — les fenêtres.
#
# ⚠ Cette bibliothèque ne possède AUCUN état de jeu. Elle reçoit une
# description de données en lecture seule et dessine ; ce qu'elle veut faire
# faire, elle le pose en intention et `app` la route. Une fenêtre qui ne peut
# pas muter l'état ne peut pas devenir le panneau de 3 000 lignes qui tripote
# les membres privés d'autrui — c'est le correctif que réclamait l'audit d'où
# viennent ces règles, et il est ici une contrainte de lien et non une
# intention.
#
# DEUX cibles, exactement pour la raison qui en donne deux à `render` :
#
#   ro_ui     le contexte ImGui et les fenêtres. Ni SDL, ni OpenGL, donc bâtie
#             et TESTÉE sur le runner Linux sous ASan, où SDL n'est même pas
#             téléchargé.
#   ro_ui_gl  le DOS : SDL3 pour les entrées, OpenGL 3 pour le dessin.
#
# ⚠ ET CETTE SÉPARATION-LÀ N'EST PAS PORTÉE PAR L'ÉDITEUR DE LIENS. vcpkg
# n'installe qu'UNE bibliothèque ImGui, dos compris : un fichier de `ro_ui` qui
# inclurait `imgui_impl_sdl3.h` compilerait et se lierait sans un mot. C'est le
# seul endroit du dépôt où la couche redeviendrait une convention, et c'est
# pourquoi `scripts/check_hygiene.py` la vérifie mécaniquement — voir
# `rule_ui_stays_platform_free`.

# 🔴 IMGUI EST BÂTI AVEC « wchar32 » (voir `vcpkg.json`), ET SANS ÇA AUCUN
# EMOJI NE PEUT S'AFFICHER. Par défaut `ImWchar` fait seize bits et
# `IM_UNICODE_CODEPOINT_MAX` vaut 0xFFFF : tout point de code au-dessus —
# c'est-à-dire TOUS les emoji, qui vivent au plan 1 (U+1F300…) — est remplacé par
# U+FFFD au décodage de l'UTF-8. Ce n'est ni une question de police ni d'atlas :
# le caractère n'atteint jamais le rendu, et l'on cherche longtemps du côté de la
# police.
#
# ⚠ L'OPTION CHANGE L'ABI D'IMGUI : `ImWchar` n'a plus la même taille, donc la
# bibliothèque et nous devons être d'accord. Elle se pose sur le PORT, qui la
# propage en définition d'interface — jamais dans nos seules cibles.
find_package(imgui CONFIG REQUIRED)

add_library(ro_ui STATIC
    src/achievement_panes.cpp
    src/achievement_window.cpp
    src/quest_window.cpp
    src/quest_window_detail.cpp
    src/quest_window_list.cpp
    src/tracker_panel.cpp
    src/afk_clock.cpp
    src/align_grid.cpp
    src/bank_window.cpp
    src/status_window.cpp
    src/stat_preset_window.cpp
    src/entry_queue_prompts.cpp
    src/entry_queue_window.cpp
    src/memorial_dungeon_window.cpp
    src/barter_window.cpp
    src/buying_store_log_window.cpp
    src/buying_store_own_window.cpp
    src/buying_store_sell_window.cpp
    src/buying_store_setup_window.cpp
    src/bitmap_button.cpp
    src/card_album_book.cpp
    src/card_album_dialogs.cpp
    src/card_album_link.cpp
    src/card_album_window.cpp
    src/card_drop.cpp
    src/card_insert_window.cpp
    src/cart_choice_window.cpp
    src/party_booking_rules.cpp
    src/party_booking_window.cpp
    src/ranking_window.cpp
    src/repair_window.cpp
    src/spell_list_window.cpp
    src/cash_shop_slots.cpp
    src/cash_shop_window.cpp
    src/npc_illustration_window.cpp
    src/character_doll.cpp
    src/character_doll_gif.cpp
    src/character_staff.cpp
    src/character_stats.cpp
    src/character_titles.cpp
    src/craft_atlas_grants.cpp
    src/craft_atlas_window.cpp
    src/craft_window_parts.cpp
    src/make_item_window.cpp
    src/market_window.cpp
    src/item_sheet.cpp
    src/database_labels.cpp
    src/database_window.cpp
    src/weapon_refine_window.cpp
    src/character_window.cpp
    src/preset_tab.cpp
    src/chat_bar.cpp
    src/chat_bubble.cpp
    src/broadcast_balloon.cpp
    src/chat_frame.cpp
    src/chat_command_hints.cpp
    src/chat_command_list.cpp
    src/chat_input.cpp
    src/chat_input_chips.cpp
    src/chat_input_commands.cpp
    src/chat_whisper_box.cpp
    src/chat_emote_names.cpp
    src/chat_link.cpp
    src/chat_link_offer.cpp
    src/chat_log.cpp
    src/chat_log_place.cpp
    src/chat_macros.cpp
    src/chat_markup.cpp
    src/chat_picture_viewer.cpp
    src/chat_rich_art.cpp
    src/chat_room_make_window.cpp
    src/chat_room_password_window.cpp
    src/chat_room_window.cpp
    src/whisper_window.cpp
    src/chat_strip.cpp
    src/chat_tab_menu.cpp
    src/chat_url_prompt.cpp
    src/chat_window.cpp
    src/commands_page.cpp
    src/commands_page_autoloot.cpp
    src/commands_page_presets.cpp
    src/commands_page_texts.cpp
    src/emblem_canvas.cpp
    src/guild_confirmations.cpp
    src/guild_emblem_modal.cpp
    src/guild_emblem_paint.cpp
    src/guild_lists.cpp
    src/guild_members.cpp
    src/guild_positions.cpp
    src/guild_skills.cpp
    src/guild_tab.cpp
    src/homunculus_tab.cpp
    src/homunculus_staff.cpp
    src/mercenary_tab.cpp
    src/pet_hatch_window.cpp
    src/pet_window.cpp
    src/profiles_page.cpp
    src/loading_screen.cpp
    src/patch_screen.cpp
    src/list_keys.cpp
    src/log_window.cpp
    src/character_create_window.cpp
    src/character_delete_window.cpp
    src/character_select_window.cpp
    src/character_select_customize.cpp
    src/character_select_amend.cpp
    src/character_select_bars.cpp
    src/seat_layout.cpp
    src/web_door.cpp
    src/login_window.cpp
    src/npc_dialog_layout.cpp
    src/npc_dialog_parse.cpp
    src/npc_dialog_window.cpp
    src/npc_shop_window.cpp
    src/npc_shop_saved_carts.cpp
    src/minimap_battle_mates.cpp
    src/minimap_layers.cpp
    src/minimap_settings.cpp
    src/minimap_window.cpp
    src/world_map_flight.cpp
    src/world_map_warp_agent.cpp
    src/world_map_marks.cpp
    src/world_map_window.cpp
    src/modal_box.cpp
    src/link_notice.cpp
    src/chat_block_log_window.cpp
    src/player_report_window.cpp
    src/entity_inspector_window.cpp
    src/monster_sheet_preview.cpp
    src/monster_sheet_tables.cpp
    src/monster_sheet_text.cpp
    src/monster_sheet_window.cpp
    src/mvp_tracker_overlays.cpp
    src/mvp_tracker_parts.cpp
    src/mvp_tracker_table.cpp
    src/mvp_tracker_window.cpp
    src/nav_panel.cpp
    src/navigation_results.cpp
    src/navigation_search_tip.cpp
    src/navigation_window.cpp
    src/options_rows.cpp
    src/options_search.cpp
    src/options_window.cpp
    src/packet_window.cpp
    src/party_friend_dialogs.cpp
    src/party_friend_rows.cpp
    src/party_friend_window.cpp
    src/skin_art.cpp
    src/escape_stack.cpp
    src/font_atlas_build.cpp
    src/frame_meter.cpp
    src/damage_meter_window.cpp
    src/damage_meter_records.cpp
    src/damage_meter_group.cpp
    src/game_menu.cpp
    src/music_player_window.cpp
    src/menu_icons.cpp
    src/map_sign_banner.cpp
    src/skin_popup.cpp
    src/skin_tooltip.cpp
    src/signboard_tip.cpp
    src/hotkey_window.cpp
    src/inventory_cell_menu.cpp
    src/inventory_footer.cpp
    src/inventory_window.cpp
    src/cart_window.cpp
    src/storage_rows.cpp
    src/storage_shelves.cpp
    src/storage_window.cpp
    src/transfer_freeze.cpp
    src/item_grid.cpp
    src/item_link_cell.cpp
    src/item_obtain_toast.cpp
    src/shortcut_bar.cpp
    src/skin_palette.cpp
    src/system_fonts.cpp
    src/skin_look_sample.cpp
    src/skin_tabs.cpp
    src/skin_slider.cpp
    src/skin_stepper.cpp
    src/skin_text.cpp
    src/skin_combo.cpp
    src/description_odds.cpp
    src/description_panels.cpp
    src/doll_overlay.cpp
    src/bug_report_window.cpp
    src/book_window.cpp
    src/skill_description_window.cpp
    src/description_script.cpp
    src/description_server_tabs.cpp
    src/description_window.cpp
    src/table_cells.cpp
    src/rich_text.cpp
    src/skin_widgets.cpp
    src/skin_scrollbar.cpp
    src/hud_frame.cpp
    src/pvp_hud_frames.cpp
    src/pick_boxes.cpp
    src/party_grid.cpp
    src/party_grid_tile.cpp
    src/sound_zones.cpp
    src/map_cells.cpp
    src/map_editor_effects_panel.cpp
    src/map_editor_ground_panel.cpp
    src/map_editor_panel.cpp
    src/map_editor_row_menu.cpp
    src/staff_tools.cpp
    src/staff_skill_id.cpp
    src/unlock_notice.cpp
    src/quick_cast_look.cpp
    src/camera_hud.cpp
    src/zone_capture.cpp
    src/stat_bars.cpp
    src/entity_bars.cpp
    src/gauge_fill.cpp
    src/entity_sample.cpp
    src/cast_bars.cpp
    src/damage_texts.cpp
    src/path_canvas.cpp
    src/deal_proposal.cpp
    src/deal_window.cpp
    src/mail_window.cpp
    src/mail_compose_window.cpp
    src/entity_menu.cpp
    src/store_search_window.cpp
    src/target_hud.cpp
    src/hud_caption.cpp
    src/status_portrait.cpp
    src/trade_widgets.cpp
    src/vending_setup_window.cpp
    src/vending_shop_window.cpp
    src/vending_visit_window.cpp
    src/view_equip_window.cpp
    src/warp_list_window.cpp
    src/talkbox_window.cpp
    src/companion_tactic_window.cpp
    src/companion_ai_window.cpp
    src/companion_ai_settings.cpp
    src/companion_ai_rules.cpp
    src/companion_ai_profiles_ui.cpp
    src/companion_ai_players_ui.cpp
    src/status_bar.cpp
    src/status_preview.cpp
    src/status_cell.cpp
    src/palette_editor.cpp
    src/palette_pickers.cpp
    src/thumbnail_grid.cpp
    src/palette_doll.cpp
    src/style_preview.cpp
    src/skill_cell.cpp
    src/skill_list.cpp
    src/skill_chain.cpp
    src/skill_window.cpp
    src/skinned_window.cpp
    src/desc_panel_frame.cpp
    src/ui_context.cpp
    src/window_bounds.cpp
    src/window_places.cpp
    src/triple_clipboard.cpp
    src/hour_range_text.cpp
    src/wheel_drag.cpp
    src/wheel_gate.cpp
    src/window_name.cpp)
add_library(ro::ui ALIAS ro_ui)

target_include_directories(ro_ui PUBLIC include)

# `render` est dans le DAG de `ui` et y restera : une fenêtre finira par montrer
# une image du jeu — une icône d'objet, un portrait — et c'est `render` qui sait
# les lire. Rien n'en a encore besoin, mais la dépendance est déclarée là où le
# graphe l'autorise plutôt que découverte le jour où elle manque.
target_link_libraries(ro_ui
    PUBLIC ro_core ro_render imgui::imgui
    PRIVATE ro_warnings)

if(NOT MOONCLIENT_PLATFORM_LAYERS)
    return()
endif()

find_package(SDL3 CONFIG REQUIRED)

add_library(ro_ui_gl STATIC
    gl/ui_backend_gl.cpp)
add_library(ro::ui_gl ALIAS ro_ui_gl)

target_include_directories(ro_ui_gl PUBLIC gl)

target_link_libraries(ro_ui_gl
    PUBLIC ro_ui
    PRIVATE SDL3::SDL3 ro_warnings)

# Les en-têtes que chaque source relit, précompilés : voir cmake/build_speed.cmake.
if(TARGET ro_ui)
  moonclient_precompile_headers(ro_ui GLM)
endif()
