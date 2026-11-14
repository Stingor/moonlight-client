// L'ÉTAGE DU CARNET DE CHASSE MVP, ce que le joueur en voit au-delà de la
// table : les résultats dits dans le chat, l'invitation qui attend le serveur,
// les alertes empilées, la propriété du groupe, les tombes et les liens.
//
// 🔴 LES RÉSULTATS NE DISENT PAS À QUELLE COMMANDE ILS RÉPONDENT : l'étage les
// apparie dans l'ordre d'envoi. Ces essais tiennent la file — appariement,
// borne, vidage par partie et par liaison perdue —, dont une erreur prêterait à
// une commande le refus d'une autre.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/mvp_tracker.h"
#include "ro/ui/mvp_tracker_window.h"
#include "ro/vfs/map_display_names.h"

#include "mvp/mvp_tracker_stage.h"
#include "mvp_tracker_stage_bench.h"

using namespace mvp_tracker_stage_bench;
using moonclient::kMvpAnswerPatienceMs;
using moonclient::kMvpLostAnswerMs;
using moonclient::kMvpMostAlertsShown;
using moonclient::kMvpMostAwaitedResults;
using moonclient::kMvpPresenceRefreshMs;
using moonclient::MvpTrackerPieces;

namespace {

constexpr std::uint8_t kOk = 0;
constexpr auto kFull = static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kFull);
constexpr auto kNoInvite = static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kNoInvite);
constexpr auto kTargetInGroup = static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kTargetInGroup);
constexpr std::uint8_t kUnknownCode = 200;
// Mort il y a 58 minutes, retour au plus tôt dans 2 : dans le préavis.
constexpr std::int64_t kKillS = kLocalS - 3480;

[[nodiscard]] std::vector<std::string> Lines(std::vector<std::string> lines) { return lines; }

}  // namespace

TEST_CASE("Carnet MVP, étage : sans groupe, la propriété ne se sait pas", "[app][mvp]") {
  const ro::sim::MvpGroup none;
  CHECK_FALSE(moonclient::MvpOwnership(none, "NotreNom").has_value());
  CHECK_FALSE(moonclient::MvpOwnership(none, "").has_value());
}

TEST_CASE("Carnet MVP, étage : la propriété se déduit de notre nom parmi les membres",
          "[app][mvp]") {
  ro::sim::MvpGroup group;
  group.group_id = 9;
  group.owner_user_id = 100;
  group.members = {{.user_id = 100, .name = "Chef"},
                   {.user_id = 200, .name = "Moi"},
                   {.user_id = 100, .name = "Chef2"}};
  CHECK(moonclient::MvpOwnership(group, "Chef") == std::optional<bool>(true));
  CHECK(moonclient::MvpOwnership(group, "Moi") == std::optional<bool>(false));
  // C'EST LE COMPTE QUI POSSÈDE : sa deuxième tête aussi.
  CHECK(moonclient::MvpOwnership(group, "Chef2") == std::optional<bool>(true));
  // Notre tête absente de la liste, ou sans nom : on ne sait pas.
  CHECK_FALSE(moonclient::MvpOwnership(group, "Absent").has_value());
  CHECK_FALSE(moonclient::MvpOwnership(group, "").has_value());

  // LA VUE le porte, d'après le nom passé à chaque avancée.
  Bench bench;
  bench.Apply(
      GroupWith(9, 100, {{.user_id = 100, .name = "Chef"}, {.user_id = 200, .name = "Moi"}}));
  bench.own_name = "Moi";
  bench.Advance(false);
  CHECK(bench.windows.view->owner_known);
  CHECK_FALSE(bench.windows.view->i_own);
  bench.own_name = "Chef";
  bench.Advance(false);
  CHECK(bench.windows.view->owner_known);
  CHECK(bench.windows.view->i_own);
  bench.own_name.clear();
  bench.Advance(false);
  CHECK_FALSE(bench.windows.view->owner_known);
  CHECK_FALSE(bench.windows.view->i_own);
}

TEST_CASE("Carnet MVP, étage : Dissoudre envoie la commande 3", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.windows.answer.dissolve_group = true;
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kDissolveGroup));
  CHECK(bench.Silent());
}

