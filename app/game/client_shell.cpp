// LA COQUILLE : ce qui s'ouvre avant toute partie, et la boucle qui les fait
// vivre.
//
// ⚠ L'ORDRE DE `Open` EST UN CONTRAT, et il ne se rattrape pas : les réglages
// se lisent avant la fenêtre parce qu'ils en portent la TAILLE, l'interface
// s'ouvre après la fenêtre parce qu'elle s'y accroche, et l'habillage arrive
// après les deux parce qu'il faut un contexte graphique pour le téléverser.

#include "game/client_shell.h"

#include <filesystem>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/core/log.h"

#include "game/game.h"
#include "game/save_folder.h"
#include "game/skin_reload.h"
#include "game/system_fonts.h"
#include "options/screen_choice.h"
#include "shell/skin_folder.h"
#include "world/map_music.h"
#include "world/map_stage.h"

namespace moonclient {
namespace {

constexpr int kMillisecondsPerSecond = 1000;

// OÙ L'INTERFACE RETIENT SES FENÊTRES : dans `savedata\` du dossier du jeu,
// avec les autres réglages du joueur — voir `save_folder.h`. Un lancement
// d'essai, qui a son propre dossier, a donc aussi son propre fichier.
//
// ⚠ L'ANCIEN EMPLACEMENT EST LE DOSSIER QUE LE SYSTÈME RÉSERVE AU PROGRAMME
// (`SDL_GetPrefPath`). Tant que `savedata\` n'a pas son fichier, on y recopie
// celui-là : sans cela, chaque fenêtre du joueur reviendrait une fois à sa
// place d'usine. L'ancien n'est jamais déplacé ni effacé — un échec de copie
// laisse seulement les places d'usine, et le journal le dit.
//
// ⚠ ET LE DOSSIER SE CRÉE ICI : ImGui écrit son fichier sans créer le dossier
// qui le porte, et une première partie sans `savedata\` perdrait tout.
std::filesystem::path InterfaceSettingsFile(const std::filesystem::path& client_root) {
  const std::filesystem::path folder = client_root / kSaveFolder;
  const std::filesystem::path file = folder / kInterfaceSettingsFile;
  std::error_code failed;
  std::filesystem::create_directories(folder, failed);
  if (std::filesystem::exists(file, failed)) {
    return file;
  }
  char* const former_folder = SDL_GetPrefPath("moonlight", "moonclient");
  if (former_folder == nullptr) {
    return file;
  }
  const std::filesystem::path former =
      std::filesystem::path(former_folder) / kInterfaceSettingsFile;
  SDL_free(former_folder);
  if (!std::filesystem::exists(former, failed)) {
    return file;
  }
  if (std::filesystem::copy_file(former, file, failed)) {
    ro::core::LogInfo("interface : réglages repris de {}", former.string());
  } else {
    ro::core::LogWarning("interface : {} ne se recopie pas ({}) — fenêtres à leur place d'usine",
                         former.string(), failed.message());
  }
  return file;
}

}  // namespace

ClientShell::ClientShell(std::filesystem::path client_root, std::filesystem::path archive_root,
                         std::vector<std::string> archives, std::string web)
    : client_root_(std::move(client_root)),
      archive_root_(std::move(archive_root)),
      archives_(std::move(archives)),
      web_(std::move(web)),
      layouts_(client_root_) {}

void ClientShell::ReportStage(const char* what) {
  // CE QUE CHAQUE ÉTAPE COÛTE, en millisecondes : un chargement se VOIT, et le
  // mesurer est le seul moyen de savoir s'il empire.
  const std::uint64_t now = SDL_GetTicks();
  ro::core::LogInfo("chargement {} : {} ms", what, now - stage_started_ms_);
  stage_started_ms_ = now;
  // ⚠ ENTRE DEUX ÉTAPES, ET NON PENDANT : voir `ClientShell::KeepAlive`. Une
  // étape ne se coupe pas en deux, mais leur COUTURE est un endroit sûr où
  // rendre la main — et il y en a une par poste du journal.
  KeepAlive();
}

ro::core::Result<void> ClientShell::OpenWindow(int width, int height) {
  stage_started_ms_ = SDL_GetTicks();

  // ── LES RÉGLAGES AVANT LA FENÊTRE ─────────────────────────────────────────
  // ⚠ LA POLICE SE CHOISIT AVANT LA PREMIÈRE TRAME : ImGui grave ses textures
  // au fil des trames, et refaire l'atlas en cours de route lui reprendrait ce
  // qu'il vient de poser.
  //
  // ⚠ ET LA TAILLE : ouvrir à la taille d'usine pour redimensionner ensuite se
  // voit — la fenêtre naît petite et saute. Lire ce fichier ne demande rien de
  // graphique.
  settings_.emplace(client_root_);
  // LES SKINS INSTALLÉS, pour la liste des options et pour le choix lu ici.
  settings_->options().pages().skin.SeeInstalled(InstalledSkins(client_root_));
  // L'HABILLAGE DU SKIN, rangé dans son dossier.
  settings_->options().pages().skin_look.Open(
      SkinFolder(client_root_, settings_->options().pages().skin.active()),
      SkinLookProfilePath(client_root_));
  // ⚠ LA LANGUE DÈS MAINTENANT : l'écran de mise à jour parle avant que le
  // reste des réglages s'applique. SANS CIBLE, donc sans rien enregistrer ;
  // l'application complète, plus loin, la reposera sans effet.
  {
    GameOptions& options = settings_->options();
    std::string why;
    if (!options.SetLanguage(std::string(options.language()), OptionTargets{}, &why)) {
      ro::core::LogWarning("langue : {} — l'interface reste en français", why);
    }
  }

  const ro::render::Resolution chosen = settings_->options().saved_resolution();
  const int first_width = chosen.width > 0 ? chosen.width : width;
  const int first_height = chosen.height > 0 ? chosen.height : height;
  // ⚠ LE TITRE N'A PAS ENCORE DE CARTE À NOMMER, et c'est le signe même de ce
  // que ce lot change : la fenêtre existe avant qu'on sache où l'on entre. Il
  // porte donc le nom et la version SEULS, sans le tiret qui introduit une
  // carte ; la partie le renomme à chaque image. Voir `WindowTitle`.
  // ⚠ ET SUR L'ÉCRAN CHOISI, ou le principal s'il a été débranché.
  const ScreenChoice saved_screen = settings_->options().saved_screen();
  auto context = ro::render::GlContext::CreateWindowed(
      WindowTitle(), first_width, first_height,
      [&saved_screen](std::span<const ro::render::ScreenInfo> connected) {
        return PickScreen(saved_screen, connected);
      });
  if (!context) {
    return std::unexpected(context.error());
  }
  gl_ = std::move(*context);
  ReportStage("fenêtre");

  // L'INTERFACE, aussitôt après : elle est ce qui décide, à chaque événement,
  // si le geste revient au jeu ou à une fenêtre.
  auto opened = UiStage::Open(gl_->window(), windows_, settings_->options().font_family(),
                              client_root_ / kLooseFontFolder, InterfaceSettingsFile(client_root_));
  if (!opened) {
    return std::unexpected(opened.error());
  }
  interface_.emplace(std::move(*opened));
  // LA DISPOSITION « Default », avant la première image : les fenêtres naissent
  // à leur place.
  layouts_.Start(*interface_);
  // Les polices du dossier du jeu, que la section des dégâts offre au choix.
  settings_->options().SetDamageFonts(interface_->LooseFontNames());
  ReportStage("interface");
  return {};
}

ro::core::Result<void> ClientShell::OpenArchives() {
  auto vfs = ro::vfs::Vfs::Mount(mount_options());
  if (!vfs) {
    return std::unexpected(vfs.error());
  }
  vfs_.emplace(std::move(*vfs));
  SeedDefaultSkin(*vfs_, client_root_);
  skin_shown_ = std::string(settings_->options().pages().skin.active());
  UseSkin(*vfs_, client_root_, skin_shown_);
  ReportStage("archives");

  // L'habillage, le curseur, la police : tout ce qui ne dépend d'aucune partie.
  LoadShellPieces(*vfs_, pieces_);
  interface_->UseSkin(pieces_.skin);
  interface_->SetSkinLook(settings_->options().pages().skin_look.look());
  ReportStage("pièces de la coquille");

  // Le SON. Une machine sans carte son ne doit pas empêcher de jouer : on le
  // dit une fois, et on continue en silence.
  audio_ = OpenAudio();
  // ⚠ AVEC LE SON ET NON AVEC LA PARTIE : l'écran de connexion montre une
  // ville, et il lui faut sa musique. Voir `ClientShell::music`.
  music_.emplace(*vfs_, client_root_);
  ReportStage("son");

  // 🔴 LES RÉGLAGES DE LA COQUILLE S'APPLIQUENT MAINTENANT, et non à la fin du
  // démarrage comme avant. La langue, le plein écran, la taille et les volumes
  // ne dépendent d'aucune carte : les appliquer en dernier voulait dire que
  // tout ce qui précédait s'était affiché dans la langue par défaut, dans une
  // fenêtre à la mauvaise taille. Ce qui manque encore à cette passe — la
  // conversation, les effets — n'existe pas à cette heure, et la partie la
  // rejouera avec la liste complète.
  //
  // ⚠ LA MUSIQUE, ELLE, N'Y MANQUE PLUS : elle appartient désormais à la
  // coquille, et l'écran de connexion en joue avant qu'aucune partie n'existe.
  settings_->ApplyOptions(OptionTargets{.audio = &audio_,
                                        .music = &*music_,
                                        .interface = &*interface_,
                                        .display = gl_.get(),
                                        .file = &settings_->file()});
  return {};
}

void ClientShell::EnsureBackdropPieces() {
  if (backdrop_pieces_loaded_ || !vfs_.has_value()) {
    return;
  }
  backdrop_pieces_loaded_ = true;
  LoadBackdropPieces(*vfs_, pieces_);
  KeepAlive();
}

void ClientShell::EnsureWorldPieces() {
  if (world_pieces_loaded_ || !vfs_.has_value()) {
    return;
  }
  world_pieces_loaded_ = true;
  backdrop_pieces_loaded_ = true;
  LoadWorldPieces(*vfs_, pieces_);
  // ⚠ LA FILE SE SERT APRÈS, comme à chaque couture d'étape : c'est le plus
  // long chargement du client, et la fenêtre existe déjà.
  //
  // ⚠ ET ELLE NE JOURNALISE RIEN : chaque appelant a sa propre mesure d'étape,
  // et deux lignes pour le même chargement se lisent comme deux chargements.
  KeepAlive();
}

bool ClientShell::FollowSkinChoice() {
  const std::string_view chosen = settings_->options().pages().skin.active();
  if (!vfs_.has_value() || !interface_.has_value() || chosen == skin_shown_) {
    return false;
  }
  skin_shown_ = std::string(chosen);
  ro::core::LogInfo("skin : {} posé en jeu", skin_shown_);
  UseSkin(*vfs_, client_root_, skin_shown_);
  ReloadSkinPieces(*vfs_, pieces_, world_pieces_loaded_);
  interface_->UseSkin(pieces_.skin);
  // L'HABILLAGE RÉGLÉ VIT DANS LE DOSSIER DU SKIN : il change avec lui.
  SkinLookOptions& look = settings_->options().pages().skin_look;
  look.Open(SkinFolder(client_root_, skin_shown_), SkinLookProfilePath(client_root_));
  interface_->SetSkinLook(look.look());
  return true;
}

void ClientShell::KeepAlive() const {
  // ⚠ SANS FENÊTRE, RIEN À SERVIR : les étapes d'ouverture appellent cette
  // fonction entre elles, et les premières ont lieu avant que la fenêtre existe.
  if (gl_ == nullptr) {
    return;
  }
  SDL_PumpEvents();
}

void ClientShell::WaitForFrameLimit(std::uint32_t frame_started_ms) {
  const int limit = settings_->options().frame_limit();
  if (limit <= 0) {
    return;
  }
  // ⚠ ON MESURE DEPUIS LE DÉBUT DE L'IMAGE, pas depuis la présentation : ce
  // qu'on plafonne est la CADENCE, et compter après coup ajouterait le temps de
  // dessin au temps d'attente — le plafond serait alors toujours manqué.
  const auto wanted_ms = static_cast<std::uint32_t>(kMillisecondsPerSecond / limit);
  const auto spent_ms = static_cast<std::uint32_t>(SDL_GetTicks()) - frame_started_ms;
  if (spent_ms >= wanted_ms) {
    return;
  }
  // 🔴 CE SOMMEIL N'EST PAS PRÉCIS, et c'est acceptable : le système rend la
  // main à la milliseconde près au mieux. Un plafond tenu à une image près
  // demanderait une attente active, qui brûlerait le cœur qu'on cherche
  // justement à laisser tranquille.
  SDL_Delay(wanted_ms - spent_ms);
}

ro::core::Result<void> ClientShell::Run(Screen& screen) {
  while (screen.running()) {
    // Une seule horloge par image : le réseau, les intentions et le dessin
    // voient le même instant.
    const auto now_ms = static_cast<std::uint32_t>(SDL_GetTicks());
    // ⚠ ET UNE SEULE MESURE DE LA FENÊTRE, en tête : voir `width()`.
    SDL_GetWindowSize(gl_->window(), &width_, &height_);
    if (auto ok = screen.Frame(now_ms); !ok) {
      return ok;
    }
    gl_->SwapBuffers();
    WaitForFrameLimit(now_ms);
  }
  // ⚠ CE QUI ATTENDAIT S'ÉCRIT MAINTENANT. Le délai d'anti-rebond n'a plus le
  // temps d'expirer : un réglage posé juste avant de quitter serait perdu.
  screen.Close();
  settings_->Flush();
  layouts_.Flush();
  // LES SONS D'AMBIANCE PARTENT AVEC L'ÉCRAN QUI LES A LANCÉS : la ville du
  // fond de la connexion ne suit pas le joueur dans la partie, ni la carte
  // quittée dans la sélection du personnage. Leurs sources meurent avec
  // l'écran ; leurs bruits, eux, jouent dans le mélangeur, qui reste.
  audio_.StopSounds(ro::audio::SoundChannel::kAmbience);
  return {};
}

}  // namespace moonclient
