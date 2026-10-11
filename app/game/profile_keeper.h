#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/options_window.h"
#include "ro/ui/profiles_page.h"

#include "game/profile_book.h"

namespace moonclient {

// CE QU'UNE FAMILLE DE PROFILS SAIT FAIRE de l'état qu'elle range : le relever,
// le poser, dire ses défauts, et lire ou écrire son fichier.
template <typename Profile>
class ProfilePort {
 public:
  ProfilePort() = default;
  ProfilePort(const ProfilePort&) = delete;
  ProfilePort& operator=(const ProfilePort&) = delete;
  ProfilePort(ProfilePort&&) = delete;
  ProfilePort& operator=(ProfilePort&&) = delete;
  virtual ~ProfilePort() = default;

  // Grandit à chaque changement de l'état rangé.
  [[nodiscard]] virtual std::uint64_t Revision() = 0;
  [[nodiscard]] virtual Profile Capture() = 0;
  virtual void Pose(const Profile& profile) = 0;
  // L'état d'origine du client : ce que « Réinitialiser » pose.
  [[nodiscard]] virtual Profile Defaults() = 0;
  // Faux pour un fichier qu'on ne doit pas écraser.
  [[nodiscard]] virtual bool Load(ProfileBook<Profile>& into) = 0;
  virtual void Save(const ProfileBook<Profile>& from) = 0;
};

// LE TITRE ET L'AIDE D'UNE FAMILLE, traduits.
struct ProfileFamilyText {
  const char* title = "";
  const char* help = "";
  // L'identité ImGui de la page : deux familles sur une même page d'options ne
  // se marchent pas dessus.
  const char* id = "";
};

// ⚠ UNE FONCTION ET NON UN TEXTE : appelée à chaque image, elle suit la langue,
// et la traduction s'y fait sur des littéraux que le relevé des catalogues voit.
using ProfileFamilyTextFn = ProfileFamilyText (*)();

// ── UNE FAMILLE DE PROFILS, VIVANTE ────────────────────────────────────────
//
// ⚠ LE PROFIL POSÉ SUIT LE JOUEUR, comme chez les extensions de WoW : ce qui
// change l'état change le profil posé, qui s'enregistre quand l'état s'est tu.
//
// ⚠ CHAQUE ÉCRITURE RELIT LE FICHIER D'ABORD : un autre client sur la même
// installation a pu y écrire ses propres profils entre-temps.
//
// Chaque image de jeu : le dessin — par le corps que `Body` rend au panneau —,
// puis `Tick`.
template <typename Profile>
class ProfileKeeper {
 public:
  ProfileKeeper(ProfilePort<Profile>& port, ProfileFamilyTextFn text) : port_(port), text_(text) {}

  // Au démarrage : lit le fichier, et part du profil `active` — « Default »
  // s'il n'existe pas. `pose` : pose aussi ce profil.
  void Start(bool pose, std::string_view active = kDefaultProfileName) {
    writable_ = Reload();
    active_ = std::string(book_.Find(active) != nullptr ? active : kDefaultProfileName);
    if (const Named<Profile>* named = book_.Find(active_); pose && named != nullptr) {
      port_.Pose(named->profile);
    }
    Settle();
    ComposeView();
  }

  // À l'entrée en jeu : pose le profil de ce personnage.
  void EnterCharacter(std::string_view character) {
    Flush();
    // Un geste resté en attente visait la partie d'avant.
    asked_ = {};
    character_ = std::string(character);
    if (writable_) {
      writable_ = Reload();
    }
    const bool default_just_kept = KeepDefault();
    const std::string name = book_.ProfileOf(character_);
    // ⚠ « Default » QU'ON VIENT D'ÉCRIRE EST L'ÉTAT PRÉSENT : le reposer ne
    // changerait rien, et effacerait les places qu'ImGui a relues de son
    // fichier pour des fenêtres pas encore ouvertes.
    if (!(default_just_kept && name == active_)) {
      if (const Named<Profile>* named = book_.Find(name); named != nullptr) {
        port_.Pose(named->profile);
      }
    }
    active_ = name;
    Settle();
    ComposeView();
  }

