#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/zone_move.h"
#include "ro/render/cursor.h"
#include "ro/render/screen_overlay.h"
#include "ro/render/screen_tint.h"
#include "ro/sim/bank.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/character_create_window.h"
#include "ro/ui/character_delete_window.h"
#include "ro/ui/character_select_window.h"
#include "ro/ui/login_window.h"

#include "game/client_shell.h"
#include "lobby/enter_veil.h"
#include "lobby/lobby_flow.h"
#include "shell/backdrop_scene.h"
#include "shell/banquet_dolls.h"
#include "shell/login_settings.h"
#include "shell/newcomer_preview.h"
#include "shell/seat_layout_file.h"
#include "web/http_transport.h"
#include "web/system_secret_store.h"
#include "web/web_credentials.h"
#include "web/web_login.h"

namespace moonclient {

// L'ÉCRAN PAR LEQUEL ON ENTRE : une ville derrière, un formulaire devant.
//
// 🔴 IL NE POSSÈDE NI LA FENÊTRE NI LA BOUCLE. Il reçoit la coquille, comme la
// partie, et il rend une IMAGE — c'est ce qui garantit que les deux écrans ont
// exactement le même rythme, le même plafond de cadence et la même façon de
// présenter. Voir `Screen`.
//
// ⚠ IL NE DÉCIDE RIEN DU PROTOCOLE : `LobbyFlow` tient les états, les refus et
// les bornes du décor, sur un banc qui tourne sans fenêtre. Cet écran-ci
// n'aiguille que des INTENTIONS — ce que la fenêtre a demandé, ce que la
// machine à états répond — et dessine.
class LobbyScreen : public Screen {
 public:
  LobbyScreen(ClientShell& shell, std::string host, std::uint16_t port);

  // Demande le décor vivant, et ouvre l'écran sur le chargement.
  //
  // ⚠ AUCUNE CARTE N'EST MONTÉE ICI : l'écran reste noir, avec « Chargement »,
  // jusqu'à ce qu'une scène soit prête — la ville du serveur, ou le repli local
  // s'il n'en vient aucune. Voir `WatchBackdrop`.
  [[nodiscard]] ro::core::Result<void> Open(std::uint32_t now_ms);

  // OUVRIR DROIT SUR LA TABLÉE, au retour de la partie : à appeler avant
  // `Open`. Ni formulaire, ni mot de passe, ni ville de fond — la salle couvre
  // l'écran, et la session du décor tiendrait la seule place que le serveur
  // accorde à cette adresse.
  //
  // `door` est la porte par laquelle le joueur était entré : « Revenir au
  // login » doit l'y ramener, comme sans l'aller-retour par la partie. ⚠ CE
  // N'EST PAS LA RETENIR D'UN LANCEMENT À L'AUTRE, ce que `LoginWindowState`
  // refuse : l'aller-retour se fait dans le même lancement.
  void ResumeAt(const lobby::Comeback& comeback, ro::ui::LoginDoor door) {
    resume_ = comeback;
    resume_door_ = door;
  }

  // Donne à voir les octets que le serveur de zone envoie au joueur, pour
  // enregistrer la session. Vide, rien n'est observé. À appeler avant `Open`.
  void ObserveZone(ro::net::ReceiveObserver observer) { flow_.ObserveZone(std::move(observer)); }

  // De quoi revenir à la tablée depuis la partie, pris à la remise.
  [[nodiscard]] const lobby::Comeback& comeback() const noexcept { return comeback_; }
  // La porte du formulaire, à reprendre au retour. Voir `ResumeAt`.
  [[nodiscard]] ro::ui::LoginDoor door() const noexcept { return form_.door; }

  [[nodiscard]] bool running() const noexcept override { return running_; }
  [[nodiscard]] ro::core::Result<void> Frame(std::uint32_t now_ms) override;
  void Close() override;

  // Vrai quand la main passe à la partie : le joueur est entré.
  //
  // ⚠ FAUX NE VEUT PAS DIRE ÉCHEC : le joueur a pu simplement fermer la fenêtre.
  [[nodiscard]] bool entered() const noexcept { return entered_; }

