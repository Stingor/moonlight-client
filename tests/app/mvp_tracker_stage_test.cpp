// L'ÉTAGE DU CARNET DE CHASSE MVP, contre un serveur en boucle locale :
// instantané, portes, invitation, import, alertes, signalement, lignes
// détachées, sprites et faits des liens.
//
// 🔴 LES TRAMES SONT ÉCRITES À LA MAIN, aux décalages du serveur, et ce qui
// part se relit octet par octet : un étage qui oublierait une commande ne se
// verrait sinon qu'en jeu.

#include "mvp/mvp_tracker_stage.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/mvp_tracker.h"
#include "ro/proto/map_packets.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/mvp_tracker.h"
#include "ro/sim/whisper.h"
#include "ro/vfs/vfs.h"

#include "mvp_tracker_stage_bench.h"
#include "support/loopback_server.h"
#include "support/temp_directory.h"

using namespace mvp_tracker_stage_bench;

TEST_CASE("Carnet MVP, étage : l'instantané part à chaque vérification, carnet allumé",
          "[app][mvp]") {
  Bench bench;
  // ÉTEINT : rien, même vérifié.
  CHECK_FALSE(bench.Advance(true));
  CHECK(bench.Silent());
  // ALLUMÉ ENSUITE : la demande armée part, une seule fois.
  bench.SwitchOn();
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  bench.Advance(true);
  CHECK(bench.Silent());
  // VÉRIFICATION PERDUE PUIS RETROUVÉE : une nouvelle zone, un nouvel instantané.
  bench.Advance(false);
  CHECK(bench.Silent());
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  // OUVRIR REDEMANDE.
  bench.stage.RequestOpen();
  bench.Advance(true);
  CHECK(bench.windows.window.open);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  // ÉTEINT PUIS RALLUMÉ SANS QUITTER LA ZONE : le serveur n'a rien diffusé
  // entre-temps, la demande repart.
  bench.windows.window.open = false;
  bench.Switch(false);
  bench.Advance(true);
  CHECK(bench.Silent());
  bench.Switch(true);
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
}

TEST_CASE("Carnet MVP, étage : une nouvelle partie repart de zéro", "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  const std::int64_t kill_s = kLocalS - 3480;
  const auto Hunt = [&] {
    bench.Apply(Catalogue(kSlots));
    bench.Apply(Observations(1, {{.slot_id = 1, .source = 2, .kill_s = kill_s, .reported_s = 1}}));
    bench.Apply(Favorites({1}));
  };
  Hunt();
  bench.Apply(Group(0, 9, "Chasse"));
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  CHECK(bench.stage.TakeAlertSound());
  // La fenêtre, ouverte et épinglée, au milieu de gestes de la partie d'avant.
  bench.windows.window.open = true;
  bench.windows.window.pinned = true;
  bench.windows.window.confirm_leave = true;
  bench.windows.window.drag_slot = 1;
  bench.windows.window.focus_slot = 2;
  bench.windows.answer.leave_group = true;

  bench.stage.StartSession(bench.windows);
  CHECK_FALSE(bench.stage.book().catalog_known());
  CHECK(bench.stage.book().slots().empty());
  CHECK(bench.stage.book().favorites().empty());
  CHECK(bench.stage.book().group().group_id == 0);
  CHECK(bench.windows.window.open);
  CHECK(bench.windows.window.pinned);
  CHECK_FALSE(bench.windows.window.confirm_leave);
  CHECK(bench.windows.window.drag_slot == ro::ui::kMvpNoSlotView);
  CHECK(bench.windows.window.focus_slot == ro::ui::kMvpNoSlotView);

  // 🔴 RECONNUE DÈS SA PREMIÈRE IMAGE : l'instantané se redemande quand même, et
  // la réponse de la partie d'avant ne part pas.
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  CHECK(bench.Silent());
  // La même observation re-sonne pour le personnage qui entre.
  Hunt();
  bench.Advance(true);
  CHECK(bench.stage.TakeAlertSound());
}

TEST_CASE("Carnet MVP, étage : la bascule ouvre puis ferme, la croix n'envoie rien", "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  bench.stage.RequestToggle();
  bench.Advance(true);
  CHECK(bench.windows.window.open);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  bench.stage.RequestToggle();
  bench.Advance(true);
  CHECK_FALSE(bench.windows.window.open);
  CHECK(bench.Silent());
  bench.stage.RequestToggle();
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  // LA CROIX : la fenêtre baisse son drapeau, rien ne part.
  bench.windows.window.open = false;
  bench.Advance(true);
  CHECK(bench.Silent());
}

