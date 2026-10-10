#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace moonclient {

// ── DES PROFILS NOMMÉS, ET CELUI DE CHAQUE PERSONNAGE ──────────────────────
//
// La forme commune des profils de disposition et des profils de réglages : une
// liste de profils nommés, et pour chaque personnage le nom de celui qu'il pose
// en entrant en jeu. Un personnage sans fiche pose « Default ».

// LE PROFIL QUI EXISTE TOUJOURS, et que nul ne supprime ni ne renomme. Un nom
// gravé : la page le traduit.
inline constexpr std::string_view kDefaultProfileName = "Default";
// AU PLUS CE NOMBRE DE PROFILS, « Default » compris, écrit ou non.
inline constexpr std::size_t kProfileMost = 16;
// UN NOM DE PROFIL tient dans ce nombre d'octets, en UTF-8.
inline constexpr std::size_t kNamedProfileMostBytes = 32;

template <typename Profile>
struct Named {
  std::string name;
  Profile profile;

  [[nodiscard]] friend bool operator==(const Named&, const Named&) = default;
};

// Le profil qu'un personnage pose. Le nom du personnage : un nom n'en désigne
// qu'un sur le serveur.
struct ProfileBinding {
  std::string character;
  std::string profile;

  [[nodiscard]] friend bool operator==(const ProfileBinding&, const ProfileBinding&) = default;
};

template <typename Profile>
struct ProfileBook {
  std::vector<Named<Profile>> profiles;
  std::vector<ProfileBinding> characters;

  [[nodiscard]] const Named<Profile>* Find(std::string_view name) const {
    const auto found = std::ranges::find(profiles, name, &Named<Profile>::name);
    return found == profiles.end() ? nullptr : &*found;
  }

  // Le profil de ce personnage : celui de sa fiche s'il existe encore,
  // « Default » sinon.
  [[nodiscard]] std::string ProfileOf(std::string_view character) const {
    const auto bound = std::ranges::find(characters, character, &ProfileBinding::character);
    if (bound != characters.end() && Find(bound->profile) != nullptr) {
      return bound->profile;
    }
    return std::string(kDefaultProfileName);
  }

  // Retient le profil de ce personnage, à la place de l'ancien.
  void Bind(std::string_view character, std::string_view profile) {
    std::erase_if(characters, [character](const ProfileBinding& binding) {
      return binding.character == character;
    });
    characters.push_back(
        ProfileBinding{.character = std::string(character), .profile = std::string(profile)});
  }

  // Remplace le profil de ce nom, ou l'ajoute à la fin.
  void Put(std::string_view name, const Profile& profile) {
    const auto found = std::ranges::find(profiles, name, &Named<Profile>::name);
    if (found != profiles.end()) {
      found->profile = profile;
      return;
    }
    profiles.push_back(Named<Profile>{.name = std::string(name), .profile = profile});
  }

  // Retire ce profil ; les personnages qui le posaient reviennent à
  // « Default ». ⚠ « Default » NE SE RETIRE PAS.
  void Remove(std::string_view name) {
    if (name == kDefaultProfileName) {
      return;
    }
    std::erase_if(profiles, [name](const Named<Profile>& named) { return named.name == name; });
    std::erase_if(characters,
                  [name](const ProfileBinding& binding) { return binding.profile == name; });
  }

  // Renomme ce profil ; les personnages qui le posaient suivent. Faux quand
  // l'ancien manque, que le nouveau est pris, ou que l'un des deux est
  // « Default ».
  bool Rename(std::string_view from, std::string_view to) {
    if (from == kDefaultProfileName || to == kDefaultProfileName || to.empty() ||
        Find(to) != nullptr) {
      return false;
    }
    const auto found = std::ranges::find(profiles, from, &Named<Profile>::name);
    if (found == profiles.end()) {
      return false;
    }
    found->name = std::string(to);
    for (ProfileBinding& binding : characters) {
      if (binding.profile == from) {
        binding.profile = std::string(to);
      }
    }
    return true;
  }

  // Les profils tels que la page les montre : « Default » compris, écrit ou non.
  [[nodiscard]] std::size_t ShownCount() const {
    return profiles.size() + (Find(kDefaultProfileName) != nullptr ? 0 : 1);
  }

  // Un profil de ce nom peut-il naître ? Un nom déjà pris se remplace toujours.
  [[nodiscard]] bool RoomFor(std::string_view name) const {
    return Find(name) != nullptr || name == kDefaultProfileName || ShownCount() < kProfileMost;
  }

  [[nodiscard]] friend bool operator==(const ProfileBook&, const ProfileBook&) = default;
};

}  // namespace moonclient