  // La liaison de zone, la carte et le monde de départ, une fois entré.
  // LA LIAISON DE ZONE, prise à l'entrée et gardée jusqu'à la remise.
  //
  // 🔴 ELLE NE SE DEMANDE PLUS À LA MACHINE À ÉTATS APRÈS COUP, et c'est un
  // défaut payé à l'écran : la coquille appelle `Close()` en rendant la main,
  // `Close()` rend la session du décor par `Release()`, et `Release()` ferme
  // TOUTES les liaisons — y compris celle du serveur de zone qu'on s'apprêtait
  // à confier à la partie. Le joueur arrivait alors sur sa carte, seul, sans
  // inventaire et sans personne : `Game::PumpNetwork` sort en silence sur une
  // liaison fermée, à chaque image.
  [[nodiscard]] ro::net::Connection TakeLink() noexcept { return std::move(link_); }
  [[nodiscard]] ro::sim::World TakeWorld() noexcept { return std::move(world_); }
  [[nodiscard]] const std::string& map_name() const noexcept { return map_name_; }
  // De quoi rentrer sur un AUTRE serveur de zone : voir `ro::net::ZoneTicket`.
  [[nodiscard]] const ro::net::ZoneTicket& ticket() const noexcept { return ticket_; }

 private:
  // Ce que la souris et le clavier ont fait de cette image.
  void PumpEvents();
  // Compose le monde de départ, prend la liaison, et rend la main à la partie.
  void HandOff();
  // Une carte à monter, et d'où elle vient.
  struct PendingMount {
    std::string map_name;
    // Vrai pour la ville du serveur, qui vise ensuite l'endroit où la session
    // a posé son spectateur.
    bool live = false;
  };
  // Décide quelle carte monter — la ville du serveur dès que la session la
  // nomme, le repli local si elle est hors de portée ou tarde — et la monte
  // UNE IMAGE après que l'écran de chargement a paru. Voir `Loading`.
  void WatchBackdrop(std::uint32_t now_ms);
  // Demande le repli local, une seule fois par lancement.
  void AskLocalBackdrop();
  // Demande une carte : elle se montera derrière l'écran de chargement.
  void AskMount(std::string map_name, bool live);
  // Monte la carte maintenant. ⚠ BLOQUANT : à n'appeler qu'une fois l'écran de
  // chargement présenté.
  void MountNow(const PendingMount& mount);
  // Vrai tant que l'écran de chargement doit tenir : une carte attend d'être
  // montée, ou l'écran s'est ouvert et aucune scène n'est encore prête.
  [[nodiscard]] bool Loading() const;
  // Ce que la fenêtre a demandé, aiguillé vers la machine à états.
  void Route(const ro::ui::LoginWindowAction& asked);
  // Ce que la scène de sélection a demandé.
  void RouteSeats(const ro::ui::CharacterSelectAction& asked);
  // Ce que la fenêtre de création a demandé.
  void RouteCreate(const ro::ui::CharacterCreateAction& asked);
  // Ce que la machine à états dit à la fenêtre de création.
  [[nodiscard]] ro::ui::CharacterCreateView ComposeNewcomer(
      const ro::ui::CharacterCreateAction& asked, std::uint32_t now_ms);
  // Ouvre la création à cet emplacement, sur un formulaire neuf.
  void OpenCreate(int slot);
  // La referme, et rend les vignettes de sa grille.
  void CloseCreate();
  // Ce que la machine à états dit à la confirmation de suppression.
  [[nodiscard]] ro::ui::CharacterDeleteView ComposeDoomed() const;
  // Ce que la confirmation de suppression a demandé.
  void RouteDelete(const ro::ui::CharacterDeleteAction& asked);
  // Ouvre la confirmation sur ce personnage, cible FIGÉE.
  void OpenDelete(std::uint32_t character_id);
  void CloseDelete();
  // Ce que la machine à états dit à la fenêtre.
  // ⚠ NON CONSTANTE, et c'est le cache d'images qui l'impose : lire une image
  // de l'installation la CHARGE la première fois. Même raison que
  // `ComposeSeats`.
  [[nodiscard]] ro::ui::LoginWindowView Compose();
  // Une ville est-elle montrée derrière le formulaire ? ⚠ DEUX TERMES : le
  // joueur doit la vouloir, et il faut en avoir une de montée.
  [[nodiscard]] bool ShowingCity() const;
  // Monte ce qui manque à la ville de fond, et demande celle du serveur.
  // ⚠ BLOQUE PLUS D'UNE SECONDE : à n'appeler qu'après l'avoir annoncé.
  void RaiseBackdrop(std::uint32_t now_ms);
  // Écrit — ou efface — ce que la porte Moonlight retient.
  void SaveWebMemory();
  // Ce que la porte Moonlight a demandé, aiguillé vers son parcours.
  // Ce que la porte Moonlight montre. ⚠ VIT DANS `lobby_web_door.cpp`, avec
  // le reste de cette porte.
  [[nodiscard]] ro::ui::WebDoorView ComposeWebDoor();
  void RouteWebDoor(const ro::ui::WebDoorAction& asked);
  // Vide le champ du mot de passe du site — sauf s'il est retenu.
  void ForgetWebPassword();
  // Ouvre la page de fusion de comptes du site dans le navigateur.
  void OpenMergePage();
  // Ouvre la page d'autorisation Discord dès que le site l'a nommée.
  void OpenDiscordPage();
  // Le parcours web vient d'obtenir son jeton : on se connecte avec.
  void EnterWithWebToken();
  // LA TABLÉE, composée depuis la liste : les convives, leurs pantins, la
  // salle. ⚠ À CHAQUE IMAGE, et c'est ce qu'il faut — les pantins se
  // recomposent seuls quand l'image change, et pas avant.
  [[nodiscard]] ro::ui::CharacterSelectView ComposeSeats(std::uint32_t now_ms);
  // Monte ce que la tablée demande : la garde-robe, et le décor de la salle.
  //
  // ⚠ À L'ARRIVÉE DE LA LISTE ET NON AU DÉMARRAGE : c'est le plus long
  // chargement du client, et le formulaire doit paraître tout de suite.
  void OpenBanquet();
  // Désigne le dernier personnage joué, s'il est là. Voir `recent_characters.h`.
  void PreferLastPlayed();
  // ── LA MISE EN PAGE DE LA TABLÉE ── vit dans `lobby_layout.cpp`.
  //
  // Lit la mise en page et les mises en page enregistrées.
  void LoadLayout();
  // Grave la mise en page, et seulement si la scène l'a marquée.
  //
  // ⚠ À CHAQUE SORTIE DE L'ÉCRAN, et pas seulement en quittant le mode : entrer
  // en jeu, revenir au login ou fermer le client ne repassent pas par
  // « Terminer ».
  void SaveLayoutIfDirty();
  void SaveLayout();
  // Relit la galerie : le joueur vient peut-être de déposer une image.
  void ScanBackgrounds();
  // Ouvre le dossier des décors dans l'explorateur, créé au besoin.
  void OpenBackgroundFolder();
  // Ce que la scène montre de la personnalisation : la salle, la galerie, les
  // mises en page enregistrées.
  void ComposeLayout(ro::ui::CharacterSelectView& view);
  // Ce que la personnalisation a demandé. Rend vrai si le verbe était le sien.
  bool RouteLayout(const ro::ui::CharacterSelectAction& asked);
  // Applique ce que la langue, la police et le décor ont demandé.
  void RouteFirstScreen(const ro::ui::LoginFirstScreenAction& asked);
  // La forme du curseur. ⚠ LE MÊME BESOIN QUE DANS LA PARTIE : l'habillage
  // cache celui du système, donc tout écran doit composer le nôtre.
  void ComposeCursor(std::uint32_t now_ms);
  // Les cibles des réglages : ce que la coquille possède, sans monde.
  [[nodiscard]] OptionTargets SettingsTargets();
  // Voir la définition : l'habillage, le filet de l'affichage, l'écriture.
  void FollowSettings(std::uint32_t now_ms);

