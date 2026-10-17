#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/image.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_pick.h"
#include "ro/render/armed_skill.h"
#include "ro/render/boss_badge.h"
#include "ro/render/camera.h"
#include "ro/render/cursor.h"
#include "ro/render/entity_names.h"
#include "ro/render/floating_number.h"
#include "ro/render/font_atlas.h"
#include "ro/render/name_plate.h"
#include "ro/render/name_plate_layout.h"
#include "ro/render/screen_overlay.h"
#include "ro/render/screen_text.h"
#include "ro/render/signboard.h"
#include "ro/render/sprite_renderer.h"
#include "ro/render/title_table.h"
#include "ro/sim/actor.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/entity_bars.h"
#include "ro/vfs/item_name.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "world/map_stage.h"
#include "world/name_plate_view.h"
#include "world/signboard_pick.h"
#include "world/versus_gate.h"

// Ce que le client peint PAR-DESSUS la scène, et qui n'appartient pas au monde :
// le curseur, les enseignes, les incantations. Tout s'y décrit en pixels de
// fenêtre, et rien n'y est dessiné : ces fonctions rendent des quads, que le
// renderer prend.
namespace moonclient {

// Ce que le SOL sous la souris permet.
enum class GroundHover : std::uint8_t {
  // Une case où l'on peut aller.
  kOpen,
  // Une case interdite — ou pas de case du tout : le ciel, le dehors de la
  // carte. Le client refuse les deux de la même façon.
  kRefused,
  // La carte n'a pas de grille : elle ne se prononce pas, et on y marche en
  // ligne droite.
  kUnknown,
};

// Ce que le sol permet sur la case désignée.
//
// La case est DONNÉE, et non cherchée ici : la retrouver soi-même relancerait
// la marche du rayon le long du terrain, que la même image a déjà payée pour le
// curseur au sol et pour la marche tenue. `cell` est vide quand la souris ne
// vise pas la carte — le ciel, ou le dehors.
[[nodiscard]] GroundHover GroundHoverOn(const MapStage& stage,
                                        const std::optional<ro::sim::CellPosition>& cell);

// La forme que le curseur prend au-dessus de ce que la souris désigne.
//
// Ce que le client montre dit ce qu'un clic ferait : une épée sur ce qu'on peut
// frapper, une main sur ce qu'on peut ramasser, une bulle sur qui parle, une
// porte sur un portail.
//
// SANS ACTEUR, c'est la CASE qui décide, et non plus la flèche par défaut : une
// case interdite porte le panneau d'interdiction. Les deux décisions sont
// séparées dans le client comme ici — l'acteur d'abord, le sol seulement quand
// il n'y en a pas.
//
// ⚠ LE PROPRE PERSONNAGE DU JOUEUR NE PASSE PAS PAR ICI : `LeftClick` et
// `Game::ComposeCursor` l'écartent avant, comme le client d'origine, dont la toute première ligne
// de survol compare le GID désigné à celui du compte. On marche à travers soi-même, et une porte
// ouverte ne doit pas y changer quoi que ce soit.
[[nodiscard]] ro::render::CursorShape CursorShapeOver(const ro::sim::Actor* hovered,
                                                      GroundHover ground, VersusGate versus);

// Le sprite des curseurs, tel qu'il est chargé. Les pointeurs ne sont pas
// possédés.
struct CursorSprite {
  const ro::formats::SprSprite* sprite = nullptr;
  const ro::formats::ActAnimation* animation = nullptr;
};

// La police que le client prend faute d'autre : celle du système, la même
// qu'emploie le client d'origine. Le chemin dépend de la plateforme, et c'est
// la seule raison pour laquelle il vit ici plutôt que dans la couche de rendu.
[[nodiscard]] std::filesystem::path DefaultFontPath();

// La hauteur des noms, en pixels.
inline constexpr int kNameFontPixels = 12;

// L'ÉTIQUETTE D'UN ACTEUR, centrée sous ses pieds — une ou deux lignes.
//
// `titles` peut être nul : sans la table de l'installation, un titre ne s'écrit
// pas et le reste tient debout.
// ⚠ `bars_under_feet` EST LA BARRE QUE L'ACTEUR PORTE VRAIMENT SOUS SES PIEDS,
// ou rien : le nom ne s'en écarte que s'il la chevauche.

// LE NOM D'UN OBJET AU SOL, composé depuis la table de l'installation. Vide
// pour tout autre acteur, et pour un objet que la table ne nomme pas.
//
// 🔴 LE SERVEUR NE NOMME JAMAIS UN OBJET AU SOL : `clif_name` ne traite pas
// `BL_ITEM`, si bien qu'une demande de nom ne rend rien. Le client le COMPOSE,
// comme il compose celui d'un objet du sac — même table, même règle — et c'est
// pourquoi un butin n'a pas de `name` que la réception aurait pu remplir.
//
// ⚠ LE FIL NE DONNE NI CARTES NI RAFFINEMENT sur un objet au sol : le nom sort
// donc nu, avec ses seuls emplacements. Le client d'origine n'en montre pas
// davantage.
[[nodiscard]] std::string GroundItemName(const ro::sim::Actor& actor,
                                         const ro::vfs::ItemTable* items,
                                         const ro::vfs::ItemWords& words);

// L'INSIGNE QUE CE RANG MÉRITE, ou nul quand l'installation n'a pas le fichier.
// L'image n'est pas possédée : elle doit vivre aussi longtemps que les quads.
//
// 🔴 C'EST ICI QUE LE RANG DEVIENT UNE IMAGE, et nulle part ailleurs : `render`
// ne connaît pas `sim`, il ne connaît que deux bitmaps.
[[nodiscard]] const ro::formats::Image* BossBadgeOf(ro::sim::BossRank rank,
                                                    const ro::render::BossBadges& badges);

// `name` REMPLACE celui de l'acteur quand il n'est pas vide : c'est par là que
// passe le nom composé d'un objet au sol, que le monde ne porte pas.
//
// `emblem` est l'emblème de guilde de l'acteur, ou nul — voir `EmblemOnPlate`.
// Il ne se possède pas non plus.
//
// ⚠ ELLE NE DÉCIDE PAS SI L'ÉTIQUETTE PARAÎT : c'est `ShowsEntityName` qui le
// dit, chez l'appelant, parce que lui seul sait ce que la souris désigne et ce
// qui est visé. Ici, `names` ne sert qu'à décider de ce que l'étiquette MONTRE.
[[nodiscard]] std::vector<ro::render::ScreenQuad> DrawnName(
    const ro::render::FontAtlas& font, const ro::render::Camera& camera, float aspect_ratio,
    const ro::render::DrawnActor& drawn, const ro::sim::Actor& actor,
    const ro::render::TitleTable* titles, const ro::sim::World& world,
    const std::optional<ro::ui::ScreenBand>& bars_under_feet,
    const ro::render::EntityNamesLook& names, const ro::render::BossBadges& badges,
    const ro::formats::Image* emblem, float viewport_width, float viewport_height,
    const ro::sim::BannedWords* banned, std::string_view name = {});

// Les enseignes des acteurs dessinés : l'échoppe qu'ils tiennent, le salon
// qu'ils ont ouvert. Chacune au-dessus du sien, dans la surface que `surfaces`
// tient pour sa sorte. Voir `ro/render/signboard.h`.
//
// `look` dit lesquelles se peignent et comment. Une sorte éteinte — les
// échoppes, les salons — ne se peint pas, ni ne se clique, puisqu'elle ne
// laisse pas de cadre.
//
// `spotlit_owner_id` désigne le marchand du dernier résultat cliqué dans la
// recherche d'échoppes : son enseigne se peint en jaune pâle. Zéro : aucune.
//
// Les mots de `banned` se masquent dans les titres ; nul, rien ne se masque.
//
// `hits`, s'il est donné, reçoit le cadre de chaque enseigne peinte, dans
// l'ordre de la peinture : c'est ce qu'un clic vise. Voir `SignboardHit`.
[[nodiscard]] std::vector<ro::render::ScreenQuad> DrawnSignboards(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, const ro::sim::PartyRoster& party,
    const ro::render::FontAtlas& font, const ro::render::SignboardSurfaces& surfaces,
    const ro::ui::EntityBarsLook& bars, const ro::render::SignboardLook& look,
    std::uint32_t spotlit_owner_id, float viewport_width, float viewport_height,
    const ro::sim::BannedWords* banned, std::vector<SignboardHit>* hits = nullptr);

// ⚠ LES JAUGES DE VIE NE SONT PLUS ICI. Elles se règlent espèce par espèce et
// écrivent parfois leurs nombres : elles sont donc passées en ImGui, où vit la
// police du chat. Voir `world/entity_bar_view.h`, qui les récolte, et
// `ro/ui/entity_bars.h`, qui les peint.

// Les quads du curseur à cet instant, la pointe sur la souris.
//
// `elapsed_ms` se compte depuis le changement de FORME : deux animations ne
// jouent qu'une fois et se figent, et une horloge qui ne repart pas les
// laisserait éteintes. `pressed` dit si le bouton gauche est enfoncé, ce dont
// deux formes dépendent.
[[nodiscard]] std::vector<ro::render::ScreenQuad> DrawnCursor(const CursorSprite& cursor,
                                                              ro::render::CursorShape shape,
                                                              glm::vec2 mouse_px, float elapsed_ms,
                                                              bool pressed);

// LE RANG DU SORT EN MAIN, en chiffres, à côté du curseur.
//
// 🔴 LE MÊME SPRITE QUE LES DÉGÂTS, et c'est le client qui le veut : il charge
// `이팩트\숫자` ici comme là, une image par chiffre dans sa première action.
//
// ⚠ IL SE DESSINE APRÈS LE CURSEUR, comme chez lui : sous la pointe, il
// disparaîtrait derrière l'anneau de ciblage sur les grandes échelles.
//
// Vide sans sprite, sans rang, ou quand le réglage l'éteint.
[[nodiscard]] std::vector<ro::render::ScreenQuad> DrawnArmedLevel(
    const ro::render::DigitSprite& digits, const ro::render::ArmedSkillLook& look,
    std::uint16_t level, glm::vec2 mouse_px);

}  // namespace moonclient
