#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/render/palette_ramps.h"
#include "ro/render/style_set.h"

namespace moonclient {

// UN CALQUE DÉSIGNÉ : son type et sa clé de rendu — la coupe et la couleur pour
// la tête, l'apparence affichée pour une coiffe ou une cape, zéro pour la peau.
struct LayerAt {
  ro::render::StyleLayer layer = ro::render::StyleLayer::kHead;
  std::uint32_t key = 0;

  [[nodiscard]] friend bool operator==(const LayerAt&, const LayerAt&) = default;
  [[nodiscard]] friend auto operator<=>(const LayerAt&, const LayerAt&) = default;
};

// LE PREMIER RANG DU RESTE, dans un geste : en deçà, le rang désigne une pièce ;
// à partir de lui, la classe de teinte `rank - kRestRankFirst` du reste.
inline constexpr std::size_t kRestRankFirst = 16;
static_assert(kRestRankFirst >= ro::render::kLayerAdjustCount);

// Un réglage de teinture, pour un geste qui en touche plusieurs.
struct LayerEdit {
  std::size_t rank = 0;
  ro::render::RampAdjust adjust;
};

// Les retouches qu'un calque porte au plus : celles que le fil admet.
inline constexpr std::size_t kLayerRetouchesMost = 32;

// La profondeur de l'historique des calques, en pas.
inline constexpr std::size_t kLayerHistoryDepth = 64;

// LES TEINTURES DE CALQUES QUE LE JOUEUR COMPOSE, ET LEUR SEUL ÉCRIVAIN.
//
// 🔴 CE QUI EST PORTÉ VIENT DU SERVEUR, CE QUI EST COMPOSÉ VIT ICI. Une retouche
// n'existe que pour le calque touché ; un calque que personne n'a touché suit ce
// que le serveur dit qu'on porte. Une retouche qui retombe sur ce qu'on porte —
// l'écho d'une validation, ou un geste revenu au point de départ — s'efface
// d'elle-même : il n'y a plus rien à valider.
//
// 🔴 UN PROFIL PAR CALQUE, celui que le sprite permet : les rampes quand il en a,
// les classes de teinte sinon. Une teinture d'un autre profil que celui qu'on
// règle ne se reprend pas rang par rang : ses rangs désignent d'autres pièces.
//
// ⚠ UN SEUL HISTORIQUE POUR TOUS LES CALQUES, à part de celui du corps : Ctrl+Z
// défait le dernier geste de l'onglet qu'on regarde, qu'il soit sur la tête ou
// sur un chapeau.
//
// ⚠ AUCUNE ENTRÉE-SORTIE : ni garde-robe, ni liaison. `StyleView` mesure et
// envoie ; ceci se prouve seul.
class StyleLayers {
 public:
  // CE QUE LE SERVEUR DIT QUE NOUS PORTONS, à chaque image : il ne fait rien
  // tant que `revision` ne change pas. Les têtes portées se retiennent, pour
  // « adapter à ma nouvelle coupe ».
  void Observe(std::span<const ro::render::LayerDye> worn, std::uint32_t revision);

  // La teinture portée de ce calque, ou rien.
  [[nodiscard]] const ro::render::LayerDye* Worn(LayerAt at) const;
  // CE QUE CE CALQUE MONTRE AU PROFIL DEMANDÉ : la retouche, sinon ce qu'il
  // porte, sinon rien de réglé. Une teinture d'un autre profil rend des
  // réglages neutres.
  [[nodiscard]] ro::render::LayerDye Current(LayerAt at, std::uint8_t profile) const;

  // ── LES GESTES ──────────────────────────────────────────────────────────
  //
  // 🔴 UN GESTE, UN PAS : tous les réglages d'un même trait — un glissement
  // dans le nuancier — ne font qu'un pas. Zéro : un pas à lui seul.