TEST_CASE("Carnet MVP, étage : le résultat d'une invitation se dit dans le chat, carnet fermé",
          "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.windows.answer.invite_member = "Am\xC3\xA9lie";
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kInvite));
  CHECK(bench.stage.TakeNotices().empty());

  SECTION("accepté : la ligne exacte, une fois") {
    bench.Apply(Result(kOk));
    CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Am\xC3\xA9lie."}));
    CHECK(bench.stage.TakeNotices().empty());
  }
  SECTION("refusé : la raison du serveur") {
    bench.Apply(Result(kTargetInGroup));
    CHECK(bench.stage.TakeNotices() ==
          Lines({std::string("Carnet MVP : invitation à Am\xC3\xA9lie refusée — ") +
                 ro::ui::MvpResultText(kTargetInGroup)}));
  }
  SECTION("un code inconnu se dit par son numéro") {
    bench.Apply(Result(kUnknownCode));
    const std::vector<std::string> notices = bench.stage.TakeNotices();
    REQUIRE(notices.size() == 1);
    CHECK(notices[0].ends_with("(code 200)."));
  }
}

TEST_CASE("Carnet MVP, étage : carnet ouvert, le bandeau suffit, le chat se tait", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.windows.window.open = true;
  bench.Advance(true);
  bench.windows.answer.invite_member = "Bob";
  bench.Advance(true);
  bench.Apply(Result(kOk));
  bench.Apply(Result(kFull));
  CHECK(bench.stage.TakeNotices().empty());
  // REPLIÉ, il ne se voit pas : le chat le dit.
  bench.windows.window.was_collapsed = true;
  bench.Advance(true);
  bench.stage.Invite("Bob", bench.link);
  bench.Apply(Result(kOk));
  CHECK(bench.stage.TakeNotices().size() == 1);
}

TEST_CASE("Carnet MVP, étage : une saisie se dit « transmise », ou refusée", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Catalogue(kSlots));
  bench.windows.answer.manual_slot = 1;
  bench.windows.answer.manual_kill_time_s = kLocalS - 60;
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kReportManual));
  SECTION("accepté") {
    bench.Apply(Result(kOk));
    CHECK(bench.stage.TakeNotices() ==
          Lines({"Carnet MVP : heure de mort de Baphomet transmise au groupe."}));
  }
  SECTION("refusé") {
    bench.Apply(Result(kFull));
    CHECK(bench.stage.TakeNotices() ==
          Lines({std::string("Carnet MVP : saisie de Baphomet refusée — ") +
                 ro::ui::MvpResultText(kFull)}));
  }
}

TEST_CASE("Carnet MVP, étage : l'import, carnet fermé au clic, se dit même carnet ouvert",
          "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Catalogue(kSlots));
  bench.Apply(Group(0, 9, "Chasse"));
  bench.Advance(true);
  bench.stage.Import(MvpLink{.monster_id = 1039,
                             .map = "gef_dun03",
                             .kill_time_s = kLocalS - 600,
                             .delay1_min = 60,
                             .name = "Baphomet"},
                     "Bob", bench.link);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kReportManual));
  // L'import ouvre le carnet sur la ligne avant que le serveur ne réponde.
  bench.Advance(true);
  REQUIRE(bench.windows.window.open);
  bench.Apply(Result(kOk));
  CHECK(bench.stage.TakeNotices() ==
        Lines({"Carnet MVP : heure de mort de Baphomet transmise au groupe."}));
}

TEST_CASE("Carnet MVP, étage : les résultats s'apparient dans l'ordre d'envoi", "[app][mvp]") {
  Bench bench;
  // ALLUMÉ : l'instantané part, et n'attend aucun résultat.
  bench.SwitchOn();
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  SECTION("deux commandes, deux résultats : chacun à la sienne") {
    bench.windows.answer.kick_member = "Zed";
    bench.Advance(true);
    CHECK(bench.Next().cmd == Raw(MvpCommand::kKick));
    bench.stage.Invite("Bob", bench.link);
    bench.Apply(Result(kTargetInGroup));
    bench.Apply(Result(kOk));
    CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Bob."}));
  }
  SECTION("l'ordre se garde dans une même réponse de la fenêtre") {
    bench.windows.answer.kick_member = "Zed";
    bench.windows.answer.invite_member = "Bob";
    bench.Advance(true);
    // Envoyées dans l'ordre de la réponse : l'invitation, puis l'exclusion.
    CHECK(bench.Next().cmd == Raw(MvpCommand::kInvite));
    CHECK(bench.Next().cmd == Raw(MvpCommand::kKick));
    bench.Apply(Result(kFull));
    bench.Apply(Result(kOk));
    CHECK(bench.stage.TakeNotices() ==
          Lines({std::string("Carnet MVP : invitation à Bob refusée — ") +
                 ro::ui::MvpResultText(kFull)}));
  }
  SECTION("l'instantané et le favori ne reçoivent aucun résultat") {
    bench.Apply(Catalogue(kSlots));
    bench.windows.answer.favorite_slot = 1;
    bench.windows.answer.favorite_on = true;
    bench.Advance(true);
    bench.stage.Invite("Bob", bench.link);
    bench.Apply(Result(kOk));
    CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Bob."}));
  }
  SECTION("un résultat que rien n'attend ne s'apparie à rien") {
    bench.Apply(Result(kFull));
    CHECK(bench.stage.TakeNotices().empty());
    // La file n'en est pas décalée.
    bench.stage.Invite("Bob", bench.link);
    bench.Apply(Result(kOk));
    CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Bob."}));
  }
}

