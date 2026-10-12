#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/game_menu.h"
#include "ro/ui/hotkey_window.h"

#include "chat/macro_commands.h"
#include "game/settings_file.h"
#include "game/settings_profiles.h"
#include "game/ui_stage.h"
#include "input/camera_rules.h"
#include "input/hotkeys.h"
#include "items/item_layout.h"
#include "items/storage_favorites.h"
#include "items/storage_tab_names.h"
#include "options/game_options.h"

namespace moonclient {

// LES RÉGLAGES, VUS DE LA BOUCLE : ce que les fenêtres montrent, et ce que le
// menu déclenche.
//
// ⚠ C'est ici que la table des raccourcis devient des LIGNES — un nom d'action,
// un libellé de touche — parce que `ui` ne sait pas ce qu'est un code de touche
// et n'a pas à l'apprendre. Même frontière que partout ailleurs.
//
// ⚠ LES TOUCHES SUIVENT LE PROFIL DE RÉGLAGES, comme les options hors de la
// machine : deux personnages sur un même profil ont les mêmes. `hotkeys.yml`
// ne fait plus que les donner avant l'entrée en jeu, et aux profils qui n'en
// ont pas. Voir `SettingsProfilePort`.
class GameSettings {
 public:
  // La racine du client : c'est sous elle que vit `savedata`.
  explicit GameSettings(std::filesystem::path client_root);

  [[nodiscard]] Hotkeys& bindings() noexcept { return bindings_; }

  // OÙ LE JOUEUR A POSÉ SES OBJETS. ⚠ ELLE VIT ICI parce que c'est ici qu'on
  // écrit sur le disque : la vue du sac s'en sert, elle ne la possède pas.
  [[nodiscard]] const ItemLayout& item_layout() const noexcept { return item_layout_; }

  // POSE UN OBJET SUR UNE CASE, et l'écrit. ⚠ `sitting_there` est ce qui s'y
  // trouvait : les deux s'ÉCHANGENT, voir `ItemLayout::Place`.
  void PlaceItem(std::uint32_t item_id, int cell, std::uint32_t sitting_there);
  // Repose la grille à plat. ⚠ ÉCRIT AUSSI : un oubli qui ne survit pas au
  // redémarrage n'en est pas un.
  void ForgetItemPlaces();

  // LES FAVORIS DU COFFRE, et ils sont à NOUS : le serveur n'en marque aucun
  // dans un coffre. Voir `items/storage_favorites.h`.
  [[nodiscard]] const StorageFavorites& storage_favorites() const noexcept {
    return storage_favorites_;
  }
  // Bascule, et écrit. ⚠ TOUT DE SUITE, comme le placement d'un objet : un clic
  // est un geste isolé, il n'y a pas de rafale à amortir.
  void ToggleStorageFavorite(std::uint32_t item_id);

  // LE NOM ET L'ICÔNE QUE LE JOUEUR DONNE À SES COFFRES. ⚠ ENTIÈREMENT CLIENT :
  // le serveur appelle le sien « Storage Alt 3 » et n'en saura jamais rien.
  [[nodiscard]] const StorageTabNames& storage_tab_names() const noexcept {
    return storage_tab_names_;
  }
  void NameStorageShelf(std::uint8_t storage_id, std::string_view name);
  void DecorateStorageShelf(std::uint8_t storage_id, std::uint32_t item_id);
  // Rend le coffre à son nom de serveur et lui retire son icône, d'un seul
  // enregistrement.
  void ForgetStorageShelf(std::uint8_t storage_id);
  // BLOQUE OU DÉBLOQUE LE CLIC GAUCHE SUR CE PNJ, et écrit la liste. Rend ce
  // qu'il est DEVENU. Voir `world/blocked_npcs.h`.
  bool ToggleBlockedNpc(std::uint32_t id, std::string_view name);

  [[nodiscard]] GameOptions& options() noexcept { return options_; }
  [[nodiscard]] SettingsFile& file() noexcept { return file_; }
  // Les profils de réglages. Voir `SettingsProfiles`.
  [[nodiscard]] SettingsProfiles& profiles() noexcept { return profiles_; }

  // Pose dans le moteur les options qui viennent d'être lues.
  //
  // ⚠ UNE FOIS QUE TOUT EST OUVERT, et non à la construction : le mélangeur, la
  // caméra et l'interface n'existent pas encore quand le fichier se lit.
  void ApplyOptions(const OptionTargets& to);

  // Refait les lignes que les fenêtres montrent. ⚠ SEULEMENT CELLES QUI SONT
  // OUVERTES : composer des libellés pour une fenêtre fermée serait du temps
  // donné à rien.
  void Refresh(UiStage::Windows& windows, std::uint32_t now_ms);

