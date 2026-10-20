#pragma once

#include <cstdint>
#include <optional>

#include "ro/net/connection.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/head_button.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/menu_icons.h"

#include "game/game_pieces.h"
#include "game/menu_icon_compose.h"
#include "game/ui_stage.h"
#include "items/bank_view.h"
#include "items/card_album_view.h"

namespace moonclient {

class CashShopStage;
class AchievementStage;
class QuestStage;
class BattlegroundStage;
class MailStage;
class MvpTrackerStage;

// CE QUE LA GRILLE D'ICÔNES DE MENU MONTRE, ET CE QU'ELLE OUVRE.
//
// 🔴 UNE ICÔNE OUVRE PAR LE MÊME CHEMIN QUE SA TOUCHE. Celles qui ont une action
// de raccourci passent par `TriggerAction`, exactement comme Alt+E : la bascule,
// les refus dits dans la conversation, la demande de banque au serveur — tout
// vient de là, et un second chemin finirait par s'en écarter.

// Ce que la partie et ses fenêtres disent à la grille, relevé pour
// `ComposeMenuIcons`. ⚠ LES VUES PORTENT SUR LES FENÊTRES ET SUR LES PIÈCES :
// les faits ne survivent pas à l'image.
//
// 🔴 ELLE REFAIT LA TÊTE DU STATUT quand l'apparence de `character_id` a changé
// — voir `StatusHeadKeyOf` —, et seulement alors.
[[nodiscard]] MenuIconFacts MenuIconFactsOf(const ro::sim::World& world,
                                            const UiStage::Windows& windows, GamePieces& pieces,
                                            std::uint32_t character_id);

// CE QUI DÉCIDE DE LA TÊTE DU STATUT : celle du personnage du joueur, avec la
// coiffure et la couleur que sa recette de style impose, comme sur la carte.
// Rien tant qu'aucun personnage n'est chargé.
[[nodiscard]] std::optional<ro::render::StatusHeadKey> StatusHeadKeyOf(
    const ro::sim::World& world, ro::render::ActorWardrobe& wardrobe, std::uint32_t character_id);

// Ce qu'un clic peut toucher.
struct MenuIconTargets {
  UiStage::Windows* windows = nullptr;
  BankView* bank = nullptr;
  CardAlbumView* card_album = nullptr;
  CashShopStage* cash_shop = nullptr;
  MailStage* mail = nullptr;
  AchievementStage* achievements = nullptr;
  QuestStage* quests = nullptr;
  // Le carnet de chasse MVP : son icône bascule par le même chemin que sa touche.
  MvpTrackerStage* mvp_tracker = nullptr;
  // Le champ de bataille : en file, son icône rouvre « en attente ».
  BattlegroundStage* battleground = nullptr;
  ro::net::Connection* link = nullptr;
  ro::sim::World* world = nullptr;
  std::uint32_t now_ms = 0;
};

// Joue la commande cliquée.
void ApplyMenuCommand(ro::ui::MenuCommand command, const MenuIconTargets& to);

}  // namespace moonclient