TEST_CASE("Carnet MVP, étage : l'invitation reçue pendant la poignée de main s'impose",
          "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  // AVANT TOUTE AVANCÉE, fenêtre fermée.
  bench.Apply(Group(1, 12, "Les Chasseurs"));
  bench.Advance(false);
  REQUIRE(bench.windows.view != nullptr);
  CHECK(bench.windows.view->pending_invite == 12);
  CHECK(bench.windows.view->pending_invite_name == "Les Chasseurs");
  CHECK_FALSE(bench.windows.window.open);

  SECTION("accepter : la commande part, l'invitation attend le succès, le carnet s'ouvre") {
    bench.windows.answer.accept_invite = true;
    CHECK_FALSE(bench.Advance(true));
    CHECK(bench.Next().cmd == Raw(MvpCommand::kAcceptInvite));
    CHECK(bench.windows.view->pending_invite == 12);
    CHECK(bench.windows.view->invite_waiting);
    CHECK_FALSE(bench.windows.window.open);
    bench.Apply(Result(0));
    CHECK(bench.stage.book().pending_invite() == 0);
    // Ouvrir ne touche aucun réglage.
    CHECK_FALSE(bench.Advance(false));
    CHECK(bench.windows.view->pending_invite == 0);
    CHECK(bench.windows.window.open);
    CHECK(bench.windows.window.raise);
  }
  SECTION("refuser : la commande part, le succès efface, rien ne s'ouvre") {
    bench.windows.answer.decline_invite = true;
    CHECK_FALSE(bench.Advance(true));
    CHECK(bench.Next().cmd == Raw(MvpCommand::kDeclineInvite));
    CHECK(bench.windows.view->pending_invite == 12);
    bench.Apply(Result(0));
    CHECK_FALSE(bench.Advance(false));
    CHECK(bench.stage.book().pending_invite() == 0);
    CHECK_FALSE(bench.windows.window.open);
  }
}

TEST_CASE("Carnet MVP, étage : seul le groupe d'une réponse à l'instantané tient le congé",
          "[app][mvp]") {
  // Un compte sur deux clients : celui-ci a congédié l'invitation de son
  // groupe plein ; l'autre l'a refusée ; le groupe la renvoie.
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Group(1, 12, "Les Chasseurs"));
  bench.windows.answer.accept_invite = true;
  bench.Advance(true);
  bench.Apply(Result(static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kFull)));
  bench.windows.answer.dismiss_invite = true;
  bench.Advance(true);
  REQUIRE(bench.windows.view->pending_invite == 0);
  const std::uint32_t later_ms = kNowMs + moonclient::kMvpPresenceRefreshMs;
  SECTION("la réponse finit sur le groupe : l'invitation poussée ensuite se montre") {
    ApplySnapshotReply(bench, later_ms, 0);
    bench.Advance(true, later_ms);
    bench.Apply(Group(1, 12, "Les Chasseurs"), later_ms);
    bench.Advance(true, later_ms);
    CHECK(bench.windows.view->pending_invite == 12);
  }
  SECTION("un groupe poussé seul ne répond à aucun instantané") {
    // Exclusion, dissolution ou départ depuis l'autre session.
    bench.Apply(Group(0, 0, ""), later_ms);
    bench.Apply(Group(1, 12, "Les Chasseurs"), later_ms);
    bench.Advance(true, later_ms);
    CHECK(bench.windows.view->pending_invite == 12);
  }
  SECTION("la réponse qui la renvoie la garde congédiée") {
    ApplySnapshotReply(bench, later_ms, 12);
    bench.Advance(true, later_ms);
    CHECK(bench.windows.view->pending_invite == 0);
  }
  SECTION("une liaison perdue au milieu d'une réponse ne la finit pas") {
    bench.Apply(Catalogue(kSlots), later_ms);
    bench.Advance(false, later_ms);
    bench.Advance(true, later_ms);
    bench.Apply(Group(0, 0, ""), later_ms);
    bench.Apply(Group(1, 12, "Les Chasseurs"), later_ms);
    bench.Advance(true, later_ms);
    CHECK(bench.windows.view->pending_invite == 12);
  }
}

