#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "ro/render/gl_texture.h"
#include "ro/render/overlay_renderer.h"
#include "ro/sim/guild.h"
#include "ro/ui/skin_art.h"

#include "guild/emblem_plates.h"
#include "web/emblem_web.h"
#include "web/http_transport.h"

namespace moonclient {

// LES EMBLÈMES DES GUILDES : ceux que l'onglet montre, et ceux des étiquettes.
//
// 🔴 UNE VERSION PAR GUILDE, ET L'IMAGE SE TÉLÉCHARGE : le serveur de carte ne
// publie qu'un numéro, et c'est son service web qui sert l'image — voir
// `web/emblem_web.h`. Quand le numéro change, l'image d'avant ne vaut plus rien.
//
// ⚠ ELLES SE GARDENT SUR DISQUE, sous le nom du monde, de la guilde et de la
// version : un emblème ne change pas sans changer de version, et le
// retélécharger à chaque session serait redemander ce qu'on a.
//
// ⚠ LE CHEMIN HISTORIQUE COMPTE AUSSI : un serveur qui pousse l'image par le fil
// la voit décompressée et rangée ici, comme une image téléchargée.
class GuildEmblems {
 public:
  // `origin` vide : l'installation ne déclare pas de service, et seul le chemin
  // historique pourra montrer un emblème. `overlay` est la couche d'écran qui
  // téléverse les images des étiquettes : elle survit à la partie, et doit les
  // oublier quand la partie s'en va.
  GuildEmblems(std::string origin, std::filesystem::path cache_folder,
               ro::render::OverlayRenderer* overlay);
  ~GuildEmblems();

  GuildEmblems(const GuildEmblems&) = delete;
  GuildEmblems& operator=(const GuildEmblems&) = delete;

  void SetIdentity(web::EmblemIdentity identity);
  [[nodiscard]] bool service_known() const noexcept { return web_ != nullptr; }

  // Une fois par image : ce qu'il faut demander, ce qui est arrivé.
  //
  // ⚠ LA GUILDE DU JOUEUR, PUIS TOUT CE QUE LES ÉTIQUETTES ONT RÉCLAMÉ : chaque
  // guilde affichée dont la version est connue, comme le client d'origine.
  void Advance(const ro::sim::GuildBook& book, std::uint32_t now_ms);

  // L'emblème en place de la guilde du joueur, ou une image vide tant qu'il
  // n'est pas là.
  [[nodiscard]] ro::ui::SkinImage Of(std::int32_t guild_id) const;
  // L'image telle que servie — un BMP, ou un GIF —, ou nul.
  [[nodiscard]] const std::string* ImageOf(std::int32_t guild_id) const;

  // Ce que les étiquettes lisent, et où elles notent ce qui leur manque.
  [[nodiscard]] EmblemPlates& plates() noexcept { return plates_; }

  // Dépose un BMP. Faux quand un dépôt attend déjà, ou sans service.
  [[nodiscard]] bool Upload(std::int32_t guild_id, std::string bmp);
  [[nodiscard]] std::optional<web::EmblemUploadOutcome> TakeUploadOutcome();

 private:
  using Key = EmblemPlates::Key;

  // L'emblème de la guilde du joueur, en texture pour l'onglet.
  struct Shown {
    Key key{0, 0};
    std::optional<ro::render::GlTexture> texture;
    float width = 0.0F;
    float height = 0.0F;
  };

  // Va chercher une image : sur le fil, sur disque, puis au service.
  void Fetch(const ro::sim::GuildBook& book, Key key, std::uint32_t now_ms);
  // Range une image arrivée, décodée pour les étiquettes et, si `to_disk`, sur
  // disque. Faux quand elle est illisible : rien n'est alors écrit ni rangé.
  [[nodiscard]] bool Keep(Key key, std::string image, bool to_disk);
  // Refait la texture de l'onglet quand la guilde ou sa version change.
  void ShowOwn(Key key);
  [[nodiscard]] std::filesystem::path CachePath(Key key) const;

  std::filesystem::path cache_folder_;
  ro::render::OverlayRenderer* overlay_ = nullptr;
  std::string world_name_;
  std::unique_ptr<web::HttpTransport> transport_;
  std::unique_ptr<web::EmblemWeb> web_;
  EmblemPlates plates_;
  // Les images telles que servies, pour l'éditeur qui reprend l'emblème en place.
  std::map<Key, std::string> served_;
  Shown own_;
  // Les versions déjà cherchées sur disque : un fichier absent ne se redemande
  // pas à chaque image pendant que le téléchargement tourne.
  std::set<Key> looked_on_disk_;
};

}  // namespace moonclient
