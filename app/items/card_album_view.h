#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "ro/net/card_album.h"
#include "ro/net/connection.h"
#include "ro/net/moonclient_handshake.h"
#include "ro/net/world_update.h"
#include "ro/render/card_thumbnails.h"
#include "ro/render/item_icons.h"
#include "ro/sim/card_album.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/card_album_link.h"
#include "ro/ui/card_album_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/package_items.h"

#include "items/card_album_delta.h"

namespace moonclient {

// L'ALBUM DE CARTES, VU DE LA PARTIE.
//
// Il fait ce que la fenêtre ne peut pas faire seule : il parle au serveur, il
// nomme et classe les cartes du catalogue, il prépare leurs vignettes, et il
// DIT ce que le serveur a répondu.
//
// 🔴 CE TROISIÈME POINT EST À NOUS : le serveur ne renvoie qu'un CODE et l'état
// complet, jamais ce qu'on lui a demandé. C'est donc ici qu'on retient la
// commande partie, pour pouvoir dire « Poring : pochette DÉBLOQUÉE » plutôt
// que « c'est fait ». Et le SUCCÈS se dit autant que l'échec : un sacrifice
// consomme une carte, et ne dire que la perte tairait ce qu'elle a payé.
//
// ⚠ IL N'APPLIQUE RIEN D'AVANCE : un sacrifice est irréversible, et afficher un
// résultat que le serveur n'a pas confirmé serait mentir sur un paiement.

// Ce que la vue lit de l'installation. ⚠ DES POINTEURS : chacun peut manquer
// sur une installation incomplète, et l'album se passe alors de ce qu'il porte.
struct CardAlbumPieces {
  const ro::vfs::ItemTable* items = nullptr;
  ro::render::ItemIcons* icons = nullptr;
  ro::render::CardThumbnails* thumbnails = nullptr;
  const ro::vfs::PackageItems* packages = nullptr;
};

class CardAlbumView {
 public:
  explicit CardAlbumView(const std::filesystem::path& client_root);

  // Une trame d'album. ⚠ ELLE PEUT ÉMETTRE : un sacrifice réussi range aussitôt
  // le reste de la pile dans la pochette qu'il vient d'ouvrir.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                 ro::net::Connection& link, std::uint32_t now_ms);

  // Recompose ce que la fenêtre montrera. ⚠ SEULEMENT FENÊTRE OUVERTE : le
  // catalogue compte neuf cents cartes, et les nommer pour personne serait du
  // gaspillage.
  void Compose(const CardAlbumPieces& pieces, const ro::sim::World& world,
               const ro::ui::CardAlbumWindowState& state, bool staff);
  [[nodiscard]] const ro::ui::CardAlbumView& view() const noexcept { return view_; }

  // Route ce que la fenêtre a demandé au serveur, et au fichier du staff. Les
  // descriptions, la base de données et l'aperçu restent à `Game`.
  //
  // ⚠ À CHAQUE IMAGE, fenêtre fermée comprise : c'est aussi ici que les comptes
  // rendus s'éteignent, à l'heure.
  void Apply(const ro::ui::CardAlbumWindowAction& asked, ro::net::Connection& link,
             std::uint32_t now_ms);

  // Le raccourci : ouvrir — et demander l'état — ou refermer.
  void Toggle(ro::ui::CardAlbumWindowState& state, ro::net::Connection& link, std::uint32_t now_ms);

  // « VOIR DANS L'ALBUM » : ouvrir l'album, puis en tourner les pages jusqu'à la
  // pochette de cette carte, surlignée — scellée comprise.
  //
  // ⚠ UN ALBUM QUI CHARGE FAIT ATTENDRE LE SAUT : ouvrir demande l'état au
  // serveur comme le raccourci, et c'est sa réponse qui tourne les pages. Une
  // carte que le catalogue ne range nulle part se dit dans l'album.
  void Show(std::uint32_t card_id, ro::ui::CardAlbumWindowState& state, ro::net::Connection& link,
            std::uint32_t now_ms);

  // Les cartes que les tables de l'installation connaissent, dans n'importe
  // quel ordre : elles décident où « Voir dans l'album » paraît.
  void NoteCardItems(std::vector<std::uint32_t> card_ids);
  // Ce que le menu d'un objet lit pour cette entrée. ⚠ DES VUES SUR CETTE VUE :
  // elles ne valent que jusqu'à la prochaine trame d'album. `available` : l'album
  // s'ouvre dans cette partie.
  [[nodiscard]] ro::ui::CardAlbumLinkFacts LinkFacts(bool available) const noexcept;