TEST_CASE("Carnet MVP, étage : chaque intention de la fenêtre part au serveur", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Catalogue(kSlots));
  bench.windows.answer.create_group = "Chasse";
  bench.windows.answer.invite_member = "Am\xC3\xA9lie";
  bench.windows.answer.kick_member = "Bob";
  bench.windows.answer.leave_group = true;
  bench.windows.answer.favorite_slot = 2;
  bench.windows.answer.favorite_on = true;
  bench.windows.answer.manual_slot = 1;
  bench.windows.answer.manual_kill_time_s = kLocalS - 60;
  bench.Advance(true);
  const Command create = bench.Next();
  CHECK(create.cmd == Raw(MvpCommand::kCreateGroup));
  CHECK(create.text == "Chasse");
  const Command invite = bench.Next();
  CHECK(invite.cmd == Raw(MvpCommand::kInvite));
  // Le nom part en CP1252 : un identifiant comparé à l'octet par le serveur.
  CHECK(invite.text == "Am\xE9lie");
  const Command kick = bench.Next();
  CHECK(kick.cmd == Raw(MvpCommand::kKick));
  CHECK(kick.text == "Bob");
  CHECK(bench.Next().cmd == Raw(MvpCommand::kLeaveGroup));
  const Command favorite = bench.Next();
  CHECK(favorite.cmd == Raw(MvpCommand::kFavorite));
  CHECK(favorite.a == 2);
  CHECK(favorite.b == 1);
  const Command manual = bench.Next();
  CHECK(manual.cmd == Raw(MvpCommand::kReportManual));
  CHECK(manual.a == 1);
  CHECK(manual.b == static_cast<std::uint32_t>(kLocalS - 60));
  CHECK(manual.text.empty());
  CHECK(bench.Silent());
  // La réponse est consommée : l'image suivante n'envoie rien.
  bench.Advance(true);
  CHECK(bench.Silent());
}

TEST_CASE("Carnet MVP, étage : l'import d'un <MVPL> scripté vise le bon créneau", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Catalogue(kSlots));
  bench.Apply(Group(0, 9, "Chasse"));
  // Le monstre d'un créneau scripté n'est pas au catalogue, et la carte arrive
  // en capitales : le repli sur la carte, sans la casse, le retrouve.
  const MvpLink link{.monster_id = 1646,
                     .map = "LHZ_DUN03",
                     .kill_time_s = kLocalS - 600,
                     .tomb_x = 123,
                     .tomb_y = 45,
                     .name = "MVP BioLab 3"};
  CHECK(bench.stage.SlotOf(link) == 7);
  bench.Advance(true);
  bench.stage.Import(link,
                     "Exp\xC3\xA9"
                     "diteur",
                     bench.link);
  const Command imported = bench.Next();
  CHECK(imported.cmd == Raw(MvpCommand::kReportManual));
  CHECK(imported.a == 7);
  CHECK(imported.b == static_cast<std::uint32_t>(kLocalS - 600));
  CHECK(imported.text ==
        "123,45|Exp\xE9"
        "diteur");
  // Le carnet s'ouvre SUR la ligne.
  bench.Advance(false);
  CHECK(bench.windows.window.open);
  CHECK(bench.windows.window.focus_slot == 7);
  CHECK(bench.windows.window.focus_ms == kNowMs);
  CHECK_FALSE(bench.windows.window.focus_scrolled);
}