TEST_CASE("Carnet MVP, étage : la file des réponses est bornée, et se vide", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Advance(true);
  SECTION("au-delà de la borne, la plus ancienne sort") {
    for (std::size_t sent = 0; sent <= kMvpMostAwaitedResults; ++sent) {
      bench.stage.Invite("N" + std::to_string(sent), bench.link);
    }
    for (std::size_t answered = 0; answered <= kMvpMostAwaitedResults; ++answered) {
      bench.Apply(Result(kOk));
    }
    const std::vector<std::string> notices = bench.stage.TakeNotices();
    REQUIRE(notices.size() == kMvpMostAwaitedResults);
    CHECK(notices.front() == "Carnet MVP : invitation envoyée à N1.");
    CHECK(notices.back() == "Carnet MVP : invitation envoyée à N16.");
  }
  SECTION("une nouvelle partie n'attend plus rien de la précédente") {
    bench.stage.Invite("Ancien", bench.link);
    bench.stage.StartSession(bench.windows);
    bench.Apply(Result(kFull));
    CHECK(bench.stage.TakeNotices().empty());
    bench.Advance(true);
    bench.stage.Invite("Neuf", bench.link);
    bench.Apply(Result(kOk));
    CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Neuf."}));
  }
  SECTION("la vérification perdue vide la file") {
    bench.stage.Invite("Ancien", bench.link);
    bench.Advance(false);
    bench.Apply(Result(kFull));
    CHECK(bench.stage.TakeNotices().empty());
    bench.Advance(true);
    bench.stage.Invite("Neuf", bench.link);
    bench.Apply(Result(kOk));
    CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Neuf."}));
  }
}

TEST_CASE("Carnet MVP, étage : hors vérification, rien ne part et rien n'attend", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  // Le serveur jetterait la commande sans répondre : partie, elle prêterait
  // la réponse suivante à une autre.
  bench.Advance(false);
  bench.stage.Invite("Perdu", bench.link);
  // 🔴 UN GESTE QUI NE PART PAS SE DIT, avec sa raison.
  CHECK(bench.stage.TakeNotices() ==
        Lines({std::string("Carnet MVP : invitation à Perdu non envoyée — ") +
               ro::ui::MvpLinkPendingText()}));
  const std::string nothing_left =
      std::string("Carnet MVP : rien n'est parti — ") + ro::ui::MvpLinkPendingText();
  bench.windows.answer.kick_member = "Zed";
  bench.Advance(false);
  CHECK(bench.Silent());
  CHECK(bench.stage.TakeNotices() == Lines({nothing_left}));
  // La vue le sait : la fenêtre grise ce qui envoie.
  CHECK_FALSE(bench.windows.view->link_ready);
  // Une réponse refusée ne grise pas les boutons d'une invitation.
  bench.Apply(Group(1, 12, "Les Chasseurs"));
  bench.windows.answer.accept_invite = true;
  bench.Advance(false);
  CHECK(bench.Silent());
  CHECK_FALSE(bench.windows.view->invite_waiting);
  CHECK(bench.stage.TakeNotices() == Lines({nothing_left}));
  // Vérifié, la commande suivante reçoit SA réponse.
  bench.Advance(true);
  CHECK(bench.windows.view->link_ready);
  bench.stage.Invite("Bob", bench.link);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kInvite));
  bench.Apply(Result(kOk));
  CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Bob."}));
}

