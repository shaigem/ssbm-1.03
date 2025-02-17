#include "hsd/aobj.h"
#include "hsd/archive.h"
#include "hsd/dobj.h"
#include "hsd/gobj.h"
#include "hsd/jobj.h"
#include "hsd/memory.h"
#include "hsd/mobj.h"
#include "hsd/tobj.h"
#include "melee/menu.h"
#include "melee/rules.h"
#include "melee/text.h"
#include "rules/saved_config.h"
#include "rules/values.h"
#include "util/texture_swap.h"
#include "util/mempool.h"
#include "util/patch_list.h"
#include "util/melee/match.h"
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

// "Ready to start" banner animation frame
extern "C" u8 CSSReadyFrames;

// Rule name text
extern "C" ArchiveModel MenMainCursorRl_Top;

// Menu frame
extern "C" ArchiveModel MenMainPanel_Top;

// Mode values
extern "C" ArchiveModel MenMainCursorRl01_Top;

// Handicap values
extern "C" ArchiveModel MenMainCursorRl03_Top;

extern "C" ArchiveModel MenMainNmRl_Top;

extern "C" HSD_GObj *RulesMenuGObj;

extern "C" struct {
	ArchiveModel *mode;
	ArchiveModel *stock_count;
	ArchiveModel *handicap;
	ArchiveModel *damage_ratio;
	ArchiveModel *stage_selection;
} MenuRuleValueModels;

extern "C" void Menu_UpdateRuleDisplay(HSD_GObj *gobj, bool index_changed, bool value_changed);
extern "C" void Menu_RulesMenuInput(HSD_GObj *gobj);

template<string_literal line1, string_literal line2>
static constexpr auto make_description_text()
{
	return text_builder::build(
		text_builder::kern(),
		text_builder::left(),
		text_builder::color<170, 170, 170>(),
		text_builder::scale<179, 179>(),
		text_builder::type_speed<0, 0>(),
		text_builder::fit(),
		text_builder::text<line1>(),
		text_builder::end_fit(),
		text_builder::br(),
		text_builder::fit(),
		text_builder::text<line2>(),
		text_builder::end_fit(),
		text_builder::reset_scale(),
		text_builder::end_color());
}

template<string_literal line1>
static constexpr auto make_description_text()
{
	return text_builder::build(
		text_builder::kern(),
		text_builder::center(),
		text_builder::color<170, 170, 170>(),
		text_builder::scale<179, 179>(),
		text_builder::offset<0, -10>(),
		text_builder::br(),
		text_builder::type_speed<0, 0>(),
		text_builder::fit(),
		text_builder::text<line1>(),
		text_builder::end_fit(),
		text_builder::reset_scale(),
		text_builder::end_color());
}

constexpr auto crew_description = make_description_text<
	"Participate in a crew battle.">();

constexpr auto lgl_description = make_description_text<
	"Set the LGL in the event of",
	"a time-out.">();

constexpr auto atl_description = make_description_text<
	"Set the ATL in the event of",
	"a time-out.">();

constexpr auto menu_music_description = make_description_text<
	"Customize the menu music.">();

constexpr auto stage_music_description = make_description_text<
	"Customize the stage music.">();

static mempool pool;

PATCH_LIST(
	// Hide left/right arrows for menu music when selected
	// cmplwi r24, 4
	std::pair { Menu_UpdateRuleDisplay+0x2B8, 0x28180004u },
	// bge 0x44
	std::pair { Menu_UpdateRuleDisplay+0x2BC, 0x40800044u },

	// Display submenu arrow for menu music when selected
	// subi r0, r24, 4
	std::pair { Menu_UpdateRuleDisplay+0x364, 0x3818FFFCu },
	// cmplwi r0, 2
	std::pair { Menu_UpdateRuleDisplay+0x36C, 0x28000002u },

	// Use submenu anim frames for menu music
	// subi r0, r25, 4
	std::pair { Menu_UpdateRuleDisplay+0x484, 0x3819FFFCu },
	// cmplwi r0, 2
	std::pair { Menu_UpdateRuleDisplay+0x488, 0x28000002u },

	// Allow pressing A on menu music
	// cmplwi r0, 4
	std::pair { Menu_RulesMenuInput+0x54,     0x28000004u },
	// bge 0xC
	std::pair { Menu_RulesMenuInput+0x58,     0x4080000Cu },

	// Don't allow scrolling through menu music values
	// cmplwi r5, 4
	std::pair { Menu_RulesMenuInput+0x420,    0x28050004u },
	// bge 0x20C
	std::pair { Menu_RulesMenuInput+0x424,    0x4080020Cu }
);

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

