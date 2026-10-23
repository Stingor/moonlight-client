// CE QUE LE MENU D'ÉCHAP ET LES MACROS DÉCLENCHENT.
//
// ⚠ À PART DE LA BOUCLE, et pour la raison qui a déjà sorti le dessin du monde :
// `game.cpp` orchestre les étapes d'une image, il ne décide pas de ce qu'un
// bouton fait. Ces gestes-là parlent au SERVEUR ou au fichier de réglages, et
// c'est le seul point commun qui les rassemble.

#include <array>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/net/action.h"
#include "ro/net/character_select_return.h"

#include "chat/chat_view.h"
#include "chat/macro_commands.h"
#include "game/connected_session.h"
#include "game/game.h"
#include "game/mvp_alert_sound.h"

namespace moonclient {

ro::ui::SessionRights Game::Rights() const noexcept {
  return ro::ui::SessionRights{
      .staff_tools = ro::ui::StaffToolsAllowed(world_.group_level(), link_.is_open())};
}

bool Game::CanResurrect() const {
  // ⚠ LES DEUX VOIES DU SERVEUR, RECOPIÉES DE `pc_revive_item` : un objet du
  // groupe TOKEN_OF_SIEGFRIED, ou l'état de régénération. N'en regarder qu'une
  // laisserait le bouton absent là où le geste marcherait.
  //
  // 🔴 LES TROIS IDENTIFIANTS SONT CEUX DU GROUPE DU SERVEUR, relevés dans
  // `db/re/item_group_db.yml` — ni devinés, ni réduits au plus connu. Le
  // « F_ » et le « E_ » sont les versions d'événement, et un joueur qui n'a que
  // celles-là peut se relever tout autant.
  constexpr std::array<std::uint32_t, 3> kSiegfriedTokens = {6293, 6316, 7621};
  for (const ro::sim::InventoryItem& item : world_.inventory().items()) {
    if (std::ranges::find(kSiegfriedTokens, item.item_id) != kSiegfriedTokens.end()) {
      return true;
    }
  }
  // ⚠ LE NUMÉRO VIENT DE L'ÉNUMÉRATION `efst_type` DU SERVEUR — seule autorité
  // sur les identifiants d'état, comme pour les altérations de
  // `status_effects.h`.
  constexpr ro::sim::StatusEffectId kEfstLightOfRegene = 580;
  for (const ro::sim::ActiveStatus& status : world_.player_statuses()) {
    if (status.effect == kEfstLightOfRegene) {
      return true;
    }
  }
  return false;
}

void Game::ApplyMenuGesture(ro::ui::GameMenuChoice chosen, std::uint32_t now_ms) {
  switch (chosen) {
    case ro::ui::GameMenuChoice::kSavePointCommand:
      // 🔴 UNE COMMANDE, PAS UN PAQUET, et elle part par le chemin de la barre
      // de saisie : rien dans le protocole ne téléporte un personnage DEBOUT.
      // Le détour a un avantage — un refus du serveur revient dans la
      // conversation, à sa place, au lieu de disparaître.
      SubmitChat(*chat_dispatch_, std::string(kSavePointChatCommand), now_ms);
      break;
    case ro::ui::GameMenuChoice::kRespawn:
      // Celle-ci EST un paquet, et le serveur ne la sert qu'à un mort.
      if (link_.is_open()) {
        ReportRefusal(ro::net::RequestRespawn(link_));
      }
      break;
    case ro::ui::GameMenuChoice::kResurrect:
      if (link_.is_open()) {
        ReportRefusal(ro::net::RequestAutoRevive(link_));
      }
      break;
    case ro::ui::GameMenuChoice::kCharacterSelect:
      // ⚠ UNE SEULE DEMANDE À LA FOIS : deux clics enverraient deux requêtes,
      // et la seconde partirait vers un serveur qui a déjà rendu la main.
      if (link_.is_open() && !character_select_asked_ms_.has_value()) {
        ReportRefusal(ro::net::RequestCharacterSelect(link_));
        character_select_asked_ms_ = now_ms;
      }
      break;
    case ro::ui::GameMenuChoice::kNothing:
    case ro::ui::GameMenuChoice::kBackToGame:
    case ro::ui::GameMenuChoice::kOptions:
    case ro::ui::GameMenuChoice::kShortcuts:
    case ro::ui::GameMenuChoice::kChatMacros:
    case ro::ui::GameMenuChoice::kStyleEditor:
    case ro::ui::GameMenuChoice::kMusicPlayer:
    case ro::ui::GameMenuChoice::kMinigames:
    case ro::ui::GameMenuChoice::kStaffTools:
    case ro::ui::GameMenuChoice::kQuit:
      // Les autres ne parlent à personne : les réglages les ont déjà joués.
      break;
  }
}

bool Game::FollowCharacterSelect(const ro::net::Frame& received, std::uint32_t now_ms) {
  if (!character_select_asked_ms_.has_value()) {
    return false;
  }
  switch (ro::net::CharacterSelectAnswerOf(received)) {
    case ro::net::CharacterSelectAnswer::kNone:
      return false;
    case ro::net::CharacterSelectAnswer::kGranted:
      // 🔴 LA MAIN PASSE AU SERVEUR DE PERSONNAGES : la partie s'arrête ici, et
      // la boucle rouvre la tablée avec les jetons de la connexion.
      back_to_characters_ = true;
      running_ = false;
      return true;
    case ro::net::CharacterSelectAnswer::kRefused:
      break;
  }
  // ⚠ LE REFUS SE DIT, sinon le bouton aurait l'air mort. Le serveur ne dit
  // pas pourquoi : ce sont ses deux seules raisons.
  character_select_asked_ms_.reset();
  world_.RecordChat(ro::sim::ChatKind::kServer, 0,
                    ro::core::Tr("Impossible de revenir à la sélection du personnage maintenant "
                                 "(combat récent ou personnage caché)."),
                    now_ms);
  return false;
}

void Game::ExpireCharacterSelect(std::uint32_t now_ms) {
  // Dix secondes : le serveur répond en quelques millisecondes, et au-delà le
  // joueur doit pouvoir redemander.
  constexpr std::uint32_t kCharacterSelectPatienceMs = 10000;
  if (!character_select_asked_ms_.has_value() || back_to_characters_ ||
      now_ms - *character_select_asked_ms_ < kCharacterSelectPatienceMs) {
    return;
  }
  character_select_asked_ms_.reset();
  world_.RecordChat(ro::sim::ChatKind::kServer, 0,
                    ro::core::Tr("Le serveur n'a pas répondu au retour à la sélection du "
                                 "personnage."),
                    now_ms);
}

ro::ui::GameMenuFacts Game::MenuFacts() const noexcept {
  const ro::sim::Actor* self = world_.player();
  // ⚠ LIAISON PERDUE, PERSONNE N'EST MORT : une disposition de mort bloquerait
  // le menu sur deux boutons dont aucun ne pourrait plus rien demander au
  // serveur, et le joueur n'aurait plus que la fermeture de la fenêtre.
  const bool dead = link_.is_open() && self != nullptr && self->posture == ro::sim::Posture::kDead;
  return ro::ui::GameMenuFacts{.staff = Rights().staff_tools,
                               .online = link_.is_open(),
                               .dead = dead,
                               .can_resurrect = dead && CanResurrect()};
}

void Game::ApplyCaptureGesture(const UiStage::Answers& answers, std::uint32_t now_ms) {
  // LA ZONE QUI VIENT D'ÊTRE TRACÉE. ⚠ ELLE S'ÉCRIT DANS LE FICHIER SUR-LE-CHAMP :
  // c'est le seul geste de l'outil qui recouvre l'écran, et le refaire à chaque
  // lancement est précisément ce qu'on cherche à éviter.
  if (const std::optional<ro::ui::ZoneRect> drawn = recorder_.Note(answers.capture);
      drawn.has_value()) {
    if (settings_->options().NoteCaptureZone(*drawn)) {
      settings_->file().Touch();
    }
  }

  // 🔴 L'IDENTIFIANT SE TRADUIT, IL NE SE TRANSTYPE PAS. Ce `switch` recevait un
  // `static_cast` nu, et `GameOption` tient sur HUIT BITS : les identifiants
  // COMPOSÉS — ceux des jauges, des barres, des étiquettes — y étaient TRONQUÉS
  // et retombaient sur une option valable. Cocher « Montrer pour cette espèce »
  // sur le premier onglet des étiquettes (numéro 4000) déclenchait ainsi
  // l'enregistrement d'un film, et la ligne d'à côté prenait une capture.
  // LE BOUTON « OUVRIR L'ALBUM » de sa page : il ouvre, il ne referme pas.
  if (answers.options.changed == CardAlbumRowId(CardAlbumRow::kOpen) && !windows_.card_album.open) {
    card_album_.Toggle(windows_.card_album, link_, now_ms);
  }
  // CEUX DU CARNET DE CHASSE MVP : « Ouvrir le carnet » ouvre un carnet allumé,
  // il ne referme pas ; « Tout remettre au carnet » ne demande rien.
  // « Écouter » joue le son choisi, comme le jouerait l'alerte.
  if (answers.options.changed == MvpTrackerRowId(MvpTrackerRow::kOpen)) {
    pieces_.mvp_tracker.RequestOpen();
  } else if (answers.options.changed == MvpTrackerRowId(MvpTrackerRow::kPutBackAll)) {
    pieces_.mvp_tracker.PutBackAllLines();
  } else if (answers.options.changed == MvpTrackerRowId(MvpTrackerRow::kAlertListen)) {
    MvpTrackerOptions& page = settings_->options().pages().mvp_tracker;
    const MvpTrackerSettings hunt = page.settings();
    static_cast<void>(PlayMvpAlertSound({.audio = audio_,
                                         .vfs = *vfs_,
                                         .client_root = client_root_,
                                         .tone = hunt.alert_tone,
                                         .custom_file = hunt.alert_wav,
                                         .reported = &page.alert_failure_reported(),
                                         .custom_held = &page.alert_sound_held(),
                                         .trouble = &page.alert_trouble()}));
  }
  const std::optional<GameOption> named = NamedOption(answers.options.changed);
  if (!named.has_value()) {
    return;
  }
  switch (*named) {
    case GameOption::kCaptureZone:
      recorder_.AskZone();
      break;
    case GameOption::kCaptureRecord:
      recorder_.ToggleRecording(now_ms);
      break;
    case GameOption::kCapturePicture:
      recorder_.AskPicture();
      break;
    case GameOption::kStorageResetColumns:
      // ⚠ UNE DEMANDE POSÉE DANS LA FENÊTRE, que sa table consommera : elle
      // seule peut défaire un ordre qu'ImGui garde pour elle, et le joueur peut
      // la poser coffre fermé.
      windows_.storage.reset_columns = true;
      break;
    case GameOption::kInventoryForgetPlaces:
      // ⚠ ICI ET NON DANS `GameOptions` : la table vit dans SON fichier, et
      // c'est `GameSettings` qui sait où elle s'écrit.
      settings_->ForgetItemPlaces();
      break;
    default:
      // ⚠ UN CAS PAR DÉFAUT ICI, À REBOURS DU RESTE DU DÉPÔT : ce n'est pas un
      // aiguillage sur toutes les options, c'est un FILTRE sur trois d'entre
      // elles. Les énumérer toutes pour ne rien en faire ferait cent lignes
      // qu'il faudrait rallonger à chaque réglage ajouté ailleurs.
      break;
  }
}

void Game::ApplyMacroAnswers(const ro::ui::ChatMacroAsk& asked, std::uint32_t now_ms) {
  if (asked.changed) {
    settings_->NoteChatMacros(windows_);
  }
  if (asked.open_hotkeys) {
    windows_.hotkeys.open = true;
    windows_.hotkeys.tab_request = static_cast<int>(HotkeyTab::kMacros);
  }
  // LES VALEURS D'USINE : les dix commandes d'émote dont le client remplit ses
  // lignes. ⚠ ELLES REMONTENT DANS LE FICHIER COMME UNE FRAPPE, par le même
  // chemin — sans quoi elles seraient perdues au prochain démarrage.
  if (asked.reset) {
    for (std::size_t slot = 0; slot < ro::ui::kChatMacroCount; ++slot) {
      const std::string factory_default = ChatMacroDefault(slot);
      std::snprintf(windows_.macros.rows[slot].data(), windows_.macros.rows[slot].size(), "%s",
                    factory_default.c_str());
    }
    settings_->NoteChatMacros(windows_);
  }
  // 🔴 UNE MACRO PART PAR LE MÊME CHEMIN QU'UNE LIGNE TAPÉE, et c'est ce qui
  // garde `/commande` et `@commande` identiques dans les deux cas. Un second
  // chemin d'envoi finirait par se comporter autrement sans que personne ne
  // sache dire pourquoi.
  if (asked.send >= 0 && static_cast<std::size_t>(asked.send) < ro::ui::kChatMacroCount) {
    const char* line = windows_.macros.rows[static_cast<std::size_t>(asked.send)].data();
    if (line[0] != '\0') {
      SubmitChat(*chat_dispatch_, std::string(line), now_ms);
    }
  }
}

}  // namespace moonclient