TEST_CASE("Carnet MVP, étage : les faits d'un lien, dans l'ordre fixe de la règle", "[app][mvp]") {
  Bench bench;
  bench.Apply(Catalogue(kSlots));
  const MvpLink known{.monster_id = 1039,
                      .map = "gef_dun03",
                      .kill_time_s = kLocalS - 600,
                      .tomb_x = 10,
                      .tomb_y = 20,
                      .name = "Baphomet"};
  MvpLink unknown = known;
  unknown.map = "prt_fild01";
  // L'INTERRUPTEUR D'ABORD : éteint, le carnet n'envoie rien, liaison ou non.
  bench.Advance(false);
  CHECK(Said(bench.stage.FactsFor(unknown)->import_refusal) == Said(ro::ui::MvpTrackerOffText()));
  bench.SwitchOnPastSnapshots();
  // LA LIAISON ENSUITE : hors vérification, le serveur jetterait la saisie.
  bench.Advance(false);
  CHECK(Said(bench.stage.FactsFor(unknown)->import_refusal) == Said(ro::ui::MvpLinkPendingText()));
  bench.Advance(true);
  // SANS GROUPE ENSUITE, même quand le créneau est introuvable.
  CHECK(Said(bench.stage.FactsFor(unknown)->import_refusal) ==
        "Créez un groupe de chasse pour tenir un carnet.");
  CHECK(Said(bench.stage.FactsFor(known)->import_refusal) ==
        "Créez un groupe de chasse pour tenir un carnet.");
  bench.Apply(Group(0, 9, "Chasse"));
  CHECK(Said(bench.stage.FactsFor(unknown)->import_refusal) ==
        "Ce créneau n'est pas dans votre catalogue.");
  CHECK(bench.stage.FactsFor(known)->import_refusal == nullptr);
  // UNE SAISIE DÉJÀ LÀ ne prime pas : on peut la remplacer.
  bench.Apply(Observations(1, {{.slot_id = 1, .source = 0, .kill_s = kLocalS - 900}}));
  CHECK(bench.stage.FactsFor(known)->import_refusal == nullptr);
  // UN LIEN QUI NE PORTE QUE L'INSTANT EXACT ne s'importe pas.
  MvpLink exact_only = known;
  exact_only.kill_time_s = 0;
  exact_only.exact_respawn_s = kLocalS + 600;
  CHECK(Said(bench.stage.FactsFor(exact_only)->import_refusal) ==
        "Ce lien ne porte pas d'heure de mort.");
  // UNE TOMBE, UN KILL, UN MIROIR — ou une source inconnue plus haute.
  constexpr std::array<std::uint8_t, 4> kOutranking = {1, 2, 3, 9};
  for (const std::uint8_t source : kOutranking) {
    INFO("source " << static_cast<int>(source));
    bench.Apply(Observations(1, {{.slot_id = 1, .source = source, .kill_s = kLocalS - 900}}));
    CHECK(Said(bench.stage.FactsFor(known)->import_refusal) ==
          "Votre groupe en sait déjà plus : il l'a vu de ses yeux.");
  }
  // Et l'import d'un lien refusé n'envoie rien, n'ouvre rien, et le chat dit
  // pourquoi.
  bench.Advance(true);
  bench.stage.Import(known, "Sting", bench.link);
  CHECK(bench.Silent());
  CHECK(bench.stage.TakeNotices() ==
        std::vector<std::string>{"Carnet MVP : saisie de Baphomet non envoyée — Votre groupe en "
                                 "sait déjà plus : il l'a vu de ses yeux."});
  bench.Advance(true);
  CHECK_FALSE(bench.windows.window.open);
  // La tombe se guide si elle est connue et la carte nommée.
  CHECK(bench.stage.FactsFor(known)->tomb_known);
  MvpLink no_tomb = known;
  no_tomb.tomb_x = -1;
  CHECK_FALSE(bench.stage.FactsFor(no_tomb)->tomb_known);
  // ⚠ LES POINTEURS TIENNENT JUSQU'À L'AVANCÉE SUIVANTE.
  const ro::ui::MvpLinkMenuFacts* first = bench.stage.FactsFor(unknown);
  for (int more = 0; more < 100; ++more) {
    static_cast<void>(bench.stage.FactsFor(known));
  }
  CHECK(Said(first->import_refusal) == "Ce créneau n'est pas dans votre catalogue.");
}

TEST_CASE("Carnet MVP, étage : une heure que la commande ne porte pas ne s'importe pas",
          "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Catalogue(kSlots));
  bench.Apply(Group(0, 9, "Chasse"));
  constexpr std::int64_t kBeyondCommandS = std::int64_t{1} << 32;
  // 🔴 2^32 + T SE TRONQUERAIT EN T, une heure plausible que le serveur prendrait.
  const MvpLink beyond{.monster_id = 1039,
                       .map = "gef_dun03",
                       .kill_time_s = kBeyondCommandS + (kLocalS - 600),
                       .tomb_x = 10,
                       .tomb_y = 20,
                       .name = "Baphomet"};
  // Le lien porte bien une heure : le motif le dit, au lieu de prétendre le
  // contraire.
  constexpr const char* kBeyondReason =
      "L'heure de mort de ce lien est hors de portée de la saisie.";
  bench.Advance(true);
  CHECK(Said(bench.stage.FactsFor(beyond)->import_refusal) == kBeyondReason);
  bench.stage.Import(beyond, "Sting", bench.link);
  CHECK(bench.Silent());
  // La première heure qu'elle ne porte plus.
  MvpLink first_beyond = beyond;
  first_beyond.kill_time_s = kBeyondCommandS;
  CHECK(Said(bench.stage.FactsFor(first_beyond)->import_refusal) == kBeyondReason);
  // La dernière heure qu'elle porte passe encore.
  MvpLink last = beyond;
  last.kill_time_s = kBeyondCommandS - 1;
  CHECK(bench.stage.FactsFor(last)->import_refusal == nullptr);
}

