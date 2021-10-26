#include "hsd/archive.h"
#include "hsd/dobj.h"
#include "hsd/gobj.h"
#include "hsd/jobj.h"
#include "hsd/mobj.h"
#include "hsd/tobj.h"
#include "melee/menu.h"
#include "melee/rules.h"
#include "melee/text.h"
#include "rules/values.h"
#include "util/compression.h"
#include "util/patch_list.h"
#include "util/melee/text_builder.h"
#include <gctypes.h>

#include "resources/rules/ledge_grab_limit.tex.h"
#include "resources/rules/air_time_limit.tex.h"
#include "resources/rules/menu_music.tex.h"
#include "resources/rules/menu_music_header.tex.h"
#include "resources/rules/stage_music.tex.h"
#include "resources/rules/stage_music_header.tex.h"
#include "resources/rules/mode_values.tex.h"

struct RulesMenuData {
	u8 menu_type;
	u8 selected;
	u8 mode;
	u8 time_limit;
	union {
		u8 handicap;
		u8 ledge_grab_limit;
	};
	union {
		u8 damage_ratio;
		u8 air_time_limit;
	};
	u8 stage_selection_mode;
	u8 pad008;
	u8 stock_count;
	u8 state;
	u8 pad00B;
	struct {
		HSD_JObj *root1;
		HSD_JObj *root2;
		HSD_JObj *root3;
		HSD_JObj *rules[Rule_Max];
	} jobj_tree;
	struct {
		HSD_JObj *tree[9];
	} value_jobj_trees[Rule_Max];
	Text *description_text;
};

// Rule name text
extern "C" ArchiveModel MenMainCursorRl_Top;

// Menu frame
extern "C" ArchiveModel MenMainPanel_Top;

// Mode values
extern "C" ArchiveModel MenMainCursorRl01_Top;

extern "C" ArchiveModel MenMainNmRl_Top;

extern "C" struct {
	ArchiveModel *mode;
	ArchiveModel *stock_count;
	ArchiveModel *handicap;
	ArchiveModel *damage_ratio;
	ArchiveModel *stage_selection;
} MenuRuleValueModels;

extern "C" void Menu_UpdateRuleDisplay(HSD_GObj *gobj, bool index_changed, bool value_changed);
extern "C" void Menu_RulesMenuInput(HSD_GObj *gobj);

constexpr auto lgl_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Set the LGL in the event of">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"a time-out.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());

constexpr auto atl_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Set the ATL in the event of">(),
	text_builder::end_fit(),
	text_builder::br(),
	text_builder::fit(),
	text_builder::ascii<"a time-out.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());

constexpr auto menu_music_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::offset<0, -10>(),
	text_builder::br(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Customize the menu music.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());

constexpr auto stage_music_description = text_builder::build(
	text_builder::kern(),
	text_builder::left(),
	text_builder::color<170, 170, 170>(),
	text_builder::textbox<179, 179>(),
	text_builder::offset<0, -10>(),
	text_builder::br(),
	text_builder::unk06<0, 0>(),
	text_builder::fit(),
	text_builder::ascii<"Customize the stage music.">(),
	text_builder::end_fit(),
	text_builder::end_textbox(),
	text_builder::end_color());
	
const auto patches = patch_list {
	// Hide left/right arrows for menu music when selected
	// cmplwi r24, 4
	std::pair { (char*)Menu_UpdateRuleDisplay+0x2B8, 0x28180004u },
	// bge 0x44
	std::pair { (char*)Menu_UpdateRuleDisplay+0x2BC, 0x40800044u },

	// Display submenu arrow for menu music when selected
	// subi r0, r24, 4
	std::pair { (char*)Menu_UpdateRuleDisplay+0x364, 0x3818FFFCu },
	// cmplwi r0, 2
	std::pair { (char*)Menu_UpdateRuleDisplay+0x36C, 0x28000002u },

	// Use submenu anim frames for menu music
	// subi r0, r25, 4
	std::pair { (char*)Menu_UpdateRuleDisplay+0x484, 0x3819FFFCu },
	// cmplwi r0, 2
	std::pair { (char*)Menu_UpdateRuleDisplay+0x488, 0x28000002u },

	// Allow pressing A on menu music
	// cmplwi r0, 4
	std::pair { (char*)Menu_RulesMenuInput+0x54,     0x28000004u },
	// bge 0xC
	std::pair { (char*)Menu_RulesMenuInput+0x58,     0x4080000Cu },

	// Don't allow scrolling through menu music values
	// cmplwi r5, 4
	std::pair { (char*)Menu_RulesMenuInput+0x420,    0x28050005u },
	// bge 0x20C
	std::pair { (char*)Menu_RulesMenuInput+0x424,    0x4080020Cu },
};

