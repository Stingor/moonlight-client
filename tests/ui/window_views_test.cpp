#include <catch2/catch_test_macros.hpp>

#include "ro/ui/cart_window.h"
#include "ro/ui/character_staff.h"
#include "ro/ui/character_window.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/skill_window.h"
#include "ro/ui/storage_window.h"

// 🔴 `ui` NE POSSÈDE AUCUN ÉTAT DE JEU. Ce qu'une fenêtre sait du monde et de ses
// sœurs lui descend par sa VUE, recomposée à chaque image par `app` ; son ÉTAT ne
// garde que ce qu'elle décide elle-même d'une image à l'autre. Un fait du monde
// recopié dans l'état se lirait encore après que le monde a changé.
//
// ⚠ CHAQUE CHAMP SE VÉRIFIE DES DEUX CÔTÉS : absent de l'état, présent dans la
// vue. Sans le second, une faute de frappe dans le concept rendrait le premier
// vrai sans rien prouver.

namespace {

template <typename T>
concept CarriesCartAvailable = requires(T window) { window.cart_available; };
template <typename T>
concept CarriesStorageOpen = requires(T window) { window.storage_open; };
template <typename T>
concept CarriesCartOpen = requires(T window) { window.cart_open; };
template <typename T>
concept CarriesInventoryOpen = requires(T window) { window.inventory_open; };
template <typename T>
concept CarriesQuickInsert = requires(T window) {
  window.quick_insert_equip;
  window.quick_insert_answered;
  window.quick_insert_cards;
};
template <typename T>
concept CarriesStorageName = requires(T window) { window.name; };
template <typename T>
concept CarriesShelves = requires(T window) {
  window.shelves;
  window.open_shelf;
};
template <typename T>
concept CarriesSubCategories = requires(T window) { window.sub_categories; };
template <typename T>
concept CarriesStaffAllowed = requires(T window) { window.staff_allowed; };
template <typename T>
concept CarriesCompanions = requires(T window) { window.companions; };
template <typename T>
concept CarriesShownFlags = requires(T window) {
  window.equipment_shown;
  window.costumes_shown;
};
template <typename T>
concept CarriesAvatarImage = requires(T window) { window.avatar_image; };
template <typename T>
concept CarriesImage = requires(T window) { window.image; };
template <typename T>
concept CarriesStaffWorld = requires(T window) {
  window.jobs;
  window.own_job;
  window.riding;
  window.falcon;
  window.cart_level;
  window.server_speed;
};
template <typename T>
concept CarriesStaff = requires(T window) { window.staff; };

}  // namespace

TEST_CASE("Sac : ce qu'il sait du monde vit dans sa vue, pas dans son état", "[ui][sac]") {
  using State = ro::ui::InventoryWindowState;
  using View = ro::ui::InventoryWindowView;
  CHECK_FALSE(CarriesCartAvailable<State>);
  CHECK(CarriesCartAvailable<View>);
  CHECK_FALSE(CarriesStorageOpen<State>);
  CHECK(CarriesStorageOpen<View>);
  // L'état de la charrette est celui d'une AUTRE fenêtre : le sac le lit, il ne
  // le tient pas.
  CHECK_FALSE(CarriesCartOpen<State>);
  CHECK(CarriesCartOpen<View>);
  CHECK_FALSE(CarriesQuickInsert<State>);
  CHECK(CarriesQuickInsert<View>);
}

TEST_CASE("Charrette : ce qu'elle sait du monde vit dans sa vue, pas dans son état",
          "[ui][charrette]") {
  using State = ro::ui::CartWindowState;
  using View = ro::ui::CartWindowView;
  CHECK_FALSE(CarriesStorageOpen<State>);
  CHECK(CarriesStorageOpen<View>);
  CHECK_FALSE(CarriesInventoryOpen<State>);
  CHECK(CarriesInventoryOpen<View>);
}

TEST_CASE("Coffre : ce qu'il sait du monde vit dans sa vue, pas dans son état", "[ui][coffre]") {
  using State = ro::ui::StorageWindowState;
  using View = ro::ui::StorageWindowView;
  // Le nom que le serveur donne au coffre ouvert, et les coffres qu'on peut
  // ouvrir : c'est la session du monde qui les tient.
  CHECK_FALSE(CarriesStorageName<State>);
  CHECK(CarriesStorageName<View>);
  CHECK_FALSE(CarriesShelves<State>);
  CHECK(CarriesShelves<View>);
  CHECK_FALSE(CarriesSubCategories<State>);
  CHECK(CarriesSubCategories<View>);
  CHECK_FALSE(CarriesCartAvailable<State>);
  CHECK(CarriesCartAvailable<View>);
  CHECK_FALSE(CarriesInventoryOpen<State>);
  CHECK(CarriesInventoryOpen<View>);
  CHECK_FALSE(CarriesCartOpen<State>);
  CHECK(CarriesCartOpen<View>);
}

TEST_CASE("Fiche : ce qu'elle sait du monde vit dans sa vue, pas dans son état", "[ui][fiche]") {
  using State = ro::ui::CharacterWindowState;
  using View = ro::ui::CharacterWindowView;
  // Le droit du staff : relu à chaque image, il ne se retient pas.
  CHECK_FALSE(CarriesStaffAllowed<State>);
  CHECK(CarriesStaffAllowed<View>);
  CHECK_FALSE(CarriesCompanions<State>);
  CHECK(CarriesCompanions<View>);
  // Ce que le serveur dit des deux cases : la fenêtre demande, l'écho répond.
  CHECK_FALSE(CarriesShownFlags<State>);
  CHECK(CarriesShownFlags<View>);
  // La silhouette composée par `app` ; la place de l'avatar ne garde que sa
  // boîte, sa pose et sa direction.
  CHECK_FALSE(CarriesImage<ro::ui::CharacterAvatarState>);
  CHECK(CarriesAvatarImage<View>);
}

TEST_CASE("Volet du staff : ce qu'il sait du monde vit dans sa vue", "[ui][fiche][staff]") {
  CHECK_FALSE(CarriesStaffWorld<ro::ui::StaffPaneState>);
  CHECK(CarriesStaffWorld<ro::ui::StaffPaneView>);
}

TEST_CASE("Grimoire : le droit du staff lui descend, il ne le garde pas", "[ui][grimoire]") {
  // ⚠ LE DROIT EST UN PARAMÈTRE DE `DrawGrimoireTab`, celui de la fiche. La
  // contre-épreuve prend la fiche, dont `staff` est le volet : le concept se
  // vérifie donc sur un type qui porte bien ce nom.
  CHECK_FALSE(CarriesStaff<ro::ui::SkillWindowState>);
  CHECK(CarriesStaff<ro::ui::CharacterWindowState>);
}