TEST_CASE("Carnet MVP, étage : l'écho de notre chuchotement s'importe à notre nom", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Catalogue(kSlots));
  bench.Apply(Group(0, 9, "Chasse"));
  const MvpLink link{.monster_id = 1039,
                     .map = "gef_dun03",
                     .kill_time_s = kLocalS - 600,
                     .tomb_x = 10,
                     .tomb_y = 20,
                     .name = "Baphomet"};
  const ro::sim::WhisperFacts echo{.side = ro::sim::WhisperSide::kSent,
                                   .peer_name = "Destinataire"};
  bench.Advance(true);
  bench.stage.Import(link, moonclient::MvpWhisperSenderOf(echo, "NotreNom"), bench.link);
  const Command imported = bench.Next();
  CHECK(imported.text == "10,20|NotreNom");
}

TEST_CASE("Carnet MVP, étage : un favori sonne une fois par observation", "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  bench.Apply(Catalogue(kSlots));
  // Mort il y a 58 minutes, retour au plus tôt dans 2 : dans le préavis de 5.
  const std::int64_t kill_s = kLocalS - 3480;
  bench.Apply(Observations(1, {{.slot_id = 1, .source = 2, .kill_s = kill_s, .reported_s = 1}}));
  bench.Apply(Favorites({1}));
  bench.Advance(false);
  CHECK(bench.stage.TakeAlertSound());
  CHECK_FALSE(bench.stage.TakeAlertSound());
  REQUIRE(bench.windows.view != nullptr);
  REQUIRE_FALSE(bench.windows.view->alerts.empty());
  CHECK(bench.windows.view->alerts.front().label == "Baphomet");
  CHECK(bench.windows.view->alerts.front().map == "gef_dun03");
  // LA MÊME OBSERVATION ne re-sonne pas ; le bandeau tient vingt secondes.
  bench.Advance(false, kNowMs + 20000);
  CHECK_FALSE(bench.stage.TakeAlertSound());
  CHECK_FALSE(bench.windows.view->alerts.empty());
  bench.Advance(false, kNowMs + 20001);
  CHECK(bench.windows.view->alerts.empty());
  // UNE OBSERVATION PLUS FRAÎCHE a le droit de re-sonner.
  bench.Apply(Observations(2, {{.slot_id = 1, .source = 3, .kill_s = kill_s, .reported_s = 2}}));
  bench.Advance(false);
  CHECK(bench.stage.TakeAlertSound());

  SECTION("deux favoris à la même image : un seul son") {
    bench.Apply(Observations(2, {{.slot_id = 1, .source = 2, .kill_s = kill_s, .reported_s = 3},
                                 {.slot_id = 2, .source = 2, .kill_s = kill_s, .reported_s = 3}}));
    bench.Apply(Favorites({1, 2}));
    bench.Advance(false);
    CHECK(bench.stage.TakeAlertSound());
    CHECK_FALSE(bench.stage.TakeAlertSound());
    // LES ALERTES S'EMPILENT : le créneau déjà allumé garde sa place, le
    // nouveau s'ajoute après lui.
    REQUIRE(bench.windows.view->alerts.size() == 2);
    CHECK(bench.windows.view->alerts[0].label == "Baphomet");
    CHECK(bench.windows.view->alerts[1].label == "Osiris");
  }
  SECTION("le son coupé : le bandeau reste, rien ne sonne") {
    REQUIRE(bench.options.Change(
        {.changed = MvpTrackerRowId(MvpTrackerRow::kAlertSound), .on = false}));
    bench.Apply(Observations(2, {{.slot_id = 1, .source = 2, .kill_s = kill_s, .reported_s = 4}}));
    bench.Advance(false);
    CHECK_FALSE(bench.stage.TakeAlertSound());
    CHECK_FALSE(bench.windows.view->alerts.empty());
  }
  SECTION("plus d'une heure après l'ouverture : ce n'est plus une nouvelle") {
    const std::int64_t old_s = kLocalS - 3600 - 3601;
    bench.Apply(Observations(2, {{.slot_id = 1, .source = 2, .kill_s = old_s, .reported_s = 5}}));
    bench.Advance(false);
    CHECK_FALSE(bench.stage.TakeAlertSound());
  }
  SECTION("éteint : ni son ni bandeau") {
    REQUIRE(
        bench.options.Change({.changed = MvpTrackerRowId(MvpTrackerRow::kEnabled), .on = false}));
    bench.Apply(Observations(2, {{.slot_id = 1, .source = 2, .kill_s = kill_s, .reported_s = 6}}));
    bench.Advance(false);
    CHECK_FALSE(bench.stage.TakeAlertSound());
    CHECK(bench.windows.view->alerts.empty());
  }
}