TEST_CASE("Carnet MVP, étage : un geste offert puis retenu par la liaison tombée se dit",
          "[app][mvp]") {
  // La fenêtre a dessiné la liaison vérifiée ; elle est tombée avant que ses
  // gestes partent.
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Catalogue(kSlots));
  bench.Advance(true);
  REQUIRE(bench.windows.view->link_ready);
  const std::string nothing_left =
      std::string("Carnet MVP : rien n'est parti — ") + ro::ui::MvpLinkPendingText();
  SECTION("les gestes du groupe et de la table : une ligne pour tous") {
    bench.windows.answer.create_group = "Chasse";
    bench.windows.answer.kick_member = "Zed";
    bench.windows.answer.leave_group = true;
    bench.windows.answer.dissolve_group = true;
    bench.windows.answer.favorite_slot = 2;
    bench.windows.answer.favorite_on = true;
    bench.Advance(false);
    CHECK(bench.Silent());
    CHECK(bench.stage.TakeNotices() == Lines({nothing_left}));
  }
  SECTION("chacun seul se dit aussi") {
    using Answer = ro::ui::MvpTrackerAction;
    const std::vector<void (*)(Answer&)> gestures = {
        [](Answer& answer) { answer.create_group = "Chasse"; },
        [](Answer& answer) { answer.kick_member = "Zed"; },
        [](Answer& answer) { answer.leave_group = true; },
        [](Answer& answer) { answer.dissolve_group = true; },
        [](Answer& answer) { answer.favorite_slot = 2; },
        [](Answer& answer) { answer.accept_invite = true; },
        [](Answer& answer) { answer.decline_invite = true; },
    };
    for (const auto gesture : gestures) {
      gesture(bench.windows.answer);
      bench.Advance(false);
      CHECK(bench.stage.TakeNotices() == Lines({nothing_left}));
    }
    CHECK(bench.Silent());
  }
  SECTION("la saisie dit son créneau") {
    bench.windows.answer.manual_slot = 1;
    bench.windows.answer.manual_kill_time_s = kLocalS - 600;
    bench.Advance(false);
    CHECK(bench.Silent());
    CHECK(bench.stage.TakeNotices() ==
          Lines({std::string("Carnet MVP : saisie de Baphomet non envoyée — ") +
                 ro::ui::MvpLinkPendingText()}));
  }
  SECTION("parti, il ne dit rien") {
    bench.windows.answer.kick_member = "Zed";
    bench.Advance(true);
    CHECK(bench.Next().cmd == Raw(MvpCommand::kKick));
    CHECK(bench.stage.TakeNotices().empty());
  }
}

TEST_CASE("Carnet MVP, étage : l'import hors vérification se dit, et n'ouvre rien", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Catalogue(kSlots));
  bench.Apply(GroupWith(9, 1, {}));
  const MvpLink link{.monster_id = 1039,
                     .map = "gef_dun03",
                     .kill_time_s = kLocalS - 600,
                     .tomb_x = 10,
                     .tomb_y = 20,
                     .name = "Baphomet"};
  bench.Advance(false);
  bench.stage.Import(link, "Sting", bench.link);
  CHECK(bench.Silent());
  CHECK(bench.stage.TakeNotices() ==
        Lines({std::string("Carnet MVP : saisie de Baphomet non envoyée — ") +
               ro::ui::MvpLinkPendingText()}));
  // Une ouverture sur la ligne ressemblerait à un succès.
  bench.Advance(false);
  CHECK_FALSE(bench.windows.window.open);
}

TEST_CASE("Carnet MVP, étage : une réponse perdue ne décale pas les suivantes", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Advance(true);
  bench.stage.Invite("Perdu", bench.link);
  const std::uint32_t later_ms = kNowMs + kMvpLostAnswerMs + 1;
  bench.Advance(true, later_ms);
  bench.stage.Invite("Bob", bench.link);
  SECTION("dans le délai, la réponse va encore à la plus ancienne") {
    bench.Apply(Result(kFull), kNowMs + kMvpLostAnswerMs);
    const std::vector<std::string> notices = bench.stage.TakeNotices();
    REQUIRE(notices.size() == 1);
    CHECK(notices[0].starts_with("Carnet MVP : invitation à Perdu refusée"));
  }
  SECTION("au-delà, la plus ancienne sort, et la suivante s'apparie juste") {
    bench.Apply(Result(kOk), later_ms);
    CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Bob."}));
  }
}

TEST_CASE("Carnet MVP, étage : une réponse en retard s'apparie encore à sa commande",
          "[app][mvp]") {
  // Les boutons se rendent après la patience ; la réponse, elle, n'est pas
  // perdue pour autant — le serveur écrit en base avant de répondre.
  static_assert(kMvpLostAnswerMs > kMvpAnswerPatienceMs);
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Advance(true);
  bench.stage.Invite("Lent", bench.link);
  const std::uint32_t late_ms = kNowMs + kMvpAnswerPatienceMs + 1;
  bench.Advance(true, late_ms);
  bench.stage.Invite("Bob", bench.link);
  bench.Apply(Result(kFull), late_ms);
  bench.Apply(Result(kOk), late_ms);
  const std::vector<std::string> notices = bench.stage.TakeNotices();
  REQUIRE(notices.size() == 2);
  CHECK(notices[0].starts_with("Carnet MVP : invitation à Lent refusée"));
  CHECK(notices[1] == "Carnet MVP : invitation envoyée à Bob.");
}

