#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <SDL3/SDL.h>

#include "ro/net/connection.h"
#include "ro/render/camera.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/cart_window.h"
#include "ro/ui/chat_macros.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/craft_atlas_window.h"
#include "ro/ui/game_menu.h"
#include "ro/ui/hotkey_window.h"
#include "ro/ui/navigation_window.h"
#include "ro/ui/palette_editor.h"
#include "ro/ui/party_friend_window.h"
#include "ro/ui/pet_window.h"
#include "ro/ui/skill_window.h"
#include "ro/ui/world_map_window.h"

#include "capture/zone_recorder.h"
#include "character/preset_view.h"
#include "character/shortcut_view.h"
#include "character/skill_book_view.h"
#include "chat/chat_view.h"
#include "effects/effect_bench_view.h"
#include "input/camera_rig.h"
#include "input/hotkeys.h"
#include "items/bank_view.h"
#include "items/card_album_view.h"
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
class MvpTrackerStage;

// CE QU'UNE TOUCHE FAIT, hors de la boucle qui la reçoit.
//
// À part de `click_action.h`, qui traite la souris, et pour la même raison : la
// boucle de la fenêtre orchestre, elle ne décide pas. Un `else if` de plus par
// touche l'aurait fait grossir jusqu'à ce que son plafond de lignes le signale
// — ce qu'il a fait.
struct KeyPress {
  // Ce que la touche peut changer. Tout est ouvert en écriture : une touche
  // agit, elle ne consulte pas.
  // ⚠ CE N'EST PLUS UNE LIGNE DE SAISIE, C'EST L'ÉTAT DE LA CHATBOX. Entrée ne
  // fait plus qu'une chose : lui donner le clavier. Le curseur, la sélection, le
  // collage et les accents appartiennent au champ de l'interface, qui les fait
  // mieux que ce qu'on écrivait à la main.
  ro::ui::ChatWindowState* chat = nullptr;
  SkillBookPanel* skills = nullptr;
  // LA FICHE DE PERSONNAGE, dont le grimoire est un onglet. C'est Alt+S dans le
  // jeu ; S tout court ouvre l'AUTRE grimoire, celui qui arme une compétence.
  // Les deux touches se ressemblent parce que les deux fenêtres se ressemblent :
  // l'une montre l'arbre, l'autre sert à lancer.
  ro::ui::CharacterWindowState* sheet = nullptr;
  // L'HOMONCULE ET LE MERCENAIRE EXISTENT-ILS ? Sans eux, Alt+R et Ctrl+R ne font
  // rien : le jeu refuse d'ouvrir leur fenêtre.
  bool homunculus_present = false;
  bool mercenary_present = false;
  // L'IA DES DEUX COMPAGNONS : Alt+T et Ctrl+T basculent leur attente, et Échap
  // désarme le skill de compagnon qui attend sa cible.
  CompanionAiStage* companion_ai = nullptr;
  // LA FICHE DU PET. ⚠ LA TOUCHE NE FAIT QUE DEMANDER LA BASCULE : la fiche se
  // redemande au serveur en même temps, et c'est la partie qui tient la
  // connexion.
  ro::ui::PetWindowState* pet = nullptr;
  // LE SAC. Alt+E dans le jeu, et un joueur connaît cette touche avant de
  // connaître ce client.
  ro::ui::InventoryWindowState* inventory = nullptr;
  // LA CHARRETTE. Alt+W dans le jeu.
  ro::ui::CartWindowState* cart = nullptr;
  // LA FENÊTRE GROUPE / AMIS : Alt+Z sur le groupe, Alt+H sur les amis.
  ro::ui::PartyFriendWindowState* party = nullptr;
  // LA BANQUE. Ctrl+B dans le jeu. ⚠ LA VUE ET NON L'ÉTAT DE FENÊTRE : la
  // touche n'ouvre rien, elle demande au serveur — et la vue porte la
  // cadence qui empêche deux demandes de se défaire l'une l'autre.
  BankView* bank = nullptr;
  // L'ALBUM DE CARTES : sa vue, qui demande l'état au serveur, et sa fenêtre.
  CardAlbumView* card_album = nullptr;
  ro::ui::CardAlbumWindowState* card_album_window = nullptr;
  // LA BOUTIQUE DE VOTE : sa touche parle au serveur, comme celle de la
  // banque.
  CashShopStage* cash_shop = nullptr;
  // LE COURRIER : ouvrir la boîte demande sa liste au serveur.
  MailStage* mail = nullptr;
  // LES SUCCÈS : la fenêtre renaît sur le résumé à chaque ouverture.
  AchievementStage* achievements = nullptr;
  QuestStage* quests = nullptr;
  MemorialStage* memorial = nullptr;
  // LES SALONS : la touche ouvre le formulaire, ou montre la salle.
  ChatRoomStage* chat_rooms = nullptr;
  // Le menu du jeu, qu'Échap ouvre quand il n'a plus rien à fermer.
  ro::ui::GameMenuState* menu = nullptr;
  // LES DIX MACROS DE CHAT. Alt+M ouvre leur fenêtre, Alt+1 à Alt+0 les
  // envoient — et c'est la MÊME structure qui porte les deux, parce que c'est la
  // fenêtre elle-même qui tient les textes pendant la partie.
  ro::ui::ChatMacroState* macros = nullptr;
  // L'ÉDITEUR DE STYLE. ⚠ UN SIMPLE INTERRUPTEUR : la fenêtre ne demande rien
  // au serveur pour s'ouvrir, et tout ce qu'on y compose reste un aperçu
  // jusqu'à la validation.
  ro::ui::PaletteEditorState* style = nullptr;
  // L'ATLAS DES RECETTES, simple interrupteur : il ne demande rien au serveur.
  ro::ui::CraftAtlasWindowState* craft_atlas = nullptr;
  // LA NAVIGATION, simple interrupteur ; et la minimap, montrée ou masquée.
  ro::ui::NavigationWindowState* navigation = nullptr;
  // LA CARTE DU MONDE, simple interrupteur.
  ro::ui::WorldMapWindowState* world_map = nullptr;
  bool* minimap_shown = nullptr;
  // LE CARNET DE CHASSE MVP : joué à l'image suivante, là où l'étage a les
  // réglages en main — éteint, la touche n'ouvre rien.
  MvpTrackerStage* mvp_tracker = nullptr;
  // LA BOÎTE DU RAPPORT DE BUG, qu'on ouvre sans contexte.
  ro::ui::BugReportWindowState* bug_report = nullptr;
  // L'ENREGISTREUR DE ZONE, pour ses trois touches. ⚠ IL N'EST PAS UNE FENÊTRE :
  // ses gestes sont des ordres à une machine, pas un interrupteur d'affichage,
  // et c'est pourquoi il descend ici en entier plutôt que par un drapeau.
  ZoneRecorder* recorder = nullptr;
  // ⚠ LA TABLE DES RACCOURCIS EST LA SEULE SOURCE : c'est elle qui dit ce qu'une
  // touche déclenche, et c'est elle que la configuration écrit.
  Hotkeys* bindings = nullptr;
  // L'état de la configuration, pour la CAPTURE : tant qu'elle attend une touche,
  // celle-ci lui revient au lieu de déclencher une action.
  ro::ui::HotkeyWindowState* hotkeys = nullptr;
  // Les presets : la touche qu'attend leur onglet — dont l'état est dans
  // `sheet` —, et celles qui les remettent, fiche fermée comprise.
  PresetView* presets = nullptr;
  // Un champ de saisie a le clavier : Alt et F1 à F12 le traversent pour la
  // table des raccourcis, et F1 à F12 seules pour remettre un preset.
  bool text_input = false;
  // Le mode Ciblage : ses quatre raccourcis, Échap, et les sorts ciblés armés
  // au clavier.
  Targeting* targeting = nullptr;
  // QUICK CAST : il retient chaque frappe fraîche, pour savoir ensuite si un
  // sort s'est armé au clavier.
  QuickCast* quick_cast = nullptr;
  // L'ÉCRAN DE VEILLE, DEMANDÉ À LA TOUCHE. ⚠ UNE DEMANDE ET NON UN APPEL, la
  // même que celle du bouton « Essayer maintenant » : entrer en veille demande
  // la caméra et le compteur d'inactivité, et ce fichier ne connaît ni l'une ni
  // l'autre. La boucle consomme.
  bool* afk_now = nullptr;
  // LA BARRE DE RACCOURCIS : ses neuf touches passent par la même table que le
  // reste, et c'est elle qui sait laquelle est quelle case.
  ShortcutBarView* shortcuts = nullptr;
  EffectBenchPanel* bench = nullptr;
  ro::render::Camera* camera = nullptr;
  const CameraRig* rig = nullptr;
  ro::net::Connection* link = nullptr;
  ro::sim::World* world = nullptr;
  // La fenêtre, que la saisie de texte ouvre et ferme.
  SDL_Window* window = nullptr;
  // ⚠ ÉCHAP NE QUITTE QU'EN DERNIER RECOURS : tant qu'une fenêtre est ouverte,
  // il la ferme. Ce drapeau porte le verdict de l'image précédente — voir
  // `EscapeStack::has_work`.
  bool escape_closes_a_window = false;
  // UN ATELIER RÉCLAME ENTRÉE — son réglage « Entrée lance… », ou la
  // confirmation d'un refine : la touche ne va pas au chat.
  bool enter_claimed = false;
  // Vrai tant que la fenêtre tourne ; l'échappement le met à faux.
  bool* running = nullptr;
  std::uint32_t now_ms = 0;
};

// Traite une touche enfoncée.
//
// Rend la ligne de conversation à ENVOYER quand l'entrée la valide, et rien
// sinon : la boucle seule sait à qui la remettre, et la lui faire router
// d'ici mêlerait le clavier au réseau.
//
// ⚠ LA SAISIE PREND TOUT le clavier tant qu'elle est ouverte : sans cela,
// écrire « r » recadrerait la caméra au milieu d'un mot.
[[nodiscard]] std::optional<std::string> HandleKey(const KeyPress& to, const SDL_KeyboardEvent& key,
                                                   const ro::render::Camera& framed_map);

// Une action d'appui, touche ou double-clic. Rend la ligne à envoyer.
[[nodiscard]] std::optional<std::string> TriggerAction(const KeyPress& to, ActionRef action,
                                                       bool repeat,
                                                       const ro::render::Camera& framed_map);

// Pose le geste sur la ligne que la configuration attend. Rend vrai s'il lui
// était destiné ; un geste qui ne convient pas à l'action n'est pas avalé.
[[nodiscard]] bool CaptureGesture(ro::ui::HotkeyWindowState* hotkeys, Hotkeys* bindings,
                                  MouseGesture gesture, SDL_Keymod held);

}  // namespace moonclient
