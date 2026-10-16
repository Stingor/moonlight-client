#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include <SDL3/SDL.h>

#include <glm/glm.hpp>

#include "ro/net/connection.h"
#include "ro/render/camera.h"
#include "ro/render/gl_context.h"
#include "ro/sim/pickup_intent.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/chat_macros.h"
#include "ro/ui/palette_editor.h"
#include "ro/ui/pet_window.h"
#include "ro/ui/skill_window.h"
#include "ro/ui/status_window.h"

#include "capture/zone_recorder.h"
#include "character/grimoire_keys.h"
#include "character/preset_view.h"
#include "character/shortcut_view.h"
#include "character/skill_book_view.h"
#include "chat/chat_view.h"
#include "effects/effect_bench_view.h"
#include "effects/effect_view.h"
#include "game/ui_stage.h"
#include "input/camera_rig.h"
#include "input/hotkeys.h"
#include "items/bank_view.h"
#include "items/card_album_view.h"
#include "world/actor_view.h"
#include "world/click_action.h"
#include "world/entity_menu.h"
#include "world/map_editor_session.h"
#include "world/map_stage.h"
#include "world/quick_cast.h"
#include "world/targeting.h"

namespace moonclient {

class CashShopStage;
class AchievementStage;
class QuestStage;
class MemorialStage;
class MailStage;
class ChatRoomStage;
class CompanionAiStage;
class TradeStages;

// À QUI REVIENT LE GESTE, hors de la boucle qui le reçoit.
//
// À part de `key_action.h` et de `click_action.h`, et pour la raison qui les a
// déjà sortis : la boucle orchestre, elle ne décide pas. Un `case` de plus l'a
// fait dépasser son plafond de lignes, et le plafond a fait son travail.
//
// ⚠ CE FICHIER PORTE MAINTENANT UNE SEULE QUESTION, et c'est ce qui justifie
// qu'il existe : l'interface est arrivée, et « le jeu ou une fenêtre ? » se
// tranche désormais à chaque événement. Cette décision était auparavant un
// ORDRE DE PRIORITÉ écrit à la main dans les commentaires de la boucle — « le
// banc d'abord, il est dessiné par-dessus » — juste tant que personne
// n'ajoutait de panneau, et dont le défaut ne se voyait pas.

// Tout ce qu'un événement peut atteindre.
//
// Passé par pointeurs plutôt que gardé : le routage ne POSSÈDE rien, il met à
// jour ce que la boucle tient déjà. Les regrouper évite une liste de paramètres
// dont l'ordre deviendrait un piège.
struct EventRouting {
  // La fenêtre, et l'interface qui dit à qui revient le geste.
  ro::render::GlContext* gl = nullptr;
  const UiStage* interface = nullptr;