  [[nodiscard]] ro::ui::HotkeyWindowView hotkey_view() const noexcept {
    return ro::ui::HotkeyWindowView{.rows = rows_, .tabs = tab_names_};
  }

  // Ce que le menu du clic droit des macros propose, DÉJÀ TRADUIT. Recomposé
  // par `Refresh`, et seulement quand cette fenêtre-là est ouverte.
  [[nodiscard]] std::span<const ro::ui::ChatMacroCommandGroup> macro_commands() const noexcept {
    return macro_commands_.groups();
  }

  // Fait ce que le joueur a demandé au menu, à la configuration et aux options.
  //
  // ⚠ ENREGISTRE DÈS QUE QUELQUE CHOSE BOUGE. Un réglage qu'il faut penser à
  // enregistrer est un réglage qu'on perd.
  void Apply(const UiStage::Answers& answers, const OptionTargets& to, UiStage::Windows& windows,
             bool* running, std::uint32_t now_ms);

  // ── LES DIX MACROS DE CHAT ────────────────────────────────────────
  // Le fichier les descend UNE FOIS dans les tampons de la fenêtre, au
  // démarrage ; ensuite ce sont les tampons qui font foi.
  //
  // 🔴 PAS DE DESCENTE À CHAQUE IMAGE, à rebours du sol uni : un `InputText`
  // édite le tampon qu'on lui a donné, et le réécrire sous lui effacerait la
  // frappe en cours. La remontée, elle, se fait sur DEMANDE de la fenêtre —
  // comparer dix chaînes par image coûterait plus que le drapeau qu'elle rend
  // déjà.
  void RestoreChatMacros(UiStage::Windows& windows) const;
  void NoteChatMacros(const UiStage::Windows& windows);

  // CE QUE L'ÉTABLI DU STAFF VIENT DE RÉGLER, remonté dans le fichier.
  //
  // 🔴 APRÈS LE DESSIN, et c'est l'autre moitié de la descente faite par
  // `Refresh` : entre les deux, la fenêtre a pu cocher la case ou changer la
  // couleur. Un relevé plutôt qu'un drapeau, pour la même raison que la place
  // de la fenêtre — un réglage de plus dans cette section n'aurait rien à
  // penser à lever.
  //
  // ⚠ L'ÉCRITURE EST DIFFÉRÉE : `Touch` relance l'anti-rebond, si bien qu'un
  // nuancier qu'on promène ne produit qu'une écriture.
  void NoteStaffTools(const UiStage::Windows& windows);

  // LA CASE « LISSER LES ICÔNES » DE L'EN-TÊTE DU GRIMOIRE, remontée dans le
  // fichier. Même aller-retour que l'établi : `Refresh` descend la valeur du
  // fichier dans l'état de la fenêtre, le dessin la laisse cocher, et ceci la
  // remonte — dans cet ordre, sinon la descente de l'image suivante effacerait
  // le clic avant qu'on l'ait lu.
  void NoteGrimoireIcons(const UiStage::Windows& windows);

  // Recul et inclinaison du joueur, dehors et dedans, d'une partie à l'autre.
  [[nodiscard]] const CameraMemory& camera_memory() const noexcept { return camera_memory_; }
  void NoteCameraMemory(const CameraMemory& seen);

  // OÙ LA POIGNÉE DU BANDEAU D'OBTENTION VIENT D'ÊTRE POSÉE.
  //
  // ⚠ UNE POSITION POSÉE À LA SOURIS S'ÉCRIT, MAIS PAS À CHAQUE IMAGE :
  // `Touch` arme l'anti-rebond du fichier, si bien qu'un glisser de deux
  // secondes ne produit qu'une écriture.
  void NoteItemObtainAnchor(int pos_x, int pos_y);

  // RETIENT SI LE VOLET DU STAFF ÉTAIT OUVERT. ⚠ MUET SANS LE DROIT : le volet
  // est alors caché par force, et l'enregistrer effacerait le souvenir.
  void NoteCharacterStaffPane(const UiStage::Windows& windows);

  // RETIENT SI LE STAFF VEUT ÊTRE IGNORÉ DES MONSTRES, d'une connexion à l'autre.
  void NoteStaffBattleIgnore(bool wanted);

  // RETIENT LA POSE DE L'AVATAR ET SA DIRECTION. ⚠ SANS GARDE, contrairement au
  // volet : elles ne dépendent d'aucun droit, et une fiche jamais ouverte les
  // laisse telles qu'elles étaient.
  void NoteCharacterAvatar(const UiStage::Windows& windows);

