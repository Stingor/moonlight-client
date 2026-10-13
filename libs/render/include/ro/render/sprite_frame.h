#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_layers.h"

namespace ro::render {

// Un calque résolu, prêt à dessiner.
//
// Les coordonnées sont en pixels, dans le repère du sprite : l'origine est le
// point que le .act pose à l'emplacement de l'acteur. Aucune dépendance à
// OpenGL — c'est une transformation de données pure, donc vérifiable par des
// assertions numériques, ce qui compte d'autant plus ici que les erreurs de
// composition ne se voient qu'à l'œil.
struct SpriteQuad {
  // Quelle image, et dans laquelle des deux sections du .spr.
  std::int32_t image_index = -1;
  std::int32_t section = 0;
  // Coins dans l'ordre haut-gauche, haut-droit, bas-droit, bas-gauche.
  std::array<glm::vec2, 4> corners{};
  // Le miroir échange les abscisses de texture, il ne retourne PAS le quad
  // autour de son origine.
  bool mirrored = false;
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
};

// Les rangs d'animation d'un acteur, dans l'ordre où le .act les range.
//
// Ce sont des rangs de GROUPE, pas des index d'action : chacun couvre huit
// entrées consécutives, une par direction. Voir ActionIndex.
//
// Le rang ne se lit nulle part dans le fichier : c'est une CONVENTION du jeu,
// et le .act ne porte que des numéros. Le client indexe bien par « rang × 8 +
// direction », et reconnaît l'attaque aux index à plat 0x50 à 0x60, c'est-à-dire
// aux rangs 10 et 11. Cela désigne la
// table des sprites de JOUEUR — attente, marche, assis, ramassage, garde,
// attaque, blessure… — dont la marche occupe le rang un.
inline constexpr std::size_t kAnimationIdle = 0;
inline constexpr std::size_t kAnimationWalk = 1;
// Les autres rangs d'un JOUEUR, dont le .act en porte treize.
inline constexpr std::size_t kAnimationSit = 2;
inline constexpr std::size_t kAnimationPickup = 3;
// LA GARDE, que le client appelle « ready fight » : arme levée, sur place. Ce
// n'est pas un coup — ceux-là sont aux rangs dix et onze — mais la pose que
// prend un personnage en combat entre deux frappes, et celle sous laquelle un
// joueur se reconnaît. C'est pourquoi la fiche de personnage la montre par
// défaut.
inline constexpr std::size_t kAnimationReady = 4;
inline constexpr std::size_t kAnimationHurt = 6;
inline constexpr std::size_t kAnimationDie = 8;
// Les deux rangs d'attaque d'un joueur ; lequel dépend du métier et de l'arme,
// voir AttackAnimation dans sprite_catalog.h.
inline constexpr std::size_t kAnimationAttack2 = 10;
inline constexpr std::size_t kAnimationAttack3 = 11;
// L'INCANTATION, et c'est le dernier rang d'un joueur. Les outils qui lisent
// un .act le nomment « Casting spell ».
//
// Le client y arrive par un chemin à lui : l'incantation ordinaire pose l'état
// huit, que la table ordinaire des états ne connaît pas — mais le choix de la
// motion ne l'y envoie jamais, car il a un cas pour cet état-là, qui en rend
// bien le douzième rang. Trois métiers montés y font exception.
inline constexpr std::size_t kAnimationCast = 12;
// Ceux d'une CRÉATURE — monstre, PNJ, familier —, dont le .act n'en porte
// que cinq : attente, marche, attaque, blessure, mort.
inline constexpr std::size_t kCreatureAnimationAttack = 2;
inline constexpr std::size_t kCreatureAnimationHurt = 3;
inline constexpr std::size_t kCreatureAnimationDie = 4;
// Ceux d'un HOMONCULE, dont le .act en porte davantage : le rang que son
// emploi d'une compétence joue, et un second rang d'attaque, tiré au sort une
// fois sur deux à chaque coup — sauf pour Eleanor, qui n'a que le premier.
inline constexpr std::size_t kHomunculusAnimationSkill = 5;
inline constexpr std::size_t kHomunculusAnimationAttack2 = 6;
inline constexpr std::uint32_t kSingleAttackHomunculusJob = 6052;

// Ce qu'un acteur FAIT, tel que le client le nomme. Le rang d'animation qui
// le montre dépend de la disposition du .act.
//
// ⚠ `kReady` N'EST PAS UNE ACTION QU'ON DEMANDE, c'est un REPOS : celui de qui
// vient de se battre. Voir `kAnimationReady` et `ro::sim::GuardsLeaving`.
enum class Motion : std::uint8_t {
  kIdle,
  kWalk,
  kSit,
  kPickup,
  kReady,
  kAttack,
  kCast,
  kHurt,
  kDie
};

// La disposition du .act : treize rangs pour un joueur, cinq pour une
// créature. Le client choisit d'après la VUE de l'acteur — un joueur n'en a
// pas, une créature porte sa classe.
enum class SpriteLayout : std::uint8_t { kPlayer, kCreature };

// Le rang d'animation d'une motion. Une créature ne s'assoit ni ne ramasse :
// sa table rend alors le rang de l'attaque, comme chez le client. L'attaque
// d'un joueur a deux rangs ; sans catalogue pour choisir, c'est le premier.
//
// ⚠ Une créature qui INCANTE reste à l'ATTENTE, et non à l'attaque : son
// fichier n'a pas de rang d'incantation, et lui prêter celui de l'attaque la
// ferait frapper pendant qu'elle prépare son sort.
[[nodiscard]] std::size_t AnimationFor(Motion motion, SpriteLayout layout) noexcept;

// Vrai pour une motion qui se rejoue en boucle : l'attente, la marche, l'assise
// et la garde. Les autres se jouent UNE fois depuis leur première image et
// s'arrêtent sur la dernière — un mort reste à terre —, ou tiennent une image
// que l'appelant désigne : l'incantation tient sa première.
[[nodiscard]] bool MotionLoops(Motion motion) noexcept;

// De combien la cadence d'une MARCHE se resserre, d'après la vitesse du pas.
//
// Le client multiplie le délai d'image par la durée du pas rapportée à 432 ms,
// plafonnée à un : un personnage
// rapide fait défiler ses images d'autant, sans quoi il glisse au lieu de
// marcher. Une vitesse inconnue laisse la cadence du fichier.
// Combien d'images une unité du monde fait défiler, à délai d'image UN.
//
// La marche ne s'anime pas sur une horloge : le client accumule la DISTANCE
// parcourue (une racine carrée sur les abscisses et les ordonnées du monde) et
// en tire l'image :
//
//   image = (distance × 0,37 × 4) / délai, modulo le nombre d'images
//
// où le délai est celui que le fichier déclare, en tics de vingt-quatre
// millisecondes. Rien d'autre n'intervient : c'est pour cela qu'un personnage
// rapide lève les pieds plus souvent, sans qu'aucune vitesse d'animation ne
// soit posée nulle part, et qu'un personnage lent traîne.
inline constexpr float kWalkFramesPerWorldUnit = 0.37F * 4.0F;

// L'image d'une marche, d'après la distance parcourue en unités du monde.
[[nodiscard]] std::size_t WalkFrameAt(float travelled_world_units, float frame_interval_ms,
                                      std::size_t frame_count) noexcept;

// L'IMAGE DE MARCHE QUE TIENT QUI GLISSE — le bond de Body Relocation — au lieu
// de lever les pieds : la septième, ou la dernière si l'action en a moins.
[[nodiscard]] std::size_t GlideHeldWalkFrame(std::size_t frame_count) noexcept;

// L'index d'action correspondant à une animation vue dans une direction.
//
// Les actions d'un .act sont rangées par groupes de huit, une par direction.
[[nodiscard]] constexpr std::size_t ActionIndex(std::size_t animation,
                                                std::size_t direction) noexcept {
  return animation * kDirectionCount + (direction % kDirectionCount);
}

// L'index de direction d'un .act pour une direction de BOUSSOLE.
//
// Les deux numérotations ne coïncident pas, et aucun fichier ne le dit :
//
//   * un .act tourne dans le sens des aiguilles d'une montre EN PARTANT DU
//     SUD, c'est-à-dire de la face tournée vers la caméra — sud, sud-ouest,
//     ouest, nord-ouest, nord, nord-est, est, sud-est ;
//   * le serveur tourne dans l'autre sens en partant du NORD — les tables
//     dirx et diry, dans src/map/unit.cpp — nord, nord-ouest, ouest,
//     sud-ouest, sud, sud-est, est, nord-est.
//
// Elles tombent d'accord à l'EST et à l'OUEST, et s'opposent partout ailleurs.
// Prendre l'une pour l'autre donne donc un personnage impeccable de profil et
// retourné face à la caméra quand il devrait montrer son dos : un défaut que
// sa moitié correcte cache.
//
// La caméra entre en compte : ce qu'elle montre d'un personnage IMMOBILE change
// quand elle tourne autour de la scène, d'un huitième de tour à la fois. Sans
// cet angle, tout est juste tant qu'on ne fait pas pivoter la vue, et faux
// ensuite.
[[nodiscard]] std::size_t SpriteDirection(std::size_t compass_direction,
                                          float camera_yaw_degrees = 0.0F) noexcept;

// L'INVERSE DE `SpriteDirection` sous une caméra qui n'a pas pivoté : la
// direction de boussole qu'un acteur doit tenir pour montrer cette direction
// du .act. C'est ainsi qu'un acteur factice se tourne comme l'image qu'il
// double — un pantin qu'on fait tourner à la molette.
[[nodiscard]] std::size_t CompassOfSpriteDirection(std::size_t sprite_direction) noexcept;

// LA DIRECTION D'ÉCRAN D'UN CORPS DONT LE CLIENT TIENT L'ANGLE DE FACE — voir
// `ro::sim::ClientAimDegrees` : le sud vaut zéro, l'est quatre-vingt-dix —,
// vu d'une caméra tournée de `camera_yaw_degrees`, dans la convention de
// `SpriteDirection`. Sur un angle multiple de quarante-cinq, les deux
// s'accordent ; c'est l'angle EXACT d'une visée qui les sépare.
//
// 🔴 DEUX DÉCOUPAGES, ET LE CLIENT CHOISIT PAR RANG. L'angle relatif à la
// caméra, ramené dans le tour, s'ARRONDIT au huitième le plus proche pour un
// joueur au repos, en marche ou assis, et se TRONQUE au huitième inférieur pour
// tout le reste — un monstre toujours, un joueur qui frappe, incante ou
// encaisse. Une même visée montre donc une face pendant le coup et parfois
// une autre au repos : voir `ScreenDirectionRounds`.
[[nodiscard]] std::size_t SpriteDirectionOfDegrees(float client_body_degrees,
                                                   float camera_yaw_degrees, bool rounded) noexcept;

// Vrai quand ce rang, chez ce métier, ARRONDIT l'angle au lieu de le tronquer :
// les trois premiers rangs — attente, marche, assise — d'un corps de joueur,
// d'un mercenaire ou d'un homoncule évolué ; l'attente seule d'un PNJ ; jamais
// un monstre.
[[nodiscard]] bool ScreenDirectionRounds(std::uint32_t job_id, bool player_layout,
                                         std::size_t animation) noexcept;

// Résout les calques d'une image d'animation, dans l'ordre de dessin du fichier.
//
// ⚠ `apply_rotation` vaut faux pour composer un ACTEUR : le jeu construit un
// rectangle aligné aux axes et ignore l'angle des calques. Des costumes en
// portent pourtant un ; le respecter serait fidèle au FICHIER et infidèle au
// JEU, et donnerait un chapeau qui pivote sur la tête. La rotation reste
// disponible pour les usages où elle est réelle, comme les effets.
[[nodiscard]] std::vector<SpriteQuad> BuildFrameQuads(const formats::SprSprite& sprite,
                                                      const formats::ActAnimation& animation,
                                                      std::size_t action_index,
                                                      std::size_t frame_index,
                                                      bool apply_rotation = false);

// L'ancre de RÉFÉRENCE d'une image : la première de la liste.
//
// ⚠ Pour une action de rang inférieur à huit — le premier groupe, celui des
// poses au repos — l'ancre se lit sur l'IMAGE 0 et non sur l'image courante.
// Sans cela les pièces rapportées VIBRENT pendant l'animation d'attente.
[[nodiscard]] std::optional<formats::ActAnchor> FrameAnchor(const formats::ActAnimation& animation,
                                                            std::size_t action_index,
                                                            std::size_t frame_index);

// L'image à afficher d'une action après `elapsed_ms` millisecondes.
//
// La cadence vient du fichier, en unités RO converties en millisecondes. Une
// action sans image rend 0 ; une durée négative est ramenée au début plutôt
// que de produire un index négatif.
[[nodiscard]] std::size_t FrameAtTime(const formats::ActAction& action, float elapsed_ms);

// L'image d'une action jouée UNE fois : la dernière, passé son terme.
[[nodiscard]] std::size_t FrameAtTimeOnce(const formats::ActAction& action, float elapsed_ms);

// LE MARQUEUR DU COUP dans la liste des sons d'un .act. Ce n'est pas un son —
// il ne finit pas par « .wav » — mais l'image qui le porte est celle où
// l'attaque d'une CRÉATURE touche : c'est là que sa munition part, que sa
// cible bronche et que ses dégâts paraissent.
inline constexpr std::string_view kAttackSoundMarker = "atk";

// L'IMAGE DE FRAPPE D'UNE ACTION : la première image dont le son est le
// marqueur du coup ; sinon l'avant-dernière, et la seule pour une action d'une
// image. Zéro pour une action sans image ou hors du fichier.
//
// ⚠ C'EST L'IMAGE D'UNE CRÉATURE. Un joueur tient la sienne d'une table par
// métier et par arme — voir `DamageFrame` —, et son fichier n'est pas lu.
[[nodiscard]] std::size_t AttackSoundFrame(const formats::ActAnimation& animation,
                                           std::size_t action_index);

// Le décalage qui accroche une pièce à celle qui la porte.
//
// ⚠ Il vaut « ancre du parent moins ancre de la pièce », et UNIQUEMENT si les
// deux attributs d'ancre coïncident : sans ce test on accroche des pièces qui
// n'ont rien à voir l'une avec l'autre. En l'absence d'ancre exploitable, le
// décalage est nul — une pièce mal placée vaut mieux qu'un personnage amputé.
[[nodiscard]] glm::vec2 AttachOffset(const std::optional<formats::ActAnchor>& parent,
                                     const std::optional<formats::ActAnchor>& piece);

}  // namespace ro::render
