// LES ÉVÉNEMENTS D'UNE IMAGE DE PARTIE : ce que SDL a reçu, vu d'abord par le
// compteur d'inactivité, puis par l'interface, enfin routé vers le jeu.

#include <SDL3/SDL.h>

#include "game/game.h"
#include "game/window_events.h"

namespace moonclient {

void Game::PumpEvents(std::uint32_t now_ms) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // 🔴 LE COMPTEUR D'INACTIVITÉ VOIT L'ÉVÉNEMENT LE PREMIER, ET PEUT L'AVALER. Il
    // faut décider d'avaler AU MOMENT MÊME où l'on constate le geste : un drapeau
    // « on s'est réveillé » consommé plus tard laisserait passer le clic de l'image
    // en cours — celui-là même qui déplacerait le personnage dans une direction que
    // le joueur n'a pas choisie, puisqu'il visait un décor vu sous un autre angle.
    if (NoteAfkInput(event, now_ms)) {
      continue;
    }
    // L'INTERFACE VOIT TOUT, AVANT TOUT LE MONDE : il lui faut avoir vu la souris
    // bouger, et ce qu'elle en dit décide à qui revient le geste (`window_events.h`).
    interface_->TakeEvent(event);
    // 🔴 UNE PARTIE DE SURVIVORS PREND LE CLAVIER, LA SOURIS ET LA MOLETTE : rien
    // n'en parvient au jeu, sauf les touches de l'enregistreur. Fenêtre, sortie
    // et redimensionnement passent, et la perte de focus, notée par le mode,
    // passe aussi.
    if (survivors_.owns_screen() && SurvivorsTakes(event, now_ms)) {
      continue;
    }
    RouteEvent({.gl = &*gl_,
                .interface = &*interface_,
                .link = &link_,
                .world = &world_,
                .stage = &*stage_,
                .planner = &planner_,
                .effects = &*effects_,
                .effect_nodes = &*effect_nodes_,
                .camera = &camera_,
                .rig = &camera_rig_,
                .chat = &windows_.chat.main,
                .grimoire = &grimoire_,
                .skill_tree = pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
                .sheet = &windows_.character,
                .status = &windows_.status.state,
                .basic_info = &windows_.basic_info.state,
                .grimoire_keys = &pieces_.grimoire_keys,
                .homunculus_present = pieces_.homunculus.view().present,
                .mercenary_present = pieces_.mercenary.view().present,
                .companion_ai = &pieces_.companion_ai,
                .pet = &windows_.pet,
                .inventory = &windows_.inventory,
                .cart = &windows_.cart,
                .party = &windows_.party.window,
                .bank = &bank_,
                .card_album = &card_album_,
                .card_album_window = &windows_.card_album,
                .cash_shop = &pieces_.cash_shop,
                .mail = &pieces_.mail,
                .achievements = &pieces_.achievements,
                .quests = &pieces_.quests,
                .memorial = &pieces_.memorial,
                .trade = &pieces_.trade,
                .chat_rooms = &pieces_.chat_rooms,
                .chat_room_form_open = windows_.chat_room.make.open,
                .menu = &windows_.menu,
                .macros = &windows_.macros,
                .style = &windows_.style,
                .craft_atlas = &windows_.craft_atlas.state,
                .navigation = &windows_.navigation.state,
                .world_map = &windows_.navigation.world_map,
                .minimap_shown = &shell_.layouts().minimap().shown,
                .mvp_tracker = &pieces_.mvp_tracker,
                .database_toggle = &windows_.monster_sheet.database.toggle_asked,
                .bug_report = &windows_.bug_report,
                .recorder = &recorder_,
                .hotkeys = &windows_.hotkeys,
                .bindings = &settings_->bindings(),
                .presets = &*presets_,
                .targeting = &targeting_,
                .quick_cast = &quick_cast_,
                .entity_menu = &menu_gestures_,
                .afk_now = &afk_requested_,
                .shortcuts = &*shortcuts_,
                .shortcut_wheel = &pieces_.shortcut_wheel,
                .escape_closes_a_window = interface_->escape_has_work(),
                .enter_claimed = CraftStage::ClaimsEnter(
                                     windows_.make_item, windows_.weapon_refine,
                                     settings_->options().pages().make_item.settings(),
                                     settings_->options().pages().weapon_refine.settings()) ||
                                 SurvivorsClaimsEnter(windows_.minigames),
                .no_shift = settings_->options().no_shift(),
                .no_ctrl = settings_->options().no_ctrl(),
                .shop_one_click = settings_->options().shops().one_click(),
                .bench = &effect_bench_,
                .map_editor = Rights().staff_tools && map_editor_.active() ? &map_editor_ : nullptr,
                .map_editor_view = &windows_.staff_tools.map_editor,
                .chat_dispatch = &*chat_dispatch_,
                .scene_actors = &scene_actors_,
                .mouse = &mouse_,
                .world_mouse = &world_mouse_,
                .running = &running_,
                .size = &size_,
                .aspect_ratio = &aspect_ratio_,
                .pickup = &pickup_,
                .attack = &attack_,
                .now_ms = now_ms},
               event);
  }
}

}  // namespace moonclient