TEST_CASE("Carnet MVP, étage : l'icône s'allume avec un favori, carnet allumé", "[app][mvp]") {
  Bench bench;
  bench.Apply(Catalogue(kSlots));
  bench.Apply(Observations(1, {{.slot_id = 1, .source = 2, .kill_s = kLocalS - 3480}}));
  bench.Apply(Favorites({1}));
  bench.Advance(false);
  CHECK_FALSE(bench.windows.badge_lit);
  bench.SwitchOn();
  bench.Advance(false);
  CHECK(bench.windows.badge_lit ==
        ro::sim::MvpBadgeLit(bench.stage.book(), kLocalS, bench.options.settings().alert_lead_min));
  CHECK(bench.windows.badge_lit);
  bench.Apply(Favorites({}));
  bench.Advance(false);
  CHECK_FALSE(bench.windows.badge_lit);
}

TEST_CASE("Carnet MVP, étage : les lignes détachées se posent, se suivent et s'enregistrent",
          "[app][mvp]") {
  const TempDirectory client;
  Bench bench;
  bench.stage.SeeSaveRoot(client.path());
  bench.SwitchOn();
  bench.Apply(Catalogue(kSlots));
  const auto file_text = [&client] {
    std::ifstream file(client.path() / "savedata" / moonclient::kMvpLineFileName);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
  };

  bench.windows.answer.pin_slot = 1;
  bench.windows.answer.pin_x = 300;
  bench.windows.answer.pin_y = 40;
  bench.Advance(false);
  CHECK(bench.stage.line_count() == 1);
  REQUIRE(bench.windows.view->lines.size() == 1);
  CHECK(bench.windows.view->lines[0].slot_id == 1);
  CHECK(bench.windows.view->lines[0].x == 300);
  CHECK(file_text().find("gef_dun03") != std::string::npos);

  // LE MÊME CRÉNEAU deux fois : une seule ligne.
  bench.windows.answer.pin_slot = 1;
  bench.Advance(false);
  CHECK(bench.stage.line_count() == 1);

  // DÉPLACÉE puis relâchée : la place s'écrit.
  bench.windows.answer.moved_line = 0;
  bench.windows.answer.moved_x = 512;
  bench.windows.answer.moved_y = 64;
  bench.windows.answer.lines_released = true;
  bench.Advance(false);
  CHECK(bench.windows.view->lines[0].x == 512);
  CHECK(file_text().find("512") != std::string::npos);

  // UN CATALOGUE RENUMÉROTÉ, de même taille : la ligne retrouve son créneau.
  std::vector<Slot> renumbered = kSlots;
  renumbered[0].slot_id = 40;
  renumbered[1].slot_id = 41;
  bench.Apply(Catalogue(renumbered));
  bench.Advance(false);
  REQUIRE(bench.windows.view->lines.size() == 1);
  CHECK(bench.windows.view->lines[0].slot_id == 40);

  // UN CATALOGUE SANS CE MONSTRE : la ligne reste au livre, hors de la vue.
  bench.Apply(Catalogue({kSlots[1]}));
  bench.Advance(false);
  CHECK(bench.stage.line_count() == 1);
  CHECK(bench.windows.view->lines.empty());
  bench.Apply(Catalogue(kSlots));
  bench.Advance(false);
  CHECK(bench.windows.view->lines.size() == 1);

  // RETIRÉE : effacée, et le fichier suit.
  bench.windows.window.line_sizes = {ImVec2(80.0F, 20.0F)};
  bench.windows.answer.removed_line = 0;
  bench.Advance(false);
  CHECK(bench.stage.line_count() == 0);
  CHECK(bench.windows.view->lines.empty());
  CHECK(bench.windows.window.line_sizes.empty());
  CHECK(file_text().find("gef_dun03") == std::string::npos);

  // ET RELUES à la partie suivante.
  bench.windows.answer.pin_slot = 2;
  bench.Advance(false);
  MvpTrackerStage next;
  next.SeeSaveRoot(client.path());
  CHECK(next.line_count() == 1);
  next.PutBackAllLines();
  CHECK(next.line_count() == 0);
  CHECK(file_text().find("moc_pryd06") == std::string::npos);
}