  // Applique le geste de la page, puis enregistre le profil posé quand l'état
  // n'a plus bougé depuis un moment.
  void Tick(std::uint32_t now_ms) {
    if (asked_.gesture != ro::ui::ProfileGesture::kNone) {
      const ro::ui::ProfilesPageAction asked = std::exchange(asked_, {});
      Apply(asked);
    }
    if (const std::uint64_t revision = port_.Revision(); revision != seen_revision_) {
      seen_revision_ = revision;
      last_change_ms_ = now_ms;
      if (!dirty_) {
        dirty_ = true;
        first_change_ms_ = now_ms;
      }
    }
    if (dirty_ &&
        (now_ms - last_change_ms_ >= kQuietMs || now_ms - first_change_ms_ >= kLongestWaitMs)) {
      Flush();
    }
    // ⚠ À CHAQUE IMAGE : la langue a pu changer, et le libellé de « Default »
    // avec.
    ComposeView();
  }

  // Enregistre tout de suite ce qui attend : avant de quitter.
  void Flush() {
    if (!dirty_) {
      return;
    }
    dirty_ = false;
    if (!writable_ || !Reload()) {
      return;
    }
    Profile captured = port_.Capture();
    // ⚠ RIEN À ÉCRIRE QUAND RIEN N'A CHANGÉ : un réglage de la machine fait
    // grandir la révision sans toucher au profil.
    if (const Named<Profile>* named = book_.Find(active_);
        named != nullptr && named->profile == captured) {
      return;
    }
    book_.Put(active_, captured);
    port_.Save(book_);
  }

  // Le corps de la page. ⚠ IL RETIENT `this` : ceci doit survivre au panneau.
  [[nodiscard]] ro::ui::OptionSectionBody Body() {
    return [this](const ro::ui::SkinArt& art, ro::ui::OptionBodyTabs&) {
      ro::ui::ProfilesPageView view = view_;
      const ProfileFamilyText text = text_();
      view.title = text.title;
      view.help = text.help;
      view.id = text.id;
      ro::ui::ProfilesPageAction asked = ro::ui::DrawProfilesPage(view, state_, art);
      if (asked.gesture != ro::ui::ProfileGesture::kNone) {
        Ask(std::move(asked));
      }
    };
  }

  // LE GESTE DE LA PAGE, appliqué au prochain `Tick`.
  void Ask(ro::ui::ProfilesPageAction asked) { asked_ = std::move(asked); }

  [[nodiscard]] const std::string& active() const noexcept { return active_; }
  [[nodiscard]] const ProfileBook<Profile>& book() const noexcept { return book_; }

 private:
  // L'ÉTAT SE RANGE QUAND IL S'EST TU CE TEMPS-LÀ : un curseur qu'on fait
  // glisser change à chaque image.
  static constexpr std::uint32_t kQuietMs = 1000;
  // ET AU PLUS TARD AU BOUT DE CELUI-CI : un état que l'appelant change à
  // chaque image ne laisse jamais le calme revenir.
  static constexpr std::uint32_t kLongestWaitMs = 10000;

  // LE NOM QUE LE JOUEUR A TAPÉ : le libellé traduit de « Default » le désigne.
  [[nodiscard]] static std::string Typed(std::string name) {
    if (name == ro::core::Tr("Défaut")) {
      return std::string(kDefaultProfileName);
    }
    return name.substr(0, kNamedProfileMostBytes);
  }

  // Ce que porte ce profil : l'état présent s'il est posé, son fichier sinon,
  // les défauts pour « Default » jamais écrit.
  [[nodiscard]] Profile Content(const std::string& name) {
    if (name == active_) {
      return port_.Capture();
    }
    const Named<Profile>* named = book_.Find(name);
    return named != nullptr ? named->profile : port_.Defaults();
  }

  void Apply(const ro::ui::ProfilesPageAction& asked) {
    // CE QUI ATTEND VA D'ABORD AU PROFIL POSÉ : changer de profil ne doit pas
    // perdre le dernier changement.
    Flush();
    if (writable_) {
      writable_ = Reload();
    }
    static_cast<void>(KeepDefault());
    const auto exists = [this](const std::string& name) {
      return book_.Find(name) != nullptr || name == kDefaultProfileName;
    };
    const std::string fresh = Typed(asked.new_name);
    switch (asked.gesture) {
      case ro::ui::ProfileGesture::kNone:
        return;
      case ro::ui::ProfileGesture::kLoad:
        if (exists(asked.target)) {
          Pose(asked.target);
        }
        break;
      case ro::ui::ProfileGesture::kSaveAs:
        if (fresh.empty() || !book_.RoomFor(fresh)) {
          break;
        }
        book_.Put(fresh, port_.Capture());
        Pose(fresh);
        break;
      case ro::ui::ProfileGesture::kDuplicate:
        if (fresh.empty() || exists(fresh) || !exists(asked.target) || !book_.RoomFor(fresh)) {
          break;
        }
        book_.Put(fresh, Content(asked.target));
        break;
      case ro::ui::ProfileGesture::kRename:
        if (book_.Rename(asked.target, fresh) && active_ == asked.target) {
          active_ = fresh;
        }
        break;
      case ro::ui::ProfileGesture::kReset:
        if (!exists(asked.target)) {
          break;
        }
        if (asked.target == active_) {
          port_.Pose(port_.Defaults());
          book_.Put(active_, port_.Capture());
        } else {
          book_.Put(asked.target, port_.Defaults());
        }
        break;
      case ro::ui::ProfileGesture::kDelete:
        if (asked.target == kDefaultProfileName || book_.Find(asked.target) == nullptr) {
          break;
        }
        book_.Remove(asked.target);
        if (active_ == asked.target) {
          Pose(std::string(kDefaultProfileName));
        }
        break;
    }
    Store();
    Settle();
    ComposeView();
  }

