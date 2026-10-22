#include "game/window_events.h"

#include <optional>
#include <string>

#include "ro/core/i18n.h"
#include "ro/net/action.h"
#include "ro/render/actor_pick.h"
#include "ro/render/ground_pick.h"
#include "ro/render/model_pick.h"
#include "ro/sim/world.h"

#include "game/chat_room_stage.h"
#include "game/companion_ai_stage.h"
#include "game/companion_gestures.h"
#include "game/map_editor_keys.h"
#include "game/trade_stages.h"
#include "input/key_action.h"
#include "input/key_gate.h"
#include "world/attack_intent.h"
#include "world/click_action.h"
#include "world/entity_menu.h"
#include "world/follow_action.h"
#include "world/signboard_pick.h"

namespace moonclient {
namespace {

// La taille de la FENÊTRE, en pixels de fenêtre — celle où la souris vit.
//
// ⚠ CE N'EST PAS LA TAILLE DU TAMPON DE RENDU. Sur un écran à facteur
// d'échelle les deux diffèrent, et s'en remettre à l'un pour l'autre fait viser
// à côté d'autant.
struct WindowPixels {
  float width = 0.0F;
  float height = 0.0F;
};

[[nodiscard]] WindowPixels PointerSpaceOf(const ro::render::GlContext& gl) {
  int width = 0;
  int height = 0;
  SDL_GetWindowSize(gl.window(), &width, &height);
  return {static_cast<float>(width), static_cast<float>(height)};
}

[[nodiscard]] CompanionPointer CompanionPointerOf(const EventRouting& to, glm::vec2 pointer) {
  const WindowPixels window = PointerSpaceOf(*to.gl);
  return {.world = to.world,
          .stage = to.stage,
          .camera = to.camera,
          .drawn = to.scene_actors->drawn,
          .drawn_ids = to.scene_actors->ids,
          .aspect_ratio = *to.aspect_ratio,
          .viewport_width = window.width,
          .viewport_height = window.height,
          .pointer = pointer};
}

// UN CLIC QUI OUVRE UNE ÉCHOPPE : sur son enseigne, ou sur le marchand qui la
// porte, comme au client d'origine. Rend vrai s'il a été pris — il ne fait
// alors ni marcher ni frapper.
//
// ⚠ UNE COMPÉTENCE ARMÉE GARDE LE CLIC : c'est une visée, et un soin se lance
// justement sur un marchand. ⚠ ET LE MARCHAND NE PREND LE CLIC QUE S'IL LE
// LAISSAIT DESCENDRE AU SOL : sur une carte où il se frappe, le clic reste une
// attaque, et l'échoppe s'ouvre par l'enseigne.
//
// ⚠ SANS `/shopping`, SEUL LE SECOND APPUI D'UN DOUBLE-CLIC OUVRE. Le premier,
// tombé sur une enseigne, est tout de même pris : elle couvre le sol, et le
// clic n'y fait pas marcher.
//
// ⚠ UNE ENSEIGNE EST UNE FENÊTRE, LE MARCHAND EST DU MONDE : sous une souris
// figée (`world_takes_clicks` faux), seule l'enseigne répond encore.
// 🔴 UN MARCHAND TROP LOIN se rejoint d'abord : voir `ArmStoreApproach`.
void VisitOrApproachStore(const EventRouting& to, std::uint32_t owner_id,
                          ro::sim::SignboardKind kind) {
  if (ArmStoreApproach(*to.world, owner_id, *to.attack, *to.pickup)) {
    to.grimoire->CancelPending();
  } else {
    to.trade->VisitStore(owner_id, kind);
  }
}

[[nodiscard]] bool OpensStore(const EventRouting& to, glm::vec2 pointer, WindowPixels window,
                              bool shift_held, bool second_of_double, bool world_takes_clicks) {
  if (to.trade == nullptr || to.grimoire->aim() != SkillAim::kNothing) {
    return false;
  }
  // L'ENSEIGNE D'UN SALON ne s'ouvre qu'au double-clic, `/shopping` ou non :
  // c'est la seule porte que le client d'origine lui donne. Le premier appui
  // est pris quand même — l'enseigne couvre le sol.
  if (const std::optional<SignboardHit> top = TopSignboardAt(to.trade->signboards(), pointer);
      top.has_value() && top->kind == ro::sim::SignboardKind::kChatRoom) {
    const ro::sim::Actor* host = to.world->Find(top->owner_id);
    if (second_of_double && to.chat_rooms != nullptr && host != nullptr &&
        host->signboard.has_value()) {
      to.chat_rooms->AskJoin(host->signboard->room_id, host->signboard->status);
    }
    return true;
  }
  const bool opens = to.shop_one_click || second_of_double;
  if (const std::optional<SignboardHit> board = to.trade->SignboardAt(pointer); board.has_value()) {
    if (opens) {
      VisitOrApproachStore(to, board->owner_id, board->kind);
    }
    return true;
  }
  if (!opens || !world_takes_clicks) {
    return false;
  }
  if (to.scene_actors == nullptr) {
    return false;
  }
  const std::optional<std::size_t> picked =
      ro::render::PickActor(*to.camera, *to.aspect_ratio, to.scene_actors->drawn, pointer.x,
                            pointer.y, window.width, window.height);
  if (!picked.has_value() || *picked >= to.scene_actors->ids.size()) {
    return false;
  }
  const ro::sim::Actor* merchant = to.world->Find(to.scene_actors->ids[*picked]);
  if (merchant == nullptr || merchant->id == to.world->player_id() ||
      !merchant->signboard.has_value() || !SignboardOpensStore(merchant->signboard->kind) ||
      ActorTakesClick(*merchant, VersusGateOf(*to.world, shift_held, to.no_shift))) {
    return false;
  }
  VisitOrApproachStore(to, merchant->id, merchant->signboard->kind);
  return true;
}

// LE DIALOGUE EST-IL À L'ÉCRAN, ET MONTRE-T-IL UNE LISTE ? La fenêtre suit le
// monde sans rien décider : c'est donc le monde qui répond, et la boucle n'a pas
// à interroger l'affichage.
[[nodiscard]] bool NpcDialogShown(const ro::sim::World* world) {
  return world != nullptr && world->npc_dialog().has_value();
}

[[nodiscard]] bool NpcMenuShown(const ro::sim::World* world) {
  return NpcDialogShown(world) && world->npc_dialog()->prompt == ro::sim::NpcPrompt::kMenu;
}

[[nodiscard]] KeyPress KeyPressOf(const EventRouting& to) {
  return {.chat = to.chat,
          .skills = to.grimoire,
          .sheet = to.sheet,
          .homunculus_present = to.homunculus_present,
          .mercenary_present = to.mercenary_present,
          .status = to.status,
          .basic_info = to.basic_info,
          .companion_ai = to.companion_ai,
          .pet = to.pet,
          .inventory = to.inventory,
          .cart = to.cart,
          .party = to.party,
          .bank = to.bank,
          .card_album = to.card_album,
          .card_album_window = to.card_album_window,
          .cash_shop = to.cash_shop,
          .mail = to.mail,
          .achievements = to.achievements,
          .quests = to.quests,
          .memorial = to.memorial,
          .chat_rooms = to.chat_rooms,
          .menu = to.menu,
          .macros = to.macros,
          .style = to.style,
          .craft_atlas = to.craft_atlas,
          .navigation = to.navigation,
          .world_map = to.world_map,
          .minimap_shown = to.minimap_shown,
          .mvp_tracker = to.mvp_tracker,
          .market = to.trade != nullptr ? &to.trade->market() : nullptr,
          .database_toggle = to.database_toggle,
          .bug_report = to.bug_report,
          .recorder = to.recorder,
          .bindings = to.bindings,
          .hotkeys = to.hotkeys,
          .presets = to.presets,
          .targeting = to.targeting,
          .quick_cast = to.quick_cast,
          .afk_now = to.afk_now,
          .shortcuts = to.shortcuts,
          .bench = to.bench,
          .camera = to.camera,
          .rig = to.rig,
          .link = to.link,
          .world = to.world,
          .attack = to.attack,
          .planner = to.planner,
          .window = to.gl->window(),
          .escape_closes_a_window = to.escape_closes_a_window,
          .enter_claimed = to.enter_claimed,
          .running = to.running,
          .now_ms = to.now_ms};
}

[[nodiscard]] ro::render::Camera FramedMapOf(const EventRouting& to) {
  const WindowPixels window = PointerSpaceOf(*to.gl);
  const float aspect = window.height > 0.0F ? window.width / window.height : 1.0F;
  return WholeMapCamera(*to.stage, aspect);
}

void OnKeyDown(const EventRouting& to, const SDL_KeyboardEvent& key, bool text_input) {
  // LE MODE ÉDITEUR prend ses touches avant le jeu : voir `TakeMapEditorKey`.
  if (to.map_editor != nullptr && TakeMapEditorKey(*to.map_editor, key, text_input)) {
    return;
  }
  KeyPress press = KeyPressOf(to);
  press.text_input = text_input;
  const std::optional<std::string> said = HandleKey(press, key, FramedMapOf(to));
  if (said.has_value()) {
    SubmitChat(*to.chat_dispatch, *said, to.now_ms);
  }
}

// L'APPUI COMPLÈTE UN DOUBLE-CLIC. Le gauche et le droit suivent la règle du
// jeu, `DoubleClickClocks` ; le bouton du milieu, que le jeu ne lit pas, garde
// celle du système.
//
// ⚠ UN SEUL APPEL PAR APPUI : chaque appel avance l'horloge du bouton.
[[nodiscard]] bool SecondOfDouble(const EventRouting& to, const SDL_MouseButtonEvent& button) {
  if (button.button == SDL_BUTTON_LEFT) {
    return to.world_mouse->double_click.NotePress(ClickButton::kLeft, to.now_ms);
  }
  if (button.button == SDL_BUTTON_RIGHT) {
    return to.world_mouse->double_click.NotePress(ClickButton::kRight, to.now_ms);
  }
  return button.clicks >= 2;
}

// Un événement de souris ne porte pas les modificateurs : on les relit.
[[nodiscard]] std::optional<ActionRef> ActionOfGesture(const EventRouting& to,
                                                       MouseGesture gesture) {
  if (to.bindings == nullptr) {
    return std::nullopt;
  }
  const SDL_Keymod held = SDL_GetModState();
  return to.bindings->ActionForGesture(gesture, (held & SDL_KMOD_CTRL) != 0,
                                       (held & SDL_KMOD_SHIFT) != 0, (held & SDL_KMOD_ALT) != 0);
}

[[nodiscard]] bool CtrlHeld() { return (SDL_GetModState() & SDL_KMOD_CTRL) != 0; }

// LE COMPAGNON QU'UN CLIC SUR LE MONDE COMMANDE, d'après la table des
// raccourcis. ⚠ MAJ N'EN FAIT PAS PARTIE : il range l'ordre dans la file, et
// aucune liaison d'ordre ne le nomme.
[[nodiscard]] std::optional<ro::sim::AiCompanion> OrderedBy(const EventRouting& to,
                                                            MouseGesture click, SDL_Keymod held) {
  if (to.bindings == nullptr) {
    return std::nullopt;
  }
  const std::optional<ActionRef> action =
      to.bindings->ActionFor(KeyOfGesture(click), (held & SDL_KMOD_CTRL) != 0,
                             /*shift=*/false, (held & SDL_KMOD_ALT) != 0);
  if (action == ActionRef(GameAction::kHomunculusOrder)) {
    return ro::sim::AiCompanion::kHomunculus;
  }
  if (action == ActionRef(GameAction::kMercenaryOrder)) {
    return ro::sim::AiCompanion::kMercenary;
  }
  return std::nullopt;
}

void OnDoubleClick(const EventRouting& to, MouseGesture gesture) {
  const std::optional<ActionRef> action = ActionOfGesture(to, gesture);
  if (!action.has_value()) {
    return;
  }
  const std::optional<std::string> said =
      TriggerAction(KeyPressOf(to), *action, /*repeat=*/false, FramedMapOf(to));
  if (said.has_value()) {
    SubmitChat(*to.chat_dispatch, *said, to.now_ms);
  }
}

void OnDrag(const EventRouting& to, MouseGesture gesture, const SDL_MouseMotionEvent& motion) {
  const std::optional<ActionRef> action = ActionOfGesture(to, gesture);
  if (!action.has_value() || !action->named().has_value() || to.rig == nullptr) {
    return;
  }
  CameraDrag drag{.dx_px = motion.xrel, .dy_px = motion.yrel};
  switch (*action->named()) {
    case GameAction::kCameraOrbit:
      drag.orbit = true;
      break;
    case GameAction::kCameraTilt:
      drag.tilt = true;
      break;
    case GameAction::kCameraOrbitZoom:
      drag.orbit = true;
      drag.zoom = true;
      break;
    case GameAction::kCameraTiltZoom:
      drag.tilt = true;
      drag.zoom = true;
      break;
    default:
      return;
  }
  DragCamera(*to.camera, drag, to.rig->limits(), CtrlHeld());
  to.rig->NoteDrag();
}

// `second_of_double` : l'appui complète un double-clic, selon `DoubleClickClock`.
void OnLeftPress(const EventRouting& to, const SDL_MouseButtonEvent& button,
                 bool second_of_double) {
  to.world_mouse->held_click = HeldClick::kNothing;
  const WindowPixels window = PointerSpaceOf(*to.gl);
  const glm::vec2 pointer{button.x, button.y};
  // LE BANC AVANT TOUT, et avant même la garde de la liaison : il sert à
  // juger une figure, ce qui ne demande rien au serveur.
  if (to.bench->Click(*to.effects, *to.effect_nodes, *to.world, *to.stage, *to.planner, pointer,
                      window.width, window.height, to.now_ms)) {
    return;
  }
  // LE MODE ÉDITEUR : un clic dans le monde y désigne le modèle sous la souris,
  // et ne fait rien d'autre — ni marcher, ni viser. Ctrl+clic le traverse : le
  // clic redevient celui du jeu, Ctrl compris comme absent.
  const bool through_editor = to.map_editor != nullptr && CtrlHeld();
  // Marcher ramène la caméra sur le personnage.
  if (through_editor) {
    to.map_editor->ReleaseViewPoint();
  }
  if (to.map_editor != nullptr && !through_editor) {
    // Une poignée sous l'appui se saisit, avant de désigner quoi que ce soit.
    if (to.map_editor->Grab(
            pointer, to.map_editor_view != nullptr ? *to.map_editor_view : ro::ui::MapEditorView(),
            (SDL_GetModState() & SDL_KMOD_SHIFT) != 0)) {
      return;
    }
    if (window.height > 0.0F) {
      const ro::render::ScreenRay ray =
          ro::render::MakeScreenRay(*to.camera, window.width / window.height, pointer.x, pointer.y,
                                    window.width, window.height);
      to.map_editor->Pick(ro::render::PickSceneModel(to.stage->scene, ray));
    }
    return;
  }
  if (!to.link->is_open() || to.chat_room_form_open) {
    return;
  }
  // Le mode Ciblage rejoue le double-clic à l'image suivante, après le clic qui
  // l'accompagne.
  if (to.targeting != nullptr) {
    to.targeting->NoteWorldPress(second_of_double);
  }
  // ⚠ UN ÉVÉNEMENT DE SOURIS NE PORTE PAS LES MODIFICATEURS, contrairement à un
  // événement de clavier : il faut les demander à l'état courant.
  const SDL_Keymod modifiers = through_editor
                                   ? static_cast<SDL_Keymod>(SDL_GetModState() & ~SDL_KMOD_CTRL)
                                   : SDL_GetModState();
  const bool shift_held = (modifiers & SDL_KMOD_SHIFT) != 0;
  const bool ctrl_held = (modifiers & SDL_KMOD_CTRL) != 0;
  const bool world_takes_clicks = WorldTakesClicks(
      MouseRouteOf(*to.world, *to.grimoire, to.world_mouse->engagement, to.now_ms));
  if (OpensStore(to, pointer, window, shift_held, second_of_double, world_takes_clicks)) {
    return;
  }
  // 🔴 LA SOURIS FIGÉE — mort, dialogue, commerce, coffre — NE FAIT PLUS QUE
  // DÉSARMER : ni marche, ni attaque, ni contact, ni ordre aux compagnons.
  if (!world_takes_clicks) {
    to.grimoire->Disarm();
    return;
  }
  const bool continuous = Targeting::WantsContinuousAttack(
      SiegeMap(to.world_mouse->engagement, to.world->map_mode()), ctrl_held, to.no_ctrl);
  WorldClick scene{to.link,
                   to.world,
                   to.stage,
                   to.camera,
                   to.grimoire,
                   to.scene_actors->drawn,
                   to.scene_actors->ids,
                   *to.aspect_ratio,
                   window.width,
                   window.height,
                   to.planner,
                   shift_held,
                   VersusGateOf(*to.world, shift_held, to.no_shift),
                   continuous,
                   to.now_ms,
                   to.targeting,
                   to.entity_menu};
  scene.engagement = &to.world_mouse->engagement;
  scene.alt_held = (modifiers & SDL_KMOD_ALT) != 0;
  scene.second_of_double = second_of_double;
  scene.ctrl_held = ctrl_held;
  scene.no_shift = to.no_shift;
  scene.ties = to.world_mouse->ties;
  scene.ground_units = to.scene_actors->ground_units;
  scene.ground_unit_floor_pixels = to.scene_actors->ground_unit_floor_pixels;
  // LES COMPAGNONS AVANT LE PERSONNAGE : un skill de compagnon armé, ou le
  // clic gauche qu'une liaison d'ordre nomme.
  if (to.companion_ai != nullptr &&
      CompanionLeftPress(*to.companion_ai, CompanionPointerOf(to, pointer),
                         to.grimoire->aim() != SkillAim::kNothing,
                         OrderedBy(to, MouseGesture::kLeftClick, modifiers), shift_held,
                         scene.second_of_double)) {
    return;
  }
  const HeldClick held = LeftClick(scene, pointer,
                                   ClickIntents{.pickup = *to.pickup,
                                                .attack = *to.attack,
                                                .walk_clock = to.grimoire->approach_clock(),
                                                .deferred_walk = &to.world_mouse->deferred_walk,
                                                .player_gesture = to.scene_actors->player_gesture});
  // Ce qui suit tant que le bouton reste enfoncé : le jeu marche — ou tourne —
  // tant qu'on tient, et vers là où la souris pointe alors.
  to.world_mouse->held_click = held;
  to.world_mouse->held_since_ms = to.now_ms;
}

// TOUT APPUI GAUCHE, fenêtres comprises. `second_of_double` : voir
// `SecondOfDouble`.
void OnLeftButtonDown(const EventRouting& to, const SDL_MouseButtonEvent& button,
                      bool over_interface, bool second_of_double) {
  // TOUT APPUI GAUCHE ARRÊTE LE SUIVI, fenêtre ou monde : le tic le lira.
  to.world_mouse->follow.left_pressed = to.world_mouse->follow.active();
  // 🔴 TOUT APPUI GAUCHE COMPTE POUR L'ENGAGEMENT, celui qui tombe sur une
  // fenêtre compris : continu, il le fait retomber, et cliquer la barre de
  // raccourcis éteint la flèche.
  if (to.link->is_open()) {
    NoteLeftPress(to.world_mouse->engagement, second_of_double,
                  MouseRouteOf(*to.world, *to.grimoire, to.world_mouse->engagement, to.now_ms));
  }
  // À L'APPUI, comme dans le jeu : attendre le relâchement retarde chaque coup
  // et chaque pas de la durée du clic.
  if (!over_interface) {
    to.world_mouse->armed_at_press.reset();
    OnLeftPress(to, button, second_of_double);
    return;
  }
  // 🔴 L'APPUI FRAIS SUR UNE FENÊTRE DÉSARME, quelle que soit la fenêtre et
  // même sur sa partie vide : voir `PressOnWindowDisarms`. La compétence
  // désarmée reste lisible le temps de l'image, pour le nom qu'on vient de
  // cliquer.
  to.world_mouse->armed_at_press.reset();
  if (to.link->is_open() &&
      PressOnWindowDisarms(second_of_double,
                           to.targeting != nullptr && to.targeting->frame_hovered())) {
    if (const ro::sim::KnownSkill* armed = to.grimoire->armed(); armed != nullptr) {
      to.world_mouse->armed_at_press = *armed;
    }
    to.grimoire->Disarm();
    if (to.companion_ai != nullptr) {
      to.companion_ai->Disarm();
    }
  }
}

// MAJ + APPUI DU BOUTON DROIT SUR UN AUTRE JOUEUR : le suivi commence — voir
// `world/follow_rules.h`. Sur tout le reste, l'appui ne fait rien de plus — et
// la souris figée ne suit personne.
void OnShiftRightPress(const EventRouting& to, const SDL_MouseButtonEvent& button) {
  if (!to.link->is_open() || to.scene_actors == nullptr ||
      !WorldTakesClicks(
          MouseRouteOf(*to.world, *to.grimoire, to.world_mouse->engagement, to.now_ms))) {
    return;
  }
  const WindowPixels window = PointerSpaceOf(*to.gl);
  const std::optional<std::size_t> picked =
      ro::render::PickActor(*to.camera, *to.aspect_ratio, to.scene_actors->drawn, button.x,
                            button.y, window.width, window.height);
  if (!picked.has_value() || *picked >= to.scene_actors->ids.size()) {
    return;
  }
  const ro::sim::Actor* hovered = to.world->Find(to.scene_actors->ids[*picked]);
  if (hovered != nullptr && CanFollow(*hovered, to.world->player_id())) {
    static_cast<void>(StartFollowing(to.world_mouse->follow, *to.world, *hovered, to.now_ms));
  }
}

// LE MENU D'UNE ENTITÉ S'OUVRE AU RELÂCHEMENT DU BOUTON DROIT, comme au client
// d'origine — pas à l'appui. On ne fait ici que relever QUI est sous le curseur
// et ce qui était tenu : c'est la partie qui décide si ce clic lui ouvre un
// menu, avec des réglages et des droits que le routage n'a pas.
void OnRightRelease(const EventRouting& to, const SDL_MouseButtonEvent& button,
                    bool over_interface) {
  if (to.entity_menu == nullptr || !to.link->is_open() || to.scene_actors == nullptr) {
    return;
  }
  // CTRL+ALT+CLIC DROIT, pour le staff : le menu rapide du modèle, sinon du sol.
  const SDL_Keymod held = SDL_GetModState();
  if (to.entity_menu->staff_models && (held & SDL_KMOD_CTRL) != 0 && (held & SDL_KMOD_ALT) != 0) {
    const WindowPixels window = PointerSpaceOf(*to.gl);
    if (window.height > 0.0F) {
      const ro::render::ScreenRay ray =
          ro::render::MakeScreenRay(*to.camera, window.width / window.height, button.x, button.y,
                                    window.width, window.height);
      to.entity_menu->model_ask = ro::render::PickSceneModel(to.stage->scene, ray);
      if (!to.entity_menu->model_ask.has_value()) {
        to.entity_menu->ground_ask = ro::render::PickGround(
            to.stage->scene.gnd, nullptr, *to.camera, window.width / window.height, button.x,
            button.y, window.width, window.height);
      }
    }
    return;
  }
  // ⚠ UNE ENSEIGNE COUVRE L'ACTEUR QU'ELLE SURMONTE : le bouton relâché sur
  // elle ne vise que son marchand, celui d'une échoppe ; un salon, personne.
  // Sous `/shopping`, l'appui a en outre refermé la visite en cours.
  if (to.trade != nullptr && to.trade->SignboardCovers(glm::vec2{button.x, button.y})) {
    if (const std::optional<SignboardHit> board = to.trade->SignboardAt({button.x, button.y});
        board.has_value()) {
      to.entity_menu->ask = EntityMenuAsk{.id = board->owner_id,
                                          .gesture = WorldMenuGesture{.over_interface = false}};
    }
    return;
  }
  const SDL_Keymod modifiers = SDL_GetModState();
  // LE CLIC DROIT QU'UNE LIAISON D'ORDRE NOMME — Alt + clic droit, l'homoncule,
  // par défaut : ni après un glissé de caméra, ni au second appui d'un
  // double-clic.
  const glm::vec2 released_at{button.x, button.y};
  const bool clicked =
      to.world_mouse->right_pressed_at.has_value() &&
      glm::distance(*to.world_mouse->right_pressed_at, released_at) <= kCompanionClickSlopPixels;
  // Un ordre au compagnon est un clic sur le monde : la souris figée l'ignore,
  // et le menu reste.
  const bool world_takes_clicks = WorldTakesClicks(
      MouseRouteOf(*to.world, *to.grimoire, to.world_mouse->engagement, to.now_ms));
  if (clicked && world_takes_clicks && to.companion_ai != nullptr &&
      CompanionRightRelease(*to.companion_ai, CompanionPointerOf(to, released_at),
                            OrderedBy(to, MouseGesture::kRightClick, modifiers),
                            (modifiers & SDL_KMOD_SHIFT) != 0,
                            to.world_mouse->right_press_doubled)) {
    return;
  }
  const WindowPixels window = PointerSpaceOf(*to.gl);
  const std::optional<std::size_t> picked =
      ro::render::PickActor(*to.camera, *to.aspect_ratio, to.scene_actors->drawn, button.x,
                            button.y, window.width, window.height);
  if (!picked.has_value() || *picked >= to.scene_actors->ids.size()) {
    return;
  }
  to.entity_menu->ask =
      EntityMenuAsk{.id = to.scene_actors->ids[*picked],
                    .gesture = WorldMenuGesture{
                        .left_held = (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0,
                        .shift_held = (modifiers & SDL_KMOD_SHIFT) != 0,
                        .alt_held = (modifiers & SDL_KMOD_ALT) != 0,
                        .over_interface = over_interface}};
}

void OnWheel(const EventRouting& to, const SDL_MouseWheelEvent& wheel,
             const ro::ui::InputGate& gate) {
  const bool over_interface = gate.mouse_to_ui;
  const WindowPixels window = PointerSpaceOf(*to.gl);
  if (!over_interface) {
    // LE BANC D'ABORD : il est dessiné par-dessus, donc il reçoit la molette
    // avant la caméra et avant le grimoire.
    if (to.bench->Scroll(*to.mouse, wheel.y, window.width, window.height)) {
      return;
    }
    if (MouseWheel(*to.grimoire, *to.world, to.skill_tree, *to.mouse, wheel.y, window.width,
                   window.height, to.now_ms)) {
      return;
    }
  }
  const float steps = wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -wheel.y : wheel.y;
  if (steps == 0.0F || to.rig == nullptr) {
    return;
  }
  const std::optional<ActionRef> action = ActionOfGesture(to, MouseGesture::kWheel);
  if (!action.has_value() || !action->named().has_value()) {
    return;
  }
  // Au-dessus d'une fenêtre, seule une molette avec Ctrl ou Maj touche la
  // caméra : une liste qu'on fait défiler ne zoome pas.
  // Et jamais quand un widget survolé s'est réservé la molette : Maj et Ctrl y
  // règlent sa valeur, la caméra ne bougerait qu'en même temps.
  const SDL_Keymod held = SDL_GetModState();
  if (over_interface && ((held & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT)) == 0 || gate.wheel_to_ui)) {
    return;
  }
  if (!to.rig->TakeWheel(to.now_ms)) {
    return;
  }
  // Un cran par événement, quelle que soit son ampleur.
  const bool forward = steps > 0.0F;
  if (*action->named() == GameAction::kCameraZoomWheel) {
    WheelZoomCamera(*to.camera, forward, to.rig->limits());
  } else if (*action->named() == GameAction::kCameraTiltWheel) {
    WheelTiltCamera(*to.camera, forward, to.rig->limits(), CtrlHeld());
  }
}

}  // namespace

void RouteEvent(const EventRouting& to, const SDL_Event& event) {
  const ro::ui::InputGate gate = to.interface->gate();
  // LA ROUE D'UTILISATION AVANT LE MONDE : ouverte, elle garde la souris pour viser.
  const WheelGate wheel_gate{
      .mouse_over_interface = gate.mouse_to_ui,
      .capturing_hotkey = to.hotkeys != nullptr && to.hotkeys->capturing >= 0};
  if (to.shortcut_wheel != nullptr && to.bindings != nullptr &&
      to.shortcut_wheel->TakeEvent(event, *to.bindings, wheel_gate, SDL_GetModState(), to.mouse)) {
    return;
  }
  switch (event.type) {
    case SDL_EVENT_QUIT:
      *to.running = false;
      break;

    case SDL_EVENT_TEXT_INPUT:
      // ⚠ PLUS RIEN À FAIRE ICI : la seule saisie du jeu est celle de
      // l'interface, et `TakeEvent` la lui a déjà donnée. Le cas reste nommé
      // pour que l'événement ne retombe pas dans le défaut sans qu'on sache
      // pourquoi.
      break;

    case SDL_EVENT_KEY_DOWN:
      // 🔴 DEUX AVALAGES, ET PAS UN SEUL. Tant que l'interface prenait le
      // clavier SANS NUANCE, la barre de conversation éteignait le jeu entier :
      // ni F1 à F9, ni Alt+E, ni Inser tant qu'elle avait le focus — et un objet
      // simplement GLISSÉ faisait la même chose, ImGui comptant l'élément tenu
      // comme une prise de clavier.
      //
      // Une SAISIE ne réclame que les touches qui écrivent : `GameOnlyKey` dit
      // celles qu'elle ne peut pas utiliser, et elles reviennent au jeu. Une
      // prise TOTALE — une modale — garde tout, parce qu'il faut lui répondre
      // avant le reste.
      //
      // 🔴 ET LA BARRE DÉPLIÉE PAR LE BATTLE MODE RÉCLAME LES FRAPPES, même
      // sans le clavier : une lettre tapée là s'écrit dedans au lieu de faire
      // marcher le personnage. Voir `ChatBarKeepsKey`.
      //
      // 🔴 ET LE DIALOGUE D'UN PNJ GARDE CE QU'IL LIT, lui aussi sans le
      // clavier : Entrée y répond au script, et déplier la conversation
      // par-dessus la page ferait taper la frappe suivante dans une phrase.
      if (!KeyGoesToGame({.interface_has_keyboard = gate.keyboard_to_ui,
                          .typing = gate.typing_to_ui,
                          .chat_bar_takes_typing =
                              to.chat != nullptr &&
                              ro::ui::ChatBarTakesTyping(to.chat->input.battle_mode,
                                                         to.chat->input.unfolded, gate.text_to_ui),
                          .npc_dialog_open = NpcDialogShown(to.world),
                          .npc_menu_open = NpcMenuShown(to.world)},
                         event.key.key, event.key.mod)) {
        break;
      }
      // LE MODE CLAVIER DU GRIMOIRE PREND SA TOUCHE AVANT TOUT : sinon F1
      // lancerait le sort de la case qu'on est en train de remplacer.
      if (to.grimoire_keys != nullptr && to.sheet != nullptr && to.bindings != nullptr &&
          to.grimoire_keys->TakeKeyDown(event.key, *to.sheet, *to.bindings, to.now_ms)) {
        break;
      }
      if (to.shortcut_wheel != nullptr && to.bindings != nullptr && to.mouse != nullptr &&
          to.shortcut_wheel->TakeKeyDown(event.key, *to.bindings, wheel_gate, *to.mouse)) {
        break;
      }
      OnKeyDown(to, event.key, gate.text_to_ui);
      break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
      // Le maintien s'arrête ici : la marche cesse de se redemander.
      //
      // ⚠ SANS CONSULTER LE LAISSEZ-PASSER, et c'est délibéré : un bouton
      // relâché au-dessus d'une fenêtre laisserait sinon la marche se
      // redemander sans fin, le relâchement n'étant jamais vu.
      if (event.button.button == SDL_BUTTON_LEFT) {
        if (to.map_editor != nullptr) {
          to.map_editor->Release();
        }
        to.world_mouse->held_click = HeldClick::kNothing;
        // Une attaque d'un seul clic qui frappait sans relâche s'arrête ici :
        // voir `ReleaseStopsAttack`.
        const bool stops_attack = ReleaseStopsAttack(to.world_mouse->engagement);
        NoteRelease(to.world_mouse->engagement);
        if (stops_attack && to.link->is_open()) {
          ReportRefusal(ro::net::RequestStopAttack(*to.link));
        }
      }
      // ⚠ LE MENU, LUI, RESPECTE LE LAISSEZ-PASSER : un clic droit relâché
      // au-dessus d'une fenêtre ne vise aucune entité.
      if (event.button.button == SDL_BUTTON_RIGHT && !gate.mouse_to_ui) {
        OnRightRelease(to, event.button, gate.mouse_to_ui);
      }
      if (event.button.button == SDL_BUTTON_RIGHT) {
        to.world_mouse->right_on_signboard = false;
        to.world_mouse->right_pressed_at.reset();
        to.world_mouse->right_press_doubled = false;
      }
      break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      // LE DOUBLE-CLIC SE RECONNAÎT AVANT TOUT, à chaque appui, fenêtre ou
      // monde, et même quand la capture d'un raccourci prend l'appui.
      const bool second_of_double = SecondOfDouble(to, event.button);
      // La capture passe avant le laissez-passer : la souris est encore sur la
      // fenêtre de configuration.
      // UN APPUI SIMPLE, pour un ordre aux compagnons ou le maintien de la roue :
      // seule une action qui attend ce geste le prend, toute autre le laisse passer.
      if (!second_of_double) {
        if (const std::optional<MouseGesture> gesture = PressGestureOf(event.button.button);
            gesture.has_value() &&
            CaptureGesture(to.hotkeys, to.bindings, *gesture, SDL_GetModState())) {
          break;
        }
      }
      if (second_of_double) {
        if (const std::optional<MouseGesture> gesture = DoubleClickGestureOf(event.button.button);
            gesture.has_value()) {
          if (CaptureGesture(to.hotkeys, to.bindings, *gesture, SDL_GetModState())) {
            break;
          }
          if (!gate.mouse_to_ui) {
            OnDoubleClick(to, *gesture);
          }
        }
      }
      // 🔴 LE BOUTON DROIT DÉSARME, et c'est tout ce qu'il fait ici. Le geste
      // vient du jeu : un curseur armé se renonce d'un clic droit, sans avoir à
      // retrouver la fenêtre d'où il vient.
      //
      // ⚠ IL NE CONSOMME RIEN ET N'EMPÊCHE PAS LA ROTATION : celle-ci se fait au
      // MOUVEMENT bouton tenu, pas à l'appui. Tourner la vue en désarmant au
      // passage est exactement ce que fait le jeu ; refuser de désarmer parce
      // que la caméra pourrait tourner laisserait le curseur armé après le
      // geste qui sert à s'en débarrasser.
      if (event.button.button == SDL_BUTTON_RIGHT && !gate.mouse_to_ui && to.grimoire != nullptr) {
        to.grimoire->Disarm();
      }
      if (event.button.button == SDL_BUTTON_RIGHT && !gate.mouse_to_ui) {
        if (to.companion_ai != nullptr) {
          to.companion_ai->Disarm();
        }
        to.world_mouse->right_pressed_at = glm::vec2{event.button.x, event.button.y};
        to.world_mouse->right_press_doubled = second_of_double;
        if ((SDL_GetModState() & SDL_KMOD_SHIFT) != 0) {
          OnShiftRightPress(to, event.button);
        }
      }
      // LE BOUTON DROIT SUR UNE ENSEIGNE : elle prend l'appui comme une fenêtre,
      // et le glissé qui suit ne tourne pas la caméra. SOUS `/shopping`, celle
      // d'une échoppe referme en outre l'échoppe qu'on visitait, quelle qu'elle
      // soit.
      if (event.button.button == SDL_BUTTON_RIGHT && !gate.mouse_to_ui && to.trade != nullptr) {
        const glm::vec2 at{event.button.x, event.button.y};
        to.world_mouse->right_on_signboard = to.trade->SignboardCovers(at);
        if (to.shop_one_click && to.trade->SignboardAt(at).has_value()) {
          to.trade->CloseVisits();
        }
      }
      if (event.button.button == SDL_BUTTON_LEFT) {
        OnLeftButtonDown(to, event.button, gate.mouse_to_ui, second_of_double);
      }
      break;
    }

    case SDL_EVENT_MOUSE_MOTION:
      // La position se suit MÊME au-dessus d'une fenêtre : c'est elle qui place
      // le curseur du jeu, et la figer le laisserait planté là où la souris est
      // entrée dans le panneau.
      *to.mouse = glm::vec2{event.motion.x, event.motion.y};
      // UNE POIGNÉE SAISIE suit la souris, fenêtres ou non, jusqu'au relâcher.
      if (to.map_editor != nullptr && to.map_editor->dragging()) {
        to.map_editor->DragTo(*to.mouse, (SDL_GetModState() & SDL_KMOD_SHIFT) != 0);
        break;
      }
      to.world_mouse->pressed = (event.motion.state & SDL_BUTTON_LMASK) != 0;
      to.world_mouse->double_click.NoteMotion();
      if (const std::optional<MouseGesture> gesture = DragGestureOf(event.motion.state);
          gesture.has_value()) {
        if (CaptureGesture(to.hotkeys, to.bindings, *gesture, SDL_GetModState())) {
          break;
        }
        if (!gate.mouse_to_ui &&
            !(*gesture == MouseGesture::kRightDrag && to.world_mouse->right_on_signboard)) {
          OnDrag(to, *gesture, event.motion);
        }
      }
      break;

    case SDL_EVENT_MOUSE_WHEEL:
      if (CaptureGesture(to.hotkeys, to.bindings, MouseGesture::kWheel, SDL_GetModState())) {
        break;
      }
      OnWheel(to, event.wheel, gate);
      break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
      NoteFocusLost(to.world_mouse->engagement);
      break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      *to.size = to.gl->drawable_size();
      if (to.size->height > 0) {
        *to.aspect_ratio = static_cast<float>(to.size->width) / static_cast<float>(to.size->height);
      }
      break;

    default:
      break;
  }
}

}  // namespace moonclient
