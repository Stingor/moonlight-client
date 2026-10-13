// LA TABLÉE DU BANQUET : choisir un personnage, en créer un, en supprimer un.
//
// 🔴 MÊME CLASSE, AUTRE UNITÉ DE TRADUCTION, et c'est la seconde fois que
// `lobby_screen.cpp` franchit les 800 lignes. La coupure tombe juste : tout ce
// qui suit regarde des PERSONNAGES — une liste, des sièges, un pantin, une
// suppression en deux temps —, là où ce qui reste regarde une ville et un
// formulaire. Les deux moitiés ne se parlent que par la machine à états.
//
// ⚠ Le montage déjà employé pour la porte Moonlight (`lobby_web_door.cpp`), et
// trois fois dans la partie.

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "ro/core/log.h"
#include "ro/core/text.h"

#include "character/character_styles.h"
#include "character/recent_characters.h"
#include "chat/banned_input.h"
#include "shell/lobby_screen.h"

namespace moonclient {
namespace {

// LE PLAFOND DU COMPTE À REBOURS, en secondes : cent jours. Une date aberrante
// — horloge locale en retard de vingt ans, champ abîmé — afficherait sinon un
// nombre d'heures que rien ne rend lisible, et le personnage paraîtrait
// définitivement bloqué.
constexpr std::int64_t kLongestCountdownSeconds = 100LL * 24 * 3600;

// COMBIEN DE SECONDES AVANT QUE LA SUPPRESSION SOIT POSSIBLE, ou moins un
// quand elle n'est pas programmée.
//
// 🔴 `DelRevDate` EST ICI UNE DATE UNIX : le fil porte un délai restant, que
// `net` ancre à la réception. La comparer demande donc l'horloge du MUR, et
// non celle du programme : `now_ms` compte depuis le lancement du client, et
// l'employer ici donnerait une échéance à quelques décennies.
//
// ⚠ ET LES DEUX HORLOGES PEUVENT DIVERGER — la nôtre et celle du serveur. Le
// compte à rebours est donc une INDICATION ; le serveur, lui, tranche. C'est
// pourquoi son refus se dit au lieu d'être présumé impossible.
[[nodiscard]] int DeleteCountdown(std::int32_t delete_date) {
  if (delete_date <= 0) {
    return -1;
  }
  const auto now = static_cast<std::int64_t>(std::time(nullptr));
  const std::int64_t left = static_cast<std::int64_t>(delete_date) - now;
  return left <= 0 ? 0 : static_cast<int>(std::min<std::int64_t>(left, kLongestCountdownSeconds));
}

// LE PLUS HAUT EMPLACEMENT OCCUPÉ, ou moins un si personne ne l'est.
//
// ⚠ IL PEUT DÉPASSER CE QUE LE COMPTE PEUT REMPLIR : `char_slots` se réduit
// quand un abonnement expire, et les personnages qui étaient au-delà RESTENT —
// le serveur les envoie toujours, et son propre client les marque en rouge. La
// scène doit donc les asseoir, faute de quoi ils disparaîtraient purement et
// simplement.
[[nodiscard]] int HighestSlot(const std::vector<ro::proto::CharacterInfo>& characters) {
  int highest = -1;
  for (const ro::proto::CharacterInfo& character : characters) {
    highest = std::max(highest, static_cast<int>(character.CharNum));
  }
  return highest;
}

}  // namespace

void LobbyScreen::OpenBanquet() {
  if (dolls_.has_value()) {
    return;
  }
  // ⚠ ICI ET NON À L'OUVERTURE DE L'ÉCRAN : c'est le plus long chargement du
  // client — six à dix secondes —, et le formulaire doit paraître tout de
  // suite. Le joueur vient de cliquer « Se connecter » : c'est le seul moment
  // du démarrage où une attente s'explique d'elle-même.
  const std::uint64_t started_ms = SDL_GetTicks();
  shell_.EnsureWorldPieces();
  ro::core::LogInfo("chargement pièces du monde : {} ms", SDL_GetTicks() - started_ms);
  if (!shell_.pieces().wardrobe.has_value()) {
    // ⚠ PAS UNE PANNE : sans garde-robe la tablée se dessine sans ses pantins,
    // et les noms suffisent à choisir. C'est la règle des accessoires.
    ro::core::LogWarning("tablée : pas de garde-robe, les convives resteront en silhouette");
    return;
  }
  dolls_.emplace(*shell_.pieces().wardrobe);
  dolls_->UseStyles(LoadCharacterStyles(shell_.client_root()));
  newcomer_dolls_.emplace(*shell_.pieces().wardrobe);
  PreferLastPlayed();
}

// 🔴 LA TABLÉE S'OUVRE SUR LE DERNIER PERSONNAGE JOUÉ, et c'est ce qui réduit
// une reconnexion à une touche. Sans mémoire, elle s'ouvrirait sur le premier
// siège occupé — ce qui a l'air juste, et oblige pourtant à viser le bon avant
// chaque entrée en jeu.
//
// ⚠ PAR L'EMPLACEMENT, PAS PAR LE RANG : `seats_.selected` est un emplacement,
// et la liste est dense. Les deux coïncident tant qu'aucun personnage n'a été
// supprimé, c'est-à-dire sur tous les comptes d'essai.
void LobbyScreen::PreferLastPlayed() {
  if (seats_.selected >= 0) {
    return;
  }
  std::vector<std::uint32_t> present;
  present.reserve(flow_.characters().size());
  for (const ro::proto::CharacterInfo& listed : flow_.characters()) {
    present.push_back(static_cast<std::uint32_t>(listed.GID));
  }
  const int rank = PreferredCharacter(LoadRecentCharacters(shell_.client_root()), present);
  if (rank >= 0) {
    seats_.selected = flow_.characters()[static_cast<std::size_t>(rank)].CharNum;
  }
}

void LobbyScreen::OpenCreate(int slot) {
  // ⚠ UN FORMULAIRE NEUF À CHAQUE OUVERTURE, et le nom d'abord : garder celui
  // d'une tentative refusée le ferait repartir tel quel au clic suivant, sur
  // un emplacement peut-être différent.
  newcomer_ = ro::ui::CharacterCreateState{};
  newcomer_.slot = slot;
  // Le sexe du COMPTE par défaut : c'est le choix le plus probable, et il reste
  // modifiable.
  newcomer_.sex = flow_.account().sex <= 1 ? flow_.account().sex : 1;
  newcomer_asked_ = ro::ui::CharacterCreateAction{};
  newcomer_banned_ = LoadBannedWords(&shell_.vfs());
  newcomer_refusal_.clear();
  flow_.ForgetCreation();
}

void LobbyScreen::CloseCreate() {
  newcomer_ = ro::ui::CharacterCreateState{};
  newcomer_refusal_.clear();
  newcomer_asked_ = ro::ui::CharacterCreateAction{};
  if (newcomer_dolls_.has_value()) {
    newcomer_dolls_->Close();
  }
  flow_.ForgetCreation();
}

ro::ui::CharacterSelectView LobbyScreen::ComposeSeats(std::uint32_t now_ms) {
  ro::ui::CharacterSelectView view;
  // ⚠ LA REMISE AUSSI : elle attend que le voile soit complet, et la scène
  // se dessine encore — inerte — pendant ce temps.
  view.busy = flow_.screen() == lobby::LobbyScreenKind::kEntering ||
              flow_.screen() == lobby::LobbyScreenKind::kHandoff;
  view.entering_seconds = veil_.seconds(now_ms);
  view.message = flow_.message();
  // ⚠ LE REFUS DE SUPPRESSION PREND LE BANDEAU, sauf quand c'est la fenêtre
  // de confirmation qui le montre déjà : le dire deux fois au même instant
  // ferait chercher deux problèmes là où il n'y en a qu'un.
  if (flow_.deletion() == ro::net::DeletionState::kRefused && !doomed_.open()) {
    view.message = flow_.deletion_refusal();
  }
  // LA SALLE ET LA PERSONNALISATION : voir `lobby_layout.cpp`.
  ComposeLayout(view);

  // 🔴 LA VUE EST INDEXÉE PAR EMPLACEMENT, PAS PAR RANG DE LISTE, et c'est ce
  // qui fait qu'un personnage s'assoit à SA place. Le serveur envoie la liste
  // DENSE ; les emplacements, eux, ont des trous dès la première suppression.
  // Recopier la liste dans l'ordre mettrait le troisième personnage au
  // troisième siège quel que soit son emplacement — et le clic enverrait alors
  // au serveur un emplacement où il n'y a personne.
  const std::vector<ro::proto::CharacterInfo> characters = flow_.characters();
  const int slot_count = std::max<int>(flow_.producible_slots(), HighestSlot(characters) + 1);
  view.slots.resize(static_cast<std::size_t>(std::max(slot_count, 0)));
  view.creatable_slots = flow_.producible_slots();
  for (const ro::proto::CharacterInfo& character : characters) {
    if (character.CharNum >= view.slots.size()) {
      continue;
    }
    ro::ui::CharacterSlotView& seat = view.slots[character.CharNum];
    seat.occupied = true;
    // ⚠ LE NOM VIENT DU FIL DANS LA PAGE DE CODE DU SERVEUR : la même
    // conversion qu'à l'entrée en jeu, sans quoi un accent s'afficherait en
    // deux octets perdus.
    seat.name = ro::core::Utf8FromCp1252(character.name);
    seat.base_level = character.level;
    seat.job_level = character.joblevel;
    seat.character_id = character.GID;
    seat.delete_in_seconds = DeleteCountdown(character.DelRevDate);
  }

  // 🔴 LES PANTINS SE COMPOSENT PAR SIÈGE, ET SEULEMENT POUR LA PAGE VISIBLE :
  // une vignette ne retient qu'une texture, et composer les quarante-cinq
  // emplacements d'un compte chargé en recomposerait vingt à chaque image.
  if (dolls_.has_value()) {
    const auto display_width = static_cast<float>(shell_.width());
    const auto display_height = static_cast<float>(shell_.height());
    for (int seat = 0; seat < ro::ui::kSeatCount; ++seat) {
      const int slot = ro::ui::SeatedSlot(seat, seats_.page);
      const int index = ro::net::IndexOfSlot(characters, static_cast<std::uint8_t>(slot));
      if (slot < 0 || slot >= static_cast<int>(view.slots.size()) || index < 0) {
        continue;
      }
      // ⚠ LE SIÈGE DE LA MISE EN PAGE VIVANTE, pose comprise : c'est celui que
      // la scène dessine, et un pantin composé ailleurs ne tomberait plus dans
      // son cadre.
      const ro::ui::CharacterSeat& place = seats_.layout.seats[static_cast<std::size_t>(seat)];
      const ro::ui::SeatFrame frame = ro::ui::SeatFrameAt(place, display_width, display_height);
      const BanquetDolls::Seated seated =
          dolls_->Seat(seat, characters[static_cast<std::size_t>(index)], flow_.account().sex,
                       place, frame, now_ms - opened_ms_);
      view.slots[static_cast<std::size_t>(slot)].doll = seated.image;
      view.seat_frames[static_cast<std::size_t>(seat)] = seated.frames;
    }
  }
  return view;
}

ro::ui::CharacterCreateView LobbyScreen::ComposeNewcomer(const ro::ui::CharacterCreateAction& asked,
                                                         std::uint32_t now_ms) {
  ro::ui::CharacterCreateView view;
  if (!newcomer_.open()) {
    return view;
  }
  if (newcomer_dolls_.has_value()) {
    view = newcomer_dolls_->Compose(newcomer_, asked, now_ms - opened_ms_);
  }
  view.pending = flow_.creation() == ro::net::CreationState::kPending;
  if (flow_.creation() == ro::net::CreationState::kRefused) {
    view.refusal = flow_.creation_refusal();
  }
  if (!newcomer_refusal_.empty()) {
    view.refusal = newcomer_refusal_;
  }
  return view;
}

void LobbyScreen::RouteCreate(const ro::ui::CharacterCreateAction& asked) {
  // 🔴 LA RÉUSSITE FERME LA FENÊTRE, ET C'EST ICI QU'ON LA LIT. Le serveur
  // n'accuse pas la demande : il pousse le personnage. Sans cette lecture, le
  // formulaire resterait ouvert sur un « Création en cours… » définitif,
  // devant une tablée qui porte déjà le nouveau venu.
  if (flow_.creation() == ro::net::CreationState::kCreated) {
    const int created = flow_.created_slot();
    CloseCreate();
    // ⚠ LE NOUVEAU VENU EST DÉSIGNÉ : c'est celui qu'on vient de faire, et le
    // geste suivant est d'entrer avec lui.
    if (created >= 0) {
      seats_.selected = created;
    }
    return;
  }

  switch (asked.verb) {
    case ro::ui::CharacterCreateVerb::kNothing:
      break;
    case ro::ui::CharacterCreateVerb::kSubmit: {
      // 🔴 UN NOM QUI PORTE UNE BALISE DE LIEN D'OBJET OU UN MOT INTERDIT NE
      // PART PAS : le client le refuse lui-même, et le serveur n'en entend rien.
      if (std::optional<std::string> refused =
              CharacterNameRefusal(newcomer_banned_, newcomer_.name.data());
          refused.has_value()) {
        newcomer_refusal_ = std::move(*refused);
        break;
      }
      newcomer_refusal_.clear();
      ro::net::CharacterDraft draft;
      // ⚠ LE NOM REPART DANS LA PAGE DE CODE DU SERVEUR : la saisie d'ImGui est
      // en UTF-8, et le fil ne l'est pas. Sans cette conversion, un accent
      // partirait en deux octets et le serveur refuserait un nom que le joueur
      // voit juste.
      draft.name = ro::core::Cp1252FromUtf8(std::string(newcomer_.name.data()));
      draft.slot = static_cast<std::uint8_t>(newcomer_.slot);
      draft.hair_style = static_cast<std::uint16_t>(newcomer_.hair_style);
      draft.hair_palette = static_cast<std::uint16_t>(newcomer_.hair_palette);
      draft.sex = newcomer_.sex;
      flow_.CreateCharacter(draft);
      break;
    }
    case ro::ui::CharacterCreateVerb::kCancel:
      CloseCreate();
      break;
  }
}

ro::ui::CharacterDeleteView LobbyScreen::ComposeDoomed() const {
  ro::ui::CharacterDeleteView view;
  if (!doomed_.open() || flow_.deletion_step() != ro::net::DeletionStep::kConfirm) {
    return view;
  }
  view.pending = flow_.deletion() == ro::net::DeletionState::kPending;
  if (flow_.deletion() == ro::net::DeletionState::kRefused) {
    view.refusal = flow_.deletion_refusal();
  }
  return view;
}

void LobbyScreen::OpenDelete(std::uint32_t character_id) {
  doomed_ = ro::ui::CharacterDeleteState{};
  doomed_.character_id = character_id;
  // 🔴 LE NOM SE RECOPIE, il ne se relit pas : la liste peut changer sous la
  // fenêtre — une création ailleurs, une réponse du serveur — et la question
  // « supprimer X ? » doit porter le nom de celui qu'on a désigné.
  for (const ro::proto::CharacterInfo& who : flow_.characters()) {
    if (who.GID == character_id) {
      doomed_.name = ro::core::Utf8FromCp1252(who.name);
      break;
    }
  }
  flow_.ForgetDeletion();
}

void LobbyScreen::CloseDelete() {
  // ⚠ LE SECRET S'EFFACE AVEC LA FENÊTRE, comme le mot de passe du
  // formulaire : c'est l'adresse du compte, et rien ne justifie qu'elle reste
  // dans un tampon qui vit toute la session.
  doomed_.secret.fill('\0');
  doomed_ = ro::ui::CharacterDeleteState{};
  flow_.ForgetDeletion();
}

void LobbyScreen::RouteDelete(const ro::ui::CharacterDeleteAction& asked) {
  // 🔴 LA RÉUSSITE FERME LA FENÊTRE, comme pour la création : le serveur
  // répond par un accusé VIDE, et sans cette lecture le formulaire resterait
  // sur « Suppression en cours… » devant une tablée dont le personnage a déjà
  // disparu.
  if (flow_.deletion_step() == ro::net::DeletionStep::kConfirm &&
      flow_.deletion() == ro::net::DeletionState::kDone) {
    CloseDelete();
    seats_.selected = -1;
    return;
  }
  switch (asked.verb) {
    case ro::ui::CharacterDeleteVerb::kNothing:
      break;
    case ro::ui::CharacterDeleteVerb::kConfirm:
      // ⚠ LE SECRET REPART DANS LA PAGE DE CODE DU SERVEUR, comme le nom d'un
      // nouveau-né : une adresse peut porter un accent.
      flow_.ConfirmDeletion(doomed_.character_id,
                            ro::core::Cp1252FromUtf8(std::string(doomed_.secret.data())));
      break;
    case ro::ui::CharacterDeleteVerb::kCancel:
      CloseDelete();
      break;
  }
}

void LobbyScreen::RouteSeats(const ro::ui::CharacterSelectAction& asked) {
  if (RouteLayout(asked)) {
    return;
  }
  switch (asked.verb) {
    case ro::ui::CharacterSelectVerb::kNothing:
    case ro::ui::CharacterSelectVerb::kBeginCustomizing:
    case ro::ui::CharacterSelectVerb::kEndCustomizing:
    case ro::ui::CharacterSelectVerb::kRescanBackgrounds:
    case ro::ui::CharacterSelectVerb::kOpenBackgroundFolder:
    case ro::ui::CharacterSelectVerb::kSavePreset:
    case ro::ui::CharacterSelectVerb::kApplyPreset:
    case ro::ui::CharacterSelectVerb::kDeletePreset:
      break;
    case ro::ui::CharacterSelectVerb::kReserveDeletion:
      flow_.ReserveDeletion(asked.character_id);
      break;
    case ro::ui::CharacterSelectVerb::kCancelDeletion:
      flow_.CancelDeletion(asked.character_id);
      break;
    case ro::ui::CharacterSelectVerb::kAskDeletion:
      // ⚠ RIEN NE PART ENCORE : ce verbe OUVRE la confirmation, où le secret
      // se saisit. Voir `character_delete_window.h`.
      OpenDelete(asked.character_id);
      break;
    case ro::ui::CharacterSelectVerb::kEnter:
      if (asked.slot >= 0) {
        // ⚠ LA MISE EN PAGE SE GRAVE EN PARTANT : l'écran s'en va, et avec lui
        // la dernière occasion. Et la personnalisation se referme : son panneau
        // n'a rien à faire sous le voile de l'entrée.
        SaveLayoutIfDirty();
        seats_.customizing = false;
        flow_.ChooseCharacter(static_cast<std::uint8_t>(asked.slot));
      }
      break;
    case ro::ui::CharacterSelectVerb::kCreate:
      OpenCreate(asked.slot);
      break;
    case ro::ui::CharacterSelectVerb::kBack: {
      SaveLayoutIfDirty();
      // ⚠ LA SÉLECTION ET LA PAGE S'OUBLIENT AVEC LA LISTE : au retour, une
      // autre connexion donnera d'autres personnages, et un emplacement retenu
      // désignerait quelqu'un d'autre. La MISE EN PAGE, elle, est celle de la
      // machine, et reste — mais la personnalisation se referme.
      ro::ui::SeatLayout kept = std::move(seats_.layout);
      seats_ = ro::ui::CharacterSelectState{};
      seats_.layout = std::move(kept);
      CloseCreate();
      CloseDelete();
      flow_.Back();
      break;
    }
    case ro::ui::CharacterSelectVerb::kQuit:
      Quit();
      break;
  }
}

void LobbyScreen::Quit() {
  SaveLayoutIfDirty();
  running_ = false;
}

}  // namespace moonclient
