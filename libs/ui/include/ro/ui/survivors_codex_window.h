#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "ro/ui/skin_art.h"
#include "ro/ui/survivors_album_window.h"

namespace ro::ui {

// LE CODEX DES SURVIVANTS : six onglets — Sorts, Cartes, Objets, Plantes,
// Monstres, Boss et événements —, une liste à gauche, une fiche à droite.
//
// 🔴 UNE ENTRÉE NON VUE NE MONTRE QUE SA SILHOUETTE ET « ??? » : jamais son
// nom, jamais son icône en couleur, jamais un de ses nombres. Elle se révèle
// la première fois qu'une partie la montre. Un sort que verrouille un exploit
// dit en plus le but de cet exploit (`unlock_feat`), et rien d'autre.
//
// `ui` ne voit aucun type de `sim` : `app` compose la vue en nombres, clés et
// identifiants, et des énumérations de `ui` en miroir des siennes. Les
// éléments et les tailles sont des rangs dans `kDatabaseElements` et
// `kDatabaseSizes`. L'onglet Cartes est l'Album, qui garde sa propre vue.

inline constexpr std::size_t kSurvivorsCodexMostLevels = 8;
// Une entrée sans élément ou sans taille (un objet, un événement).
inline constexpr std::uint8_t kNoCodexElement = 0xFF;
inline constexpr std::uint8_t kNoCodexSize = 0xFF;

enum class SurvivorsCodexTab : std::uint8_t { kSkills, kCards, kItems, kPlants, kFoes, kBosses };
inline constexpr std::size_t kSurvivorsCodexTabCount = 6;
static_assert(static_cast<std::size_t>(SurvivorsCodexTab::kBosses) + 1 == kSurvivorsCodexTabCount);

// La forme d'une fiche : ce que la fenêtre dessine à droite.
enum class SurvivorsCodexSheet : std::uint8_t {
  kSkill,
  kItem,
  kChest,
  kGem,
  kGroundCard,
  kPlant,
  kFoe,
  kBoss,
  kEvent,
};

// L'unité d'un nombre : la fenêtre dit une durée en secondes, une mesure en
// cases, un pourcentage avec son signe.
enum class SurvivorsCodexUnit : std::uint8_t {
  kPlain,
  kMs,
  kCellTenths,
  kPercent,
  kPercentHundredths,  // 1200 : « 12 % »
};

// LE LIBELLÉ D'UNE LIGNE DE CHIFFRES, que `ui` traduit.
enum class SurvivorsCodexFigureLabel : std::uint8_t {
  // Un sort, dans l'ordre de sa fiche.
  kDamage,
  kCooldown,
  kAmount,
  kPierce,
  kReach,
  kArea,
  kLength,
  kLife,
  kZoneHits,
  kChill,
  kFreeze,
  kKnockback,
  kBounces,
  kSplash,
  // Un monstre ou un boss.
  kHp,
  kHpGrowthPerMinute,
  kTouchDamage,
  kTouchInterval,
  kWalkPerCell,
  kExp,
  // Un objet au sol, un coffre, une gemme, une plante.
  kHeal,
  kZeny,
  kExpBoost,
  kWalkBoost,
  kDuration,
  // La chance qu'un coffre donne `gain_count` gains : la part du tirage qui
  // tombe sur ce compte, et non le seuil qui l'ouvre.
  kGainsOdds,
  kEvolutionsFrom,
  kGemExp,
  kDrawWeight,
  // Les élites : ce qui s'ajoute à un monstre ordinaire (XP par `kExpBoost`).
  kHpBoost,
  kTouchBoost,
};

// Une ligne de chiffres : une valeur par niveau pour un sort, une seule
// ailleurs.
struct SurvivorsCodexFigure {
  SurvivorsCodexFigureLabel label = SurvivorsCodexFigureLabel::kDamage;
  SurvivorsCodexUnit unit = SurvivorsCodexUnit::kPlain;
  std::array<std::int64_t, kSurvivorsCodexMostLevels> values = {};
  std::uint8_t count = 1;
  // Le compte de gains que nomme une ligne `kGainsOdds` ; 0 ailleurs.
  std::uint8_t gain_count = 0;
};

// Une autre entrée nommée par la fiche : la carte d'une évolution, l'évolué,
// l'objet qu'une plante lâche, l'espèce qu'un événement amène… `app` en pose
// le nom et l'icône.
//
// 🔴 UN LIEN SUIT LA RÈGLE DES ENTRÉES : non vu (`seen` faux), il ne montre que
// la silhouette de son icône et « ??? », même dans la fiche d'une entrée vue.
// Une carte est vue quand l'Album l'a trouvée.
struct SurvivorsCodexLink {
  std::uint16_t ro_id = 0;  // compétence, objet ou classe RO
  bool seen = false;
  SkinImage icon;
  const char* name = "";
};

// L'évolution d'un sort de base : sa carte, au niveau maximal, donne l'évolué.
struct SurvivorsCodexEvolution {
  SurvivorsCodexLink card;
  SurvivorsCodexLink result;
  std::uint8_t max_level = 0;
};

// D'où vient un monstre, et à quelle heure au plus tôt.
enum class SurvivorsCodexSourceKind : std::uint8_t { kWave, kEvent, kEscort, kSummon };
struct SurvivorsCodexSource {
  SurvivorsCodexSourceKind kind = SurvivorsCodexSourceKind::kWave;
  std::uint32_t at_ms = 0;
  SurvivorsCodexLink bringer;  // le boss qui l'escorte ou l'invoque ; vide sinon
};

// Un élément d'attaque et son effet sur l'entrée, en %.
struct SurvivorsCodexAffinity {
  std::uint8_t element = 0;
  std::int32_t pct = 0;
};

// La façon dont un monstre approche, en miroir des règles.
enum class SurvivorsCodexGait : std::uint8_t {
  kChase,
  kStill,
  kFlyer,
  kThief,
  kRanged,
  kCharger,
  kBlinker,
};
// Ses traits, un masque, en miroir des règles.
inline constexpr std::uint8_t kCodexTraitKnockbackImmune = 1U << 0U;
inline constexpr std::uint8_t kCodexTraitFreezeImmune = 1U << 1U;
inline constexpr std::uint8_t kCodexTraitPassesWalls = 1U << 2U;
inline constexpr std::uint8_t kCodexTraitHides = 1U << 3U;
inline constexpr std::uint8_t kCodexTraitOverHorde = 1U << 4U;
struct SurvivorsCodexBehaviour {
  SurvivorsCodexGait gait = SurvivorsCodexGait::kChase;
  std::uint8_t traits = 0;
  std::uint8_t pack = 1;
};

// La carte qu'un monstre lâche : une fois sur `odds`, ou garantie au premier
// tué de son espèce.
struct SurvivorsCodexCardDrop {
  SurvivorsCodexLink card;
  std::uint32_t odds = 0;
  bool guaranteed = false;
};

// Un coup de boss, en miroir des règles. Actif tant que les PV restants (en %)
// sont dans ]to_hp_pct, from_hp_pct].
enum class SurvivorsCodexMoveKind : std::uint8_t { kSummon, kLeap, kCharge, kQuake, kHowl, kFlee };
struct SurvivorsCodexMove {
  SurvivorsCodexMoveKind kind = SurvivorsCodexMoveKind::kSummon;
  std::uint8_t from_hp_pct = 0;
  std::uint8_t to_hp_pct = 0;
  std::uint32_t period_ms = 0;
  std::uint32_t telegraph_ms = 0;
  std::uint32_t effect_ms = 0;
  std::uint8_t count = 0;
  SurvivorsCodexLink foe;  // l'espèce invoquée ou pressée
  std::int32_t reach_tenths = 0;
  std::int32_t radius_tenths = 0;
  std::int32_t damage = 0;
  std::int32_t boost_pct = 0;
};

// La sorte d'un événement de la chronologie, en miroir des règles, plus les
// élites et la nuit.
enum class SurvivorsCodexEventKind : std::uint8_t { kSwarm, kRaid, kRing, kGroup, kElites, kNight };

struct SurvivorsCodexEntry {
  SurvivorsCodexSheet sheet = SurvivorsCodexSheet::kSkill;
  bool seen = false;
  // La clé de Codex : elle choisit la phrase d'une entrée hors des sorts.
  const char* key = "";
  // La compétence, l'objet ou la classe RO de l'entrée ; 0 pour un événement.
  std::uint16_t ro_id = 0;
  const char* name = "";
  SkinImage icon;
  // La phrase d'un sort, ⚠ EN FRANÇAIS, marquée par `TrText` : traduite par
  // `ui`. Vide ailleurs : la phrase vient de la clé.
  const char* description = "";
  std::uint8_t element = kNoCodexElement;
  std::uint8_t size = kNoCodexSize;
  std::span<const SurvivorsCodexFigure> figures;
  // Un sort verrouillé par un exploit : la clé de l'exploit, dont `ui` dit le
  // but sous la silhouette. "" sinon.
  const char* unlock_feat = "";

