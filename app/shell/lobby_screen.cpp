// L'ÉCRAN DE CONNEXION : la ville derrière, le formulaire devant, et ce qui
// passe de l'un à l'autre.
//
// ⚠ L'ORDRE DE L'IMAGE EST CELUI DE LA PARTIE, et pour les mêmes raisons : le
// monde d'abord, l'interface ensuite, le curseur en dernier. Ce qui change est
// qu'il n'y a pas de couche d'écran — une ville, ses passants et ses effets,
// puis une fenêtre.

#include "shell/lobby_screen.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/system_fonts.h"

#include "character/recent_characters.h"
#include "game/connected_session.h"
#include "options/game_options.h"
#include "options/option_targets.h"
#include "world/overlay_view.h"

namespace moonclient {
namespace {

// Recopie un texte retenu dans un champ de saisie, terminateur compris.
//
// ⚠ ELLE COUPE PLUTÔT QUE DE DÉBORDER : un champ du jeu est borné par le
// protocole, celui du site par rien — et ce qui a été écrit une fois peut
// avoir été écrit par une autre version.
template <std::size_t kBytes>
void Fill(std::array<char, kBytes>& field, std::string_view text) {
  const std::size_t kept = std::min(text.size(), field.size() - 1);
  text.copy(field.data(), kept);
  field[kept] = '\0';
}

// ⚠ LE RECOUPEMENT QUE `ui` NE PEUT PAS FAIRE : elle ne connaît pas le
// protocole, et son champ de saisie est dimensionné sur ce que `CA_LOGIN` porte.
// Une divergence est une erreur de compilation, pas une remarque en revue —
// même montage que le plafond de la banque.
static_assert(ro::ui::kLoginFieldBytes == 24,
              "le compte et le mot de passe tiennent sur 24 octets dans CA_LOGIN");

// ⚠ LE MÊME RECOUPEMENT POUR LE SECRET DE SUPPRESSION : `CH_DELETE_CHAR` porte
// une clé de cinquante octets, dont le serveur ne lit que les quarante
// premiers (`char email[40]`).
static_assert(ro::ui::kDeleteSecretBytes == 40,
              "le serveur ne lit que quarante octets de la clé de CH_DELETE_CHAR");

// COMBIEN DE TEMPS L'ÉCRAN DE CHARGEMENT ATTEND LA VILLE DU SERVEUR avant de
// monter le repli local. La poignée de main prend d'ordinaire une seconde ;
// au-delà de huit, le serveur hésite, et le joueur n'a pas à le regarder faire.
constexpr std::uint32_t kBackdropWaitLimitMs = 8'000;

// Le fond de l'écran de chargement : noir, comme celui qu'il recouvre.
constexpr std::array<float, 4> kLoadingClearColor = {0.0F, 0.0F, 0.0F, 1.0F};
}  // namespace

LobbyScreen::LobbyScreen(ClientShell& shell, std::string host, std::uint16_t port)
    : shell_(shell), flow_(std::move(host), port) {}

ro::core::Result<void> LobbyScreen::Open(std::uint32_t now_ms) {
  opened_ms_ = now_ms;
  // La mise en page de la tablée : un petit fichier, lu une fois.
  LoadLayout();
  // 🔴 DÉCOCHÉ, ON NE MONTE RIEN DU TOUT, et c'est là que ce réglage rend ce
  // qu'il promet. Celui qui l'éteint le fait parce que sa machine ou sa
  // connexion peinent : lui faire payer quand même la carte (1,4 s), la
  // garde-robe (0,3 s) et la session serait lui prendre exactement ce qu'il
  // demandait d'épargner. L'écran s'ouvre alors sur un fond uni.
  const bool wants_city = shell_.settings().options().backdrop_wanted();

  // 🔴 AUCUNE CARTE N'EST MONTÉE D'AVANCE, et l'écran s'ouvre sur le
  // chargement. Monter Prontera ici coûterait 1,4 s à CHAQUE lancement, pour la
  // montrer le temps de la poignée de main avant que la ville du serveur ne la
  // remplace — et ce remplacement figerait l'écran au milieu du formulaire.
  // L'écran noir tient jusqu'à ce qu'une scène soit prête : voir
  // `WatchBackdrop`, et le repli s'il n'en vient aucune.
  waiting_for_city_ = wants_city;

  // Le compte retenu, s'il y en a un. ⚠ LE MOT DE PASSE DU JEU, JAMAIS : voir
  // `LoginWindowState`.
  const std::string& remembered = shell_.settings().options().login_account();
  if (!remembered.empty()) {
    Fill(form_.account, remembered);
    form_.remember_account = true;
  }

  // CE QUE LA PORTE MOONLIGHT RETIENT.
  //
  // 🔴 LE MOT DE PASSE DU SITE EN FAIT PARTIE, contrairement à celui du jeu, et
  // ce n'est pas une inconséquence : celui du jeu traverse `CA_LOGIN` et n'a
  // nulle part où être scellé, celui du site ne sert qu'ici et le système sait
  // le garder. La case reste décochée par défaut — voir `SecretStore`, qui dit
  // ce que ce sceau achète et ce qu'il n'achète pas.
  const web::WebCredentials known = web::LoadWebCredentials(shell_.client_root(), web_vault_);
  Fill(form_.web.member, known.member);
  Fill(form_.web.password, known.password);
  form_.web.remember_member = known.remember_member;
  form_.web.remember_password = known.remember_password;

  // LE RETOUR D'UNE PARTIE QUI A PERDU SON SERVEUR : le formulaire, rempli,
  // et pourquoi on y est. ⚠ APRÈS LE REMPLISSAGE, pour la même raison que le
  // retour à la tablée ci-dessous.
  if (!notice_.empty()) {
    form_.door = resume_door_;
    flow_.Notify(std::exchange(notice_, std::string()));
  }

  // LE RETOUR DEPUIS LA PARTIE : droit au serveur de personnages. ⚠ PAS DE
  // VILLE DE FOND — la salle couvre l'écran, et monter une carte coûterait une
  // seconde et demie au joueur qui ne veut que changer de personnage.
  //
  // 🔴 APRÈS LE FORMULAIRE, ET PAS AVANT : cet écran est NEUF à chaque retour,
  // et « Revenir au login » depuis la tablée montre le formulaire. Sortir
  // avant de l'avoir rempli le laisserait vide — compte, membre du site et mot
  // de passe retenus compris.
  if (resume_.has_value()) {
    form_.door = resume_door_;
    flow_.Resume(*resume_);
    resume_.reset();
    resuming_ = true;
    waiting_for_city_ = false;
    return {};
  }

  // LA GARDE-ROBE, MAINTENANT : le décor vivant se peuple une seconde après
  // cet instant, et il faut de quoi habiller ses passants. ⚠ ELLE SEULE — les
  // accessoires du monde entier coûteraient six à dix secondes avant le
  // formulaire. Voir `LoadBackdropPieces`, et ce qu'elle a été mesurée coûter.
  if (!wants_city) {
    ro::core::LogInfo("décor : éteint par le joueur — l'écran de connexion reste sur un fond uni");
    return {};
  }
  const std::uint64_t wardrobe_started_ms = SDL_GetTicks();
  shell_.EnsureBackdropPieces();
  ro::core::LogInfo("chargement garde-robe : {} ms", SDL_GetTicks() - wardrobe_started_ms);

  // LE DÉCOR VIVANT, DEMANDÉ ICI SEULEMENT : l'ouvrir sans le dessiner tiendrait
  // la seule place que le serveur accorde à cette adresse pour montrer une
  // ville que personne ne regarde.
  //
  // ⚠ IL PASSE AVANT LE REPLI : si le serveur donne une ville, elle se monte
  // derrière l'écran de chargement ; s'il ne peut pas, Prontera la remplace.
  // Voir `WatchBackdrop`.
  flow_.OpenBackdrop(now_ms);
  return {};
}

void LobbyScreen::AskLocalBackdrop() {
  // UNE SEULE TENTATIVE PAR LANCEMENT : un échec ne se retente pas à chaque
  // image, et une réussite n'a pas à se refaire.
  if (local_backdrop_tried_) {
    waiting_for_city_ = false;
    return;
  }
  local_backdrop_tried_ = true;
  AskMount(std::string(kLocalBackdropMap), /*live=*/false);
}

void LobbyScreen::AskMount(std::string map_name, bool live) {
  pending_mount_ = PendingMount{.map_name = std::move(map_name), .live = live};
  loading_shown_ = false;
}

void LobbyScreen::MountNow(const PendingMount& mount) {
  const std::uint64_t started_ms = SDL_GetTicks();
  if (auto opened = backdrop_.Open(shell_.vfs(), mount.map_name); !opened) {
    // ⚠ MUET POUR LE JOUEUR, comme tout ce qui touche au décor : le repli local
    // prend la place d'une ville qui ne se monte pas, et l'illustration celle
    // d'un repli qui ne se monte pas non plus.
    ro::core::LogWarning("décor : {} ne se monte pas ({})", mount.map_name,
                         ro::core::Describe(opened.error()));
    if (mount.live && !backdrop_.ready()) {
      AskLocalBackdrop();
    } else {
      waiting_for_city_ = false;
    }
    return;
  }
  ro::core::LogInfo("chargement décor {} : {} ms", mount.map_name, SDL_GetTicks() - started_ms);
  waiting_for_city_ = false;
  // LE BANDEAU DE LA VILLE MONTÉE, comme à l'arrivée en jeu. ⚠ SON COMPTE PART
  // D'ICI : le montage bloque, et l'heure prise avant lui mangerait le bandeau.
  GamePieces& pieces = shell_.pieces();
  pieces.map_sign.Enter(pieces.map_display_names.has_value()
                            ? pieces.map_display_names->SignOf(MapSignFileOf(mount.map_name))
                            : nullptr,
                        shell_.settings().options().pages().map_sign.mode(),
                        static_cast<std::uint32_t>(SDL_GetTicks()));
  if (mount.live) {
    backdrop_.WatchCell(ro::sim::UnpackPosition(flow_.backdrop().spawn().packed_position));
  }
}

bool LobbyScreen::Loading() const {
  return pending_mount_.has_value() || (waiting_for_city_ && !backdrop_.ready()) ||
         (resuming_ && flow_.screen() == lobby::LobbyScreenKind::kConnecting);
}

void LobbyScreen::WatchBackdrop(std::uint32_t now_ms) {
  const bool wants_city = shell_.settings().options().backdrop_wanted();
  // 🔴 UNE CARTE SE MONTE UNE IMAGE APRÈS QUE L'ÉCRAN DE CHARGEMENT A PARU, et
  // jamais avant. Le montage fige la fenêtre une seconde et demie : sur le
  // formulaire ou sur la ville d'avant, ce figement se lit comme une panne ; sur
  // l'écran noir qui dit « Chargement », il se lit comme ce qu'il est.
  if (pending_mount_.has_value()) {
    if (!wants_city) {
      pending_mount_.reset();
      waiting_for_city_ = false;
      return;
    }
    if (!loading_shown_) {
      return;
    }
    const PendingMount mount = std::move(*pending_mount_);
    pending_mount_.reset();
    loading_shown_ = false;
    MountNow(mount);
    return;
  }
  if (!wants_city) {
    waiting_for_city_ = false;
    return;
  }
  const lobby::BackdropSession& live = flow_.backdrop();
  // LE DÉCOR VIVANT EST HORS DE PORTÉE : c'est seulement maintenant que le
  // repli local se monte, et seulement si aucune ville n'est déjà à l'écran.
  if (live.state() == lobby::BackdropState::kUnavailable) {
    if (!backdrop_.ready()) {
      AskLocalBackdrop();
    }
    return;
  }
  // ⚠ ET S'IL TARDE, LE REPLI AUSSI : l'écran noir ne doit pas durer tant que
  // le serveur hésite. La ville, si elle arrive ensuite, prend sa place.
  if (waiting_for_city_ && !backdrop_.ready() && now_ms - opened_ms_ >= kBackdropWaitLimitMs) {
    ro::core::LogInfo("décor : le serveur tarde — repli sur {} en attendant", kLocalBackdropMap);
    AskLocalBackdrop();
    return;
  }
  if (live.state() != lobby::BackdropState::kLive || backdrop_.map_name() == live.map_name()) {
    return;
  }
  // 🔴 LA CARTE N'EST CONNUE QU'À L'ENTRÉE, jamais avant : le serveur tire un
  // lieu au sort DEUX fois — une pour bâtir la liste, une à la sélection — et
  // les deux diffèrent. On ne peut donc pas la précharger, seulement la monter
  // quand elle arrive.
  AskMount(std::string(live.map_name()), /*live=*/true);
}

void LobbyScreen::PumpEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // L'INTERFACE VOIT TOUT, ET AVANT TOUT LE MONDE — même règle que dans la
    // partie : c'est elle qui sait ce qu'il y a sous le curseur.
    shell_.interface().TakeEvent(event);
    switch (event.type) {
      case SDL_EVENT_QUIT:
        running_ = false;
        break;
      case SDL_EVENT_MOUSE_MOTION:
        mouse_ = glm::vec2(event.motion.x, event.motion.y);
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT) {
          mouse_pressed_ = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        }
        break;
      default:
        break;
    }
  }
}