  // Monte la vignette du décor, une fois, à la première image qui en a besoin.
  void EnsureTint();

  ClientShell& shell_;
  lobby::LobbyFlow flow_;
  BackdropScene backdrop_;
  // LA VIGNETTE POSÉE SUR LA VILLE, montée à la première image qui en a une à
  // habiller. Voir `EnsureTint`, et le chemin d'ouverture qu'elle évite.
  ro::render::ScreenTint tint_;
  bool tint_tried_ = false;
  ro::ui::LoginWindowState form_;
  // LES RÉGLAGES QUE LE FORMULAIRE OUVRE DE SA BARRE DE TITRE.
  LoginSettings settings_;
  // LE PARCOURS WEB. ⚠ SON TRANSPORT VIT PLUS LONGTEMPS QUE LUI, et l'ordre de
  // déclaration le garantit : le parcours en tient une référence, et un fil
  // détaché peut encore écrire dans le casier du transport.
  web::HttpTransport web_wire_{std::string(web::kWebLoginOrigin)};
  web::WebLogin web_{web_wire_, std::string(web::kWebLoginOrigin)};
  // LE COFFRE DU SYSTÈME. ⚠ SANS ÉTAT : il n'est ici que pour que la mémoire
  // du compte sache à qui demander de sceller.
  web::SystemSecretStore web_vault_;
  ro::ui::CharacterSelectState seats_;
  ro::ui::CharacterCreateState newcomer_;
  // Ce que la fenêtre de création a demandé à l'image PRÉCÉDENTE : la tranche
  // de grille visible et la boîte du pantin, que `app` compose ensuite. Voir
  // `CreatePickerRequest`.
  ro::ui::CharacterCreateAction newcomer_asked_;
  ro::ui::CharacterDeleteState doomed_;
  // Les pantins, montés en même temps que la garde-robe. ⚠ VIDE TANT QUE LA
  // LISTE N'EST PAS ARRIVÉE : voir `OpenBanquet`.
  std::optional<BanquetDolls> dolls_;
  std::optional<NewcomerPreview> newcomer_dolls_;
  // LES MISES EN PAGE ENREGISTRÉES. La mise en page courante, elle, est dans
  // `seats_.layout` : c'est la scène qui l'édite.
  std::vector<SeatLayoutPreset> layout_presets_;
  // La galerie de décors, relue à l'entrée du mode et sur demande — jamais à
  // chaque image, c'est un parcours de disque et d'archives.
  BackgroundGallery gallery_;
  int misplaced_backgrounds_ = 0;
  // Le décor qu'on a demandé à la dernière image, pour oublier le précédent
  // quand il change. Voir `ComposeLayout`.
  std::string shown_background_;