 private:
  // Le saut lui-même, catalogue en main.
  void ShowNow(std::uint32_t card_id, std::uint32_t now_ms);
  // Le saut qu'une réponse attendait, s'il y en a un.
  void ServePendingShow(std::uint32_t now_ms);
  void Send(ro::net::CardAlbumCommand command, std::uint32_t arg, std::uint16_t amount,
            std::uint32_t card_id, ro::net::Connection& link);
  void Report(std::string text, ro::ui::CardAlbumTone tone, std::uint32_t now_ms);
  void ReportAnswer(std::uint32_t now_ms);
  void RebuildPockets(const CardAlbumPieces& pieces);
  void BuildSources(const CardAlbumPieces& pieces);
  [[nodiscard]] std::string ShortName(std::uint32_t card_id) const;

  ro::sim::CardAlbum album_;
  CardAlbumDelta delta_;

  // ── La dernière commande ──────────────────────────────────────────────────
  ro::net::CardAlbumCommand last_command_ = ro::net::CardAlbumCommand::kRefresh;
  std::uint32_t last_card_ = 0;
  std::uint16_t last_amount_ = 0;
  // Une commande est partie et attend sa réponse. Un état reçu SANS commande en
  // vol vient d'ailleurs (`@storealbum`) et se formule d'après ce qui a changé.
  bool in_flight_ = false;
  // Une demande d'état est partie, et rien n'est revenu : « Chargement… ».
  bool loading_ = false;
  int unsolicited_unlocked_ = 0;
  std::int64_t unsolicited_reserve_ = 0;

  // Le reste d'une pile offerte à une pochette scellée : sacrifiée pour une,
  // rangée pour les autres dès que le serveur confirme l'ouverture.
  std::uint16_t chain_slot_ = 0;
  std::uint32_t chain_card_ = 0;
  int chain_amount_ = 0;
  bool chained_deposit_ = false;
  // Ce dépôt suivait un sacrifice : les deux se disent ensemble.
  bool last_chained_ = false;

  // ── Ce qui se dit ─────────────────────────────────────────────────────────
  std::string notice_;
  ro::ui::CardAlbumTone notice_tone_ = ro::ui::CardAlbumTone::kDone;
  std::uint32_t notice_ms_ = 0;
  bool notice_live_ = false;
  std::string staff_notice_;
  bool staff_notice_ok_ = true;
  std::uint32_t staff_notice_ms_ = 0;
  bool staff_notice_live_ = false;
  std::uint32_t highlight_card_ = 0;
  std::uint32_t highlight_ms_ = 0;
  std::uint32_t highlight_serial_ = 0;
  // La pochette surlignée a été DEMANDÉE : voir `CardAlbumView::highlight_reveal`.
  bool highlight_reveal_ = false;
  // « Voir dans l'album » attend la réponse du serveur pour cette carte.
  std::uint32_t pending_show_ = 0;

  // ── Ce que les menus d'objet lisent ───────────────────────────────────────
  // Les deux triés par identifiant.
  std::vector<std::uint32_t> card_ids_;
  std::vector<std::uint32_t> pocket_ids_;

  // ── Ce que la fenêtre reçoit ──────────────────────────────────────────────
  std::vector<ro::ui::CardAlbumPocketView> pockets_;
  std::vector<ro::ui::CardAlbumSourceView> sources_;
  std::vector<std::string> source_names_;
  std::vector<ro::ui::CardAlbumHandCard> hand_;
  std::vector<ro::ui::CardAlbumArt> art_;
  ro::ui::CardAlbumView view_;
  // Ce qui a produit `pockets_` : quand l'un change, on les refait.
  std::uint32_t pockets_arrival_ = 0;
  std::uint32_t pockets_delta_ = 0;
  bool pockets_built_ = false;
  std::uint32_t revision_ = 0;

  // D'OÙ TOMBE UNE CARTE : les deux tables d'albums d'objets, retournées une
  // fois en index carte → chance.
  std::array<std::unordered_map<std::uint32_t, ro::ui::CardAlbumSourceMark>,
             ro::ui::kCardAlbumSourceCount>
      chances_;
  std::array<int, ro::ui::kCardAlbumSourceCount> typical_rates_{};
  bool sources_built_ = false;
};

}  // namespace moonclient