bool LobbyScreen::ShowingCity() const {
  return shell_.settings().options().backdrop_wanted() && backdrop_.ready();
}

ro::ui::LoginWindowView LobbyScreen::Compose() {
  ro::ui::LoginWindowView view;
  // 🔴 SANS VILLE, L'ILLUSTRATION DU CLIENT D'ORIGINE — et non un aplat. Le
  // décor vivant PREND SA PLACE (ADR 0057) ; l'éteindre doit donc rendre ce
  // qu'il avait pris, pas laisser un trou. C'est le seul JPEG de l'interface.
  if (!ShowingCity() && shell_.pieces().game_images.has_value()) {
    const ro::render::IconTexture& art =
        shell_.pieces().game_images->GetOurs(ro::ui::kLoginArtPath);
    if (art.usable()) {
      view.backdrop = ro::ui::SkinImage{.id = static_cast<ImTextureID>(art.texture.name()),
                                        .width = static_cast<float>(art.width_px),
                                        .height = static_cast<float>(art.height_px)};
    }
  }
  // 🔴 LA PORTE TIENT L'ÉCRAN JUSQU'AU BOUT, sauf devant le code PIN. Si elle
  // s'effaçait pendant que le JEU se connecte, le joueur verrait le
  // formulaire classique, vide, prendre la place de celui qu'il vient de
  // remplir. Ce que la connexion du jeu change est l'état de la porte
  // (`kEnteringGame`), pas la porte elle-même.
  //
  // ⚠ LE CODE PIN EST L'EXCEPTION, et c'est une question du serveur DE JEU à
  // laquelle aucune porte ne répond. Il prend l'écran.
  view.web_door_offered = flow_.screen() != lobby::LobbyScreenKind::kPincode;
  view.web = ComposeWebDoor();
  // ── LES TROIS RÉGLAGES DU PREMIER ÉCRAN ──────────────────────────────────
  const GameOptions& settings = shell_.settings().options();
  const std::span<const Language> languages = KnownLanguages();
  for (int rank = 0; rank < static_cast<int>(languages.size()); ++rank) {
    const Language& one = languages[static_cast<std::size_t>(rank)];
    // ⚠ LA PREMIÈRE N'A PAS DE CATALOGUE et n'en a pas besoin : c'est la langue
    // dans laquelle le code est écrit. Les autres se jugent sur le fichier —
    // une langue dont le catalogue manque reste proposée, mais inerte, plutôt
    // que de disparaître comme si le client ne la connaissait pas.
    const bool available = rank == 0 || std::filesystem::exists(std::filesystem::path("lang") /
                                                                (std::string(one.code) + ".txt"));
    view.first_screen.languages.push_back(
        ro::ui::LoginLanguage{.name = one.name, .available = available});
    if (settings.language() == one.code) {
      view.first_screen.language = rank;
    }
  }
  // ⚠ LES SEULES FAMILLES PRÉSENTES, comme dans les options : la place dans la
  // liste n'est pas le rang du réglage, voir `FontFamilyAtMenuRank`.
  for (const int family : ro::ui::InstalledFontFamilies()) {
    view.first_screen.fonts.emplace_back(
        ro::ui::SystemFontFamilies()[static_cast<std::size_t>(family)].label);
  }
  view.first_screen.font = ro::ui::MenuRankOfFontFamily(settings.font_family());
  view.first_screen.backdrop_wanted = settings.backdrop_wanted();
  view.first_screen.backdrop_live = flow_.backdrop().state() == lobby::BackdropState::kLive;
  // 🔴 LE VOILE NE COUVRE QUE CE QUI BLOQUE, et la distinction est tout.
  // Monter une carte prend la main pendant plus d'une seconde ; la ville du
  // SERVEUR, elle, arrive en fond sans rien figer — la voiler volerait au
  // joueur la seconde pendant laquelle il pouvait taper son compte.
  if (raise_ != BackdropRaise::kNo) {
    view.busy = ro::core::Tr("La ville arrive…");
  }

  switch (flow_.screen()) {
    case lobby::LobbyScreenKind::kForm:
      view.stage = ro::ui::LoginStage::kForm;
      break;
    case lobby::LobbyScreenKind::kPincode:
      view.stage = ro::ui::LoginStage::kPincode;
      break;
    default:
      view.stage = ro::ui::LoginStage::kConnecting;
      break;
  }
  view.message = flow_.message();
  view.pincode_refused = flow_.pincode_refused();
  return view;
}