static void replace_value_jobj(HSD_JObj *parent, HSD_JObj **jobj_tree)
{
	// Remove existing value jobj
	HSD_JObjRemoveAll(parent->child->next);

	// Create new value jobj
	const auto *value_model = MenuRuleValueModels.stock_count;
	auto *value_jobj = HSD_JObjFromArchiveModel(value_model);
	HSD_JObjReqAnimAll(value_jobj, 0.f);
	HSD_JObjAnimAll(value_jobj);
	
	// Update the stored jobj tree for the new jobj
	HSD_JObjGetTree<9>(value_jobj, jobj_tree);
	
	// Parent in the new value jobj
	HSD_JObjAddChild(parent, value_jobj);
}

static void replace_counter_jobj(RulesMenuData *data, int index)
{
	auto *parent = data->jobj_tree.rules[index];
	auto **jobj_tree = data->value_jobj_trees[index].tree;
	replace_value_jobj(parent, jobj_tree);

	// Hide ":"
	HSD_JObjSetFlagsAll(jobj_tree[4], HIDDEN);

	// Digits
	HSD_JObjAddChild(jobj_tree[7], HSD_JObjFromArchiveModel(&MenMainNmRl_Top));
	HSD_JObjAddChild(jobj_tree[8], HSD_JObjFromArchiveModel(&MenMainNmRl_Top));
}

static void replace_timer_jobj(RulesMenuData *data, int index)
{
	auto *parent = data->jobj_tree.rules[index];
	auto **jobj_tree = data->value_jobj_trees[index].tree;
	replace_value_jobj(parent, jobj_tree);

	// Minutes
	HSD_JObjAddChild(jobj_tree[2], HSD_JObjFromArchiveModel(&MenMainNmRl_Top));
	HSD_JObjAddChild(jobj_tree[3], HSD_JObjFromArchiveModel(&MenMainNmRl_Top));

	// Seconds
	HSD_JObjAddChild(jobj_tree[5], HSD_JObjFromArchiveModel(&MenMainNmRl_Top));
	HSD_JObjAddChild(jobj_tree[6], HSD_JObjFromArchiveModel(&MenMainNmRl_Top));
}

static void update_value_digit(HSD_JObj *jobj, u32 digit)
{
	HSD_JObjReqAnimAll(jobj, (float)digit);
	HSD_JObjAnimAll(jobj);
}

static void show_timer_value(HSD_JObj **jobj_tree, bool show)
{
	// Show/hide digits
	for (auto i = 2; i < 9; i++) {
		if (i == 4)
			continue;

		if (show)
			HSD_JObjClearFlagsAll(jobj_tree[i], HIDDEN);
		else
			HSD_JObjSetFlagsAll(jobj_tree[i], HIDDEN);
	}
	
	// Swap joint 4 between ":" or "NONE" with mat anim
	HSD_JObjReqAnimAll(jobj_tree[4], show ? 0.f : 1.f);
	HSD_JObjAnimAll(jobj_tree[4]);
}

static void show_counter_value(HSD_JObj **jobj_tree, bool show)
{
	show_timer_value(jobj_tree, show);

	// Hide ":" when not displaying "NONE"
	if (!show)
		HSD_JObjClearFlagsAll(jobj_tree[4], HIDDEN);
	else
		HSD_JObjSetFlagsAll(jobj_tree[4], HIDDEN);
}

static void update_lgl_value(HSD_GObj *menu_obj, u32 value)
{
	auto *data = menu_obj->get<RulesMenuData>();
	auto **jobj_tree = data->value_jobj_trees[Rule_LedgeGrabLimit].tree;
	
	if (value == 0) {
		show_counter_value(jobj_tree, false);
		return;
	}

	show_counter_value(jobj_tree, true);

	const auto time = ledge_grab_limit_values[value];

	update_value_digit(jobj_tree[7], time / 10);
	update_value_digit(jobj_tree[8], time % 10);
}

