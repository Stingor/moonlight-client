#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace moonclient {

// ── LES COMMANDES DU JOUEUR : S'ASSEOIR, ÉCHANGER, LE GROUPE, LA GUILDE ─────
//
// Celles du client du joueur, sous ses noms et sous leur double tapé sur un
// clavier coréen resté en mode latin (« /dkswrl » est « /앉기 »). Le mot se lit
// sans la casse latine ; ce qui suit se lit comme lui le lit :
//
//   · un NOM SANS ESPACE pour `/deal`, `/organize`, `/expel`, `/guild` et
//     `/breakguild` — la ligne doit compter deux mots, ni plus ni moins ;
//   · TOUT CE QUI SUIT la première espace pour `/invite` et `/guildinvite`,
//     où un nom peut en porter ;
//   · un ENTIER pour `/v` et `/bv`, lu comme `atoi` — « abc » vaut zéro — et
//     borné à l'échelle du client.
//
// ⚠ UNE FORME FAUSSE N'EST PAS UNE AUTRE COMMANDE : elle se dit au joueur
// (`malformed`), au lieu de partir au serveur ou de se taire.
enum class PlayerCommandKind : std::uint8_t {
  kNone,
  // `/sit` ET `/stand` BASCULENT TOUS DEUX, comme au client du joueur : assis,
  // on se lève ; debout, on s'assied.
  kSitOrStand,
  kDeal,
  kOrganizeParty,
  kInviteToParty,
  kLeaveParty,
  kExpelFromParty,
  kCreateGuild,
  kInviteToGuild,
  kBreakGuild,
  kAllowPartyInvites,
  kRefusePartyInvites,
  kToggleDealRefusal,
  kUserCount,
  kHelp,
  kEmotionList,
  kSoundVolume,
  kMusicVolume,
};

struct PlayerCommand {
  PlayerCommandKind kind = PlayerCommandKind::kNone;
  // LE NOM VISÉ, tel que tapé. Vide pour une commande qui n'en prend pas.
  std::string name;
  // LE VOLUME DEMANDÉ, déjà borné ; rien quand la ligne n'en donne pas — la
  // commande dit alors le volume en cours.
  std::optional<int> volume;
  bool malformed = false;
};

// L'ÉCHELLE DES VOLUMES DU CLIENT DU JOUEUR : de zéro à cent vingt-sept.
inline constexpr int kLoudestCommandVolume = 127;

// La commande que la ligne porte, `kNone` si elle n'en porte aucune.
[[nodiscard]] PlayerCommand ReadPlayerCommand(std::string_view line);

// Les deux sens de l'échelle : un volume de commande en fraction de zéro à
// un, et l'inverse, arrondi au plus proche.
[[nodiscard]] float VolumeFractionOf(int command_volume);
[[nodiscard]] int CommandVolumeOf(float fraction);

// LA FORME ATTENDUE, à dire quand la ligne ne la respecte pas : le mot et ce
// qu'il attend. Vide pour une commande qui n'a pas de forme fausse.
[[nodiscard]] std::string_view PlayerCommandWord(PlayerCommandKind kind);

}  // namespace moonclient