// CE QUE LES TROIS RÉGLAGES DU PREMIER ÉCRAN ONT DEMANDÉ.
//
// ⚠ EXÉCUTÉ APRÈS LE DESSIN, et c'est ce qui rend inoffensif le piège de la
// langue : poser un catalogue vide le catalogue d'avant, et tout libellé
// traduit encore en vol pointerait dans ce qui vient d'être jeté. La fenêtre
// n'a rendu qu'une INTENTION ; à cette heure-ci, plus rien n'est en vol.
void LobbyScreen::RouteFirstScreen(const ro::ui::LoginFirstScreenAction& asked) {
  GameOptions& settings = shell_.settings().options();
  const OptionTargets targets{.interface = &shell_.interface(), .file = &shell_.settings().file()};

  const std::span<const Language> languages = KnownLanguages();
  if (asked.language >= 0 && asked.language < static_cast<int>(languages.size())) {
    std::string why;
    if (!settings.SetLanguage(languages[static_cast<std::size_t>(asked.language)].code, targets,
                              &why)) {
      // ⚠ AU JOURNAL, PAS AU BANDEAU : le bandeau de cet écran porte le refus
      // du SERVEUR, et y mettre un catalogue manquant ferait chercher une
      // panne de connexion.
      ro::core::LogWarning("langue : {} — l'interface reste dans la langue précédente", why);
    }
  }
  if (asked.font >= 0) {
    static_cast<void>(settings.SetFontFamily(ro::ui::FontFamilyAtMenuRank(asked.font), targets));
  }
  if (asked.backdrop_toggled) {
    settings.SetBackdropWanted(asked.backdrop_wanted, targets);
    // 🔴 DÉCOCHER FERME LE DÉCOR SUR-LE-CHAMP, et ne se contente pas de le
    // retenir pour la prochaine fois : le joueur qui l'éteint le fait parce
    // que sa machine peine MAINTENANT.
    if (!asked.backdrop_wanted) {
      flow_.backdrop().Release();
      raise_ = BackdropRaise::kNo;
    } else {
      // 🔴 ET COCHER RALLUME, y compris quand le client a démarré sans : une
      // case qui se coche sans que rien ne change se lit comme une panne. Ce
      // que cela coûte — une seconde et demie de carte à monter — s'ANNONCE,
      // puis se paie. Voir `BackdropRaise`.
      raise_ = BackdropRaise::kAnnounced;
    }
  }
  if (asked.backdrop_reroll) {
    flow_.RelaunchBackdrop(static_cast<std::uint32_t>(SDL_GetTicks()));
  }
}

