// LES RÉGLAGES D'AFFICHAGE, côté GameOptions.
//
// ⚠ SÉPARÉ DE `display_options.cpp`, QUI NE PORTE QUE DU TEXTE. Celui-là ne
// connaît que des chaînes et des nombres, donc un essai l'atteint sans monter
// tout le jeu ; celui-ci parle à la fenêtre du système et à la classe qui tient
// tous les réglages.
//
// Et ils sont hors de `game_options.cpp` pour la même raison que la composition
// des lignes : le plafond de huit cents lignes. Les lignes de la page de
// l'affichage s'y composent aussi, à côté des listes qu'elles montrent.

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"

#include "options/display_options.h"
#include "options/game_options.h"
#include "options/option_row_kinds.h"
#include "options/screen_choice.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// COMBIEN DE TEMPS LE FILET LAISSE AVANT DE DÉFAIRE.
//
// ⚠ ASSEZ POUR LIRE UNE QUESTION ET VISER UN BOUTON, pas plus : c'est le temps
// qu'un écran noir reste noir, et chaque seconde y est longue. Quinze est ce que
// les jeux emploient, et ce que les joueurs reconnaissent.
constexpr std::uint32_t kDisplayRevertMs = 15000;
constexpr std::uint32_t kMillisecondsPerSecond = 1000;

// ⚠ ECRITS A LA MAIN et non composes : un libelle compose a chaque image serait
// une chaine qui ne vit pas assez longtemps pour la vue qui la designe. Le
// premier ne sert pas -- « Aucune » se traduit.
constexpr std::array<const char*, 6> kFrameLimitLabels = {"", "30", "60", "120", "144", "240"};

}  // namespace

bool GameOptions::NoteWindowMoved(const ro::render::GlContext& display) {
  std::string written = PlaceText(display.position());
  if (written == window_place_) {
    return false;
  }
  window_place_ = std::move(written);
  // 🔴 L'ÉCRAN SUIT LA PLACE. Sans lui, la fenêtre glissée sur un autre écran
  // renaîtrait sur l'ancien, et sa place gardée — qui n'y tombe pas — n'y
  // serait pas rendue.
  const int rank = display.screen_rank();
  const std::vector<ro::render::ScreenInfo> screens = ro::render::GlContext::ConnectedScreens();
  if (rank >= 0 && rank < static_cast<int>(screens.size())) {
    screen_ = ScreenChoice{.name = screens[static_cast<std::size_t>(rank)].name, .rank = rank};
  }
  return true;
}

ro::render::Resolution GameOptions::saved_resolution() const { return ResolutionOf(resolution_); }