  // 🔴 LE FILET SE DÉTEND MÊME SI PERSONNE NE REGARDE. La boîte qui pose la
  // question n'est dessinée que si le panneau est ouvert ; l'échéance, elle,
  // doit tomber quoi qu'il arrive — c'est précisément quand on ne voit rien
  // qu'elle sert.
  void TickDisplayRevert(const OptionTargets& to, std::uint32_t now_ms);

  // OÙ EST LA FENÊTRE ? Relevé à chaque image et comparé au souvenir.
  //
  // 🔴 UN RELEVÉ ET NON UN ÉVÉNEMENT, et c'est un choix. Une fenêtre bouge de
  // bien des façons — un glisser, un raccourci du système, un accrochage au
  // bord, un écran débranché —, et toutes ne passent pas par le même événement.
  // Lire la position coûte moins qu'un test de plus dans le routeur, et ne peut
  // rien manquer.
  //
  // ⚠ L'ÉCRITURE, ELLE, EST DIFFÉRÉE : `Touch` relance l'anti-rebond, qui
  // n'écrit qu'une fois le joueur arrêté. Un glisser touche donc soixante fois
  // par seconde et n'écrit qu'une.
  void NoteWindowPlace(const OptionTargets& to);

  // OUVRE LE PANNEAU SUR UNE SECTION. C'est ce qu'honore la puce d'une barre de
  // titre : le joueur vient d'une fenêtre, et il doit tomber sur SES réglages.
  //
  // ⚠ LA SECTION EST POSÉE MÊME SI LE PANNEAU ÉTAIT DÉJÀ OUVERT : un second clic
  // sur la puce depuis une autre fenêtre doit changer de section, pas ne rien
  // faire.
  //
  // `row`, quand la fenêtre n'est qu'un bloc de la section : la rangée qui
  // porte son intertitre, amenée en haut de la page.
  static void OpenOptionsAt(UiStage::Windows& windows, OptionSection section, int row = -1);

  // BASCULE UN INTERRUPTEUR DEPUIS AILLEURS QUE LE PANNEAU — le pied d'une
  // fenêtre, un raccourci.
  //
  // 🔴 PAR LE MÊME CHEMIN QUE LE PANNEAU, et c'est tout l'intérêt : écrire le
  // champ à la main ferait une seconde source de vérité, le fichier ne saurait
  // rien, et le panneau montrerait l'ancienne valeur jusqu'à sa prochaine
  // composition.
  void SetSwitch(GameOption which, bool on, const OptionTargets& to, std::uint32_t now_ms);
  // La même chose pour une rangée d'une page à elle, hors de `GameOption` :
  // `row_id` est l'identifiant que la page donne à sa rangée.
  void SetRowSwitch(int row_id, bool on, const OptionTargets& to, std::uint32_t now_ms);
  // LA MÊME CHOSE POUR UNE VALEUR — une barre, un nombre : la taille du texte du
  // fil que Ctrl + molette fait varier.
  void SetValue(GameOption which, float value, const OptionTargets& to, std::uint32_t now_ms);

  // À appeler une fois par image : c'est elle qui finit par écrire, une fois le
  // joueur arrêté. Voir l'anti-rebond de `SettingsFile`.
  void Tick(std::uint32_t now_ms);

  // Écrit tout de suite ce qui attendait. À la fermeture du jeu, où le délai
  // d'attente n'a plus le temps d'expirer.
  void Flush();

 private:
  std::filesystem::path client_root_;
  Hotkeys bindings_;
  ItemLayout item_layout_;
  StorageFavorites storage_favorites_;
  StorageTabNames storage_tab_names_;
  // Les libellés composés — « Alt+S », « Barre 3, case 7 », « Barre 3 » — et les
  // vues qui pointent dedans.
  //
  // ⚠ TROIS TAMPONS ET NON UN : `ui` reçoit des vues, et une vue ne survit pas à
  // la réallocation du vecteur qui la porte. Les trois se remplissent donc en
  // entier AVANT que la moindre vue ne se pose.
  std::vector<std::string> keys_;
  std::vector<std::string> names_;
  std::vector<std::string> groups_;
  std::vector<ro::ui::HotkeyRowView> rows_;
  std::array<std::string_view, kHotkeyTabCount> tab_names_{};

  // Les commandes que le menu du clic droit des macros propose.
  MacroCommandCatalogue macro_commands_;

  // ⚠ LE FICHIER AVANT LES OPTIONS : celles-ci s'y déclarent à la construction.
  SettingsFile file_;
  GameOptions options_;
  CameraMemory camera_memory_;
  // ⚠ APRÈS LE FICHIER ET LES OPTIONS : il les tient par référence.
  SettingsProfiles profiles_;
};

}  // namespace moonclient