TEST_CASE("Carnet MVP, étage : le refus d'une réponse à l'invitation ne s'écrit que dans la modale",
          "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.windows.window.open = true;
  bench.Advance(true);
  bench.Apply(Group(1, 12, "Les Chasseurs"));
  bench.windows.answer.accept_invite = true;
  bench.Advance(true);
  bench.Apply(Result(kFull));
  bench.Advance(true);
  CHECK(bench.windows.view->invite_refusal == kFull);
  CHECK_FALSE(bench.windows.view->result_shown);
  // Le refus d'une autre commande allume le bandeau.
  bench.windows.answer.kick_member = "Zed";
  bench.Advance(true);
  bench.Apply(Result(kFull));
  bench.Advance(true);
  CHECK(bench.windows.view->result_shown);
}

TEST_CASE("Carnet MVP, étage : l'invitation attend le serveur", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Group(1, 12, "Les Chasseurs"));
  bench.windows.answer.accept_invite = true;
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kAcceptInvite));
  CHECK(bench.stage.book().pending_invite() == 12);
  CHECK(bench.windows.view->invite_waiting);
  CHECK(bench.windows.view->invite_refusal == 0);

  SECTION("la patience est bornée, la réponse tardive compte encore") {
    bench.Advance(true, kNowMs + kMvpAnswerPatienceMs - 1);
    CHECK(bench.windows.view->invite_waiting);
    bench.Advance(true, kNowMs + kMvpAnswerPatienceMs);
    CHECK_FALSE(bench.windows.view->invite_waiting);
    CHECK(bench.windows.view->pending_invite == 12);
    bench.Apply(Result(kOk), kNowMs + kMvpAnswerPatienceMs);
    bench.Advance(true, kNowMs + kMvpAnswerPatienceMs);
    CHECK(bench.windows.view->pending_invite == 0);
    CHECK(bench.windows.window.open);
  }
  SECTION("groupe plein : l'invitation reste, avec la raison") {
    bench.Apply(Result(kFull));
    bench.Advance(true);
    CHECK(bench.windows.view->pending_invite == 12);
    CHECK(bench.windows.view->invite_refusal == kFull);
    CHECK_FALSE(bench.windows.view->invite_waiting);
    CHECK_FALSE(bench.windows.view->invite_gone);
    CHECK_FALSE(bench.windows.window.open);
    // L'instantané qui renvoie la même invitation garde la raison.
    bench.Apply(Group(1, 12, "Les Chasseurs"));
    bench.Advance(true);
    CHECK(bench.windows.view->invite_refusal == kFull);
  }
  SECTION("invitation disparue : « Fermer » la congédie, tant que son groupe ne la repousse pas") {
    bench.Apply(Result(kNoInvite));
    bench.Advance(true);
    CHECK(bench.windows.view->invite_gone);
    bench.windows.answer.dismiss_invite = true;
    bench.Advance(true);
    CHECK(bench.windows.view->pending_invite == 0);
    CHECK_FALSE(bench.windows.view->invite_gone);
    // Rien ne part pour un congé.
    CHECK(bench.Silent());
    // 🔴 L'INSTANTANÉ LA RENVOIE, juste après la trame du groupe, à chaque
    // rafraîchissement : aussi longtemps qu'il dure, elle reste congédiée.
    constexpr std::uint32_t kSnapshots = 5;
    std::uint32_t now_ms = kNowMs;
    for (std::uint32_t snapshot = 0; snapshot < kSnapshots; ++snapshot) {
      now_ms += kMvpPresenceRefreshMs;
      ApplySnapshotReply(bench, now_ms, 12);
      bench.Advance(true, now_ms);
      CHECK(bench.windows.view->pending_invite == 0);
      CHECK(bench.windows.view->pending_invite_name.empty());
    }
    // LA POIGNÉE DE MAIN d'un autre serveur la repousse seule : la même.
    bench.Advance(false, now_ms);
    bench.Apply(Group(1, 12, "Les Chasseurs"), now_ms);
    bench.Advance(true, now_ms);
    CHECK(bench.windows.view->pending_invite == 0);
    SECTION("une autre invitation se montre, sans la raison de la précédente") {
      bench.Apply(Group(1, 13, "Les Autres"));
      bench.Advance(true, now_ms);
      CHECK(bench.windows.view->pending_invite == 13);
      CHECK(bench.windows.view->pending_invite_name == "Les Autres");
      CHECK(bench.windows.view->invite_refusal == 0);
    }
    SECTION("poussée par son groupe, la même se montre de nouveau") {
      bench.Apply(Group(1, 12, "Les Chasseurs"));
      bench.Advance(true, now_ms);
      CHECK(bench.windows.view->pending_invite == 12);
      CHECK(bench.windows.view->invite_refusal == 0);
    }
  }
  SECTION("un autre refus : « Fermer » sort aussi de la modale") {
    bench.Apply(Result(kFull));
    bench.windows.answer.dismiss_invite = true;
    bench.Advance(true);
    CHECK(bench.windows.view->pending_invite == 0);
    CHECK(bench.windows.view->invite_refusal == 0);
    CHECK(bench.Silent());
  }
}

