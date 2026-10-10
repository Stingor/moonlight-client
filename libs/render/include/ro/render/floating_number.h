#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/damage_mark.h"
#include "ro/render/number_look.h"

namespace ro::render {

// Les nombres qui montent au-dessus d'un acteur frappé — les chiffres de
// dégâts.
//
// Le client ne les écrit pas avec une police : chaque chiffre est une IMAGE du
// sprite des chiffres — les dix chiffres, puis K et M —, portée par un petit
// acteur enfant de la cible, un par chiffre, qui vit sa vie propre : il
// grandit, monte, pâlit et s'éteint sur une horloge à VINGT-QUATRE
// millisecondes le tic. Les allures sont choisies par le MESSAGE du coup — voir
// `NumberStyle` —, qui fait naître chaque nombre par le message 88, et vécues
// par l'animation de l'enfant.
//
// 🔴 CES ENFANTS VIVENT DANS LE MONDE, PAS À L'ÉCRAN. Leur position est celle
// de la cible, recopiée à chaque image pour les genres 13 à 22, et c'est elle
// que l'allure déplace : en UNITÉS DU MONDE, dont chacune vaut sept pixels de
// sprite (`sprite_billboard.h`). Seul l'écart entre chiffres est un décalage en
// pixels, porté par un autre champ. Lire ces unités comme des pixels rendrait
// tout sept fois trop court.
//
// ⚠ TOUT CE QUI SE COMPOSE ICI PREND UN `NumberLook` : la vie et le chemin du
// type du nombre. Sans lui, c'est le client — `ClientLookOf` —, dont les
// constantes restent la seule source des défauts. Voir `number_look.h`.

// Le sprite des chiffres, sans extension : une action, une image par chiffre.
inline constexpr std::string_view kDigitsSpritePath = "data\\sprite\\이팩트\\숫자";

// Le tic de l'horloge des nombres, en millisecondes.
inline constexpr float kNumberTickMs = 24.0F;

// ── LES GRANDS NOMBRES ──────────────────────────────────────────────────────
//
// Sans abréviation, le client montre 999 999 au plus. Sous son réglage
// « Damage Display: Expand » (drapeau de réglage 0x10B), il abrège : d'un
// million à cent millions, le montant en MILLIERS suivi du glyphe K ; au-delà,
// en MILLIONS suivi du glyphe M. Un million de pile s'écrit donc « 1000K », et
// cent vingt-trois millions « 123M ».
inline constexpr std::uint32_t kLargestShownValue = 999'999;
inline constexpr std::uint32_t kFirstThousandsValue = 1'000'000;
inline constexpr std::uint32_t kFirstMillionsValue = 100'000'000;
inline constexpr std::uint32_t kValuesPerThousandsGlyph = 1'000;
inline constexpr std::uint32_t kValuesPerMillionsGlyph = 1'000'000;

// Les deux glyphes qui suivent les dix chiffres dans le sprite.
inline constexpr std::uint8_t kThousandsGlyph = 10;
inline constexpr std::uint8_t kMillionsGlyph = 11;

// Les glyphes d'une valeur, UNITÉS D'ABORD — l'ordre dans lequel le client les
// détache, et qui donne à chacun son rang. Un glyphe K ou M, quand il y en a
// un, vient en tête : c'est lui qui se dessine à droite.
[[nodiscard]] std::vector<std::uint8_t> NumberGlyphs(std::uint32_t value, bool abbreviate);

// ── Les coups MULTIPLES ─────────────────────────────────────────────────────
//
// Le serveur n'envoie qu'un montant, le TOTAL, et le nombre de coups qui l'ont
// fait. Le client ne montre pas ce total d'un bloc : il fait naître un nombre
// par coup, chacun deux cents millisecondes après le précédent, et chacun porte
// la somme des coups déjà portés — un tiers, deux tiers, le tout.
inline constexpr float kMultiHitIntervalMs = 200.0F;

// Au-delà, le client ne compte plus les coups et n'en montre qu'un.
inline constexpr std::uint16_t kLargestHitCount = 36;

// Les sortes de coup qui portent un compte : e_damage_type, dans
// src/map/clif.hpp. Ailleurs le champ ne veut rien dire, et le client le remet
// à un plutôt que de faire naître des nombres au hasard.
inline constexpr std::uint8_t kMultipleBlows = 8;
inline constexpr std::uint8_t kMultipleBlowsEndured = 9;
inline constexpr std::uint8_t kCriticalMultipleBlows = 13;

// Les sortes CRITIQUES, de la même énumération : un coup seul, et un coup
// multiple dont chaque coup l'est. Toutes deux posent une étoile d'impact par
// chiffre, et donnent au nombre sa couleur et son allure propres.
inline constexpr std::uint8_t kCriticalBlow = 10;

// Combien de nombres un coup fait naître, d'après sa sorte et le compte que le
// serveur annonce. Un, sauf pour un coup multiple bien annoncé.
[[nodiscard]] std::uint16_t ShownHitCount(std::uint8_t damage_type, std::uint16_t announced_hits);

enum class NumberStyle : std::uint8_t {
  // Le type 22 du client : le nombre naît petit, grossit d'un coup, monte
  // doucement et pâlit à la fin. C'est le CUMUL d'un coup multiple sur un
  // monstre — le message 27 requalifié en 111 quand il porte un cumul — et une
  // poignée de compétences envoyées par les messages 130 et 133, les coups de
  // pied du Taekwon. ⚠ PAS le coup ordinaire sur un monstre.
  kRising,
  // Le type 14, celui de TOUT COUP sur un monstre comme sur un joueur — les
  // messages 26, 39, 96 et 97 : coup ordinaire, encaissé, compétence — : il naît
  // grand, rétrécit, décrit une cloche et s'éteint plus vite — en DÉRIVANT de
  // côté dans le monde, voir `BurstDriftUnitsAt`.
  kBursting,
  // Le type 16, celui d'un coup CRITIQUE : la même cloche que le précédent, au
  // plancher d'échelle près — un et trois dixièmes au lieu d'un et deux. Le
  // client le choisit sur la sorte du coup et non sur la cible, si bien qu'un
  // critique jaillit même sur un monstre.
  kCriticalBurst,
  // Le type 114, celui d'un SOIN : il naît moins haut que le montant, monte
  // presque deux fois plus vite, RÉTRÉCIT au lieu de grossir, et pâlit dès sa
  // naissance.
  kHealing,
};

// Vrai pour les deux cloches — la jaillissante et le critique —, celles qui
// dérivent de côté.
[[nodiscard]] constexpr bool IsBurst(NumberStyle style) noexcept {
  return style == NumberStyle::kBursting || style == NumberStyle::kCriticalBurst;
}

// ── CE QUE LE CLIENT FAIT DE CHAQUE ALLURE ──────────────────────────────────
//
// Le client donne à chaque genre une vie — sa taille et son opacité au fil du
// temps — et un chemin vertical. Ici, l'une et l'autre deviennent des RÉGLAGES
// par type de dégât ; ce sont ces fonctions qui en donnent les défauts, tirés
// des constantes du client.

// SA VIE. ⚠ UNE APPROCHE, NON UNE COPIE : le client fait pâlir d'un pas fixe
// par tic et s'éteint avant zéro, et le nombre qui monte grossit au carré du
// temps. Ici, tout va d'un pas régulier jusqu'à zéro — l'écart ne dépasse pas
// un vingtième d'opacité, et un dixième de seconde de grossissement.
[[nodiscard]] NumberLife ClientLifeOf(NumberStyle style) noexcept;

// SON CHEMIN, en pixels de sprite, y vers le haut : où il arrive au-dessus de
// son point de naissance, et l'écart vertical de ses deux poignées — égales. La
// montée régulière est un chemin droit ; la cloche est EXACTE, une parabole
// dans le temps étant une courbe de Bézier.
struct NumberRoute {
  float arrival_y_pixels = 0.0F;
  float bend_y_pixels = 0.0F;
};
[[nodiscard]] NumberRoute ClientRouteOf(NumberStyle style) noexcept;

// LE RÉGLAGE QUI REND LE CLIENT POUR CE GENRE : sa vie, son chemin, et tous les
// facteurs à un.
[[nodiscard]] NumberLook ClientLookOf(NumberStyle style) noexcept;

// La vie que ce nombre suit : celle du réglage, ou celle du client.
[[nodiscard]] NumberLife LifeOf(NumberStyle style, const NumberLook& look) noexcept;

// ── LA DÉRIVE DU NOMBRE JAILLISSANT ─────────────────────────────────────────
//
// Le client fait DÉRIVER de côté le nombre d'un joueur touché — les types 14 et
// 16 —, dans le monde : depuis la position de la cible, qu'il recopie à chaque
// image, de dir × (t/3 + 3) unités, t en tics. `dir` est une diagonale de huit
// dixièmes choisie À LA NAISSANCE par quadrant de l'angle de la cible, sous le
// réglage DamageDir à zéro — celui de l'installation. Les trois autres valeurs
// du réglage, une diagonale fixe tournée par la caméra, ne sont pas portées.
inline constexpr float kBurstDriftDiagonal = 0.8F;
inline constexpr float kBurstDriftStartUnits = 3.0F;
inline constexpr float kBurstDriftUnitsPerTick = 1.0F / 3.0F;

// De combien le nombre a dérivé le long de sa diagonale, en unités du monde.
[[nodiscard]] constexpr float BurstDriftUnitsAt(float ticks) noexcept {
  return kBurstDriftStartUnits + kBurstDriftUnitsPerTick * ticks;
}

// La diagonale de la dérive, DANS LE REPÈRE DU CLIENT — x puis z, où son angle
// est celui dont le client oriente un acteur vers un point : atan2(dx, −dz) en
// degrés, le sud à zéro, l'est à quatre-vingt-dix. L'angle est tronqué en
// entier : sous quatre-vingt-dix degrés (−x, +z), sous cent quatre-vingts
// (−x, −z), sous deux cent soixante-dix (+x, −z), au-delà (+x, +z).
//
// 🔴 AU-DELÀ VEUT DIRE AU-DELÀ, SANS RETOUR AU TOUR : un angle négatif, ou d'un
// tour et plus, prend (+x, +z). Et le client en tient de tels — l'angle qu'il
// pose d'après une direction du serveur va de cent quatre-vingts à quatre cent
// quatre-vingt-quinze. Voir `ClientBodyDegrees` côté jeu.
//
// ⚠ Le z du client est l'opposé du nôtre : qui pose la dérive dans notre monde
// retourne cette composante.
[[nodiscard]] std::array<float, 2> BurstDriftClientDiagonal(float facing_degrees) noexcept;

// ── Les compétences SANS DÉGÂTS ─────────────────────────────────────────────
//
// Le serveur les annonce toutes par le MÊME paquet, et le champ qui s'appelle
// « niveau » y porte en réalité le MONTANT quand la compétence en a un. C'est
// donc la compétence qui dit ce que ce nombre veut dire, et le client n'en
// montre un que pour HUIT d'entre elles, que la table des suites nomme
// (`SkillSuiteKind::kSupportNumber`) : le soin et ses cinq parentes, en vert,
// et les deux récupérations de points de magie, en bleu. Les centaines
// d'autres qu'il traite ne posent qu'un effet.
inline constexpr std::uint16_t kHealSkill = 28;
inline constexpr std::uint16_t kSpRecoverySkill = 9;
inline constexpr std::array<std::uint8_t, 3> kHealNumberColor = {0, 255, 0};
inline constexpr std::array<std::uint8_t, 3> kSpRecoveryNumberColor = {0, 0, 255};

// La couleur du nombre qu'une compétence sans dégâts fait paraître sur sa
// cible, ou rien si elle n'en fait paraître aucun.
[[nodiscard]] std::optional<std::array<std::uint8_t, 3>> SupportNumberColor(std::uint16_t skill_id);

// ── Les EFFETS QUI COMPTENT ─────────────────────────────────────────────────
//
// Le paquet 0x0284 porte un effet ET une valeur. Le client joue l'effet dans
// tous les cas, et ne fait monter le nombre que pour DIX numéros : deux qui
// nomment ce qu'ils comptent — la vie et la magie rendues au fil du temps — et
// huit qui ne nomment qu'une COULEUR, que les scripts du serveur emploient pour
// afficher ce qu'ils veulent. Tous les autres numéros ne montrent que leur
// effet, et la valeur du paquet se perd.
//
// Les numéros sont ceux de e_effect_type (src/map/script.hpp du serveur) ; les
// couleurs sont celles que le client écrit, en ARGB, et toutes sont opaques.
//
// ⚠ Les deux premiers CHANGENT aussi la vie et la magie du joueur chez le
// client, qui les ajoute à son propre compte avant de les montrer. Nous ne
// tenons ni l'une ni l'autre, et le serveur les redit par ses paquets de
// paramètres : il n'y a rien à ajouter ici.
inline constexpr std::uint32_t kHpOverTimeEffect = 331;
inline constexpr std::uint32_t kSpOverTimeEffect = 332;
inline constexpr std::uint32_t kFirstColoredNumberEffect = 657;
inline constexpr std::uint32_t kLastColoredNumberEffect = 664;

// La couleur du nombre qu'un effet à valeur fait paraître, ou rien pour un
// effet qui n'en fait paraître aucun — l'immense majorité.
[[nodiscard]] std::optional<std::array<std::uint8_t, 3>> EffectNumberColor(std::uint32_t effect_id);

struct FloatingNumber {
  std::uint32_t value = 0;
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  // L'opacité propre de CE nombre, en facteur sur le pâlissement de l'allure et
  // par-dessus celle du réglage : la transparence qu'un nuancier donne à une
  // famille de nombres arrive par là.
  float opacity = 1.0F;
  NumberStyle style = NumberStyle::kRising;
};

// Le sprite des chiffres, tel qu'il est chargé. Les pointeurs ne sont pas
// possédés.
struct DigitSprite {
  const formats::SprSprite* sprite = nullptr;
  const formats::ActAnimation* animation = nullptr;
};

// Vrai quand le nombre a fini de vivre : au terme de sa vie — voir `LifeOf`.
// Sans réglage, celle du client : cent vingt tics pour l'allure montante et le
// soin, soixante-dix pour les cloches.
[[nodiscard]] bool FloatingNumberExpired(NumberStyle style, float elapsed_ms,
                                         const NumberLook& look);
[[nodiscard]] bool FloatingNumberExpired(NumberStyle style, float elapsed_ms);

// ⚠ CHAQUE COMPOSITION CI-DESSOUS A SA FORME SANS RÉGLAGE : c'est le client,
// pour le genre du nombre — `ClientLookOf`.

// Le nombre à l'instant donné, depuis sa naissance : ses chiffres en pixels de
// sprite autour des PIEDS de la cible, à l'échelle et à l'opacité de l'instant.
// Vide une fois le nombre éteint.
//
// ⚠ La hauteur atteinte est DANS les quads, comme le client l'y met : ses
// chiffres sont des enfants de la cible, décalés dans son repère. Un nombre
// reste donc, pour la profondeur, exactement là où sa cible se tient — et les
// nombres successifs d'une même cible se dessinent dans l'ordre où on les
// donne au lieu de s'échanger selon la hauteur qu'ils ont atteinte. La dérive
// de côté, elle, est un déplacement DANS LE MONDE : c'est à qui pose le nombre
// de l'ajouter à la position de la cible.
[[nodiscard]] ComposedActor ComposeFloatingNumber(const DigitSprite& digits,
                                                  const FloatingNumber& number, float elapsed_ms,
                                                  const NumberLook& look);
[[nodiscard]] ComposedActor ComposeFloatingNumber(const DigitSprite& digits,
                                                  const FloatingNumber& number, float elapsed_ms);

// Un nombre CRITIQUE : l'étoile d'impact derrière, le nombre devant, dans le
// même cadre.
//
// UN SEUL CADRE POUR LES DEUX, bien que le client en fasse deux sprites-enfants.
// Deux cadres séparés laisseraient l'étoile passer DEVANT le nombre — le dernier
// posé se dessine par-dessus —, alors qu'elle est un fond.
//
// 🔴 ELLE NE PREND DU NOMBRE QUE SA PLACE. Sa taille, son image et son opacité
// sont celles de son genre à elle, le quinzième, et `ComposeDamageMark` les
// calcule : elle naît à l'échelle trois quand le nombre naît à cinq, tourne
// d'une demie par tic et pâlit depuis le plein. Ce qu'elle partage vraiment
// avec lui est le CHEMIN — même hauteur de naissance, même cloche, même dérive
// —, parce que les genres quinze et seize ont le même corps dans le client.
//
// Les deux vivent soixante-dix tics et s'éteignent donc ENSEMBLE ; l'étoile
// se fige sur sa dernière image bien avant, et continue de pâlir sur place.
[[nodiscard]] ComposedActor ComposeCriticalNumber(const DigitSprite& digits,
                                                  const FloatingNumber& number,
                                                  const formats::SprSprite& mark_sprite,
                                                  const formats::ActAnimation& mark_animation,
                                                  float elapsed_ms, const NumberLook& look);
[[nodiscard]] ComposedActor ComposeCriticalNumber(const DigitSprite& digits,
                                                  const FloatingNumber& number,
                                                  const formats::SprSprite& mark_sprite,
                                                  const formats::ActAnimation& mark_animation,
                                                  float elapsed_ms);

// Le premier nombre d'un COUP DE PIED sur un monstre : l'étoile du genre
// vingt-cinq derrière, le nombre montant devant, dans le même cadre et pour la
// même raison que le critique.
//
// Le client fait naître l'étoile AVANT le nombre — son message part avant
// celui du chiffre —, si bien qu'elle se peint dessous. Elle prend du nombre
// sa PLACE, et rien d'autre : les deux genres, vingt-deux et vingt-cinq,
// montent du même dixième d'unité par tic depuis vingt-sept unités.
[[nodiscard]] ComposedActor ComposeRisingStarNumber(const DigitSprite& digits,
                                                    const FloatingNumber& number,
                                                    const formats::SprSprite& mark_sprite,
                                                    const formats::ActAnimation& mark_animation,
                                                    float elapsed_ms, const NumberLook& look);
[[nodiscard]] ComposedActor ComposeRisingStarNumber(const DigitSprite& digits,
                                                    const FloatingNumber& number,
                                                    const formats::SprSprite& mark_sprite,
                                                    const formats::ActAnimation& mark_animation,
                                                    float elapsed_ms);

// L'ÉTOILE SEULE d'un nombre qui en porte une — `kCritical` ou `kRisingStar` —,
// posée là où le nombre se trouve à cet instant, sans le nombre. C'est ce que
// garde un nombre écrit en police : son fond reste l'image du client.
[[nodiscard]] ComposedActor ComposeNumberStar(const FloatingNumber& number,
                                              const formats::SprSprite& mark_sprite,
                                              const formats::ActAnimation& mark_animation,
                                              DamageMark star, float elapsed_ms,
                                              const NumberLook& look);
[[nodiscard]] ComposedActor ComposeNumberStar(const FloatingNumber& number,
                                              const formats::SprSprite& mark_sprite,
                                              const formats::ActAnimation& mark_animation,
                                              DamageMark star, float elapsed_ms);

// ── LES NOMBRES ÉCRITS EN POLICE ────────────────────────────────────────────
//
// Au lieu des images du sprite des chiffres, le joueur peut faire écrire ses
// nombres par une police. 🔴 LA POSE RESTE CELLE DU SPRITE, au pixel près :
// même allure, même réglage, même place, même couleur. Seul le dessin des
// glyphes change, et c'est à qui écrit de le faire — `render` ne tient pas de
// police qui grandisse de un à cinq sans flou.

// Ce qu'un nombre en police porte à cet instant.
struct NumberText {
  // Le point d'appui de chaque glyphe, en pixels de sprite depuis les pieds de
  // la cible, APRÈS l'échelle — l'ordonnée DESCEND, comme celle d'un quad.
  // C'est le point où le sprite pose l'origine de ses chiffres.
  glm::vec2 landing_pixels{0.0F, 0.0F};
  // L'échelle de l'instant, réglage compris.
  float scale = 1.0F;
  // L'écart d'un centre de glyphe au suivant, en pixels de sprite AVANT
  // l'échelle.
  float pitch_pixels = kDigitPitchPixels;
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // Les glyphes du plus fort au plus faible, donc de gauche à droite : les
  // chiffres, puis « K » ou « M » quand le nombre est abrégé.
  std::string glyphs;
};

// Le nombre en police à l'instant donné, ou rien une fois éteint.
[[nodiscard]] std::optional<NumberText> ComposeFloatingNumberText(const FloatingNumber& number,
                                                                  float elapsed_ms,
                                                                  const NumberLook& look);
[[nodiscard]] std::optional<NumberText> ComposeFloatingNumberText(const FloatingNumber& number,
                                                                  float elapsed_ms);

// LA BOÎTE D'UN CHIFFRE DU SPRITE, à l'échelle un, par rapport à son point
// d'appui : ce qu'un glyphe de police doit épouser pour prendre la taille et
// la place du chiffre qu'il remplace.
struct GlyphBox {
  float height_pixels = 0.0F;
  // Où tombe le milieu du chiffre, l'ordonnée descendant.
  float middle_y_pixels = 0.0F;
};

// Celle du zéro, que tous les chiffres partagent. Rien sans sprite ou sans
// image : il n'y a alors aucune taille à épouser.
[[nodiscard]] std::optional<GlyphBox> DigitGlyphBox(const DigitSprite& digits);

}  // namespace ro::render
