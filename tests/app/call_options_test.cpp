// L'APPEL D'URGENCE DANS « GAMEPLAY » : une case qui montre ce que le serveur
// tient, et dont le clic ne fait que demander.

#include "options/call_options.h"

#include <optional>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::CallOptions;
using moonclient::CallRow;
using moonclient::kCallRowBase;

namespace {

[[nodiscard]] ro::ui::OptionRowView RowOf(const CallOptions& page) {
  std::vector<ro::ui::OptionRowView> rows;
  page.ComposeInto(rows);
  REQUIRE(rows.size() == 1);
  return rows[0];
}

[[nodiscard]] ro::ui::OptionsWindowAction Clicked(bool on) {
  ro::ui::OptionsWindowAction asked;
  asked.changed = kCallRowBase + static_cast<int>(CallRow::kCallable);
  asked.on = on;
  return asked;
}

}  // namespace

TEST_CASE("Appel d'urgence : grisée et cochée tant que le serveur se tait", "[app][call]") {
  CallOptions page;
  const ro::ui::OptionRowView row = RowOf(page);
  CHECK(row.disabled);
  // Le défaut du client d'origine.
  CHECK(row.on);
  // 🔴 UN CLIC ARRIVÉ QUAND MÊME NE PART PAS : il écraserait le vrai réglage.
  CHECK(page.Change(Clicked(false)));
  CHECK_FALSE(page.TakeRequest().has_value());
}

TEST_CASE("Appel d'urgence : la case suit le serveur, et le clic demande une fois", "[app][call]") {
  CallOptions page;
  page.NoteCallable(false);
  ro::ui::OptionRowView row = RowOf(page);
  CHECK_FALSE(row.disabled);
  CHECK_FALSE(row.on);

  REQUIRE(page.Change(Clicked(true)));
  CHECK(page.TakeRequest() == std::optional<bool>(true));
  CHECK_FALSE(page.TakeRequest().has_value());
  // ⚠ RIEN NE CHANGE AVANT L'ÉCHO : la case montre encore ce que le serveur a dit.
  CHECK_FALSE(RowOf(page).on);

  page.NoteCallable(true);
  CHECK(RowOf(page).on);
}

TEST_CASE("Appel d'urgence : /call demande l'inverse de ce que le serveur a dit", "[app][call]") {
  CallOptions page;
  // 🔴 RIEN TANT QUE LE SERVEUR SE TAIT : basculer un réglage inconnu
  // reviendrait à en choisir un au hasard.
  CHECK_FALSE(page.RequestToggle());
  CHECK_FALSE(page.TakeRequest().has_value());

  page.NoteCallable(true);
  REQUIRE(page.RequestToggle());
  CHECK(page.TakeRequest() == std::optional<bool>(false));

  page.NoteCallable(false);
  REQUIRE(page.RequestToggle());
  CHECK(page.TakeRequest() == std::optional<bool>(true));
  // La ligne nomme sa commande.
  CHECK(std::string_view(RowOf(page).command) == "/call");
}

TEST_CASE("Appel d'urgence : une autre ligne n'est pas la sienne", "[app][call]") {
  CallOptions page;
  page.NoteCallable(true);
  ro::ui::OptionsWindowAction other = Clicked(false);
  other.changed = kCallRowBase + static_cast<int>(CallRow::kCount);
  CHECK_FALSE(page.Change(other));
  CHECK_FALSE(page.TakeRequest().has_value());
}
