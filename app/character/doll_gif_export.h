#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/doll_animation.h"
#include "ro/ui/doll_gif.h"

struct SDL_Window;

namespace moonclient {

// CE QUE LE CLIC FIGE : l'apparence, la pose, la direction, l'échelle et le nom
// proposé. Tourner le pantin pendant que le dialogue est ouvert ne doit pas
// changer le fichier qu'on est en train d'enregistrer.
struct DollGifOrder {
  ro::ui::DollGifVerb verb = ro::ui::DollGifVerb::kNothing;
  ro::render::ActorLook look;
  ro::render::DollShot shot;
  std::string file_name;
};

// ── LE GIF DU PANTIN, CÔTÉ MACHINE ──────────────────────────────────────────
//
// `ui` dit « enregistrer » ou « copier » ; celui-ci compose les images, les
// encode, demande où ranger le fichier et l'écrit — ou le dépose dans le
// presse-papier —, puis rend le mot à dire.
//
// 🔴 LES IMAGES SE COMPOSENT AU CLIC, SUR LE FIL DE LA BOUCLE : la garde-robe
// n'est pas faite pour être lue de deux fils, et quarante compositions de
// sprite coûtent moins qu'une image de jeu. Ce qui coûte — quantifier et
// compresser — part sur un fil à lui, comme pour l'enregistreur de zone.
//
// 🔴 LE DIALOGUE « ENREGISTRER SOUS » NE BLOQUE PAS LA BOUCLE. Il répond plus
// tard, et peut-être depuis un autre fil : sa réponse passe par une boîte aux
// lettres gardée par un verrou, que la boucle relève à chaque image. Une boucle
// figée le temps qu'on choisisse un nom ferait tomber la connexion.
//
// ⚠ L'ENCODAGE COURT PENDANT QUE LE DIALOGUE EST OUVERT : le fichier s'écrit
// dès que les deux ont répondu, et le joueur n'attend que rarement.
//
// 🔴 IL VIT AVEC LES ACCESSOIRES DU CLIENT, ET NON AVEC LA PARTIE : un
// changement de partie pendant que le dialogue est ouvert ne perd pas le choix.
// Les images sont déjà composées ; la partie suivante relève la réponse,
// écrit le fichier et le dit dans SA conversation.
class DollGifExport {
 public:
  DollGifExport();
  DollGifExport(const DollGifExport&) = delete;
  DollGifExport& operator=(const DollGifExport&) = delete;
  DollGifExport(DollGifExport&&) = delete;
  DollGifExport& operator=(DollGifExport&&) = delete;
  ~DollGifExport();

  // LANCE L'EXPORT. Sans effet pendant un autre : le bouton est grisé, mais un
  // clic peut précéder d'une image le grisé.
  //
  // `window` est la fenêtre du client, dont le dialogue est l'enfant ; nulle,
  // il s'ouvre seul. `client_root` donne le dossier des captures, créé au
  // besoin.
  void Start(const DollGifOrder& order, ro::render::ActorWardrobe& wardrobe, SDL_Window* window,
             const std::filesystem::path& client_root);

  // UN EXPORT QUI N'A PAS PU PARTIR — ni garde-robe, ni joueur : le clic ne
  // reste pas muet. Sans effet pendant un export en cours.
  void Refuse(const std::string& cause);

  // RÉCOLTE CE QUI EST PRÊT, à chaque image. Rend la ligne à écrire dans la
  // conversation après un enregistrement réussi.
  [[nodiscard]] std::optional<std::string> Tick();

  // Ce que la fiche en montre. ⚠ LE MESSAGE POINTE ICI : la vue ne vaut que
  // pour l'image.
  [[nodiscard]] ro::ui::DollGifView view() const noexcept;

  // La boîte aux lettres du dialogue : voir le `.cpp`.
  struct DialogMailbox;

 private:
  void OpenSaveDialog(SDL_Window* window, const std::filesystem::path& folder,
                      const std::string& file_name);
  void Encode(std::vector<ro::formats::Image> frames, std::vector<int> delays_cs);
  [[nodiscard]] std::optional<std::string> Deliver();
  void Fail(const std::string& cause);

  ro::ui::DollGifVerb verb_ = ro::ui::DollGifVerb::kNothing;
  std::filesystem::path client_root_;
  std::shared_ptr<DialogMailbox> mailbox_;

  std::thread encoder_;
  std::atomic<bool> encoded_{false};
  // ⚠ ÉCRITS PAR LE FIL, LUS APRÈS LE DRAPEAU SEULEMENT.
  std::vector<std::uint8_t> bytes_;
  std::string encode_error_;

  // Le dernier mot dit au joueur. ⚠ UNE COPIE, ET NON LE POINTEUR DE `Tr` :
  // celui-ci appartient au catalogue et meurt au changement de langue.
  std::string message_;
};

}  // namespace moonclient
