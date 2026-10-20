#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ro::core {

// L'AVANCEMENT D'UN CHARGEMENT BLOQUANT : des étapes pondérées, et dans chacune
// un compte de ce qui est fait sur un total.
//
// ⚠ DES DONNÉES PURES, ET UN SEUL SENS. Celui qui charge dit où il en est ;
// celui qui montre lit la fraction et décide seul s'il peint. Un chargeur
// appelé sans écran — un outil, un essai — reçoit un pointeur nul et se tait :
// voir `LoadStepScope`.
//
// ⚠ UNE ÉTAPE À LA FOIS : en ouvrir une ferme la précédente. Deux étapes ne
// s'emboîtent pas, et une étape ne figure qu'une fois dans un plan.

// CE QU'UNE ÉTAPE CHARGE. Le libellé que le joueur lit se traduit plus haut,
// dans la couche qui le montre.
enum class LoadStep : std::uint8_t {
  // L'habillage, le curseur, la police de la coquille.
  kInterface,
  // Le mélangeur et la musique.
  kSound,
  // La garde-robe des personnages.
  kWardrobe,
  // Les tables du client : objets, métiers, quêtes, titres…
  kTables,
  // Les fichiers de la carte, lus et décodés.
  kMap,
  // Les ombres cuites du terrain.
  kLights,
  // Les textures du terrain.
  kTerrain,
  // Le maillage du sol, arrondi ou non.
  kGround,
  // Les nappes d'eau.
  kWater,
  // Les modèles du décor.
  kModels,
  // Les effets posés sur la carte et leurs textures.
  kMapEffects,
  // La grille de marche et la lumière que le sol renvoie aux acteurs.
  kWalkGrid,
  // Le terrain et l'eau envoyés à la carte graphique.
  kUpload,
  // Les modèles envoyés à la carte graphique.
  kModelUpload,
  // Les catalogues d'effets animés de la partie.
  kEffects,
};
// ⚠ LA DERNIÈRE ÉTAPE FERME LE COMPTE : une étape ajoutée se range avant elle.
inline constexpr std::size_t kLoadStepCount = static_cast<std::size_t>(LoadStep::kEffects) + 1;

// UNE ÉTAPE DU PLAN, et ce qu'elle coûte d'ordinaire. Seul le RAPPORT des
// durées compte : la fraction est la part du temps prévu déjà écoulé.
struct LoadStepWeight {
  LoadStep step = LoadStep::kTables;
  float expected_ms = 0.0F;
};

class LoadProgress;

// CELUI QU'UN CHANGEMENT INTÉRESSE : l'écran qui montre l'avancement. Il est
// prévenu à chaque ouverture, avance et fermeture d'étape, et choisit seul s'il
// peint.
class LoadObserver {
 public:
  LoadObserver() = default;
  LoadObserver(const LoadObserver&) = delete;
  LoadObserver& operator=(const LoadObserver&) = delete;
  LoadObserver(LoadObserver&&) = delete;
  LoadObserver& operator=(LoadObserver&&) = delete;
  virtual ~LoadObserver() = default;

  virtual void Changed(const LoadProgress& progress) = 0;
};

class LoadProgress {
 public:
  // Une étape que le plan ne nomme pas s'affiche, mais ne pèse rien.
  explicit LoadProgress(std::span<const LoadStepWeight> plan, LoadObserver* observer = nullptr);

  // Ouvre `step`, à `total` éléments ; zéro quand rien ne s'y compte, et
  // l'étape ne pèse alors qu'à sa fermeture.
  void Begin(LoadStep step, std::size_t total);
  // `count` éléments de plus sont faits, sans dépasser le total.
  void Advance(std::size_t count = 1);
  // L'étape ouverte est finie, qu'elle ait compté ou non tous ses éléments.
  void End();

  // LA PART FAITE DU PLAN, de 0 à 1 : la somme des durées prévues, chacune
  // multipliée par la part faite de son étape, sur la somme des durées.
  [[nodiscard]] float fraction() const noexcept;
  // L'étape ouverte, ou rien entre deux étapes.
  [[nodiscard]] std::optional<LoadStep> step() const noexcept { return step_; }
  // Le compte de l'étape ouverte.
  [[nodiscard]] std::size_t done() const noexcept { return done_; }
  [[nodiscard]] std::size_t total() const noexcept { return total_; }

