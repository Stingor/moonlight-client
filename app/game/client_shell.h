#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/audio/audio_engine.h"
#include "ro/core/error.h"
#include "ro/render/gl_context.h"
#include "ro/vfs/vfs.h"

#include "game/client_config.h"
#include "game/game_pieces.h"
#include "game/game_settings.h"
#include "game/ui_layouts.h"
#include "game/ui_stage.h"
#include "game/zone_stream_capture.h"
#include "world/map_music.h"

namespace moonclient {

// CE QUE LA BOUCLE FAIT TOURNER : un écran, quel qu'il soit.
//
// 🔴 DEUX ÉCRANS PARTAGENT LA MÊME BOUCLE, et c'est ce que cette interface
// achète. L'écran de connexion et la partie ont chacun leur image, leur façon
// de s'arrêter et ce qu'ils écrivent en partant ; ce qu'ils N'ONT PAS à avoir
// chacun, c'est un rythme, un plafond de cadence et une manière de présenter —
// deux boucles, ce serait deux réponses à ces trois questions, et elles
// divergeraient.
class Screen {
 public:
  Screen() = default;
  Screen(const Screen&) = delete;
  Screen& operator=(const Screen&) = delete;
  Screen(Screen&&) = delete;
  Screen& operator=(Screen&&) = delete;
  virtual ~Screen() = default;

  // Vrai tant que cet écran a la main.
  [[nodiscard]] virtual bool running() const noexcept = 0;

  // UNE IMAGE. ⚠ ELLE NE PRÉSENTE PAS : la coquille échange les tampons.
  [[nodiscard]] virtual ro::core::Result<void> Frame(std::uint32_t now_ms) = 0;

  // Écrit ce qui attendait, au moment de rendre la main.
  virtual void Close() = 0;
};

// LA COQUILLE : ce qui existe AVANT qu'il y ait une partie, et qui lui survit.
//
// 🔴 LA FENÊTRE NAÎT AVANT LA CARTE, et c'est tout l'objet de cette classe.
// Jusqu'ici la fenêtre appartenait à la partie : `Game::Open` montait les
// archives, chargeait une carte, PUIS ouvrait la fenêtre. Un écran de connexion
// est impossible dans cet ordre — il faudrait une carte pour le dessiner, et
// l'on n'apprend sur quelle carte on entre qu'une fois connecté.
//
// Ce qu'elle possède ne dépend d'aucune carte et d'aucune session : les
// archives du joueur, ses réglages, la fenêtre et son contexte graphique,
// l'interface, l'habillage, le curseur, la police, le mélangeur. Une partie —
// et demain l'écran de connexion — les REÇOIT.
//
// ⚠ L'ORDRE DES MEMBRES EST L'ORDRE DE DESTRUCTION, À L'ENVERS, et il reprend
// les contraintes qui vivaient dans `Game` :
//   · les interrupteurs de fenêtre AVANT l'interface, qui en garde l'adresse ;
//   · le VFS AVANT tout ce qui en garde une référence ;
//   · les accessoires APRÈS le contexte graphique, donc détruits AVANT lui :
//     ils portent des textures, et les rendre au pilote une fois le contexte
//     parti ne veut plus rien dire.
//
// ⚠ ELLE NE SE DÉPLACE NI NE SE COPIE : le contexte graphique et celui de
// l'interface se désignent eux-mêmes, et une partie garde l'adresse de ce
// qu'elle contient.
class ClientShell {
 public:
  // `client_root` : le dossier du jeu, où vivent les réglages et le skin.
  // `archive_root` : où lire `archives`, celles que déclare `moonclient.yml`,
  // par priorité décroissante. C'est le dossier du jeu chez le joueur ; un
  // essai monte celles d'une installation sans y écrire.
  // `web` : l'adresse du service web du serveur, vide sans service.
  ClientShell(std::filesystem::path client_root, std::filesystem::path archive_root,
              std::vector<std::string> archives, std::string web);

  ClientShell(const ClientShell&) = delete;
  ClientShell& operator=(const ClientShell&) = delete;
  ClientShell(ClientShell&&) = delete;
  ClientShell& operator=(ClientShell&&) = delete;
  ~ClientShell() = default;

  // Lit les réglages, pose la langue, ouvre la fenêtre et l'interface — tout
  // ce qui ne demande AUCUNE archive.
  //
  // `width` et `height` ne servent qu'à DÉFAUT de taille enregistrée : le
  // réglage l'emporte, et ouvrir à la taille d'usine pour redimensionner
  // ensuite se voit — la fenêtre naît petite et saute.
  //
  // ⚠ HORS DU CONSTRUCTEUR : chaque étape peut échouer pour une raison que le
  // joueur doit lire, et un constructeur n'a que l'exception pour le dire — ce
  // que ce dépôt n'emploie pas.
  [[nodiscard]] ro::core::Result<void> OpenWindow(int width, int height);