  // Un sort.
  bool has_evolution = false;
  SurvivorsCodexEvolution evolution;
  bool evolved = false;
  SurvivorsCodexEvolution evolved_from;  // `result` est l'entrée elle-même

  // Un monstre, un boss.
  bool arrives = false;  // faux : aucune source ne l'amène
  std::uint32_t arrives_ms = 0;
  std::span<const SurvivorsCodexSource> sources;
  std::span<const SurvivorsCodexAffinity> weaknesses;   // par écart décroissant
  std::span<const SurvivorsCodexAffinity> resistances;  // 0 % : une immunité
  SurvivorsCodexBehaviour behaviour;
  bool drops_card = false;
  SurvivorsCodexCardDrop card;

  // Un boss ou un événement : l'heure ; une élite, la première.
  std::uint32_t at_ms = 0;
  bool final = false;  // sa chute est la victoire
  SurvivorsCodexLink escort;
  std::uint8_t escort_count = 0;
  std::span<const SurvivorsCodexMove> moves;
  SurvivorsCodexEventKind event_kind = SurvivorsCodexEventKind::kGroup;
  SurvivorsCodexLink event_foe;
  std::uint8_t event_count = 0;
  // Les élites : l'intervalle et la dernière ; la nuit : minuit.
  std::uint32_t interval_ms = 0;
  std::uint32_t until_ms = 0;

