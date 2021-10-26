#include "hsd/archive.h"
#include "hsd/dobj.h"
#include "hsd/gobj.h"
#include "hsd/jobj.h"
#include "hsd/memory.h"
#include "hsd/mobj.h"
#include "hsd/tobj.h"
#include "melee/menu.h"
#include "melee/rules.h"
#include "melee/scene.h"
#include "melee/text.h"
#include "menu/stage_select.h"
#include "util/compression.h"
#include "util/mempool.h"
#include "util/patch_list.h"
#include "util/melee/text_builder.h"
#include <gctypes.h>
#include <ogc/gx.h>

#include "resources/rules/controller_fix.tex.h"
#include "resources/rules/controller_fix_values.tex.h"
#include "resources/rules/latency.tex.h"
#include "resources/rules/latency_values.tex.h"
#include "resources/rules/widescreen.tex.h"
#include "resources/rules/og_stage_select.tex.h"

struct ExtraRulesMenuData {
	u8 menu_type;
	u8 selected;
	u8 stock_time_limit;
	// Pause is moved up a row
	u8 pause;
	union {
		u8 friendly_fire;
		ucf_type controller_fix;
	};
	union {
		u8 score_display;
		u8 low_latency;
	};
	union {
		u8 sd_penalty;
		u8 widescreen;
	};
	u8 pad007;
	u8 state;
	struct {
		HSD_JObj *root1;
		HSD_JObj *root2;
		HSD_JObj *root3;
		HSD_JObj *rules[Rule_Max];
	} jobj_tree;
	struct {
		HSD_JObj *tree[7];
	} value_jobj_trees[ExtraRule_Max];
	Text *description_text;
};

// Rule name text
extern "C" ArchiveModel MenMainCursorRl_Top;

// Controller fix values
extern "C" ArchiveModel MenMainCursorTr03_Top;

extern "C" struct {
	f32 unselected;
	f32 selected;
} ExtraRuleTextAnimFrames[6];

extern "C" struct {
	u8 values[3];
} ExtraRuleDescriptions[6];

extern "C" HSD_GObj *Menu_SetupExtraRulesMenu(u8 state);

constexpr auto hax_fix_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Maximize shield drop's range and fix">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"1.0 cardinal and dash out of crouch.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());

constexpr auto ucf_fix_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Reduce shield drop's range and remove 1.0">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"cardinal and dash out of crouch fixes.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());
	
constexpr auto ucf_type_descriptions = std::array {
	hax_fix_description.data(),
	ucf_fix_description.data()
};

constexpr auto latency_normal_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Play with the normal amount">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"of visual latency.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());

constexpr auto latency_lcd_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Reduce visual latency by half">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"a frame. Recommended for LCDs.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());

constexpr auto latency_lightning_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Reduce visual latency by one">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"frame.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());

constexpr auto latency_descriptions = std::array {
	latency_normal_description.data(),
	latency_lcd_description.data(),
	latency_lightning_description.data()
};

constexpr auto widescreen_off_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Play with the normal aspect">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"ratio.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());

constexpr auto widescreen_on_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Play with a widescreen aspect">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"ratio.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());

constexpr auto widescreen_descriptions = std::array {
	widescreen_off_description.data(),
	widescreen_on_description.data()
};

constexpr auto oss_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Open the original stage select screen">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"and play without stage modifications.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());
	
static mempool pool;

static const auto patches = patch_list {
	// Swap text for pause and friendly fire
	std::pair { &ExtraRuleTextAnimFrames[ExtraRule_Pause].unselected,        24.f },
	std::pair { &ExtraRuleTextAnimFrames[ExtraRule_Pause].selected,          25.f },
	std::pair { &ExtraRuleTextAnimFrames[ExtraRule_FriendlyFire].unselected, 22.f },
	std::pair { &ExtraRuleTextAnimFrames[ExtraRule_FriendlyFire].selected,   23.f },

	// Swap description for pause and friendly fire
	std::pair { &ExtraRuleDescriptions[ExtraRule_Pause].values[0],           (u8)0x3A },
	std::pair { &ExtraRuleDescriptions[ExtraRule_Pause].values[1],           (u8)0x3B },

	// Apply 3-value model to index 3 instead of 4
	// cmpwi r27, 3
	std::pair { (char*)Menu_SetupExtraRulesMenu+0x574,                       0x2C1B0003u },
	// nop
	std::pair { (char*)Menu_SetupExtraRulesMenu+0x57C,                       0x60000000u },
};