TEST_CASE("Carnet MVP, étage : les alertes s'empilent, un instant exact dit son heure",
          "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  bench.Apply(Catalogue(kSlots));
  bench.Apply(
      Observations(1, {{.slot_id = 1, .source = 2, .kill_s = kKillS, .reported_s = 1},
                       {.slot_id = 2, .source = 3, .reported_s = 1, .exact_s = kLocalS + 120}}));
  bench.Apply(Favorites({1, 2}));
  bench.Advance(false);
  const ro::ui::MvpTrackerView& view = *bench.windows.view;
  REQUIRE(view.alerts.size() == 2);
  CHECK(view.alerts[0].slot_id == 1);
  CHECK_FALSE(view.alerts[0].exact);
  CHECK(view.alerts[0].at_clock.empty());
  CHECK(view.alerts[1].slot_id == 2);
  CHECK(view.alerts[1].exact);
  CHECK(view.alerts[1].at_clock == moonclient::MvpClockText(kLocalS + 120));

  SECTION("cliquée, l'alerte ouvre le carnet sur sa ligne, au premier plan, et s'éteint") {
    bench.windows.answer.alert_slot = 2;
    bench.Advance(false);
    CHECK(bench.windows.window.open);
    CHECK(bench.windows.window.raise);
    CHECK(bench.windows.window.focus_slot == 2);
    REQUIRE(bench.windows.view->alerts.size() == 1);
    CHECK(bench.windows.view->alerts[0].slot_id == 1);
  }
  SECTION("chacune s'éteint après son délai, la plus récente reste") {
    bench.Apply(Observations(2, {{.slot_id = 1, .source = 3, .kill_s = kKillS, .reported_s = 2}}));
    const std::uint32_t later_ms = kNowMs + 5000;
    bench.Advance(false, later_ms);
    bench.Advance(false, kNowMs + ro::ui::kMvpAlertShownMs + 1);
    REQUIRE(bench.windows.view->alerts.size() == 1);
    CHECK(bench.windows.view->alerts[0].slot_id == 1);
    bench.Advance(false, later_ms + ro::ui::kMvpAlertShownMs + 1);
    CHECK(bench.windows.view->alerts.empty());
  }
}

TEST_CASE("Carnet MVP, étage : la pile d'alertes est bornée, la plus ancienne sort", "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  std::vector<Slot> slots;
  std::vector<Seen> seen;
  std::vector<std::uint16_t> favorites;
  for (std::uint16_t slot_id = 1; slot_id <= kMvpMostAlertsShown + 1; ++slot_id) {
    slots.push_back({.slot_id = slot_id,
                     .mob_id = static_cast<std::uint16_t>(1000 + slot_id),
                     .delay1_ms = 3600000,
                     .map = "prt_maze03",
                     .name = "MVP"});
    seen.push_back({.slot_id = slot_id, .source = 2, .kill_s = kKillS, .reported_s = 1});
    favorites.push_back(slot_id);
  }
  bench.Apply(Catalogue(slots));
  bench.Apply(Observations(1, seen));
  bench.Apply(Favorites(favorites));
  bench.Advance(false);
  const ro::ui::MvpTrackerView& view = *bench.windows.view;
  REQUIRE(view.alerts.size() == kMvpMostAlertsShown);
  CHECK(view.alerts.front().slot_id == 2);
  CHECK(view.alerts.back().slot_id == kMvpMostAlertsShown + 1);
}

