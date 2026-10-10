#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ro/render/game_images.h"
#include "ro/ui/map_sign_banner.h"
#include "ro/vfs/map_display_names.h"

namespace moonclient {

// QUELLES CARTES MONTRENT LEUR BANDEAU. ⚠ L'ORDRE EST CELUI DE LA LISTE DU
// PANNEAU, et le rang est ce que le fichier de réglages retient.
enum class MapSignMode : int {
  // Toutes celles qui ont un titre, et celles que la table annonce. LE DÉFAUT :
  // la table livrée n'annonce que des cartes récentes, et « comme le jeu » ne
  // montrerait le bandeau ni dans une ville ni dans un champ classique.
  kEveryTitled = 0,
  // Celles que la table annonce, comme le jeu.
  kLikeGame,
  kNever,
  kCount,
};

// CETTE CARTE MONTRE-T-ELLE SON BANDEAU, sous ce réglage ?
[[nodiscard]] bool MapSignWanted(const ro::vfs::MapSign& sign, MapSignMode mode) noexcept;

// LE BANDEAU DU NOM DE LA CARTE, entre la table des cartes et l'écran.
//
// 🔴 IL NAÎT AU CHARGEMENT D'UNE CARTE, pas à chaque arrivée : un téléport sur
// la même carte ne recharge rien, chez le client comme ici, et ne rejoue donc
// pas le bandeau.
class MapSignStage {
 public:
  // Une carte vient d'être chargée. `sign` nul, ou écarté par le réglage : un
  // bandeau encore visible de la précédente s'éteint.
  void Enter(const ro::vfs::MapSign* sign, MapSignMode mode, std::uint32_t now_ms);

  // La vue à dessiner, ou rien : pas de bandeau, bandeau fini, ou image absente
  // de l'installation — ce que `GameImages` dit au journal.
  //
  // ⚠ LA VUE POINTE DANS CET OBJET : elle vit jusqu'au prochain `Enter`.
  [[nodiscard]] std::optional<ro::ui::MapSignView> Stage(ro::render::GameImages* images,
                                                         std::uint32_t now_ms);

  void Forget() noexcept { shown_.reset(); }

  // Un bandeau est-il en cours, image trouvée ou non ?
  [[nodiscard]] bool showing() const noexcept { return shown_.has_value(); }

 private:
  std::optional<ro::vfs::MapSign> shown_;
  std::string image_path_;
  std::uint32_t entered_ms_ = 0;
};

// Le chemin de l'image d'un fond, sous le dossier d'interface, en UTF-8.
[[nodiscard]] std::string MapSignImagePath(std::string_view background);

// Le nom sous lequel la table range une carte chargée par son nom sans
// extension — ce que `MapStage::name` porte.
[[nodiscard]] std::string MapSignFileOf(std::string_view map_name);

}  // namespace moonclient