static void update_atl_value(HSD_GObj *menu_obj, u32 value)
{
	auto *data = menu_obj->get<RulesMenuData>();
	auto **jobj_tree = data->value_jobj_trees[Rule_AirTimeLimit].tree;

	if (value == 0) {
		show_timer_value(jobj_tree, false);
		return;
	}

	show_timer_value(jobj_tree, true);

	const auto time = air_time_limit_values[value];
	const auto minutes = time / 60;
	const auto seconds = time % 60;

	update_value_digit(jobj_tree[2], minutes / 10);
	update_value_digit(jobj_tree[3], minutes % 10);
	update_value_digit(jobj_tree[5], seconds / 10);
	update_value_digit(jobj_tree[6], seconds % 10);
}

static void hide_rule_value(RulesMenuData *data, int index)
{
	auto *cursor = data->jobj_tree.rules[index]->child;
	
	// Hide value background + arrows
	for (auto *jobj : HSD_JObjGetFromTreeByIndices<6, 13>(cursor))
		HSD_JObjSetFlagsAll(jobj, HIDDEN);

	// Hide value
	HSD_JObjSetFlagsAll(cursor->next, HIDDEN);
}

extern "C" HSD_GObj *orig_Menu_SetupRulesMenu(u8 state);
extern "C" HSD_GObj *hook_Menu_SetupRulesMenu(u8 state)
{
	auto *menu_obj = orig_Menu_SetupRulesMenu(state);
	auto *data = menu_obj->get<RulesMenuData>();

	// Replace rule name textures
	const auto *rule_names = MenMainCursorRl_Top.matanim_joint->child->child->next->matanim;
	rule_names->texanim->imagetbl[3]->img_ptr = decompress(ledge_grab_limit_tex_data);
	rule_names->texanim->imagetbl[4]->img_ptr = decompress(air_time_limit_tex_data);
	rule_names->texanim->imagetbl[5]->img_ptr = decompress(menu_music_tex_data);
	rule_names->texanim->imagetbl[6]->img_ptr = decompress(stage_music_tex_data);

	// Replace menu header names
	const auto *name1 = MenMainPanel_Top.matanim_joint->child->next->next->child->next->matanim;
	name1->texanim->imagetbl[2]->img_ptr = decompress(stage_music_header_tex_data);

	const auto *name2 = MenMainPanel_Top.joint->child->next->next->child->next->next;
	name2->u.dobj->mobjdesc->texdesc->imagedesc->img_ptr =
		decompress(menu_music_header_tex_data);

	// Replace mode value texture
	MenMainCursorRl01_Top.joint->child->u.dobj->mobjdesc->texdesc->imagedesc->img_ptr =
		decompress(mode_values_tex_data);
	
	// Replace rule value models
	replace_counter_jobj(data, Rule_LedgeGrabLimit);
	replace_timer_jobj(data, Rule_AirTimeLimit);
	
	// Display initial values
	update_lgl_value(menu_obj, data->ledge_grab_limit);
	update_atl_value(menu_obj, data->air_time_limit);
	
	hide_rule_value(data, Rule_MenuMusic);
	
	return menu_obj;
}

extern "C" void orig_Menu_RulesMenuInput(HSD_GObj *gobj);
extern "C" void hook_Menu_RulesMenuInput(HSD_GObj *gobj)
{
	orig_Menu_RulesMenuInput(gobj);

	if (MenuSelectedIndex != Rule_MenuMusic || !(MenuButtons & MenuButton_A))
		return;
		
	// Open menu music
	Menu_CreateRandomStageMenu();
	GObj_Free(gobj);
}

extern "C" void orig_Menu_UpdateRuleValue(HSD_GObj *menu_obj, HSD_JObj *jobj, u8 index, u32 value);
extern "C" void hook_Menu_UpdateRuleValue(HSD_GObj *menu_obj, HSD_JObj *jobj, u8 index, u32 value)
{
	if (index == Rule_LedgeGrabLimit)
		update_lgl_value(menu_obj, value);
	else if (index == Rule_AirTimeLimit)
		update_atl_value(menu_obj, value);
	else
		orig_Menu_UpdateRuleValue(menu_obj, jobj, index, value);
}

extern "C" void orig_Menu_CreateRuleDescriptionText(RulesMenuData *data, int rule, int value);
extern "C" void hook_Menu_CreateRuleDescriptionText(RulesMenuData *data, int rule, int value)
{
	orig_Menu_CreateRuleDescriptionText(data, rule, value);
	
	auto *text = data->description_text;

	switch (rule) {
	case Rule_LedgeGrabLimit: text->data = lgl_description.data(); break;
	case Rule_AirTimeLimit:   text->data = atl_description.data(); break;
	case Rule_MenuMusic:      text->data = menu_music_description.data(); break;
	case Rule_StageMusic:     text->data = stage_music_description.data(); break;
	}
}