  // Monte les archives, puis tout ce qui en vient : habillage, curseur, son,
  // et les réglages qui s'y appliquent.
  //
  // 🔴 SÉPARÉ DE LA FENÊTRE POUR LE PATCHER : il réécrit les archives, donc il
  // passe avant qu'on les ouvre — mais il a besoin d'une fenêtre pour se
  // montrer. Entre ces deux appels, rien n'est monté.
  [[nodiscard]] ro::core::Result<void> OpenArchives();

  // Tourne jusqu'à ce que la partie s'arrête.
  //
  // 🔴 UNE SEULE BOUCLE, ET ELLE EST ICI. Une bibliothèque qui possède la
  // boucle impose son rythme au reste ; la partie ne fournit qu'une IMAGE, et
  // c'est la coquille qui présente, plafonne la cadence et referme. C'est ce
  // qui permettra à l'écran de connexion de partager exactement la même.
  [[nodiscard]] ro::core::Result<void> Run(Screen& screen);

  [[nodiscard]] const std::filesystem::path& client_root() const noexcept { return client_root_; }
  // Ce que montent la partie et tout ce qui remonte les archives à part.
  [[nodiscard]] ro::vfs::MountOptions mount_options() const {
    return ro::vfs::MountOptions{.root = archive_root_, .archives = archives_};
  }
  [[nodiscard]] const std::string& web() const noexcept { return web_; }
  // Le fuseau horaire du serveur, lu dans `moonclient.yml` : voir
  // `ClientConfig::server_time_zone`.
  void SetServerTimeZone(std::string zone) { server_time_zone_ = std::move(zone); }
  [[nodiscard]] const std::string& server_time_zone() const noexcept { return server_time_zone_; }

  // L'ENREGISTREMENT DU FLUX DE ZONE, quand une option d'essai le demande.
  // Voir `ZoneStreamCapture`.
  void CaptureZoneStreams(std::filesystem::path first_file) {
    zone_capture_.emplace(std::move(first_file));
  }
  // L'observateur d'une liaison de zone neuve, ou rien sans enregistrement.
  [[nodiscard]] ro::net::ReceiveObserver ObserveZoneLink() {
    return zone_capture_.has_value() ? zone_capture_->ObserveNewLink() : ro::net::ReceiveObserver();
  }
  [[nodiscard]] ro::vfs::Vfs& vfs() noexcept { return *vfs_; }
  [[nodiscard]] ro::render::GlContext& gl() noexcept { return *gl_; }
  [[nodiscard]] GameSettings& settings() noexcept { return *settings_; }
  [[nodiscard]] UiStage::Windows& windows() noexcept { return windows_; }
  [[nodiscard]] UiStage& interface() noexcept { return *interface_; }
  // Les profils de disposition. Voir `UiLayouts`.
  [[nodiscard]] UiLayouts& layouts() noexcept { return layouts_; }
  [[nodiscard]] GamePieces& pieces() noexcept { return pieces_; }
  [[nodiscard]] ro::audio::AudioEngine& audio() noexcept { return audio_; }

  // LA MUSIQUE DE LA CARTE OÙ L'ON SE TROUVE, quel que soit l'écran.
  //
  // 🔴 ELLE VIVAIT DANS LA PARTIE, ET C'EST CE QUI LAISSAIT L'ÉCRAN DE
  // CONNEXION MUET. Son décor n'est pas une illustration : c'est une ville,
  // filmée en direct, et une ville sans sa musique n'est pas la ville. Deux
  // écrans la veulent donc — même raison que la garde-robe, montée ici pour
  // ne pas être lue deux fois.
  //
  // ⚠ ET LA PARTAGER FAIT MIEUX QUE LES ADDITIONNER : le morceau ne se relance
  // pas quand il ne change pas. Entrer en jeu sur la ville qu'on avait sous les
  // yeux garde donc la musique qui jouait, sans coupure.
  [[nodiscard]] MapMusic& music() noexcept { return *music_; }

  // LA TAILLE DE LA FENÊTRE, mesurée UNE FOIS par image, en tête de boucle.
  //
  // 🔴 ICI ET NON DANS LE DESSIN DU MONDE, où elle se relevait. La couche
  // d'écran vit dans le repère de la FENÊTRE, celui où la souris se déplace, et
  // les deux doivent venir de la même mesure — sinon un panneau se dessine là
  // où le clic ne le trouve pas. Tant que le monde se dessinait à chaque image,
  // la relever là revenait au même ; un écran qui ne dessine AUCUN monde — le
  // formulaire de connexion — l'aurait laissée sur sa valeur de la veille.
  [[nodiscard]] int width() const noexcept { return width_; }
  [[nodiscard]] int height() const noexcept { return height_; }