void LobbyScreen::Route(const ro::ui::LoginWindowAction& asked) {
  RouteFirstScreen(asked.first_screen);
  switch (asked.verb) {
    case ro::ui::LoginVerb::kNothing:
      break;
    case ro::ui::LoginVerb::kSubmit: {
      // ⚠ LE COMPTE SE RETIENT ICI, À L'ENVOI, et non à la frappe : ce que le
      // joueur retient est le compte avec lequel il SE CONNECTE, pas celui
      // qu'il a commencé à taper puis effacé.
      shell_.settings().options().SetLoginAccount(
          form_.remember_account ? std::string(form_.account.data()) : std::string());
      flow_.Submit(std::string(form_.account.data()), std::string(form_.password.data()));
      // 🔴 LE MOT DE PASSE S'EFFACE DU FORMULAIRE À L'INSTANT OÙ IL PART. Il ne
      // sert plus à rien — la demande le porte —, et le laisser dans un tampon
      // qui vit toute la session en ferait une copie de plus à oublier.
      form_.password.fill('\0');
      break;
    }
    case ro::ui::LoginVerb::kSubmitPincode:
      if (auto sent = flow_.SubmitPincode(std::string(form_.pincode.data())); !sent.has_value()) {
        ro::core::LogWarning("code PIN : {}", ro::core::Describe(sent.error()));
      }
      form_.pincode.fill('\0');
      break;
    case ro::ui::LoginVerb::kQuit:
      Quit();
      break;
  }
}

