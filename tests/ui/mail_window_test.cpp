// LES RÈGLES DE LA BOÎTE DE COURRIER qui ne se voient pas d'un coup d'œil.
//
// 🔴 L'ONGLET « COMPTE » SE MASQUE TANT QU'IL EST VIDE, et l'onglet courant se
// replie sur « Reçus » quand il s'efface sous le joueur : un onglet masqué mais
// courant laisserait une liste vide sans onglet pour la nommer.

#include "ro/ui/mail_window.h"

#include <array>
#include <cstring>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::ui::MailBox;
using ro::ui::MailboxTab;
using ro::ui::MailboxWindowState;
using ro::ui::MailConfirm;
using ro::ui::MailRef;
using ro::ui::MailRowView;

namespace {

[[nodiscard]] MailRowView Row(std::int64_t mail_id, MailBox box, std::uint8_t carried = 0) {
  MailRowView row;
  row.ref = MailRef{.mail_id = mail_id, .box = box};
  row.carried = carried;
  row.sender = "Untel";
  row.title = "Bonjour";
  return row;
}

}  // namespace

TEST_CASE("L'onglet Compte se masque vide, et l'onglet courant se replie") {
  const std::array<MailRowView, 2> rows = {Row(1, MailBox::kNormal), Row(2, MailBox::kReturned)};
  const ro::ui::MailboxTabs tabs = ro::ui::CountMailboxTabs(rows);
  CHECK_FALSE(tabs.show_account);
  CHECK(tabs.counts[static_cast<std::size_t>(MailboxTab::kAll)] == 2);

  MailboxWindowState state;
  state.tab = MailboxTab::kAccount;
  state.selected = MailRef{.mail_id = 9, .box = MailBox::kAccount};
  state.confirm = MailConfirm::kDelete;
  ro::ui::SettleMailboxTab(state, tabs);
  CHECK(state.tab == MailboxTab::kNormal);
  CHECK_FALSE(state.selected.has_value());
  CHECK(state.confirm == MailConfirm::kNone);

  // Un courrier de compte arrivé : l'onglet revient, et l'on y reste.
  const std::array<MailRowView, 1> account = {Row(3, MailBox::kAccount)};
  const ro::ui::MailboxTabs with_account = ro::ui::CountMailboxTabs(account);
  CHECK(with_account.show_account);
  state.tab = MailboxTab::kAccount;
  ro::ui::SettleMailboxTab(state, with_account);
  CHECK(state.tab == MailboxTab::kAccount);
}

TEST_CASE("Le filtre porte sur l'expéditeur OU le sujet, sans égard à la casse") {
  const MailRowView row = Row(1, MailBox::kNormal);
  CHECK(ro::ui::MailRowShown(row, MailboxTab::kNormal, "unt"));
  CHECK(ro::ui::MailRowShown(row, MailboxTab::kNormal, "BONJ"));
  CHECK_FALSE(ro::ui::MailRowShown(row, MailboxTab::kNormal, "zzz"));
  CHECK_FALSE(ro::ui::MailRowShown(row, MailboxTab::kReturned, ""));
  CHECK(ro::ui::MailRowShown(row, MailboxTab::kAll, ""));
}

TEST_CASE("L'expiration proche ne s'alarme que s'il reste quelque chose à récupérer") {
  constexpr std::int64_t kDay = std::int64_t{24} * 60 * 60;
  MailRowView row = Row(1, MailBox::kNormal, ro::ui::kMailCarriesItems);
  row.expire_seconds_left = 2 * kDay;
  CHECK(ro::ui::MailExpiresSoon(row));
  CHECK(ro::ui::MailExpiryLabel(row.expire_seconds_left) == "J-2");

  row.carried = 0;
  CHECK_FALSE(ro::ui::MailExpiresSoon(row));

  row.carried = ro::ui::kMailCarriesZeny;
  row.expire_seconds_left = 3 * kDay;
  CHECK_FALSE(ro::ui::MailExpiresSoon(row));

  // Sous un jour, l'heure — jamais zéro ; épuisé, « expiré » ; sans expiration,
  // rien.
  CHECK(ro::ui::MailExpiryLabel(std::int64_t{60}) == "1 h");
  CHECK(ro::ui::MailExpiryLabel(std::int64_t{0}) == "expiré");
  CHECK(ro::ui::MailExpiryLabel(std::nullopt).empty());
}