void GameOptions::PlaceWindow(const OptionTargets& to) const {
  if (to.display == nullptr) {
    return;
  }
  // ⚠ RIEN À RENDRE À LA PREMIÈRE PARTIE : le système a déjà centré la fenêtre,
  // et c'est mieux que n'importe quel coin qu'on choisirait.
  //
  // ⚠ NI QUAND LA PLACE TOMBE HORS DE L'ÉCRAN OÙ SE TIENT LA FENÊTRE : c'est
  // qu'on est retombé sur le principal, l'écran choisi étant débranché, et la
  // fenêtre y reste centrée. Un fichier qui n'a encore choisi aucun écran garde
  // sa place, où qu'elle tombe.
  if (const std::optional<ro::render::WindowPlace> where = PlaceOf(window_place_);
      where.has_value() && (screen_.name.empty() || ro::render::GlContext::ScreenRankAt(*where) ==
                                                        to.display->screen_rank())) {
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
  screen_ = revert_.screen;
  // L'écran d'abord : il repose lui-même le mode quand il déplace la fenêtre.
  Apply(GameOption::kScreen, to);
  Apply(GameOption::kDisplayMode, to);
  Apply(GameOption::kInterfaceScale, to);
  // ⚠ LA DISPOSITION SUIT D'ELLE-MÊME : les places voulues ne dépendent ni de
  // l'écran ni de l'échelle — voir `ui::WindowPlacer`.
  // 🔴 ET LE FICHIER, COMME POUR TOUT CHANGEMENT : le geste qu'on défait s'y est
  // écrit, et sans cette ligne c'est lui qui revient au lancement suivant — le
  // joueur qui a demandé l'annulation retrouve l'échelle qu'il a refusée.
  Record(to);
}

void GameOptions::ArmDisplayRevert(std::uint32_t now_ms, const DisplayRevert& before,
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
  // 🔴 L'ÉCRAN AUSSI, TOUJOURS : un moniteur branché mais éteint se liste comme
  // les autres, et la fenêtre y emporte le panneau d'où l'on défaisait.
  //
  // ⚠ LE MOTEUR BORNE L'ÉCHELLE : on compare ce qu'il a RETENU, pas ce qu'on lui
  // a demandé. Sans cela, un curseur poussé au-delà du plafond armerait le
  // filet sur un changement qui n'a pas eu lieu.
  // ⚠ L'ÉCRAN SE COMPARE PAR CE QU'IL DÉSIGNE : choisir par son nom l'écran
  // qu'on occupait sans l'avoir jamais nommé ne déplace rien.
  const std::vector<ro::render::ScreenInfo> screens = ro::render::GlContext::ConnectedScreens();
  const ro::render::Resolution wanted = ResolutionOf(resolution_);
  bool risky = interface_scale_ != before.interface_scale ||
               PickScreen(screen_, screens) != PickScreen(before.screen, screens) ||
               DisplayModeOf(display_mode_) == ro::render::DisplayMode::kFullscreen;
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
  revert_ = before;
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

void GameOptions::ComposeScreenRows() {
  // ── L'AFFICHAGE ───────────────────────────────────────────────────────────
  //
  // ⚠ SA PROPRE SECTION, la première du panneau : c'est ce qu'on vient régler
  // avant tout le reste, et souvent une seule fois.
  display_mode_names_.clear();
  display_mode_names_.push_back(Tr("Fenêtré"));
  display_mode_names_.push_back(Tr("Sans bordure"));
  display_mode_names_.push_back(Tr("Plein écran"));
  frame_limit_names_.clear();
  frame_limit_names_.push_back(Tr("Aucune"));
  for (std::size_t rank = 1; rank < kFrameLimits.size(); ++rank) {
    frame_limit_names_.push_back(kFrameLimitLabels[rank]);
  }
  // ⚠ LES ÉCRANS SE RELISENT À CHAQUE COMPOSITION : un moniteur branché panneau
  // ouvert doit paraître sans relancer le jeu.
  OfferScreens(ro::render::GlContext::ConnectedScreens(), screens_);
  screen_rows_.clear();
  screen_rows_.push_back(Choice(GameOption::kScreen, Tr("Écran"),
                                Tr("Le moniteur où le jeu s'affiche. La fenêtre s'y porte "
                                   "aussitôt, centrée, et le plein écran s'y fait. Au "
                                   "lancement, un écran débranché laisse place au principal."),
                                screens_.choices,
                                std::max(PickScreen(screen_, screens_.screens), 0)));
  screen_rows_.push_back(
      Choice(GameOption::kDisplayMode, Tr("Mode d'affichage"),
             Tr("Fenêtré laisse le bureau accessible. Sans bordure occupe tout "
                "l'écran sans changer son mode vidéo : on en sort et on y revient "
                "sans attente. Plein écran change le mode de l'écran — il seul "
                "peut imposer une résolution plus petite que le bureau, au prix "
                "d'une resynchronisation à chaque bascule."),
             display_mode_names_, display_mode_));
  screen_rows_.push_back(Choice(GameOption::kResolution, Tr("Résolution"),
                                Tr("La taille de la fenêtre, ou le mode vidéo en plein écran. Sans "
                                   "effet en sans bordure, qui suit toujours le bureau."),
                                resolution_choices_, ResolutionRank()));
  screen_rows_.push_back(
      Toggle(GameOption::kLockWindowSize, Tr("Verrouiller la taille de la fenêtre"),
             Tr("Le bord de la fenêtre ne répond plus à la souris. Pour qui a "
                "trouvé sa taille et ne veut plus la perdre d'un glissement. Sans "
                "objet hors du mode fenêtré."),
             nullptr, lock_window_size_));
  screen_rows_.push_back(
      Toggle(GameOption::kVerticalSync, Tr("Synchronisation verticale"),
             Tr("Attend le balayage de l'écran avant de montrer l'image : pas de "
                "déchirure, et la carte cesse de dessiner des images que personne "
                "ne voit. Le pilote peut l'imposer ou l'interdire."),
             nullptr, vertical_sync_));
  screen_rows_.push_back(
      Choice(GameOption::kFrameLimit, Tr("Limite d'images"),
             Tr("Un plafond, quand la synchronisation verticale est éteinte. "
                "Au-delà de ce que l'écran affiche, une image de plus ne se voit "
                "pas — elle chauffe."),
             frame_limit_names_, FrameLimitRank()));
  screen_rows_.push_back(
      Toggle(GameOption::kFrameMeter, Tr("Compteur d'images"),
             Tr("Un petit panneau posé sur le jeu : la cadence, la latence et la "
                "courbe des temps d'image."),
             nullptr, frame_meter_));
  screen_rows_.push_back(
      Toggle(GameOption::kFrameMeterLocked, Tr("Compteur figé et traversant"),
             Tr("Le compteur ne se déplace plus et laisse passer les clics au jeu "
                "en dessous. Déverrouillez-le pour le glisser ailleurs."),
             nullptr, frame_meter_locked_));
  screen_rows_.push_back(Toggle(GameOption::kFrameMeterGraph, Tr("Courbe des temps d'image"),
                                Tr("Les 120 dernières images, de 0 à 33 ms. Un pic visible est un "
                                   "à-coup — c'est précisément ce qu'un compteur moyenné cache."),
                                nullptr, frame_meter_graph_));
  screen_rows_.push_back(
      Toggle(GameOption::kFrameMeterPing, Tr("Latence serveur"),
             Tr("Mesurée sur le battement d'horloge que le client entretient déjà "
                "avec le serveur, toutes les quelques secondes : rien n'est "
                "envoyé en plus. « -- » tant qu'aucun échange n'a eu lieu."),
             nullptr, frame_meter_ping_));
}

}  // namespace moonclient