OptionTargets LobbyScreen::SettingsTargets() {
  return OptionTargets{.audio = &shell_.audio(),
                       .music = &shell_.music(),
                       .interface = &shell_.interface(),
                       .display = &shell_.gl(),
                       .file = &shell_.settings().file()};
}

// CE QUE LES RÉGLAGES DU LOGIN ONT CHANGÉ, rendu visible ET durable.
//
// ⚠ SUR TOUS LES ÉCRANS DU LOBBY, et pas seulement sous le formulaire : un
// affichage choisi juste avant de se connecter se confirme ou se défait à
// l'heure dite, tablée comprise, et le fichier s'écrit avant l'entrée en jeu —
// un plantage d'ici n'emporte pas ce qu'on a réglé.
void LobbyScreen::FollowSettings(std::uint32_t now_ms) {
  GameSettings& settings = shell_.settings();
  // ⚠ L'HABILLAGE EST COPIÉ PAR L'INTERFACE : il se repousse à chaque image,
  // comme en jeu, sinon la page Skin ne changerait rien sous les yeux.
  if (shell_.FollowSkinChoice()) {
    backdrop_.ForgetEffectTextures();
  }
  shell_.interface().SetSkinLook(settings.options().pages().skin_look.look());
  settings.TickDisplayRevert(SettingsTargets(), now_ms);
  settings.Tick(now_ms);
}

void LobbyScreen::ComposeCursor(std::uint32_t now_ms) {
  const GamePieces& pieces = shell_.pieces();
  if (pieces.cursor.sprite == nullptr) {
    return;
  }
  // ⚠ DEUX FORMES SEULEMENT : la flèche, et la main sur ce qui se clique. Les
  // autres annoncent ce qu'un clic ferait AU MONDE, et il n'y a pas de monde à
  // cliquer ici — la ville derrière est un décor, pas une carte.
  const ro::ui::InputGate gate = shell_.interface().gate();
  const ro::render::CursorShape shape =
      gate.pointing ? ro::render::CursorShape::kPoint : ro::render::CursorShape::kArrow;
  if (shape != cursor_shape_) {
    cursor_shape_ = shape;
    cursor_since_ms_ = now_ms;
  }
  cursor_quads_ = DrawnCursor(pieces.cursor, cursor_shape_, mouse_,
                              static_cast<float>(now_ms - cursor_since_ms_), mouse_pressed_);
}

void LobbyScreen::RaiseBackdrop(std::uint32_t now_ms) {
  // ⚠ AUCUNE CARTE DE REPLI D'AVANCE, comme à l'ouverture : l'illustration
  // tient l'écran jusqu'à ce que la ville du serveur arrive, et Prontera ne se
  // monte que si elle n'arrive pas. Le joueur qui décoche puis recoche retrouve
  // la carte déjà montée, s'il y en a une, sans rien payer.
  shell_.EnsureBackdropPieces();
  // ⚠ ET LA VILLE DU SERVEUR PAR-DESSUS : c'est le même geste que « Changer de
  // vue », et le seul autre que le joueur puisse demander.
  flow_.RelaunchBackdrop(now_ms);
}