  // Le jeu.
  ro::net::Connection* link = nullptr;
  ro::sim::World* world = nullptr;
  const MapStage* stage = nullptr;
  ro::sim::WalkPlanner* planner = nullptr;
  EffectsInPlay* effects = nullptr;
  EffectNodesInPlay* effect_nodes = nullptr;
  ro::render::Camera* camera = nullptr;
  CameraRig* rig = nullptr;
  // L'ÉTAT DE LA CHATBOX : Entrée lui donne le clavier, et rien d'autre ne passe
  // plus par ici — le champ de l'interface fait la saisie.
  ro::ui::ChatWindowState* chat = nullptr;
  SkillBookPanel* grimoire = nullptr;
  // LES TABLES DE COMPÉTENCES DU CLIENT, pour la molette : elles seules disent
  // quelle compétence se lance à un rang choisi. Nul quand l'installation ne
  // les porte pas.
  const ro::sim::SkillTree* skill_tree = nullptr;
  ro::ui::CharacterWindowState* sheet = nullptr;
  // La petite fenêtre de statut, qu'Alt+A bascule.
  ro::ui::StatusWindowState* status = nullptr;
  // Le mode clavier du grimoire : il prend la touche avant qu'elle n'agisse.
  GrimoireKeys* grimoire_keys = nullptr;
  // L'homoncule et le mercenaire existent : leurs touches ouvrent leur onglet.
  bool homunculus_present = false;
  bool mercenary_present = false;
  // L'IA DES DEUX COMPAGNONS : leurs ordres à la souris, leur attente au
  // clavier, et le skill de compagnon qui attend sa cible.
  CompanionAiStage* companion_ai = nullptr;
  // La fiche du pet, dont la touche demande la bascule.
  ro::ui::PetWindowState* pet = nullptr;
  ro::ui::InventoryWindowState* inventory = nullptr;
  ro::ui::CartWindowState* cart = nullptr;
  ro::ui::PartyFriendWindowState* party = nullptr;
  // LA BANQUE. ⚠ LA VUE ET NON UN ÉTAT DE FENÊTRE : sa touche ne bascule pas
  // un drapeau, elle parle au serveur.
  BankView* bank = nullptr;
  // L'ALBUM DE CARTES, pour la même raison : sa touche parle au serveur.
  CardAlbumView* card_album = nullptr;
  ro::ui::CardAlbumWindowState* card_album_window = nullptr;
  // La boutique de vote, pour la même raison : sa touche parle au serveur.
  CashShopStage* cash_shop = nullptr;
  // Le courrier, pour la même raison : sa touche parle au serveur.
  MailStage* mail = nullptr;
  // Les succès : leur fenêtre renaît sur le résumé à chaque ouverture.
  AchievementStage* achievements = nullptr;
  QuestStage* quests = nullptr;
  MemorialStage* memorial = nullptr;  // Alt+B, l'instance.
  // LES COMMERCES : un clic sur l'enseigne d'une échoppe — ou sur son
  // marchand — leur demande de l'ouvrir, et ne fait pas marcher.
  TradeStages* trade = nullptr;
  // LES SALONS : le double-clic sur l'enseigne d'un salon y entre, et Alt+C
  // ouvre le formulaire.
  ChatRoomStage* chat_rooms = nullptr;
  // Le formulaire de salon est ouvert : le salon naîtra sur la case du
  // personnage, et un clic dans le monde ne le déplace plus.
  bool chat_room_form_open = false;
  ro::ui::GameMenuState* menu = nullptr;
  // Les dix macros de chat : Alt+M ouvre leur fenêtre, Alt+1 à Alt+0 envoient.
  ro::ui::ChatMacroState* macros = nullptr;
  // L'éditeur de style, qu'une touche bascule comme les autres fenêtres.
  ro::ui::PaletteEditorState* style = nullptr;
  // L'Atlas des recettes, de même.
  ro::ui::CraftAtlasWindowState* craft_atlas = nullptr;
  // La navigation et la minimap, de même.
  ro::ui::NavigationWindowState* navigation = nullptr;
  ro::ui::WorldMapWindowState* world_map = nullptr;
  bool* minimap_shown = nullptr;
  // Le carnet de chasse MVP : sa touche est jouée par l'étage, à l'image suivante.
  MvpTrackerStage* mvp_tracker = nullptr;
  bool* database_toggle = nullptr;  // la bascule de la base de données, de même
  ro::ui::BugReportWindowState* bug_report = nullptr;
  // L'ENREGISTREUR DE ZONE : ses trois touches lui parlent directement. ⚠ IL
  // N'EST PAS UNE FENÊTRE — pas de drapeau d'ouverture à basculer, un ordre à
  // une machine.
  ZoneRecorder* recorder = nullptr;
  ro::ui::HotkeyWindowState* hotkeys = nullptr;
  Hotkeys* bindings = nullptr;
  // Les presets d'équipement : leurs touches, et celle qu'attend leur onglet.
  PresetView* presets = nullptr;
  // Le mode Ciblage : le clic désigne, le double-clic attaque, les raccourcis
  // cyclent.
  Targeting* targeting = nullptr;
  // Quick cast : chaque frappe fraîche lui est notée.
  QuickCast* quick_cast = nullptr;
  // LE MENU DU CLIC DROIT : le relâchement du bouton droit y dépose l'acteur
  // visé, et le clic gauche y lit les PNJ qu'on a rendus sourds.
  EntityMenuGestures* entity_menu = nullptr;
  // LA DEMANDE D'ÉCRAN DE VEILLE, que la touche arme et que la boucle consomme.
  bool* afk_now = nullptr;
  ShortcutBarView* shortcuts = nullptr;
  // Le verdict de la pile d'échappement, à l'image précédente.
  bool escape_closes_a_window = false;
  // Un atelier réclame Entrée : voir `CraftStage::ClaimsEnter`.
  bool enter_claimed = false;
  // `/noshift` : MAJ n'est plus nécessaire pour forcer une cible. ⚠ LA VALEUR
  // ET NON LES OPTIONS : le routage des événements n'a aucune raison de
  // connaître la classe qui les tient.
  bool no_shift = false;
  // `/noctrl` : son pendant pour l'autre touche. CTRL n'est plus nécessaire
  // pour que l'attaque se poursuive jusqu'à la chute de la cible.
  //
  // ⚠ MÊME FORME, DÉFAUT INVERSE : celui-ci est ALLUMÉ d'usine, là où
  // `/noshift` est éteint. Voir `AttackRepeats`.
  bool no_ctrl = true;
  // `/shopping` : un simple clic ouvre une échoppe, un clic droit referme la
  // visite. Éteint, il faut un double-clic. Voir `ShopOptions`.
  bool shop_one_click = true;
  EffectBenchPanel* bench = nullptr;
  // LE MODE ÉDITEUR DE CARTE, quand il est ouvert : un clic dans le monde y
  // désigne un modèle, et Échap en sort. Nul sinon, ou sans le droit.
  MapEditorSession* map_editor = nullptr;
  // Sa vue de l'image : le placement d'où part une poignée saisie.
  const ro::ui::MapEditorView* map_editor_view = nullptr;
  // Où part une ligne validée. Le routage ne l'envoie pas lui-même : c'est la
  // même séparation qu'entre `HandleKey` et la boucle.
  const ChatDispatch* chat_dispatch = nullptr;

  // Ce que la scène a peint à l'image PRÉCÉDENTE : le clic la lit pour
  // retrouver l'acteur derrière le rectangle désigné.
  const ComposedScene* scene_actors = nullptr;

  // Ce que la boucle tient d'une image à l'autre, et que le geste change.
  glm::vec2* mouse = nullptr;
  // Le bouton gauche : ce que son maintien REFAIT, et ce que son clic engage.
  WorldMouse* world_mouse = nullptr;
  bool* running = nullptr;
  ro::render::DrawableSize* size = nullptr;
  float* aspect_ratio = nullptr;
  // LES DEUX INTENTIONS QUE LE CLIC LAISSE — voir `ClickIntents`.
  std::optional<ro::sim::PickupIntent>* pickup = nullptr;
  std::optional<ro::sim::PendingAction>* attack = nullptr;

  std::uint32_t now_ms = 0;
};

// Traite un événement de la fenêtre.
//
// ⚠ L'INTERFACE L'A DÉJÀ VU quand cette fonction le reçoit : elle ne sait ce
// qu'il y a sous le curseur que si elle a vu la souris bouger, et le lui donner
// après coup lui ferait manquer d'une image ce qu'elle doit savoir maintenant.
void RouteEvent(const EventRouting& to, const SDL_Event& event);

}  // namespace moonclient
