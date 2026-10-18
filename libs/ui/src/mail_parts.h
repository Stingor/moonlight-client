#pragma once

#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/mail_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

// LES BRIQUES PARTAGÉES PAR LA BOÎTE ET L'ÉCRITURE DU COURRIER.
//
// En-tête PRIVÉ : il vit dans `src/`, personne hors de cette bibliothèque n'a
// à dessiner une demi-fenêtre de courrier.
namespace ro::ui::mail {

// L'arrondi des volets.
inline constexpr float kChildRoundingPixels = 6.0F;
// La couleur d'un refus, plus vive que celle des fenêtres de commerce : la
// nuance propre au courrier.
inline constexpr ImVec4 kRefusalText = ImVec4(0.75F, 0.15F, 0.15F, 1.0F);

// UNE PIÈCE JOINTE : icône, nom complet, quantité au-delà d'un. Même cellule
// pour la lecture et l'écriture ; le groupe couvre toute la ligne, pour que le
// survol et le clic droit ne dépendent pas du seul libellé.
[[nodiscard]] TradeItemGesture DrawItemRow(const MailItemView& line);

// LA CONFIRMATION DU COURRIER : une boîte modale à barre de titre
// « Confirmation », la question, puis « Confirmer » et « Annuler ». Échap vaut
// « non ».
//
// Son nom se compose une fois par image (`ConfirmModalName`) et sert aux trois
// temps : `AskModal`, ce dessin, `CloseModal`. Le titre change avec la langue,
// l'identité — `id` — jamais.
[[nodiscard]] std::string ConfirmModalName(std::string_view id);
[[nodiscard]] ModalAnswer DrawConfirmModal(const std::string& name, std::string_view question,
                                           const SkinArt& art);

}  // namespace ro::ui::mail