  // Pose ce profil, s'il existe, et le retient pour le personnage en jeu.
  void Pose(const std::string& name) {
    active_ = name;
    if (const Named<Profile>* named = book_.Find(name); named != nullptr) {
      port_.Pose(named->profile);
    }
    if (!character_.empty()) {
      book_.Bind(character_, name);
    }
  }

  // 🔴 « Default » S'ÉCRIT AVANT QU'ON LE QUITTE. Jamais écrit, il n'aurait
  // rien à reposer : charger « Défaut » laisserait l'état du profil d'avant, et
  // le premier enregistrement l'y rangerait. Rend vrai quand il vient d'être
  // écrit — l'état présent, puisque c'est le profil posé.
  bool KeepDefault() {
    if (book_.Find(kDefaultProfileName) != nullptr || active_ != kDefaultProfileName) {
      return false;
    }
    book_.Put(kDefaultProfileName, port_.Capture());
    Store();
    return true;
  }

  // Relit le fichier. Faux quand il ne doit pas s'écraser : le livre garde
  // alors ce qu'il a, et rien ne s'écrit.
  bool Reload() {
    ProfileBook<Profile> fresh;
    if (!port_.Load(fresh)) {
      return false;
    }
    book_ = std::move(fresh);
    return true;
  }

  void Store() {
    if (writable_) {
      port_.Save(book_);
    }
  }

  // Rien n'a changé depuis : l'état présent est la référence.
  void Settle() {
    seen_revision_ = port_.Revision();
    dirty_ = false;
  }

  void ComposeView() {
    names_.clear();
    labels_.clear();
    // « Default » EN TÊTE, et toujours là : il existe avant d'avoir été écrit.
    names_.emplace_back(kDefaultProfileName);
    for (const Named<Profile>& named : book_.profiles) {
      if (named.name != kDefaultProfileName) {
        names_.push_back(named.name);
      }
    }
    // ⚠ LE LIBELLÉ TRADUIT SE COPIE : le texte que rend `Tr` meurt au prochain
    // changement de langue. ⚠ ET LES POINTEURS SE PRENNENT UNE FOIS LES NOMS
    // RANGÉS : pris avant une réallocation, ils désigneraient un tampon libéré.
    default_label_ = ro::core::Tr("Défaut");
    for (const std::string& name : names_) {
      labels_.push_back(name == kDefaultProfileName ? default_label_.c_str() : name.c_str());
    }
    const auto active = std::ranges::find(names_, active_);
    view_ = ro::ui::ProfilesPageView{
        .names = names_,
        .labels = labels_,
        .active = active == names_.end() ? 0 : static_cast<int>(active - names_.begin()),
        .fixed = 0,
        .character = character_.empty() ? nullptr : character_.c_str(),
        .full = book_.ShownCount() >= kProfileMost};
  }

  ProfilePort<Profile>& port_;
  ProfileFamilyTextFn text_;
  ProfileBook<Profile> book_;
  bool writable_ = true;
  std::string active_{kDefaultProfileName};
  std::string character_;

  // CE QUE LA DERNIÈRE IMAGE A VU, pour reconnaître un changement.
  std::uint64_t seen_revision_ = 0;
  bool dirty_ = false;
  std::uint32_t first_change_ms_ = 0;
  std::uint32_t last_change_ms_ = 0;

  // La page.
  ro::ui::ProfilesPageState state_;
  ro::ui::ProfilesPageAction asked_;
  std::vector<std::string> names_;
  std::vector<const char*> labels_;
  std::string default_label_;
  ro::ui::ProfilesPageView view_;
};

}  // namespace moonclient
