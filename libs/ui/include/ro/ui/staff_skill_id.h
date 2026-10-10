#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ro::ui {

// L'IDENTITÉ D'UNE COMPÉTENCE, telle que le staff la lit au survol.
//
// 🔴 UNE SEULE FORME, PARTOUT : « ID : 5 (SM_BASH) », en gris, en pied de
// l'infobulle qui nomme la compétence. Le staff recopie ce numéro dans une
// commande ou un script, et le nom interne dans un fichier du serveur ; deux
// écritures selon la fenêtre l'obligeraient à les reconnaître toutes.
//
// ⚠ RIEN POUR UN JOUEUR ORDINAIRE : le droit se relit à chaque image, et un
// droit retiré fait disparaître la ligne sans attendre une reconnexion.
struct StaffSkillId {
  std::uint16_t skill_id = 0;
  // Le nom INTERNE, « SM_BASH ». Vide quand le client ne le connaît pas : la
  // ligne ne porte alors que le numéro.
  std::string_view idname;
};

// Le texte de la ligne, sans condition de droit. Vide pour une compétence nulle.
[[nodiscard]] std::string StaffSkillIdText(StaffSkillId skill);

// LA LIGNE GRISE, dans l'infobulle OUVERTE. Ne dessine rien sans `staff`.
void DrawStaffSkillId(bool staff, StaffSkillId skill);

// UNE INFOBULLE DE TEXTE, puis la ligne du staff. Remplace
// `SkinTooltipFormat("%s", text)` là où le texte nomme une compétence.
void SkinTooltipWithStaffSkillId(const char* text, bool staff, StaffSkillId skill);

}  // namespace ro::ui
