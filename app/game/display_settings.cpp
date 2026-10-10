// LES RÉGLAGES D'AFFICHAGE, côté GameOptions.
//
// ⚠ SÉPARÉ DE `display_options.cpp`, QUI NE PORTE QUE DU TEXTE. Celui-là ne
// connaît que des chaînes et des nombres, donc un essai l'atteint sans monter
// tout le jeu ; celui-ci parle à la fenêtre du système et à la classe qui tient
// tous les réglages.
//
// Et ils sont hors de `game_options.cpp` pour la même raison que la composition
// des lignes : le plafond de huit cents lignes.

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/log.h"

#include "options/display_options.h"
#include "options/game_options.h"

namespace moonclient {
namespace {

// COMBIEN DE TEMPS LE FILET LAISSE AVANT DE DÉFAIRE.
//
// ⚠ ASSEZ POUR LIRE UNE QUESTION ET VISER UN BOUTON, pas plus : c'est le temps
// qu'un écran noir reste noir, et chaque seconde y est longue. Quinze est ce que
// les jeux emploient, et ce que les joueurs reconnaissent.
constexpr std::uint32_t kDisplayRevertMs = 15000;
constexpr std::uint32_t kMillisecondsPerSecond = 1000;

}  // namespace

bool GameOptions::NoteWindowMoved(ro::render::WindowPlace where) {
  std::string written = PlaceText(where);
  if (written == window_place_) {
    return false;
  }
  window_place_ = std::move(written);
  return true;
}

ro::render::Resolution GameOptions::saved_resolution() const { return ResolutionOf(resolution_); }

void GameOptions::PlaceWindow(const OptionTargets& to) const {
  if (to.display == nullptr) {
    return;
  }
  // ⚠ RIEN À RENDRE À LA PREMIÈRE PARTIE : le système a déjà centré la fenêtre,
  // et c'est mieux que n'importe quel coin qu'on choisirait.
  if (const std::optional<ro::render::WindowPlace> where = PlaceOf(window_place_)) {
    to.display->SetPosition(*where);
  }
  // 🔴 ET DANS TOUS LES CAS ON LA RABAT. L'écran où elle était a pu disparaître,
  // et la taille qu'on vient de lui rendre peut la faire déborder d'un bureau
  // plus petit que la dernière fois.
  to.display->KeepOnScreen();
}

int GameOptions::revert_seconds(std::uint32_t now_ms) const noexcept {
  if (revert_.deadline_ms == 0) {
    return -1;
  }
  // ⚠ SOUSTRACTION NON SIGNÉE, donc un dépassement de l'horloge donnerait un
  // nombre énorme : on compare AVANT de soustraire.
  if (now_ms >= revert_.deadline_ms) {
    return 0;
  }
  const std::uint32_t left_ms = revert_.deadline_ms - now_ms;
  // Arrondi au SUPÉRIEUR : « 1 s » doit rester affiché tant qu'il reste du
  // temps, sinon le compte tombe à zéro une seconde avant l'échéance.
  return static_cast<int>((left_ms + kMillisecondsPerSecond - 1) / kMillisecondsPerSecond);
}

void GameOptions::KeepDisplay() noexcept { revert_.deadline_ms = 0; }

void GameOptions::RevertDisplay(const OptionTargets& to) {
  // ⚠ IDEMPOTENT. Deux chemins y mènent — la boîte qui rend « non » et
  // l'échéance que la boucle surveille —, et le second existe précisément parce
  // que le premier peut ne jamais être dessiné.
  if (revert_.deadline_ms == 0) {
    return;
  }
  revert_.deadline_ms = 0;
  resolution_ = revert_.resolution;
  display_mode_ = revert_.display_mode;
  interface_scale_ = revert_.interface_scale;
  Apply(GameOption::kDisplayMode, to);
  Apply(GameOption::kInterfaceScale, to);
  // ⚠ LA DISPOSITION SUIT D'ELLE-MÊME : les places voulues ne dépendent ni de
  // l'écran ni de l'échelle — voir `ui::WindowPlacer`.
  // 🔴 ET LE FICHIER, COMME POUR TOUT CHANGEMENT : le geste qu'on défait s'y est
  // écrit, et sans cette ligne c'est lui qui revient au lancement suivant — le
  // joueur qui a demandé l'annulation retrouve l'échelle qu'il a refusée.
  Record(to);
}

void GameOptions::ArmDisplayRevert(std::uint32_t now_ms, const std::string& previous_resolution,
                                   int previous_mode, float previous_scale, bool scale_changed,
                                   const OptionTargets& to) {
  // 🔴 LE FILET NE S'ARME PAS TOUJOURS, et c'est ce qui le rend supportable. En
  // fenêtré, le joueur garde sa barre de titre, son bureau et ses yeux : il peut
  // défaire lui-même, et une question de quinze secondes à chaque essai de
  // taille serait une punition.
  //
  // Il s'arme quand le changement peut CACHER LA QUESTION :
  //   • le plein écran EXCLUSIF, qu'un moniteur peut refuser tout net ;
  //   • une fenêtre plus grande que le bureau, dont les bords sortent de l'écran.
  // 🔴 L'ÉCHELLE EST TOUJOURS RISQUÉE. Elle ne peut pas éteindre l'écran, mais
  // elle peut rendre illisible le panneau même où l'on vient de la poser — et
  // c'est le seul endroit d'où la défaire. Un filet qui ne couvrirait que le
  // mode vidéo laisserait passer la panne la plus facile à provoquer.
  const ro::render::Resolution wanted = ResolutionOf(resolution_);
  bool risky =
      scale_changed || DisplayModeOf(display_mode_) == ro::render::DisplayMode::kFullscreen;
  if (!risky && to.display != nullptr && wanted.width > 0) {
    const ro::render::Resolution desktop = to.display->desktop_size();
    risky = desktop.width > 0 && (wanted.width > desktop.width || wanted.height > desktop.height);
  }
  if (!risky) {
    return;
  }
  // ⚠ CE QU'ON REND EST L'ÉTAT D'AVANT CE GESTE, et non celui d'avant le
  // précédent : un joueur qui enchaîne deux essais doit revenir au dernier qu'il
  // a VU, pas au premier.
  revert_.resolution = previous_resolution;
  revert_.display_mode = previous_mode;
  revert_.interface_scale = previous_scale;
  revert_.deadline_ms = now_ms + kDisplayRevertMs;
}

void GameOptions::ReadResolutions(const OptionTargets& to) {
  resolutions_.clear();
  resolution_labels_.clear();
  resolution_choices_.clear();
  if (to.display == nullptr) {
    return;
  }
  resolutions_ = to.display->AvailableResolutions();
  // ⚠ LA TAILLE COURANTE EN FAIT PARTIE, même si l'écran ne la déclare pas.
  // Une fenêtre se redimensionne à la souris : sans cela, la liste n'aurait
  // aucune ligne en face de ce que le joueur regarde.
  const ro::render::Resolution now = to.display->size();
  if (now.width > 0 && std::ranges::find(resolutions_, now) == resolutions_.end()) {
    resolutions_.push_back(now);
    std::ranges::sort(
        resolutions_, [](const ro::render::Resolution& left, const ro::render::Resolution& right) {
          return left.width == right.width ? left.height < right.height : left.width < right.width;
        });
  }
  resolution_labels_.reserve(resolutions_.size());
  for (const ro::render::Resolution& size : resolutions_) {
    // ⚠ LE « × » TYPOGRAPHIQUE ICI, et l'ASCII dans le fichier : celui-ci est un
    // libellé, l'autre une clé. Voir `ResolutionText`.
    resolution_labels_.push_back(std::to_string(size.width) + " × " + std::to_string(size.height));
  }
  // ⚠ LES POINTEURS APRÈS LA BOUCLE, jamais dedans : le vecteur se réalloue en
  // grandissant, et une vue posée plus tôt pendrait dans le vide.
  resolution_choices_.reserve(resolution_labels_.size());
  for (const std::string& label : resolution_labels_) {
    resolution_choices_.push_back(label.c_str());
  }
}

void GameOptions::ReadInterfaceScales() {
  interface_percents_.assign(kInterfaceScalePercents.begin(), kInterfaceScalePercents.end());
  interface_scale_labels_.clear();
  interface_scale_choices_.clear();
  // ⚠ LE FACTEUR COURANT EN FAIT PARTIE, même s'il ne tombe sur aucun palier.
  // `/echelle` accepte n'importe quelle valeur, et un fichier venu d'ailleurs
  // aussi : sans cette entrée, la liste n'aurait aucune ligne en face de ce que
  // le joueur regarde, et s'ouvrirait donc sur 50 %.
  const int now = InterfacePercentOf(interface_scale_);
  if (std::ranges::find(interface_percents_, now) == interface_percents_.end()) {
    interface_percents_.push_back(now);
    std::ranges::sort(interface_percents_);
  }
  interface_scale_labels_.reserve(interface_percents_.size());
  for (const int percent : interface_percents_) {
    interface_scale_labels_.push_back(InterfaceScaleLabel(percent));
  }
  // ⚠ LES POINTEURS APRÈS LA BOUCLE, jamais dedans : le vecteur se réalloue en
  // grandissant, et une vue posée plus tôt pendrait dans le vide.
  interface_scale_choices_.reserve(interface_scale_labels_.size());
  for (const std::string& label : interface_scale_labels_) {
    interface_scale_choices_.push_back(label.c_str());
  }
}

int GameOptions::InterfaceScaleRank() const {
  const auto found = std::ranges::find(interface_percents_, InterfacePercentOf(interface_scale_));
  return found == interface_percents_.end() ? 0
                                            : static_cast<int>(found - interface_percents_.begin());
}

float GameOptions::InterfaceScaleAt(int rank) const {
  // ⚠ UN RANG HORS LISTE REND CE QU'ON A DÉJÀ, et non un palier par défaut :
  // c'est la même prudence que pour la résolution — mieux vaut ne rien changer
  // que changer au hasard.
  if (rank < 0 || rank >= static_cast<int>(interface_percents_.size())) {
    return interface_scale_;
  }
  return InterfaceScaleOf(interface_percents_[static_cast<std::size_t>(rank)]);
}

int GameOptions::ResolutionRank() const {
  const ro::render::Resolution wanted = ResolutionOf(resolution_);
  const auto found = std::ranges::find(resolutions_, wanted);
  return found == resolutions_.end() ? 0 : static_cast<int>(found - resolutions_.begin());
}

std::string GameOptions::ResolutionTextAt(int rank) const {
  if (rank < 0 || rank >= static_cast<int>(resolutions_.size())) {
    return resolution_;
  }
  return ResolutionText(resolutions_[static_cast<std::size_t>(rank)]);
}

int GameOptions::FrameLimitRank() const {
  const auto found = std::ranges::find(kFrameLimits, frame_limit_);
  return found == kFrameLimits.end() ? 0 : static_cast<int>(found - kFrameLimits.begin());
}

int GameOptions::FrameLimitAt(int rank) {
  if (rank < 0 || rank >= static_cast<int>(kFrameLimits.size())) {
    return 0;
  }
  return kFrameLimits[static_cast<std::size_t>(rank)];
}

}  // namespace moonclient