static void replace_toggle_texture(ExtraRulesMenuData *data, int index, const u8 *tex_data)
{
	auto *tobj = data->value_jobj_trees[index].tree[1]->u.dobj->mobj->tobj;
	tobj->imagedesc = pool.add(new HSD_ImageDesc(*tobj->imagedesc));
	tobj->imagedesc->img_ptr = pool.add(decompress(tex_data));
}

static void pool_free(void *data)
{
	HSD_Free(data); // Default free gobj data
	pool.dec_ref();
}

extern "C" HSD_GObj *orig_Menu_SetupExtraRulesMenu(u8 state);
extern "C" HSD_GObj *hook_Menu_SetupExtraRulesMenu(u8 state)
{
	auto *gobj = orig_Menu_SetupExtraRulesMenu(state);
	auto *data = gobj->get<ExtraRulesMenuData>();

	// Free assets on menu exit
	gobj->user_data_remove_func = pool_free;
	
	if (pool.inc_ref() != 0)
		return gobj;

	// Replace rule name textures
	const auto *rule_names = MenMainCursorRl_Top.matanim_joint->child->child->next->matanim;
	rule_names->texanim->imagetbl[9]->img_ptr = pool.add(decompress(controller_fix_tex_data));
	rule_names->texanim->imagetbl[11]->img_ptr = pool.add(decompress(latency_tex_data));
	rule_names->texanim->imagetbl[12]->img_ptr = pool.add(decompress(widescreen_tex_data));
	rule_names->texanim->imagetbl[13]->img_ptr = pool.add(decompress(og_stage_select_tex_data));
	
	// Replace rule value textures
	replace_toggle_texture(data, ExtraRule_ControllerFix, controller_fix_values_tex_data);
	replace_toggle_texture(data, ExtraRule_Latency, latency_values_tex_data);

	// poggerga
	auto *latency_tobj = data->value_jobj_trees[ExtraRule_Latency].tree[1]->u.dobj->mobj->tobj;
	latency_tobj->imagedesc->format = GX_TF_RGB565;
		
	return gobj;
}

extern "C" void orig_Menu_CreateRandomStageMenu();
extern "C" void hook_Menu_CreateRandomStageMenu()
{
	if (MenuType != MenuType_ExtraRules)
		return orig_Menu_CreateRandomStageMenu();
		
	// Go to stage select if coming from extra rules
	use_og_stage_select = true;
	pool.dec_ref();
	Menu_ExitToMinorScene(VsScene_SSS);
}

extern "C" void orig_Menu_UpdateExtraRuleDescriptionText(HSD_GObj *gobj,
                                                         bool index_changed, bool value_changed);
extern "C" void hook_Menu_UpdateExtraRuleDescriptionText(HSD_GObj *gobj,
                                                         bool index_changed, bool value_changed)
{
	orig_Menu_UpdateExtraRuleDescriptionText(gobj, index_changed, value_changed);
	
	if (!index_changed && !value_changed)
		return;
		
	auto *data = gobj->get<ExtraRulesMenuData>();
	auto *text = data->description_text;
	
	const auto index = MenuSelectedIndex;
	const auto value = MenuSelectedValue;

	switch (index) {
	case ExtraRule_ControllerFix:  text->data = ucf_type_descriptions[value]; break;
	case ExtraRule_Latency:        text->data = latency_descriptions[value]; break;
	case ExtraRule_Widescreen:     text->data = widescreen_descriptions[value]; break;
	case ExtraRule_OldStageSelect: text->data = oss_description.data(); break;
	}
}