ro::core::Result<void> LobbyScreen::Frame(std::uint32_t now_ms) {
  PumpEvents();
  // 🔴 UNE IMAGE ENTIÈRE SÉPARE L'ANNONCE DE L'APPEL QUI BLOQUE. `kAnnounced`
  // est posé pendant le DESSIN, donc trop tard pour l'image en cours ; la
  // suivante le montre, et celle d'après seulement paie la carte. Sans ce tour,
  // la fenêtre se figerait sur un formulaire qui a l'air de ne rien faire —
  // exactement ce que le joueur a pris pour une panne.
  if (raise_ == BackdropRaise::kNow) {
    raise_ = BackdropRaise::kNo;
    RaiseBackdrop(now_ms);
  } else if (raise_ == BackdropRaise::kAnnounced) {
    raise_ = BackdropRaise::kNow;
  }
  flow_.Advance(now_ms);
  FollowSettings(now_ms);
  // Ce que la dernière image des passants a constaté, rendu à leur monde hors du
  // dessin : voir `SettleWorld`.
  backdrop_.Settle(flow_.backdrop().world(), now_ms);
  web_.Advance(now_ms);
  OpenDiscordPage();
  // ⚠ DANS LA MÊME IMAGE QUE L'OBTENTION : le jeton est à usage unique et le
  // site en régénère un à chaque demande, donc rien ne gagne à attendre — et
  // l'écran n'a pas d'état « prêt » à montrer.
  if (web_.stage() == web::WebStage::kReady) {
    EnterWithWebToken();
  }

  // ⚠ LA LISTE EST ARRIVÉE : c'est le moment de monter ce que la tablée
  // demande. Sans effet aux images suivantes.
  if (flow_.screen() == lobby::LobbyScreenKind::kCharacters) {
    OpenBanquet();
  }

  // LE VOILE DE L'ENTRÉE EN JEU suit l'écran de la machine à états. La main,
  // elle, ne passe qu'en fin d'image : voir plus bas.
  veil_.Follow(flow_.screen() == lobby::LobbyScreenKind::kEntering ||
                   flow_.screen() == lobby::LobbyScreenKind::kHandoff,
               now_ms);

  // LA SCÈNE QUI SE PRÉPARE : voir `WatchBackdrop`.
  WatchBackdrop(now_ms);

  shell_.gl().BindForDisplay();
  // 🔴 TANT QU'AUCUNE SCÈNE N'EST PRÊTE, L'ÉCRAN DE CHARGEMENT SEUL. Le
  // formulaire n'apparaît qu'avec ce qui est derrière lui ;
  // et c'est cette image-ci, une fois présentée, qui autorise le montage à la
  // suivante.
  if (Loading()) {
    shell_.gl().ClearBoundTarget(kLoadingClearColor);
    shell_.interface().DrawLoadingOnly(ro::core::Tr("Chargement…"));
    loading_shown_ = pending_mount_.has_value();
    return {};
  }
  const float aspect_ratio =
      shell_.height() > 0 ? static_cast<float>(shell_.width()) / static_cast<float>(shell_.height())
                          : 1.0F;
  // 🔴 LE RÉGLAGE COMMANDE LE DESSIN, ET NON SEULEMENT LA SESSION. Décocher
  // « Ville en fond » rend la session du serveur — donc les passants — mais
  // la SCÈNE, elle, possède sa carte : elle continuerait de peindre la ville,
  // et le réglage paraîtrait n'avoir aucun effet. C'est la contrepartie du choix
  // qui rend ce décor insensible à la perte de session (voir `BackdropScene`) :
  // ce qu'on gagne d'un côté se paie de l'autre, et se paie ici.
  //
  // ⚠ ET IL FAUT UNE VILLE À MONTRER : cochée en cours de route, la case ne
  // monte pas une carte — ce serait une seconde et demie de fenêtre figée au
  // milieu d'une saisie. Elle reprend celle qui est là, ou attend le
  // prochain lancement. Voir l'infobulle du réglage.
  const bool show_city = ShowingCity();

  // ⚠ LA MUSIQUE SUIT LA VILLE QU'ON REGARDE — donc rien quand on n'en
  // regarde aucune. Jouer le thème d'un lieu qui n'est pas à l'écran
  // nommerait un endroit où l'on n'est pas.
  //
  // ⚠ MUETTE, ELLE NE SE CHERCHE MÊME PAS : le fichier vit hors de l'archive,
  // et le lire pour ne rien en faire coûterait un accès disque par ville.
  if (shell_.audio().music_audible()) {
    shell_.music().Follow(shell_.audio(),
                          show_city ? std::string(backdrop_.map_name()) : std::string());
  }

  if (!show_city) {
    // ⚠ EFFACER RESTE NÉCESSAIRE, MÊME SOUS L'ILLUSTRATION : c'est le terrain
    // qui efface d'ordinaire, et ne rien peindre laisserait la dernière image à
    // l'écran pour toujours. L'illustration, elle, se peint AVEC l'interface —
    // voir `LoginWindowView::backdrop` — donc bien après ce nettoyage, et elle
    // manquerait s'il n'était pas fait.
    shell_.gl().ClearBoundTarget(backdrop_.background());
  } else {
    // ⚠ LA SCÈNE REFUSE ELLE-MÊME LES PASSANTS D'UNE AUTRE VILLE : on lui donne
    // le monde et le nom de la carte où il se trouve, et elle compare. Voir
    // `BackdropScene::Draw`.
    ro::render::ActorWardrobe* wardrobe =
        shell_.pieces().wardrobe.has_value() ? &*shell_.pieces().wardrobe : nullptr;
    // 🔴 LA PAGE « GRAPHICS » SE RELIT À CHAQUE IMAGE, comme en partie : la
    // ville est l'aperçu de ce que le joueur règle ici. Voir `BackdropLens`.
    const GraphicsOptions& graphics = shell_.settings().options().graphics();
    const ro::render::DrawableSize surface = shell_.gl().drawable_size();
    lens_.Begin(shell_.gl(), graphics);
    backdrop_.Draw(aspect_ratio, now_ms - opened_ms_, wardrobe, &flow_.backdrop().world(),
                   flow_.backdrop().map_name(), now_ms, &shell_.audio(),
                   static_cast<float>(surface.width), static_cast<float>(surface.height), graphics);
    // LE POST-TRAITEMENT PUIS LA VIGNETTE, ENTRE LA VILLE ET LE FORMULAIRE.
    //
    // ⚠ LA VIGNETTE N'HABILLE QUE LA VILLE VIVANTE, et c'est une conséquence
    // de l'ordre des couches, pas un choix : l'illustration de repli se peint
    // avec l'interface, sur la liste de fond d'ImGui, donc APRÈS cette passe.
    // Poser la teinte après l'interface assombrirait le formulaire avec elle.
    lens_.Finish(shell_.gl(), now_ms);
  }

  // ⚠ L'UN OU L'AUTRE, JAMAIS LES DEUX : chacun ouvre et ferme la même image
  // d'ImGui. Et la ville reste derrière la tablée — la salle du banquet est
  // opaque, mais un écran de 16/9 la montre déformée plutôt que rognée, si
  // bien qu'elle couvre bien tout le cadre.
  switch (flow_.screen()) {
    case lobby::LobbyScreenKind::kCharacters:
    case lobby::LobbyScreenKind::kEntering:
    case lobby::LobbyScreenKind::kHandoff: {
      const ro::ui::CharacterSelectView seats = ComposeSeats(now_ms);
      const ro::ui::CharacterCreateView newcomer = ComposeNewcomer(newcomer_asked_, now_ms);
      const ro::ui::CharacterDeleteView doomed = ComposeDoomed();
      const UiStage::LobbySeatsAnswers answers = shell_.interface().DrawCharacters(
          seats_, newcomer_, doomed_,
          UiStage::LobbySeatsFrame{.seats = &seats, .create = &newcomer, .remove = &doomed});
      // ⚠ CE QUE LA CRÉATION DEMANDE SE GARDE POUR L'IMAGE SUIVANTE : on ne
      // sait quelles cases de grille sont visibles qu'après les avoir
      // parcourues, donc après que `app` a eu l'occasion de composer.
      newcomer_asked_ = answers.create;
      RouteSeats(answers.seats);
      RouteCreate(answers.create);
      RouteDelete(answers.remove);
      break;
    }
    default: {
      // 🔴 LA LISTE ARRIVE AVEC UN COMPTE DÉJÀ DÉSIGNÉ, et c'est ce qui réduit
      // un retour au jeu à une seule touche. `preferred_account` prend le plus
      // récemment joué PARMI CEUX QUI SE JOUENT — jamais un compte occupé,
      // dont la conséquence se lirait au-dessus du bouton sans que personne
      // l'ait lue.
      //
      // ⚠ UNE SEULE FOIS, À L'ARRIVÉE : `chosen` vaut -1 tant que la liste
      // n'est pas là, et le reposer à chaque image écraserait le choix du
      // joueur à l'instant où il désigne autre chose.
      //
      // ⚠ ET ICI, PAS DANS `Compose` : celle-ci rend une VUE et ne touche à
      // rien — c'est ce qui permet de la lire sans se demander ce qu'elle
      // change.
      if (form_.web.chosen < 0 && !web_.accounts().empty()) {
        form_.web.chosen = web_.preferred_account();
        form_.web.scroll_to_chosen = true;
      }
      // LE NOM DE LA VILLE DU DÉCOR, comme à l'arrivée en jeu — seulement
      // quand elle se voit, et que le joueur veut le bandeau.
      GamePieces& pieces = shell_.pieces();
      const std::optional<ro::ui::MapSignView> sign =
          show_city && shell_.settings().options().pages().map_sign.mode() != MapSignMode::kNever
              ? pieces.map_sign.Stage(
                    pieces.game_images.has_value() ? &*pieces.game_images : nullptr, now_ms)
              : std::nullopt;
      const ro::ui::LoginWindowView form = Compose();
      // ⚠ LA CONNEXION LES REFERME : ils n'appartiennent qu'à cet écran-ci.
      if (form.stage == ro::ui::LoginStage::kConnecting) {
        settings_.Close();
      }
      const std::span<const ro::ui::OptionSectionView> pages =
          settings_.Compose(shell_.settings(), now_ms);
      ro::ui::OptionsWindowAction settings_asked;
      const ro::ui::LoginWindowAction asked =
          shell_.interface().DrawLobby(form_, form, sign.has_value() ? &*sign : nullptr,
                                       settings_.window(), pages, &settings_asked);
      if (asked.open_settings) {
        settings_.Open();
      }
      settings_.Route(settings_asked, shell_.settings(), SettingsTargets(), now_ms);
      Route(asked);
      RouteWebDoor(asked.web);
      break;
    }
  }

  // LE CURSEUR APRÈS TOUT, y compris après la fenêtre : il n'y en a qu'un dans
  // ce client, et c'est celui-ci.
  ComposeCursor(now_ms);
  if (!cursor_quads_.empty()) {
    shell_.pieces().overlay.Draw(cursor_quads_, static_cast<float>(shell_.width()),
                                 static_cast<float>(shell_.height()));
    cursor_quads_.clear();
  }

  // 🔴 LA MAIN PASSE EN FIN D'IMAGE, SUR UNE IMAGE PLEINEMENT NOIRE. C'est
  // celle-ci que la coquille présente, et elle reste à l'écran tout le
  // temps que la carte se charge en bloquant. Rendre la main plus tôt, sans
  // dessiner, laisserait la tablée figée à l'écran.
  veil_.Drawn(now_ms, ro::ui::kEnterFadeSeconds);
  if (flow_.screen() == lobby::LobbyScreenKind::kHandoff && veil_.covering()) {
    HandOff();
  }
  return {};
}