  // OÙ EN EST UNE DEMANDE DE RALLUMAGE DE LA VILLE.
  //
  // 🔴 TROIS ÉTATS ET NON UN BOOLÉEN, parce qu'il faut qu'une IMAGE ENTIÈRE
  // sépare l'annonce de l'appel qui bloque. La case est cochée PENDANT le
  // dessin — la vue de cette image-là est déjà composée —, donc l'annoncer
  // demande d'attendre la suivante, et de ne charger qu'à celle d'après. C'est
  // le même tour de boucle que celui qui sépare « Se connecter » de l'appel
  // réseau, et pour la même raison.
  enum class BackdropRaise : std::uint8_t { kNo, kAnnounced, kNow };
  BackdropRaise raise_ = BackdropRaise::kNo;

  bool running_ = true;
  bool entered_ = false;
  // LE VOILE DE L'ENTRÉE EN JEU. ⚠ LA MAIN NE PASSE QUE SUR UNE IMAGE DÉJÀ
  // NOIRE : c'est elle qui couvre le chargement bloquant de la carte, et rien
  // d'autre ne le couvre. Voir `lobby::EnterVeil`.
  lobby::EnterVeil veil_;
  // LE RETOUR DEPUIS LA PARTIE : demandé avant `Open`, puis en cours tant que
  // la liste n'est pas là — l'écran de chargement tient alors lieu de
  // formulaire, qui n'a rien à faire à l'écran.
  std::optional<lobby::Comeback> resume_;
  ro::ui::LoginDoor resume_door_ = ro::ui::LoginDoor::kMoonlight;
  bool resuming_ = false;
  // Pris à la remise, AVANT que `Close` ne rende la session qui le tient.
  lobby::Comeback comeback_;
  // Le repli local a-t-il déjà été tenté ? Voir `AskLocalBackdrop`.
  bool local_backdrop_tried_ = false;
  // L'ÉCRAN DE CHARGEMENT : la carte qui attend d'être montée, si elle a déjà
  // eu son image de chargement, et si l'ouverture attend encore sa première
  // scène. Voir `WatchBackdrop`.
  std::optional<PendingMount> pending_mount_;
  bool loading_shown_ = false;
  bool waiting_for_city_ = false;
  // La dernière adresse Discord tendue au navigateur, pour ne l'ouvrir qu'une
  // fois par demande. Voir `OpenDiscordPage`.
  std::string opened_browser_url_;
  // Quand l'écran s'est ouvert : l'orbite du décor s'en compte.
  std::uint32_t opened_ms_ = 0;
  std::string map_name_;
  ro::sim::World world_;
  // Prise au passage, pour qu'aucun nettoyage ne puisse la refermer. Voir
  // `TakeLink`.
  ro::net::Connection link_;
  ro::net::ZoneTicket ticket_;

  glm::vec2 mouse_{0.0F, 0.0F};
  bool mouse_pressed_ = false;
  ro::render::CursorShape cursor_shape_ = ro::render::CursorShape::kArrow;
  std::uint32_t cursor_since_ms_ = 0;
  std::vector<ro::render::ScreenQuad> cursor_quads_;
};

}  // namespace moonclient