 private:
  void Notify() const;

  std::array<float, kLoadStepCount> expected_ms_{};
  // La part faite de chaque étape, de 0 à 1.
  std::array<float, kLoadStepCount> done_share_{};
  float plan_ms_ = 0.0F;
  std::optional<LoadStep> step_;
  std::size_t done_ = 0;
  std::size_t total_ = 0;
  LoadObserver* observer_ = nullptr;
};

// UNE ÉTAPE, ouverte à la construction et fermée à la destruction — même sur
// un retour d'erreur au milieu. Muette quand `progress` est nul.
class LoadStepScope {
 public:
  LoadStepScope(LoadProgress* progress, LoadStep step, std::size_t total = 0);
  LoadStepScope(const LoadStepScope&) = delete;
  LoadStepScope& operator=(const LoadStepScope&) = delete;
  LoadStepScope(LoadStepScope&&) = delete;
  LoadStepScope& operator=(LoadStepScope&&) = delete;
  ~LoadStepScope();

  void Advance(std::size_t count = 1) const;

 private:
  LoadProgress* progress_ = nullptr;
};

// UN ÉLÉMENT COMPTÉ À SA FIN, quelle que soit la sortie de son tour de boucle —
// `continue` compris : le compte dit ce qui est FAIT, jamais ce qui commence.
class LoadItemDone {
 public:
  explicit LoadItemDone(const LoadStepScope& step) noexcept : step_(&step) {}
  LoadItemDone(const LoadItemDone&) = delete;
  LoadItemDone& operator=(const LoadItemDone&) = delete;
  LoadItemDone(LoadItemDone&&) = delete;
  LoadItemDone& operator=(LoadItemDone&&) = delete;
  ~LoadItemDone() { step_->Advance(); }

 private:
  const LoadStepScope* step_ = nullptr;
};

// LES MÊMES, POUR UNE SUITE D'ÉTAPES DANS UNE SEULE FONCTION : chacune ferme la
// précédente, et la dernière se ferme à la main. Muettes quand `progress` est
// nul.
void BeginLoadStep(LoadProgress* progress, LoadStep step, std::size_t total = 0);
void EndLoadStep(LoadProgress* progress);

// ── QUAND L'ÉCRAN SE MONTRE ──────────────────────────────────────────────────

// 🔴 UN CHARGEMENT COURT NE MONTRE RIEN : l'écran n'apparaît qu'au-delà de ce
// délai. En deçà, un écran plein cadre montré une image se lirait comme un
// clignotement noir, là où le figement passe inaperçu.
inline constexpr std::uint32_t kLoadingScreenDelayMs = 150;
// Le fondu d'apparition, depuis le noir.
inline constexpr std::uint32_t kLoadingScreenFadeMs = 250;
// ⚠ AU PLUS UNE IMAGE PAR INTERVALLE : chaque présentation prend sur le
// chargement qu'elle montre. Trente images par seconde suffisent à une barre.
inline constexpr std::uint32_t kLoadingPresentIntervalMs = 33;

// LA CADENCE DE L'ÉCRAN, sur une horloge que l'appelant passe : aucune n'est
// lue ici.
class LoadPacing {
 public:
  // `already_shown` : l'écran est déjà à l'écran, et le reste sans délai ni
  // fondu — un montage que l'écran de chargement précède déjà.
  LoadPacing(std::uint32_t started_ms, bool already_shown) noexcept
      : started_ms_(started_ms), already_shown_(already_shown) {}

  // Vrai quand une image est due à cet instant.
  [[nodiscard]] bool Due(std::uint32_t now_ms) const noexcept;
  void NotePresented(std::uint32_t now_ms) noexcept { presented_ms_ = now_ms; }
  // L'opacité de l'écran à cet instant, de 0 à 1.
  [[nodiscard]] float Opacity(std::uint32_t now_ms) const noexcept;
  // Vrai dès qu'une image a été présentée.
  [[nodiscard]] bool shown() const noexcept { return presented_ms_.has_value(); }

 private:
  std::uint32_t started_ms_ = 0;
  bool already_shown_ = false;
  std::optional<std::uint32_t> presented_ms_;
};

}  // namespace ro::core