TEST_CASE("Carnet MVP, étage : deux sprites neufs par image, une instance par classe",
          "[app][mvp]") {
  // UNE INSTALLATION SANS SPRITE : chaque case reste vide, mais l'instance se
  // crée — c'est elle que le budget compte.
  const TempDirectory install;
  install.WriteText(std::filesystem::path("data/readme.txt"), "vide");
  auto vfs = ro::vfs::Vfs::Mount(ro::vfs::MountOptions{.root = install.path()});
  REQUIRE(vfs.has_value());
  ro::render::ActorWardrobe wardrobe(*vfs, ro::render::SpriteCatalog{});
  Bench bench;
  bench.stage.See({.wardrobe = &wardrobe});
  // Trois rangs demandés, dont un en double.
  bench.windows.window.wanted_sprites = {1039, 1038, 1039, 1087};
  bench.Advance(false);
  REQUIRE(bench.windows.view != nullptr);
  REQUIRE(bench.windows.view->sprites.size() == 2);
  CHECK(bench.windows.view->sprites[0].mob_id == 1039);
  CHECK(bench.windows.view->sprites[1].mob_id == 1038);
  // L'image suivante prend la troisième, et garde les deux premières.
  bench.Advance(false);
  CHECK(bench.windows.view->sprites.size() == 3);
  // Sans garde-robe, aucun sprite : la case reste vide.
  bench.stage.See({});
  bench.Advance(false);
  CHECK(bench.windows.view->sprites.empty());
}

TEST_CASE("Carnet MVP, étage : un scripté qui tourne garde son sprite dans l'image même",
          "[app][mvp]") {
  const TempDirectory install;
  install.WriteText(std::filesystem::path("data/readme.txt"), "vide");
  auto vfs = ro::vfs::Vfs::Mount(ro::vfs::MountOptions{.root = install.path()});
  REQUIRE(vfs.has_value());
  ro::render::ActorWardrobe wardrobe(*vfs, ro::render::SpriteCatalog{});
  Bench bench;
  bench.SwitchOn();
  bench.stage.See({.wardrobe = &wardrobe});
  bench.Apply(Catalogue(kSlots));
  // Le créneau de lhz_dun03 tourne sur six monstres ; un tour complet ramène le
  // premier.
  constexpr std::uint32_t kBioLab3Mobs = 6;
  constexpr std::uint32_t kTurnMs = moonclient::kMvpScriptedSpriteTurnMs;
  constexpr std::uint32_t kCycleStartMs = kTurnMs * kBioLab3Mobs * 10;
  const auto Composed = [&bench](std::uint16_t mob_id) {
    const std::span<const ro::ui::MvpSpriteArt> sprites = bench.windows.view->sprites;
    return std::ranges::find(sprites, mob_id, &ro::ui::MvpSpriteArt::mob_id) != sprites.end();
  };
  // Les deux premiers monstres du créneau, chargés.
  bench.windows.window.wanted_sprites = {1646, 1647};
  bench.Advance(false, kCycleStartMs);
  REQUIRE(bench.windows.view->slots[2].sprite_mob_id == 1646);
  // 🔴 L'IMAGE PRÉCÉDENTE A DESSINÉ LE PREMIER, celle-ci passe au second : déjà
  // chargé, il se peint sans attendre la demande de l'image suivante.
  bench.windows.window.wanted_sprites = {1646};
  bench.Advance(false, kCycleStartMs + kTurnMs);
  REQUIRE(bench.windows.view->slots[2].sprite_mob_id == 1647);
  CHECK(Composed(1647));
  // Un monstre jamais chargé attend sa demande : rien ne se lit hors du budget.
  bench.Advance(false, kCycleStartMs + 2 * kTurnMs);
  REQUIRE(bench.windows.view->slots[2].sprite_mob_id == 1648);
  CHECK_FALSE(Composed(1648));
  CHECK_FALSE(Composed(1039));
}

