#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/entity_names.h"
#include "ro/render/screen_overlay.h"
#include "ro/sim/actor.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/cast_bars.h"
#include "ro/ui/chat_bubble.h"
#include "ro/ui/entity_bars.h"
#include "ro/ui/pick_boxes.h"
#include "ro/ui/pvp_hud_frames.h"

#include "character/skill_book_view.h"
#include "chat/chat_item_link.h"
#include "chat/chat_rich_decor.h"
#include "effects/effect_bench_view.h"
#include "game/game_pieces.h"
#include "guild/emblem_plates.h"
#include "options/battleground_options.h"
#include "world/actor_view.h"
#include "world/ground_unit_pick.h"

namespace moonclient {

// CE QUI SE PEINT EN COORDONNÉES D'ÉCRAN, PAR-DESSUS LE MONDE.
//
// Les jauges de vie et les incantations, le nom de ce que la souris désigne, les
// bulles de parole, les panneaux d'atelier. Tout cela partage
// le même renderer et le même ORDRE — chaque couche masque la précédente —, et
// c'est cet ordre qui fait la lisibilité de l'écran.
//
// ⚠ LE CURSEUR N'EN EST PAS. Il se peint après l'interface, et il n'y en a qu'un.

// Ce que la souris désigne. Cherché UNE SEULE FOIS : la forme du curseur en
// dépend autant que le nom qui s'écrit.
struct HoveredActor {
  std::optional<std::size_t> drawn_rank;
  const ro::sim::Actor* actor = nullptr;
};

// Ce que la peinture lit. Groupé plutôt qu'énuméré : neuf paramètres voisins de
// même type sont un piège, et chaque couche nouvelle en ajouterait un.
struct ScreenOverlays {
  GamePieces* pieces = nullptr;
  const ro::sim::World* world = nullptr;
  // LE GROUPE : c'est lui, et non la vie connue, qui dit qui se règle sous
  // l'onglet « Groupe ». Voir `EntityBarKindOf`.
  const ro::sim::PartyRoster* party = nullptr;
  const ro::render::Camera* camera = nullptr;
  const ComposedScene* actors = nullptr;
  // Le grimoire d'atelier — celui qui ARME une compétence — et le banc d'effets.
  SkillBookPanel* book = nullptr;
  EffectBenchPanel* bench = nullptr;
  // CE QU'UN SORT EN MAIN MONTRE. ⚠ REÇU et non gardé, comme les bulles et les
  // barres : c'est un réglage, il vit dans les options. Le rang au curseur, lui,
  // ne se peint pas ici — il part avec le curseur, tout en dernier.
  ro::render::ArmedSkillLook armed_skill;
  // OÙ DÉPOSER LES ZONES CLIQUABLES que l'établi du staff montre, ou nul
  // quand il ne les montre pas. Récoltées ici, peintes par l'étage d'interface.
  ro::ui::PickBoxesView* pick_boxes_on_screen = nullptr;
  // Ce qu'une bulle de parole montre, et combien de temps. ⚠ REÇU et non gardé :
  // c'est un réglage, il vit dans les options.
  ro::ui::ChatBubbleLook bubbles;
  // OÙ DÉPOSER CE QUE L'ÉTAGE D'INTERFACE PEINDRA. ⚠ RÉCOLTÉ ICI ET PEINT
  // AILLEURS : l'image d'ImGui n'est pas encore ouverte quand ces calques se
  // dessinent, et c'est elle qui porte la police du chat.
  std::vector<ro::ui::ChatBubbleView>* bubbles_on_screen = nullptr;
  // ET OU GARDER LES PHRASES DONT UNE BALISE A ETE TRADUITE : les vues pointent
  // dedans, et elles sont lues bien apres ce calque.
  std::deque<DecoratedChatLine>* bubble_texts = nullptr;
  // L'ICÔNE D'UN OBJET, celle du fil : un lien d'objet la montre aussi dans
  // une bulle. Nul : aucune icône.
  const ItemIconLookup* item_icon = nullptr;
  // LES BARRES DE VIE ET DE MAGIE DES ENTITÉS : récoltées ici, peintes par
  // l'étage d'interface, comme les bulles et pour la même raison — elles
  // écrivent parfois leurs nombres.
  //
  // ⚠ LE RÉGLAGE ENTIER ET NON SON SEUL MAÎTRE : l'enseigne d'un vendeur doit
  // savoir de combien monter pour ne pas se poser sur la jauge, et cela dépend
  // de l'espèce, du côté choisi et de la hauteur réglée. ⚠ REÇU et non gardé,
  // comme les bulles.
  ro::ui::EntityBarsLook entity_bars;
  std::vector<ro::ui::EntityBarView>* entity_bars_on_screen = nullptr;
  // `/showshop` : les enseignes des échoppes se peignent-elles ? Celles des
  // salons, toujours. ⚠ REÇU et non gardé.
  bool shop_signboards_shown = true;
  // Le panneau d'options est ouvert : le mode réglage des barres peut jouer.
  bool options_open = false;
  // LES BARRES D'INCANTATION : récoltées ici, peintes par l'étage d'interface.
  // Vidées quand on n'est pas connecté.
  std::vector<ro::ui::CastBarView>* cast_bars_on_screen = nullptr;
  // LES ÉTIQUETTES : ce qu'elles montrent, et LAQUELLE paraît. ⚠ CELLES-CI SE
  // PEIGNENT ICI, contre les barres — elles n'écrivent que du texte de notre
  // atlas, et n'ont donc pas besoin de la police du chat.
  ro::render::EntityNamesLook entity_names;
  // LES EMBLÈMES DE GUILDE DES ÉTIQUETTES : le carnet dit la guilde et sa
  // version, le magasin tient les images — et note celles qui manquent.
  const ro::sim::GuildBook* guild = nullptr;
  EmblemPlates* emblems = nullptr;
  // OÙ LE JOUEUR A POSÉ LES DEUX PIÈCES DU CLASSEMENT PVP. ⚠ REÇU et non gardé,
  // comme les bulles et les barres : c'est un réglage, il vit dans les options.
  // Voir `ro/ui/pvp_hud_frames.h` — le cadre porte la taille autant que la place.
  ro::ui::PvpHudLook pvp_hud;
  // CE QUE LE CHAMP DE BATAILLE MONTRE : le score et son surlignage, la marque
  // des ennemis. ⚠ REÇU et non gardé, comme le classement PVP.
  BattlegroundSettings battleground;
  // De quoi traduire, quand la partie les a lues.
  const ro::vfs::ItemTable* items = nullptr;
  const ro::vfs::ItemWords* words = nullptr;
  // Les noms des forgerons, pour les liens d'items forgés des bulles.
  CreatorNames creators;
  // Hors ligne, on ne peint ni jauge ni bulle : elles diraient l'état d'un monde
  // que personne ne tient à jour.
  bool connected = false;
  // LES UNITÉS AU SOL QUI PARLENT — une Talkie Box déclenchée —, dont le
  // message rejoint les bulles des acteurs.
  std::span<const GroundUnitPick> talkbox_speakers;
  // OÙ DÉPOSER LE HUD DE JEU — classement PvP, étiquette de ce qui est armé,
  // grimoire et banc d'atelier — au lieu de le peindre, ou nul pour le peindre
  // ici. ⚠ DÉPOSÉ, il se peint par-dessus le voile de ténèbres, qui cache tout
  // ce qui appartient au monde : voir `OverWorldLayer`.
  std::vector<ro::render::ScreenQuad>* game_hud = nullptr;
};

[[nodiscard]] HoveredActor DrawScreenOverlays(const ScreenOverlays& what, float aspect_ratio,
                                              glm::vec2 mouse_px, float viewport_width,
                                              float viewport_height, std::uint32_t now_ms);

}  // namespace moonclient