TEST_CASE("Carnet MVP, étage : une alerte rallumée passe en queue, et ne sort pas la première",
          "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  std::vector<Slot> slots;
  std::vector<Seen> seen;
  std::vector<std::uint16_t> favorites;
  const auto newest = static_cast<std::uint16_t>(kMvpMostAlertsShown + 1);
  for (std::uint16_t slot_id = 1; slot_id <= newest; ++slot_id) {
    slots.push_back({.slot_id = slot_id,
                     .mob_id = static_cast<std::uint16_t>(1000 + slot_id),
                     .delay1_ms = 3600000,
                     .map = "prt_maze03",
                     .name = "MVP"});
    if (slot_id != newest) {
      seen.push_back({.slot_id = slot_id, .source = 2, .kill_s = kKillS, .reported_s = 1});
    }
    favorites.push_back(slot_id);
  }
  bench.Apply(Catalogue(slots));
  bench.Apply(Observations(1, seen));
  bench.Apply(Favorites(favorites));
  bench.Advance(false);
  REQUIRE(bench.windows.view->alerts.size() == kMvpMostAlertsShown);
  CHECK(bench.windows.view->alerts.front().slot_id == 1);
  // Le premier se rallume — un miroir précise son kill —, puis un nouveau entre.
  bench.Apply(
      Observations(2, {{.slot_id = 1, .source = 3, .kill_s = kKillS, .reported_s = 2},
                       {.slot_id = newest, .source = 2, .kill_s = kKillS, .reported_s = 2}}));
  bench.Advance(false, kNowMs + 1000);
  const ro::ui::MvpTrackerView& view = *bench.windows.view;
  REQUIRE(view.alerts.size() == kMvpMostAlertsShown);
  // C'est le deuxième, désormais le plus ancien, qui sort.
  CHECK(view.alerts.front().slot_id == 3);
  CHECK(view.alerts[view.alerts.size() - 2].slot_id == 1);
  CHECK(view.alerts.back().slot_id == newest);
}

TEST_CASE("Carnet MVP, étage : toute ouverture met au premier plan", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.windows.window.open = true;
  bench.Advance(false);
  CHECK_FALSE(bench.windows.window.raise);
  bench.stage.RequestOpenOn(2);
  bench.Advance(false);
  CHECK(bench.windows.window.raise);
  CHECK(bench.windows.window.focus_slot == 2);
}

TEST_CASE("Carnet MVP, étage : la colonne « Délai » montrée à l'en-tête est un réglage",
          "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.windows.answer.delay_shown = true;
  CHECK(bench.Advance(false));
  CHECK(bench.options.settings().look.show_delay);
  // Déjà montrée : rien ne change.
  bench.windows.answer.delay_shown = true;
  CHECK_FALSE(bench.Advance(false));
  bench.windows.answer.delay_shown = false;
  CHECK(bench.Advance(false));
  CHECK_FALSE(bench.options.settings().look.show_delay);
}

TEST_CASE("Carnet MVP, étage : la présence se redemande, carnet ouvert et déplié", "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  bench.windows.window.open = true;
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  bench.Advance(true, kNowMs + kMvpPresenceRefreshMs - 1);
  CHECK(bench.Silent());
  SECTION("ouvert : un nouvel instantané au rythme") {
    bench.Advance(true, kNowMs + kMvpPresenceRefreshMs);
    CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
    bench.Advance(true, kNowMs + kMvpPresenceRefreshMs + 1);
    CHECK(bench.Silent());
  }
  SECTION("replié : rien") {
    bench.windows.window.was_collapsed = true;
    bench.Advance(true, kNowMs + 2 * kMvpPresenceRefreshMs);
    CHECK(bench.Silent());
  }
  SECTION("fermé : rien") {
    bench.windows.window.open = false;
    bench.Advance(true, kNowMs + 2 * kMvpPresenceRefreshMs);
    CHECK(bench.Silent());
  }
}

TEST_CASE("Carnet MVP, étage : la tombe de la minimap nomme, date et vieillit", "[app][mvp]") {
  Bench bench;
  bench.Apply(Catalogue(kSlots));
  const std::int64_t reported_s = kLocalS - 120;
  bench.Apply(Observations(1, {{.slot_id = 1,
                                .source = 1,
                                .kill_s = kLocalS - 600,
                                .reported_s = reported_s,
                                .tomb_x = 150,
                                .tomb_y = 0},
                               {.slot_id = 2,
                                .source = 3,
                                .reported_s = reported_s,
                                .tomb_x = 10,
                                .tomb_y = 20,
                                .exact_s = kLocalS + 60}}));
  const std::span<const ro::ui::MinimapTomb> tombs =
      bench.stage.TombsOn("gef_dun03", true, kLocalS);
  REQUIRE(tombs.size() == 1);
  CHECK(tombs[0].slot_id == 1);
  CHECK(tombs[0].label == "Baphomet");
  const std::int64_t from_s = kLocalS - 600 + 3600;
  CHECK(tombs[0].window_line == "Retour entre " + moonclient::MvpClockText(from_s) + " et " +
                                    moonclient::MvpClockText(from_s + 600));
  CHECK(tombs[0].age_line == "Signalée il y a " + ro::ui::FormatMvpDuration(120));
  // UN INSTANT EXACT se dit comme dans l'aperçu d'un lien.
  const std::span<const ro::ui::MinimapTomb> exact =
      bench.stage.TombsOn("moc_pryd06", true, kLocalS);
  REQUIRE(exact.size() == 1);
  CHECK(exact[0].slot_id == 2);
  CHECK(exact[0].window_line ==
        "Retour à " + moonclient::MvpClockText(kLocalS + 60) + " (Convex Mirror)");
}