static void update_lgl_value(HSD_GObj *gobj, u32 value)
{
	auto *data = gobj->get<RulesMenuData>();
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

static void update_atl_value(HSD_GObj *gobj, u32 value)
{
	auto *data = gobj->get<RulesMenuData>();
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

static void fix_rule_scale(RulesMenuData *data, int index)
{
	auto *cursor = data->jobj_tree.rules[index]->child;
	auto *label = HSD_JObjGetFromTreeByIndex(cursor, 7);
	label->scale.x = 1.5f;
	HSD_JObjSetMtxDirty(label);
}

static void fix_rule_anims(RulesMenuData *data)
{
	// Force same scale for all languages
	fix_rule_scale(data, Rule_LedgeGrabLimit);
	fix_rule_scale(data, Rule_AirTimeLimit);
	fix_rule_scale(data, Rule_MenuMusic);
	fix_rule_scale(data, Rule_StageMusic);
}

static void pool_free(void *data)
{
	HSD_Free(data); // Default free gobj data
	pool.dec_ref();
}

static void load_textures()
{
	// Replace rule name textures
	const auto *rule_names = MenMainCursorRl_Top.matanim_joint->child->child->next->matanim;
	pool.add_texture_swap(ledge_grab_limit_tex_data, rule_names->texanim->imagetbl[3]);
	pool.add_texture_swap(air_time_limit_tex_data,   rule_names->texanim->imagetbl[4]);
	pool.add_texture_swap(menu_music_tex_data,       rule_names->texanim->imagetbl[5]);
	pool.add_texture_swap(stage_music_tex_data,      rule_names->texanim->imagetbl[6]);

	// Replace menu header names
	const auto *name1 = MenMainPanel_Top.matanim_joint->child->next->next->child->next->matanim;
	pool.add_texture_swap(stage_music_header_tex_data, name1->texanim->imagetbl[2]);

	const auto *name2 = MenMainPanel_Top.joint->child->next->next->child->next->next;
	pool.add_texture_swap(menu_music_header_tex_data,
		name2->u.dobj->mobjdesc->texdesc->imagedesc);

	// Replace mode value texture
	pool.add_texture_swap(mode_values_tex_data,
		MenMainCursorRl01_Top.joint->child->u.dobj->mobjdesc->texdesc->imagedesc);
}

extern "C" HSD_GObj *orig_Menu_SetupRulesMenu(u8 state);
extern "C" HSD_GObj *hook_Menu_SetupRulesMenu(u8 state)
{
	// Handle coming back to rules from menu music
	if (MenuTypePrevious == MenuType_MenuMusic)
		MenuSelectedIndex = Rule_MenuMusic;

	if (pool.inc_ref() == 0)
		load_textures();

	auto *gobj = orig_Menu_SetupRulesMenu(state);
	auto *data = gobj->get<RulesMenuData>();

	// Free assets on menu exit
	gobj->user_data_remove_func = pool_free;

	// Replace rule value models
	replace_counter_jobj(data, Rule_LedgeGrabLimit);
	replace_timer_jobj(data, Rule_AirTimeLimit);

	// Display initial values
	update_lgl_value(gobj, data->ledge_grab_limit);
	update_atl_value(gobj, data->air_time_limit);

	hide_rule_value(data, Rule_MenuMusic);

	fix_rule_anims(data);

	return gobj;
}

static bool is_rule_locked(u16 index, u32 buttons)
{
	if (!get_settings_lock())
		return false;

	switch (index) {
	case Rule_StockCount:
	case Rule_LedgeGrabLimit:
	case Rule_AirTimeLimit:
		// Lock stocks/LGL/ATL
		return buttons & (MenuButton_Left | MenuButton_Right);
	case Rule_StageMusic:
		// Lock Stage Music
		return buttons & MenuButton_A;
	default:
		return false;
	}
}

extern "C" void orig_Menu_RulesMenuInput(HSD_GObj *gobj);
extern "C" void hook_Menu_RulesMenuInput(HSD_GObj *gobj)
{
	const auto buttons = Menu_GetButtonsHelper(PORT_ALL);
	const auto index = MenuSelectedIndex;

	if (is_rule_locked(index, buttons)) {
		// Enforce tournament lock
		Menu_PlaySFX(MenuSFX_Error);
		return;
	}

	if (index == Rule_MenuMusic && (buttons & MenuButton_A)) {
		// Open menu music
		Menu_PlaySFX(MenuSFX_Activate);
		IsEnteringMenu = true;
		MenuInputCooldown = 5;
		Menu_CreateRandomStageMenu();
		GObj_Free(gobj);
		return;
	}

	orig_Menu_RulesMenuInput(gobj);

	if (!(buttons & MenuButton_A) && (buttons & (MenuButton_B | MenuButton_Start)))
		config.save();
}

extern "C" void orig_Menu_UpdateRuleDisplay(HSD_GObj *gobj, bool index_changed, bool value_changed);
extern "C" void hook_Menu_UpdateRuleDisplay(HSD_GObj *gobj, bool index_changed, bool value_changed)
{
	orig_Menu_UpdateRuleDisplay(gobj, index_changed, value_changed);

	// Fix all rule anims since changes will have been overridden
	auto *data = gobj->get<RulesMenuData>();
	fix_rule_anims(data);
}

extern "C" void orig_Menu_UpdateRuleValue(HSD_GObj *gobj, HSD_JObj *jobj, u8 index, u32 value);
extern "C" void hook_Menu_UpdateRuleValue(HSD_GObj *gobj, HSD_JObj *jobj, u8 index, u32 value)
{
	if (index == Rule_LedgeGrabLimit)
		update_lgl_value(gobj, value);
	else if (index == Rule_AirTimeLimit)
		update_atl_value(gobj, value);
	else
		orig_Menu_UpdateRuleValue(gobj, jobj, index, value);
}

extern "C" void orig_Menu_CreateRuleDescriptionText(RulesMenuData *data, u32 rule, u32 value);
extern "C" void hook_Menu_CreateRuleDescriptionText(RulesMenuData *data, u32 rule, u32 value)
{
	orig_Menu_CreateRuleDescriptionText(data, rule, value);

	auto *text = data->description_text;

	if (rule == Rule_Mode && value == Mode_Crew) {
		text->data = crew_description.data();
		return;
	}

	switch (rule) {
	case Rule_LedgeGrabLimit: text->data = lgl_description.data(); break;
	case Rule_AirTimeLimit:   text->data = atl_description.data(); break;
	case Rule_MenuMusic:      text->data = menu_music_description.data(); break;
	case Rule_StageMusic:     text->data = stage_music_description.data(); break;
	}
}

extern "C" void orig_Menu_UpdateStockCountOrTimerText(bool show_time);
extern "C" void hook_Menu_UpdateStockCountOrTimerText(bool show_time)
{
	// Show stock count in crew mode
	u8 mode;
	if (MenuSelectedIndex == Rule_Mode)
		mode = MenuSelectedValue;
	else
		mode = RulesMenuGObj->get<RulesMenuData>()->mode;

	orig_Menu_UpdateStockCountOrTimerText(mode != Mode_Stock && mode != Mode_Crew);
}