#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/party_booking.h"
#include "ro/sim/world.h"
#include "ro/ui/party_booking_window.h"

#include "party/party_booking_places.h"

namespace ro::vfs {
class Vfs;
}  // namespace ro::vfs

namespace moonclient {

// ── LA RECHERCHE DE GROUPE ──────────────────────────────────────────────────
//
// Le modèle des annonces (`sim::PartyBooking`), la fenêtre qui les montre, et
// la case « Recherche de groupe » de la conversation, où s'écrivent les avis du
// serveur : une annonce publiée quand la liste est fermée, une publication ou
// un retrait refusés, une recherche vide.
//
// La fenêtre s'ouvre par l'icône de menu « booking » et par un bouton de la
// fenêtre de groupe.

// LES PIÈCES DE LA FENÊTRE dans les fenêtres de l'interface.
struct PartyBookingWindows {
  ro::ui::PartyBookingWindowState window;
  // La vue de l'image ; nulle avant la première avancée.
  const ro::ui::PartyBookingView* view = nullptr;
  // Ce que la fenêtre a demandé, lu à l'avancée suivante.
  ro::ui::PartyBookingAction answer;
  // UNE OUVERTURE DEMANDÉE — l'icône, le bouton du groupe —, lue à l'avancée
  // suivante.
  bool open_asked = false;
};

// L'ICÔNE DU MENU : ferme la fenêtre ouverte, en demande l'ouverture sinon.
void TogglePartyBooking(PartyBookingWindows& windows);

// La couleur des avis dans la conversation, en `0xRRGGBB` : le cyan du client
// d'origine.
inline constexpr std::uint32_t kPartyBookingChatColor = 0x00FFFF;

// Le texte d'un avis, déjà traduit.
[[nodiscard]] std::string PartyBookingNoticeText(ro::sim::PartyBookingNotice notice);

// L'heure locale d'une annonce, « 14:05 » ; vide si elle ne se convertit pas.
[[nodiscard]] std::string PartyBookingClockText(std::uint32_t unix_s);

// UN CHUCHOTEMENT DEMANDÉ depuis la liste : dans la conversation, ou dans la
// fenêtre de l'auteur.
struct PartyBookingWhisper {
  std::string name;
  bool own_window = false;
};

class PartyBookingBoard {
 public:
  // UNE NOUVELLE SESSION DE CARTE : le serveur oublie l'annonce d'un personnage
  // déconnecté, et la liste ne vaut plus.
  void StartSession();
  // La table des lieux, lue à la première demande. Absente, la fenêtre ne
  // propose aucun lieu et nomme ceux des annonces par leur code.
  void SeePlaces(ro::vfs::Vfs* vfs);
  void SetPlaces(PartyBookingPlaces places);

  // Lit une trame de la recherche de groupe, et écrit ses avis dans la
  // conversation.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                 std::uint32_t now_ms);

  // À CHAQUE IMAGE : l'ouverture demandée, l'intention de la fenêtre, la vue.
  // ⚠ ÉMET : une recherche, une publication, une modification ou un retrait.
  void Advance(PartyBookingWindows& windows, ro::net::Connection& link, ro::sim::World& world,
               std::uint32_t now_ms);

  // Le chuchotement demandé depuis la dernière fois.
  [[nodiscard]] std::optional<PartyBookingWhisper> TakeWhisper();

  [[nodiscard]] const ro::sim::PartyBooking& booking() const noexcept { return booking_; }

 private:
  void WriteNotices(ro::sim::World& world, std::uint32_t now_ms);
  void Obey(const ro::ui::PartyBookingAction& action, ro::net::Connection& link,
            const ro::sim::World& world);
  void ComposeTables();
  void Compose();
  [[nodiscard]] std::string PlaceText(std::int16_t map_code) const;
  [[nodiscard]] std::string JobsText(const ro::sim::PartyBookingJobs& jobs) const;

  ro::sim::PartyBooking booking_;
  bool list_shown_ = false;
  std::optional<PartyBookingPlaces> places_;
  bool places_tried_ = false;
  std::optional<PartyBookingWhisper> whisper_;

  // LA COMPOSITION : ce que la vue pointe.
  bool tables_composed_ = false;
  std::vector<std::vector<ro::ui::PartyBookingPlaceView>> place_views_;
  std::vector<ro::ui::PartyBookingRegionView> region_views_;
  std::array<std::vector<ro::ui::PartyBookingJobView>, ro::sim::kPartyBookingJobFamilyCount>
      job_views_;
  std::array<ro::ui::PartyBookingJobGroupView, ro::sim::kPartyBookingJobFamilyCount> job_groups_;
  std::array<std::string, ro::sim::kPartyBookingJobFamilyCount> job_group_labels_;
  // LES TEXTES D'UNE ANNONCE, que sa rangée regarde.
  struct RowTexts {
    std::string leader;
    std::string levels;
    std::string place;
    std::string jobs;
    std::string posted;
  };
  // Une entrée par annonce de la liste, puis celle du joueur.
  std::vector<RowTexts> row_texts_;
  std::vector<ro::ui::PartyBookingRowView> rows_;
  ro::ui::PartyBookingView view_;
};

}  // namespace moonclient