TEST_CASE("Carnet MVP, étage : la vue compose rangs, scriptés et tombes", "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  bench.Apply(Catalogue(kSlots));
  bench.Apply(Observations(1, {{.slot_id = 1, .source = 1, .kill_s = kLocalS - 600}}));
  bench.Apply(Favorites({2}));
  bench.Advance(false, 1500 * 3);
  const ro::ui::MvpTrackerView& view = *bench.windows.view;
  CHECK(view.enabled);
  CHECK(view.catalog_known);
  CHECK(view.server_now_s == kLocalS);
  REQUIRE(view.slots.size() == 3);
  CHECK(view.slots[0].label == "Baphomet");
  CHECK(view.slots[0].observed);
  CHECK(view.slots[0].window_known);
  CHECK(view.slots[0].window_from_s == kLocalS - 600 + 3600);
  CHECK(view.slots[0].source_raw == 1);
  CHECK_FALSE(view.slots[1].observed);
  CHECK(view.slots[1].favorite);
  CHECK(view.slots[1].tomb_x == -1);
  // LE SCRIPTÉ : son libellé anglais, son sprite qui tourne, son monstre désigné.
  CHECK(view.slots[2].label == "MVP BioLab 3");
  CHECK(view.slots[2].link_mob_id == 1646);
  CHECK(view.slots[2].sprite_mob_id == 1646 + 3);
  bench.Advance(false, 1500 * 7);
  CHECK(bench.windows.view->slots[2].sprite_mob_id == 1646 + 1);
  // Les tombes : seulement montrées, seulement connues, seulement de CETTE carte.
  CHECK(bench.stage.TombsOn("gef_dun03", true, kLocalS).empty());
  bench.Apply(Observations(
      1, {{.slot_id = 1, .source = 1, .kill_s = kLocalS - 600, .tomb_x = 150, .tomb_y = 0}}));
  CHECK(bench.stage.TombsOn("gef_dun03", false, kLocalS).empty());
  CHECK(bench.stage.TombsOn("moc_pryd06", true, kLocalS).empty());
  const std::span<const ro::ui::MinimapTomb> tombs =
      bench.stage.TombsOn("GEF_DUN03", true, kLocalS);
  REQUIRE(tombs.size() == 1);
  CHECK(tombs[0].cell.x == 150);
  CHECK(tombs[0].cell.y == 0);
  CHECK_FALSE(tombs[0].exact);
  // ET LA VUE PORTE LA TOMBE du rang.
  bench.Advance(false);
  CHECK(bench.windows.view->slots[0].tomb_x == 150);
  CHECK(bench.windows.view->slots[0].tomb_y == 0);
}

TEST_CASE("Carnet MVP, étage : l'aperçu d'un <MVPL> nomme, date et situe", "[app][mvp]") {
  const MvpTrackerStage stage;
  // Sans nom, « MVP » ; la tombe se dit même à (x, 0).
  const ro::ui::MvpLinkTip bare = stage.TipOf(
      MvpLink{.monster_id = 1039, .map = "gef_dun03", .tomb_x = 150, .tomb_y = 0}, kLocalS);
  CHECK(bare.name == "MVP");
  CHECK(bare.map == "gef_dun03");
  CHECK(bare.tomb_line == "Tombe en (150, 0)");
  CHECK(bare.respawn_line.empty());
  CHECK(bare.death_line.empty());
  // Une tombe inconnue ne se dit pas.
  CHECK(stage.TipOf(MvpLink{.map = "gef_dun03", .name = "Baphomet"}, kLocalS).tomb_line.empty());
  // L'INSTANT EXACT passe avant la fenêtre, et la mort s'ajoute.
  const ro::ui::MvpLinkTip exact = stage.TipOf(MvpLink{.monster_id = 1039,
                                                       .map = "gef_dun03",
                                                       .kill_time_s = kLocalS - 600,
                                                       .exact_respawn_s = kLocalS + 600,
                                                       .delay1_min = 60,
                                                       .name = "Baphomet"},
                                               kLocalS);
  CHECK(exact.name == "Baphomet");
  CHECK(exact.respawn_line.ends_with("(Convex Mirror)"));
  CHECK(exact.death_line.starts_with("Mort à "));
  const ro::ui::MvpLinkTip window = stage.TipOf(MvpLink{.monster_id = 1039,
                                                        .map = "gef_dun03",
                                                        .kill_time_s = kLocalS - 600,
                                                        .delay1_min = 60,
                                                        .delay2_min = 10,
                                                        .name = "Baphomet"},
                                                kLocalS);
  CHECK(window.respawn_line.starts_with("Retour entre "));
}