  // REND LA MAIN AU SYSTÈME, le temps d'un chargement.
  //
  // 🔴 CE LOT L'A RENDUE NÉCESSAIRE, et c'est le prix de ce qu'il gagne. Tant
  // que la fenêtre naissait APRÈS la carte, rien n'attendait pendant les
  // chargements : il n'y avait pas de fenêtre à servir. Maintenant elle existe
  // dès la première seconde, et une fenêtre dont personne ne vide la file de
  // messages est déclarée « (Ne répond pas) » par Windows au bout de quelques
  // secondes — elle blanchit, et le joueur croit à un plantage. Les accessoires
  // du monde coûtent à eux seuls six à dix secondes.
  //
  // ⚠ ELLE NE DESSINE RIEN, et c'est suffisant : ce que le système surveille
  // est qu'on SERVE la file, pas qu'on repeigne. Dessiner demanderait une image
  // complète, donc un état que le chargement est justement en train de bâtir.
  void KeepAlive() const;

  // CHARGE LES ACCESSOIRES DU MONDE, UNE FOIS.
  //
  // 🔴 DEUX ÉCRANS LES DEMANDENT DÉSORMAIS, et c'est ce qui les fait monter
  // ici. La partie les a toujours voulus ; la scène de sélection veut la
  // GARDE-ROBE, sans aucun monde — elle assoit vingt-cinq personnages. Les
  // laisser charger chacun de son côté les lirait DEUX FOIS quand le joueur
  // passe par l'écran de connexion, soit six à dix secondes payées pour rien.
  //
  // ⚠ ET C'EST LA COQUILLE QUI LES POSSÈDE, donc c'est à elle de savoir s'ils
  // sont là. Un drapeau chez l'appelant serait un drapeau par appelant.
  void EnsureWorldPieces();

  // LE SKIN CHOISI DANS LES OPTIONS, posé s'il a changé : la surcharge du VFS,
  // l'habillage des fenêtres et tout ce qui en a été tiré. Vrai quand il vient
  // de changer. ⚠ ENTRE DEUX IMAGES : des textures meurent.
  bool FollowSkinChoice();

  // CHARGE LA GARDE-ROBE SEULE, UNE FOIS.
  //
  // 🔴 LE DÉCOR VIVANT LA VEUT AVANT QUE LE JOUEUR AIT TAPÉ QUOI QUE CE SOIT :
  // sa ville se peuple une seconde après l'ouverture de l'écran, et les
  // accessoires du monde entier coûteraient six à dix secondes au démarrage —
  // avant le formulaire, ou en le figeant juste après. La garde-robe seule
  // coûte un tiers de seconde ; voir `LoadBackdropPieces`.
  void EnsureBackdropPieces();

 private:
  // Attend, s'il le faut, que l'image ait duré ce que le plafond demande.
  //
  // ⚠ RIEN TANT QU'IL N'Y EN A PAS : sans plafond le jeu dessine aussi vite
  // qu'il peut, et c'est la synchronisation verticale qui le retient d'ordinaire.
  void WaitForFrameLimit(std::uint32_t frame_started_ms);

  // Journalise le temps de l'étape qui s'achève, puis sert la file.
  void ReportStage(const char* what);

  std::filesystem::path client_root_;
  // Le début de l'étape de chargement en cours, en millisecondes SDL.
  std::uint64_t stage_started_ms_ = 0;
  std::filesystem::path archive_root_;
  // Par priorité décroissante. Gardées pour qui monte à nouveau — la
  // navigation.
  std::vector<std::string> archives_;
  std::string web_;
  std::string server_time_zone_ = std::string(kDefaultServerTimeZone);
  std::optional<ZoneStreamCapture> zone_capture_;
  std::optional<ro::vfs::Vfs> vfs_;
  std::optional<GameSettings> settings_;
  std::unique_ptr<ro::render::GlContext> gl_;
  // ⚠ AVANT L'INTERFACE : voir l'ordre de destruction, en tête.
  UiStage::Windows windows_;
  std::optional<UiStage> interface_;
  UiLayouts layouts_;
  // ⚠ APRÈS LE CONTEXTE GRAPHIQUE, donc détruits avant lui : ils portent des
  // textures.
  GamePieces pieces_;
  ro::audio::AudioEngine audio_;
  // ⚠ FACULTATIVE PARCE QU'ELLE LIT LA TABLE À SA CONSTRUCTION, donc après le
  // montage du VFS. Elle existe dès l'ouverture de la coquille et ne meurt plus.
  std::optional<MapMusic> music_;

  // Les accessoires du monde sont-ils lus ? Voir `EnsureWorldPieces`.
  bool world_pieces_loaded_ = false;
  bool backdrop_pieces_loaded_ = false;
  // Le skin posé sur le VFS. Voir `FollowSkinChoice`.
  std::string skin_shown_;

  int width_ = 0;
  int height_ = 0;
};

}  // namespace moonclient