TEST_CASE("Carnet MVP, étage : un lien échu le dit, un lien frais non", "[app][mvp]") {
  const MvpTrackerStage stage;
  const MvpLink stale{.monster_id = 1039,
                      .map = "gef_dun03",
                      .kill_time_s = kLocalS - 3 * 3600,
                      .delay1_min = 60,
                      .delay2_min = 10,
                      .name = "Baphomet"};
  // Une fenêtre passée : le retour n'est que probable.
  CHECK(stage.TipOf(stale, kLocalS).passed_line == "Passé : le MVP est sans doute déjà revenu.");
  MvpLink fresh = stale;
  fresh.kill_time_s = kLocalS - 600;
  CHECK(stage.TipOf(fresh, kLocalS).passed_line.empty());
  // Un instant exact passé : le retour est sûr.
  MvpLink mirrored = fresh;
  mirrored.exact_respawn_s = kLocalS - 60;
  CHECK(stage.TipOf(mirrored, kLocalS).passed_line == "Passé : le MVP est revenu.");
}

TEST_CASE("Carnet MVP, étage : le Maj+clic d'un nom partage le respawn s'il y a une heure",
          "[app][mvp]") {
  constexpr std::string_view kShare = "Maj+clic : partager le respawn dans le chat";
  constexpr std::string_view kLink = "Maj+clic : lien dans le chat";
  CHECK(moonclient::MvpNameHint(nullptr) == kLink);
  ro::ui::MvpSlotView row;
  CHECK(moonclient::MvpNameHint(&row) == kLink);
  row.kill_time_s = kLocalS - 600;
  CHECK(moonclient::MvpNameHint(&row) == kShare);
  row.kill_time_s = 0;
  row.exact_respawn_s = kLocalS + 600;
  CHECK(moonclient::MvpNameHint(&row) == kShare);
}

TEST_CASE("Carnet MVP, étage : « Y aller » vers la carte quand la tombe est inconnue",
          "[app][mvp]") {
  MvpTrackerStage stage;
  const ro::ui::MvpLinkMenuFacts* mapped = stage.FactsFor(MvpLink{.map = "gef_dun03"});
  CHECK(mapped->map_known);
  CHECK_FALSE(mapped->tomb_known);
  const ro::ui::MvpLinkMenuFacts* blank = stage.FactsFor(MvpLink{.tomb_x = 1, .tomb_y = 2});
  CHECK_FALSE(blank->map_known);
  CHECK_FALSE(blank->tomb_known);
}

TEST_CASE("Carnet MVP, étage : le nom lisible de la carte vient de la table du jeu", "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  bench.Apply(Catalogue(kSlots));
  bench.Apply(Observations(1, {{.slot_id = 1, .source = 2, .kill_s = kKillS, .reported_s = 1}}));
  bench.Apply(Favorites({1}));
  SECTION("sans table : vide") {
    bench.Advance(false);
    CHECK(bench.windows.view->slots[0].map_label.empty());
    CHECK(bench.windows.view->alerts[0].map == "gef_dun03");
  }
  SECTION("avec table : le nom lisible, et rien pour une carte qu'elle ignore") {
    auto names =
        ro::vfs::MapDisplayNames::Parse("version: 1\nmaps:\n  gef_dun03:\n    name: Glast Heim\n");
    REQUIRE(names.has_value());
    bench.stage.See(MvpTrackerPieces{.map_names = &*names});
    bench.Advance(false);
    const ro::ui::MvpTrackerView& view = *bench.windows.view;
    CHECK(view.slots[0].map_label == "Glast Heim");
    CHECK(view.slots[0].map == "gef_dun03");
    CHECK(view.slots[1].map_label.empty());
    REQUIRE(view.alerts.size() == 1);
    CHECK(view.alerts[0].map == "Glast Heim");
  }
}