  // Une plante : l'objet qu'elle lâche, et sa clé de Codex.
  SurvivorsCodexLink drop;
  const char* drop_key = "";
};

// LA VUE : une liste par onglet, dans l'ordre de la liste. L'onglet Cartes y
// reste vide : il dessine `SurvivorsAlbumView`.
struct SurvivorsCodexView {
  std::array<std::span<const SurvivorsCodexEntry>, kSurvivorsCodexTabCount> entries;
};

struct SurvivorsCodexState {
  bool open = false;
  SurvivorsCodexTab tab = SurvivorsCodexTab::kSkills;
  std::array<std::size_t, kSurvivorsCodexTabCount> selected = {};
  // La boîte du sprite de la fiche, relevée au dessin, où `app` cadre le
  // monstre choisi.
  float sprite_box_width = 0.0F;
  float sprite_box_height = 0.0F;
  // L'onglet que Tab vient de choisir : imposé à la barre jusqu'à ce qu'elle
  // le montre, la barre n'obéissant qu'à l'image d'après.
  std::optional<SurvivorsCodexTab> tab_wanted;
};

// CE QUE LA FENÊTRE REND : `closed` à l'image où sa croix la ferme. Elle se
// lit seulement ; aucune autre intention n'en sort.
struct SurvivorsCodexAsk {
  bool closed = false;
};

inline constexpr const char* kSurvivorsCodexId = "survivors_codex";

// LA FENÊTRE. Touches, fenêtre au premier plan : Tab passe à l'onglet suivant ;
// flèches haut et bas dans la liste. L'onglet Cartes dessine l'Album, qui lit
// ses propres touches.
SurvivorsCodexAsk DrawSurvivorsCodexWindow(SurvivorsCodexState& state,
                                           const SurvivorsCodexView& view,
                                           SurvivorsAlbumState& album_state,
                                           const SurvivorsAlbumView& album_view,
                                           const SkinArt& art);

// ── LES PHRASES ÉCRITES (survivors_codex_phrases.cpp) ───────────────────────
//
// Une phrase par entrée hors des sorts et des cartes, désignée par sa clé de
// Codex : ce que fait l'entrée DANS LE MODE, sans chiffre — les chiffres sont
// dans la fiche, tirés des règles, et une phrase qui en porterait périmerait.
// Les sorts et les cartes gardent la phrase de leur catalogue.

// La phrase d'une entrée, traduite ; "" pour une clé inconnue.
[[nodiscard]] const char* SurvivorsCodexPhrase(std::string_view codex_key);

// La table entière, en français, pour l'essai qui la confronte aux règles.
struct SurvivorsCodexPhraseRow {
  const char* key = "";
  const char* french = "";
};
[[nodiscard]] std::span<const SurvivorsCodexPhraseRow> SurvivorsCodexPhrases();

}  // namespace ro::ui
