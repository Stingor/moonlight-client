#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"

#include "survivors/survivors_codex_keys.h"

namespace moonclient {

// ── CE QUI A ÉTÉ VU : le relevé d'une partie, et la section `codex` du carnet ─
//
// Une liste ordonnée, sans doublon, par catégorie, dans l'ordre de la première
// rencontre. ⚠ UNE FORME INCONNUE SE GARDE, comme dans tout le carnet : une
// catégorie que ce client ne connaît pas, une catégorie connue d'une autre
// forme (gardée tant que ce client n'y écrit rien) et une entrée qui n'est pas
// une chaîne (gardée à sa place dans sa liste). Une clé inconnue du catalogue
// est une chaîne comme une autre : elle se garde et ne se montre pas.
//
// Aucun conteneur non ordonné : quelques dizaines de clés, lues en ligne.

// Une entrée d'une liste : une clé, ou le texte YAML d'une entrée qui n'est
// pas une chaîne.
struct SurvivorsCodexMark {
  std::string text;
  bool foreign = false;
  [[nodiscard]] bool operator==(const SurvivorsCodexMark&) const = default;
};

// Une catégorie que ce client ne connaît pas, recopiée telle quelle.
struct SurvivorsCodexKept {
  std::string key;
  std::string yaml;
  [[nodiscard]] bool operator==(const SurvivorsCodexKept&) const = default;
};

struct SurvivorsCodexSeen {
  std::array<std::vector<SurvivorsCodexMark>, kCodexCategoryCount> lists;
  // Le texte YAML d'une catégorie connue d'une autre forme ; vide sinon. Elle
  // se réécrit tant que sa liste est restée vide.
  std::array<std::string, kCodexCategoryCount> other_shapes;
  std::vector<SurvivorsCodexKept> foreign_categories;

  [[nodiscard]] bool Has(CodexCategory category, std::string_view key) const;
  // Ajoute en fin de liste ; vrai si la clé était neuve.
  bool Add(CodexCategory category, std::string_view key);
  [[nodiscard]] bool operator==(const SurvivorsCodexSeen&) const = default;
};

// Ajoute à `into` les clés de `from` qu'il n'a pas, dans l'ordre de `from`, et
// ne retire jamais rien ; vrai si `into` a changé. Ce que `from` garde
// d'étranger ne se recopie pas.
bool Merge(SurvivorsCodexSeen& into, const SurvivorsCodexSeen& from);

// Vrai quand `at` est dans le cadre autour du héros : le rectangle des règles
// (`spawn_frame`), aligné sur la caméra figée au nord, sur le périmètre duquel
// la horde naît. C'est ce que le joueur a sous les yeux, lu sans caméra.
[[nodiscard]] bool InHeroFrame(const ro::sim::SpawnFrame& frame, ro::sim::FieldPoint hero,
                               ro::sim::FieldPoint at) noexcept;

// CE QU'UNE IMAGE A MONTRÉ, ajouté au relevé. « Vu » est ce que le joueur a eu
// sous les yeux :
// - un sort offert à une montée de niveau, ou possédé (départ, offre prise,
//   évolution) ;
// - un monstre né dans la partie, mini-boss et boss compris, même né et tué
//   dans la même image ;
// - une plante, un objet au sol, une gemme ou une carte au sol dans le cadre
//   autour du héros ; un objet ramassé ;
// - un événement dont l'heure est passée ; les élites dès la première née ; la
//   nuit dès que l'heure de présentation atteint sa tombée.
// La partie n'en sait rien : le relevé lit son champ et ses nouvelles.
void NoteSightings(SurvivorsCodexSeen& seen, const ro::sim::SurvivorsField& field,
                   const ro::sim::SurvivorsNews& news, std::uint64_t presentation_ms);

}  // namespace moonclient