void LobbyScreen::HandOff() {
  // ENTRÉ. On compose le monde de départ avec la MÊME porte que la voie par
  // variables d'environnement — voir `SpawnPlayer` —, et l'on rend la main.
  if (const ro::proto::CharacterInfo* chosen = flow_.chosen_character(); chosen != nullptr) {
    const MapEntry entry =
        ComposeMapEntry(*chosen, flow_.account(), flow_.map_name(), flow_.spawn());
    map_name_ = entry.map_name;
    SpawnPlayer(entry, world_);
    ro::core::LogInfo("entré sur {} en ({}, {})", entry.map_name, entry.spawn.x, entry.spawn.y);
    // ⚠ NOTÉ AVANT DE PARTIR : c'est le seul endroit où l'on sait à la fois
    // QUI part et que le départ a bien lieu. Un échec d'écriture ne retient
    // personne — on perd la commodité, pas la partie.
    static_cast<void>(
        NoteRecentCharacter(shell_.client_root(), static_cast<std::uint32_t>(chosen->GID)));
    // ⚠ LA LIAISON SE PREND MAINTENANT, PAS APRÈS LA BOUCLE : ce qui vient
    // ensuite est `Close()`, qui rend la session du décor en fermant tout.
    // Une fois déplacée ici, elle ne lui appartient plus. Voir `TakeLink`.
    // ⚠ DE QUOI REVENIR À LA TABLÉE, pris ici : `Close` rendra la session qui
    // le tient.
    comeback_ = flow_.comeback();
    link_ = flow_.TakeLink();
    ticket_ = ro::net::ZoneTicket{.account = entry.account,
                                  .character_id = static_cast<std::uint32_t>(chosen->GID)};
    entered_ = true;
  } else {
    // 🔴 UNE SORTIE MUETTE EST LA PIRE DES PANNES : sans personnage à composer,
    // l'écran rend la main, `main` voit « le joueur a fermé la fenêtre » et
    // sort avec le code zéro. Sans cette ligne, le client disparaîtrait en
    // silence au clic sur un personnage, sans rien au journal ni un rapport à
    // envoyer.
    ro::core::LogError(
        "entrée en jeu : le serveur a rendu la main sans personnage choisi — "
        "rien à composer, le client s'arrête");
  }
  running_ = false;
}

void LobbyScreen::Close() {
  // 🔴 LE DÉCOR SE REND EN PARTANT, même si le joueur n'est pas entré : une
  // session spectateur que personne ne regarde tient la seule place que le
  // serveur accorde à cette adresse. `LobbyFlow` la rend déjà à la connexion ;
  // ce passage couvre l'autre sortie, celle où l'on ferme la fenêtre.
  flow_.Release();
  // ⚠ FERMER LA FENÊTRE NE PASSE PAR AUCUN BOUTON : c'est la dernière
  // occasion de graver ce que le joueur a placé.
  SaveLayoutIfDirty();
  form_.password.fill('\0');
  form_.pincode.fill('\0');
}

}  // namespace moonclient
