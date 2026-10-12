#pragma once

#include <string_view>

#include "ro/ui/ui_context.h"

#include "game/ui_stage.h"

namespace moonclient {

// LE COMPTEUR DE DÉGÂTS RANGÉ DANS LA CONVERSATION, sous cette clé du profil de
// disposition : rangé ou flottant, comme une fenêtre est ouverte ou fermée.
inline constexpr std::string_view kDamageMeterDockedKey = "damage_meter.docked";

// LES INTERRUPTEURS QUE LE PROFIL DE DISPOSITION RETIENT : l'ouverture des
// fenêtres que le joueur ouvre lui-même, leurs épingles, et le rangement du
// compteur de dégâts. Deux personnages sur le même profil retrouvent ainsi la
// même interface.
//
// ⚠ UNE FENÊTRE QUE LE SERVEUR OUVRE N'Y EST PAS — coffre, banque, boutique,
// courrier, album, échange —, ni celle dont l'ouverture DEMANDE au serveur ce
// qu'elle montre : rouverte d'un profil, elle se montrerait vide, ou ferait
// croire à une session que le serveur n'a pas ouverte.
//
// ⚠ L'IDENTITÉ, PAS LE TITRE : une clé est l'identité ImGui de la fenêtre, ou
// une clé qui en dérive. Un titre traduit rangerait la place sous un nom et
// l'ouverture sous un autre.
void RememberLayoutWindows(ro::ui::UiContext& ui, UiStage::Windows& windows);

}  // namespace moonclient