TEST_CASE("Récupérer tout vise les courriers montrés qui portent encore quelque chose") {
  const std::array<MailRowView, 3> rows = {Row(1, MailBox::kNormal, ro::ui::kMailCarriesZeny),
                                           Row(2, MailBox::kNormal, ro::ui::kMailFromSystem),
                                           Row(3, MailBox::kReturned, ro::ui::kMailCarriesItems)};
  MailboxWindowState state;
  state.tab = MailboxTab::kNormal;
  const std::vector<MailRef> normal = ro::ui::MailClaimTargets(rows, state);
  REQUIRE(normal.size() == 1);
  CHECK(normal.front().mail_id == 1);

  state.tab = MailboxTab::kAll;
  CHECK(ro::ui::MailClaimTargets(rows, state).size() == 2);

  const char* filter = "personne";
  std::memcpy(state.filter.data(), filter, std::strlen(filter));
  CHECK(ro::ui::MailClaimTargets(rows, state).empty());
}

TEST_CASE("Supprimer tout ne vise que les courriers montrés qui ne portent plus rien") {
  const std::array<MailRowView, 3> rows = {Row(1, MailBox::kNormal, ro::ui::kMailCarriesZeny),
                                           Row(2, MailBox::kNormal, ro::ui::kMailFromSystem),
                                           Row(3, MailBox::kReturned)};
  MailboxWindowState state;
  state.tab = MailboxTab::kNormal;
  const std::vector<MailRef> normal = ro::ui::MailDeleteTargets(rows, state);
  REQUIRE(normal.size() == 1);
  CHECK(normal.front().mail_id == 2);

  state.tab = MailboxTab::kAll;
  CHECK(ro::ui::MailDeleteTargets(rows, state).size() == 2);

  const char* filter = "personne";
  std::memcpy(state.filter.data(), filter, std::strlen(filter));
  CHECK(ro::ui::MailDeleteTargets(rows, state).empty());
}

TEST_CASE("« Retourner » ne vaut que pour le courrier d'un joueur, dans la réception") {
  CHECK(ro::ui::MailCanReturn(Row(1, MailBox::kNormal)));
  CHECK_FALSE(ro::ui::MailCanReturn(Row(1, MailBox::kReturned)));
  CHECK_FALSE(ro::ui::MailCanReturn(Row(1, MailBox::kNormal, ro::ui::kMailFromSystem)));
}

TEST_CASE("Le destinataire pré-rempli tient 23 caractères 1252 et se coupe entre deux caractères") {
  ro::ui::MailComposeWindowState state;
  // Vingt-trois « € » : trois octets chacun en UTF-8, le pire cas du 1252.
  std::string longest;
  for (std::size_t letter = 0; letter < ro::ui::kMailRecipientMostChars; ++letter) {
    longest += "\xE2\x82\xAC";
  }
  ro::ui::OpenMailCompose(state, longest);
  CHECK(std::string(state.recipient.data()) == longest);

  // Un de plus ne tient pas : la coupe tombe AVANT le caractère entamé.
  ro::ui::OpenMailCompose(state, longest + "\xE2\x82\xAC");
  CHECK(std::string(state.recipient.data()) == longest);
  // Décalé d'un octet, le dernier « € » serait coupé en deux : il tombe entier.
  ro::ui::OpenMailCompose(state, "a" + longest);
  CHECK(std::string(state.recipient.data()) == "a" + longest.substr(0, longest.size() - 3));
}
