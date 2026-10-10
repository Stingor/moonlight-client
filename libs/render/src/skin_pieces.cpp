#include "ro/render/skin_pieces.h"

#include <array>

#include "ro/formats/bmp.h"

namespace ro::render {
namespace {

// Les chemins, dans l'ordre de l'énumération. CATALOGUE : on n'y cherche qu'une
// entrée, et le découper ne donnerait qu'un nom qui ment.
constexpr std::array<std::string_view, kSkinPieceCount> kPaths = {
    "basic_interface\\titlebar_left.bmp",
    "basic_interface\\titlebar_mid.bmp",
    "basic_interface\\titlebar_right.bmp",
    "basic_interface\\sys_base_off.bmp",
    "basic_interface\\sys_base_on.bmp",
    "basic_interface\\sys_close_off.bmp",
    "basic_interface\\sys_close_on.bmp",
    "basic_interface\\sys_mini_off.bmp",
    "basic_interface\\sys_mini_on.bmp",
    "basic_interface\\sys_pin_off.bmp",
    "basic_interface\\sys_pin_on.bmp",
    "sysbox_lu.bmp",
    "sysbox_mu.bmp",
    "sysbox_ru.bmp",
    "sysbox_lm.bmp",
    "sysbox_rm.bmp",
    "sysbox_ld.bmp",
    "sysbox_md.bmp",
    "sysbox_rd.bmp",
    "basic_interface\\btn_out_left.bmp",
    "basic_interface\\btn_out_mid.bmp",
    "basic_interface\\btn_out_right.bmp",
    "basic_interface\\btn_over_left.bmp",
    "basic_interface\\btn_over_mid.bmp",
    "basic_interface\\btn_over_right.bmp",
    "basic_interface\\btn_press_left.bmp",
    "basic_interface\\btn_press_mid.bmp",
    "basic_interface\\btn_press_right.bmp",
    "scroll0up.bmp",
    "scroll0mid.bmp",
    "scroll0down.bmp",
    "scroll0bar_up.bmp",
    "scroll0bar_mid.bmp",
    "scroll0bar_down.bmp",
    "checkbox_0.bmp",
    "checkbox_1.bmp",
    "btn_resize.bmp",
    "basic_interface\\tabh_l_on.bmp",
    "basic_interface\\tabh_m_on.bmp",
    "basic_interface\\tabh_r_on.bmp",
    "basic_interface\\tabh_l_off.bmp",
    "basic_interface\\tabh_m_off.bmp",
    "basic_interface\\tabh_r_off.bmp",
    "basic_interface\\sbtn_out_left.bmp",
    "basic_interface\\sbtn_out_mid.bmp",
    "basic_interface\\sbtn_out_right.bmp",
    "basic_interface\\sbtn_over_left.bmp",
    "basic_interface\\sbtn_over_mid.bmp",
    "basic_interface\\sbtn_over_right.bmp",
    "basic_interface\\sbtn_press_left.bmp",
    "basic_interface\\sbtn_press_mid.bmp",
    "basic_interface\\sbtn_press_right.bmp",
    "basic_interface\\btnbar_left.bmp",
    "basic_interface\\btnbar_mid.bmp",
    "basic_interface\\btnbar_right.bmp",
    "basic_interface\\skill_upbar_left.bmp",
    "basic_interface\\skill_upbar_mid.bmp",
    "basic_interface\\skill_upbar_right.bmp",
    "basic_interface\\txtbox_btn_a.bmp",
    "basic_interface\\txtbox_btn_b.bmp",
    "basic_interface\\txtbox_btn_c.bmp",
    "radiobtn_off.bmp",
    "radiobtn_on.bmp",
    "scroll1left.bmp",
    "scroll1mid.bmp",
    "scroll1right.bmp",
    "scroll1bar_left.bmp",
    "scroll1bar_mid.bmp",
    "scroll1bar_right.bmp",
    "basic_interface\\itemwin_mid.bmp",
    "basic_interface\\btnbar_left3.bmp",
    "basic_interface\\btnbar_mid3.bmp",
    "basic_interface\\btnbar_right3.bmp",
    // ⚠ CES DEUX-LÀ NE SONT PAS DANS `basic_interface` mais dans `inventory` —
    // relevé dans l'archive, pas déduit du voisinage.
    "inventory\\icon_weight.bmp",
    "inventory\\icon_num.bmp",
    "inventory\\item_drop_lock_off.bmp",
    "inventory\\item_drop_lock_on.bmp",
    "inventory\\bt_itemdeal_lock_off.bmp",
    "inventory\\bt_itemdeal_lock_on.bmp",
    "inventory\\bt_sort_off.bmp",
    "inventory\\bt_sort.bmp",
    "inventory\\itemwin_mid_lock.bmp",
    "basic_interface\\tab_all1.bmp",
    "basic_interface\\tab_all2.bmp",
    "basic_interface\\tab_use1.bmp",
    "basic_interface\\tab_use2.bmp",
    "basic_interface\\tab_cos1.bmp",
    "basic_interface\\tab_cos2.bmp",
    "basic_interface\\tab_ammo1.bmp",
    "basic_interface\\tab_ammo2.bmp",
    "basic_interface\\tab_etc1.bmp",
    "basic_interface\\tab_etc2.bmp",
    "basic_interface\\tab_card1.bmp",
    "basic_interface\\tab_card2.bmp",
    "basic_interface\\tab_fav1.bmp",
    "basic_interface\\tab_fav2.bmp",
    "basic_interface\\tab_wea1.bmp",
    "basic_interface\\tab_wea2.bmp",
    "basic_interface\\tab_arm1.bmp",
    "basic_interface\\tab_arm2.bmp",
    "basic_interface\\tab_cash1.bmp",
    "basic_interface\\tab_cash2.bmp",
    "basic_interface\\tab_favs1.bmp",
    "basic_interface\\tab_favs2.bmp",
    "basic_interface\\tabh_all1.bmp",
    "basic_interface\\tabh_all2.bmp",
    "basic_interface\\tabh_use1.bmp",
    "basic_interface\\tabh_use2.bmp",
    "basic_interface\\tabh_cos1.bmp",
    "basic_interface\\tabh_cos2.bmp",
    "basic_interface\\tabh_ammo1.bmp",
    "basic_interface\\tabh_ammo2.bmp",
    "basic_interface\\tabh_etc1.bmp",
    "basic_interface\\tabh_etc2.bmp",
    "basic_interface\\tabh_card1.bmp",
    "basic_interface\\tabh_card2.bmp",
    "basic_interface\\tabh_fav1.bmp",
    "basic_interface\\tabh_fav2.bmp",
    "basic_interface\\tabh_wea1.bmp",
    "basic_interface\\tabh_wea2.bmp",
    "basic_interface\\tabh_arm1.bmp",
    "basic_interface\\tabh_arm2.bmp",
    "basic_interface\\tabh_cash1.bmp",
    "basic_interface\\tabh_cash2.bmp",
    "basic_interface\\tabh_favs1.bmp",
    "basic_interface\\tabh_favs2.bmp",
    // ⚠ LE SUFFIXE « 1 » EST L'ONGLET OUVERT, le « 2 » le fermé — comme pour
    // toutes les familles au-dessus. L'inverser rendrait la rangée entière
    // ouverte sauf celui qu'on regarde.
    "basic_interface\\tab_sto1.bmp",
    "basic_interface\\tab_sto2.bmp",
    "basic_interface\\tabh_sto1.bmp",
    "basic_interface\\tabh_sto2.bmp",
    // La banque : son fond entier, puis les trois états du bouton qui
    // l'ouvre. ⚠ LES DEUX FAMILLES NE SONT PAS DANS LE MÊME DOSSIER.
    "bank\\bg_bank_moon.bmp",
    "styleshop\\btn_bank_out.bmp",
    "styleshop\\btn_bank_over.bmp",
    "styleshop\\btn_bank_down.bmp",
    "btn_ok.bmp",
    "btn_ok_a.bmp",
    "btn_ok_b.bmp",
    "btn_cancel.bmp",
    "btn_cancel_a.bmp",
    "btn_cancel_b.bmp",
};

}  // namespace

std::string_view SkinPiecePath(SkinPiece piece) { return kPaths[static_cast<std::size_t>(piece)]; }

const formats::Image* SkinPieces::Get(SkinPiece piece) const {
  const std::optional<formats::Image>& image = images[static_cast<std::size_t>(piece)];
  return image.has_value() ? &*image : nullptr;
}

SkinPieces LoadSkinPieces(vfs::Vfs& vfs) {
  SkinPieces pieces;
  for (std::size_t rank = 0; rank < kSkinPieceCount; ++rank) {
    std::string path(kSkinRoot);
    path += kPaths[rank];
    // ⚠ `Read` ET NON `ReadRaw` : le chemin est écrit en UTF-8 dans nos sources
    // et le VFS le convertit vers les octets de l'archive. Le passer brut
    // chercherait un nom coréen qui n'existe sous cette forme nulle part.
    auto bytes = vfs.Read(path);
    if (!bytes) {
      pieces.missing.push_back(std::move(path));
      continue;
    }
    auto image = formats::DecodeBmp(*bytes);
    if (!image) {
      pieces.missing.push_back(std::move(path));
      continue;
    }
    formats::ApplyMagentaColorKey(*image);
    pieces.images[rank] = std::move(*image);
  }
  return pieces;
}

}  // namespace ro::render