  void Set(LayerAt at, std::uint8_t profile, std::span<const LayerEdit> edits,
           std::uint32_t stroke = 0);
  // UNE RETOUCHE : la couleur exacte d'un index, qui remplace celle qu'il avait.
  // Refusée pour l'index zéro, le magenta pur, ou un calque déjà plein.
  bool Retouch(LayerAt at, std::uint8_t profile, const ro::render::PaletteRetouch& retouch,
               std::uint32_t stroke = 0);
  // Retire la retouche de cet index.
  void Unretouch(LayerAt at, std::uint8_t profile, std::uint8_t index);
  // Une teinture entière : un code collé, une coupe adaptée.
  void Put(const ro::render::LayerDye& dye);
  // Revenir à ce que ce calque porte.
  void BackToWorn(LayerAt at);
  // REPARTIR DE ZÉRO sur ce calque, en un pas : les pièces, le reste et les
  // retouches à neutre, ce que le sprite montre sans teinture.
  void Blank(LayerAt at, std::uint8_t profile);
  // Revenir à ce que TOUS les calques portent, en un pas.
  void BackToWornAll();

  bool Undo();
  bool Redo();
  [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
  [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }
  // COMBIEN DE PAS ONT ÉTÉ RANGÉS, tous calques confondus : voir
  // `StyleSession::steps_taken`.
  [[nodiscard]] std::uint64_t steps_taken() const noexcept { return steps_taken_; }

  // ── CE QU'ON EN LIT ─────────────────────────────────────────────────────

  // Ce calque a-t-il une retouche qui diffère de ce qu'il porte ?
  [[nodiscard]] bool HasEdits(LayerAt at) const;
  [[nodiscard]] bool HasAnyEdits() const { return !edits_.empty(); }
  // LES RETOUCHES À VALIDER, triées par (calque, clé).
  [[nodiscard]] std::vector<ro::render::LayerDye> Pending() const;
  // CE QUE LE PANTIN PEINT : ce qu'on porte, chaque retouche à la place de
  // la teinture de son calque. Trié par (calque, clé), sans teinture neutre.
  [[nodiscard]] std::vector<ro::render::LayerDye> Composed() const;
  // Ce que le serveur dit que nous portons, tel quel.
  [[nodiscard]] std::span<const ro::render::LayerDye> worn() const noexcept { return worn_; }
  // Avance à chaque changement de ce qui est composé. Jamais zéro.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

  // LA DERNIÈRE TÊTE TEINTE AUTRE QUE `current`, portée ou composée pendant la
  // partie, ou rien : celle qu'une coupe neuve peut reprendre.
  [[nodiscard]] std::optional<ro::render::LayerDye> PreviousHead(std::uint32_t current) const;

 private:
  using Edits = std::map<LayerAt, ro::render::LayerDye>;

  // Range `before` comme l'état d'avant un pas, sauf si rien n'a changé ou si
  // le geste prolonge le trait en cours.
  void Step(const Edits& before, std::uint32_t stroke);
  // Range `dye` comme la retouche de `at`, en un pas.
  void Replace(LayerAt at, ro::render::LayerDye dye, std::uint32_t stroke);
  void Changed();
  // Retire les retouches qui ne diffèrent plus de ce qu'on porte.
  void Settle();
  void RememberHead(const ro::render::LayerDye& dye);

  std::vector<ro::render::LayerDye> worn_;
  std::uint32_t worn_revision_ = 0;
  Edits edits_;
  std::vector<Edits> undo_;
  std::vector<Edits> redo_;
  std::uint64_t steps_taken_ = 0;
  std::uint32_t stroke_ = 0;
  std::uint32_t revision_ = 1;
  // Les têtes teintes vues, la plus récente au bout.
  std::vector<ro::render::LayerDye> heads_;
};

// La teinture neutre ? Aucun réglage n'y change rien.
[[nodiscard]] bool NeutralDye(const ro::render::LayerDye& dye);

}  // namespace moonclient
