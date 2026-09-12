#include <pch/pch.hpp>
#include <core/settings.hpp>
#include <core/features/features.hpp>

#include "../../rendering.hpp"

namespace rendering {

	namespace detail {

		constexpr const char* sound_types[ ]{ "shop click", "home click", "bell", "killcard", "bullet casing", "coin pickup", "item drop", "popcan", "key press", "custom" };
		constexpr auto k_sound_type_count{ static_cast< int >( std::size( sound_types ) ) };

		void draw_custom_sound_picker( config::str& file_setting, std::string_view combo_label, std::string_view preview_id, float preview_volume )
		{
			const auto files = features::misc::impacts::list_custom_sounds( );

			static std::vector<std::string> cached_files{};
			static std::vector<const char*> cached_ptrs{};
			cached_files = files;
			cached_ptrs.clear( );
			cached_ptrs.reserve( cached_files.size( ) );

			for ( const auto& file : cached_files )
			{
				cached_ptrs.push_back( file.c_str( ) );
			}

			if ( !cached_ptrs.empty( ) )
			{
				auto selected{ 0 };
				for ( auto i = 0; i < static_cast< int >( cached_files.size( ) ); ++i )
				{
					if ( cached_files[ static_cast< std::size_t >( i ) ] == file_setting.value )
					{
						selected = i;
						break;
					}
				}

				if ( xui::combo( combo_label, selected, cached_ptrs.data( ), static_cast< int >( cached_ptrs.size( ) ) ) )
				{
					file_setting = cached_files[ static_cast< std::size_t >( selected ) ];
				}
			}

			xui::text_input( "file", file_setting.value, 64, "hit.wav" );

			if ( xui::button( preview_id, 96.0f, 22.0f ) )
			{
				features::misc::g_impacts.play_custom_sound( file_setting.value, preview_volume );
			}
		}
		constexpr const char* marker_types[ ]{ "classic", "damage", "both" };
		constexpr const char* impact_types[ ]{ "overlay", "sparks", "both" };

		constexpr const char* primary_weapons[ ]{ "none", "rifle", "scoped rifle", "scout", "awp", "auto sniper" };
		constexpr const char* secondary_weapons[ ]{ "none", "dual elites", "five-seven/tec-9", "deagle", "revolver" };
		constexpr const char* grenade_names[ ]{ "molotov", "he grenade", "smoke", "flashbang", "decoy" };

		constexpr const char* hat_types[ ]{ "kasa", "bucket" };

	} // namespace detail

	void menu::draw_misc( float group_w ) const
	{
		( void )group_w;
		auto& m = settings::g_misc;
		auto& mov = settings::g_movement;
		auto& impacts = m.m_impacts;
		auto& traj = m.m_projectile_trajectory;
		auto& dlights = m.m_dlight;
		auto& pen = settings::g_combat.m_penetration_crosshair;
		auto& ab = m.m_autobuy;
		auto& rem = m.m_removals;
		auto& cam = m.m_camera;
		auto& vm = m.m_viewmodel_adjust;
		auto& hud = m.m_hud;

		const auto wx = this->m_x;
		const auto wy = this->m_y;
		const auto body_x = this->m_body_x;
		const auto body_y = this->m_body_y;
		const auto body_w = this->m_body_w;
		const auto body_h = this->m_body_h;

		constexpr auto gap = tokens::gap;
		const auto col_w = ( body_w - gap * 2.0f ) / 3.0f;

		const auto col0_x = body_x;
		const auto col1_x = body_x + col_w + gap;
		const auto col2_x = body_x + ( col_w + gap ) * 2.0f;

		const auto total_h = body_h;
		const auto c0_card1_h = ( total_h - gap ) * 0.65f;
		const auto c0_card2_h = total_h - gap - c0_card1_h;

		const auto c1_card1_h = total_h;

		const auto c2_card1_h = ( total_h - gap ) * 0.48f;
		const auto c2_card2_h = total_h - gap - c2_card1_h;

		// ==================== COLUMN 1: MOVEMENT & AUTO BUY ====================
		xui::layout::set_cursor( col0_x - wx, body_y - wy );
		if ( xui::begin_child( "##misc_mov_card", col_w, c0_card1_h, true ) )
		{
			xui::text( "MOVEMENT", tokens::col_text );
			xui::layout::separator( );

			xui::checkbox( "bhop", mov.bhop );
			xui::checkbox( "autostrafe", mov.airstrafe );
			if ( xui::begin_popup( "##autostrafe_popup", 200.0f ) )
			{
				xui::checkbox( "fully directional", mov.airstrafe_fully_directional );
				xui::end_popup( );
			}
			xui::checkbox( "jumpbug", mov.jumpbug );
			xui::checkbox( "fastladder", mov.fastladder );
			xui::checkbox( "edgejump", mov.edgejump );
			xui::checkbox( "edgestop", mov.edgestop );
			xui::checkbox( "edgebug", mov.edgebug );
			if ( xui::begin_popup( "##edgebug_popup", 240.0f ) )
			{
				static const char* edgebug_modes[] = { "0: loose", "1: edge trace (default)", "2: no jump held", "3: min speed", "4: strict vz" };
				xui::combo( "mode##eb", mov.edgebug_mode.value, edgebug_modes, 5 );
				xui::slider_int( "passes##eb", mov.edgebug_passes, 1, 5, "%d" );
				xui::checkbox( "jump steps##eb", mov.edgebug_include_jump_steps );
				xui::end_popup( );
			}
			xui::checkbox( "slowwalk", mov.slowwalk );
			if ( xui::begin_popup( "##slowwalk_popup", 220.0f ) )
			{
				xui::slider_float( "speed", mov.slowwalk_speed, 1.0f, 100.0f, "%.2fs" );
				xui::end_popup( );
			}

			xui::end_child( );
		}

		xui::layout::set_cursor( col0_x - wx, ( body_y + c0_card1_h + gap ) - wy );
		if ( xui::begin_child( "##misc_autobuy_card", col_w, c0_card2_h, true ) )
		{
			xui::text( "AUTO BUY", tokens::col_text );
			xui::layout::separator( );

			xui::checkbox( "auto buy", ab.enabled );
			if ( xui::begin_popup( "##autobuy_popup", 220.0f ) )
			{
				xui::combo( "primary##ab", ab.primary_weapon, detail::primary_weapons, 6 );
				xui::combo( "secondary##ab", ab.secondary_weapon, detail::secondary_weapons, 5 );
				xui::checkbox( "armor##ab", ab.armor );
				xui::checkbox( "defuser##ab", ab.defuser );
				xui::checkbox( "taser##ab", ab.taser );
				xui::multicombo( "grenades##ab", ab.grenades, detail::grenade_names, 5 );
				xui::end_popup( );
			}

			xui::end_child( );
		}

		// ==================== COLUMN 2: HIT EFFECTS & LOGS ====================
		xui::layout::set_cursor( col1_x - wx, body_y - wy );
		if ( xui::begin_child( "##misc_hitfx_card", col_w, c1_card1_h, true ) )
		{
			xui::text( "HIT EFFECTS & LOGS", tokens::col_text );
			xui::layout::separator( );

			xui::checkbox( "hit logs", impacts.hit_log );
			if ( xui::begin_popup( "##hitlog_popup", 220.0f ) )
			{
				xui::slider_float( "duration##hl", impacts.hit_log_duration, 0.5f, 10.0f, "%.1fs" );
				xui::end_popup( );
			}

			xui::checkbox( "console logs", impacts.console_log );
			xui::checkbox( "chat logs", impacts.chat_log );

			xui::checkbox( "miss logs", impacts.miss_log );
			if ( xui::begin_popup( "##misslog_popup", 220.0f ) )
			{
				xui::slider_float( "duration##ml", impacts.miss_log_duration, 0.5f, 10.0f, "%.1fs" );
				xui::end_popup( );
			}

			xui::checkbox( "hit sound", impacts.hit_sound );
			if ( xui::begin_popup( "##hitsound_popup", 220.0f ) )
			{
				xui::combo( "type##hs", impacts.hit_sound_type.value, detail::sound_types, detail::k_sound_type_count );
				xui::slider_float( "volume##hs", impacts.hit_sound_volume, 1.0f, 100.0f, "%.0f%%" );

				if ( impacts.hit_sound_type.value == settings::misc::impacts::sound_type::custom )
				{
					detail::draw_custom_sound_picker( impacts.custom_hit_sound, "sound##hs", "preview##hs", impacts.hit_sound_volume.value );
				}

				xui::end_popup( );
			}

			xui::checkbox( "hit marker", impacts.hit_marker );
			if ( xui::begin_popup( "##hitmarker_popup", 220.0f ) )
			{
				xui::combo( "type##hm", impacts.hit_marker_type.value, detail::marker_types, 3 );
				xui::slider_float( "duration##hm", impacts.hit_marker_duration, 0.1f, 5.0f, "%.1fs" );
				xui::color_picker( "color##hm", impacts.hit_marker_color );
				xui::end_popup( );
			}

			xui::checkbox( "hit effect", impacts.hit_effect );
			if ( xui::begin_popup( "##hitfx_popup", 220.0f ) )
			{
				xui::color_picker( "color##hitfx", impacts.hit_effect_color );
				xui::slider_float( "duration##hitfx", impacts.hit_effect_duration, 0.1f, 5.0f, "%.1fs" );
				xui::slider_float( "strength##hitfx", impacts.hit_effect_strength, 1.0f, 100.0f, "%.0f%%" );
				xui::end_popup( );
			}

			xui::checkbox( "death sound", impacts.death_sound );
			if ( xui::begin_popup( "##deathsound_popup", 220.0f ) )
			{
				xui::combo( "type##ds", impacts.death_sound_type.value, detail::sound_types, detail::k_sound_type_count );
				xui::slider_float( "volume##ds", impacts.death_sound_volume, 1.0f, 100.0f, "%.0f%%" );

				if ( impacts.death_sound_type.value == settings::misc::impacts::sound_type::custom )
				{
					detail::draw_custom_sound_picker( impacts.custom_death_sound, "sound##ds", "preview##ds", impacts.death_sound_volume.value );
				}

				xui::end_popup( );
			}

			xui::checkbox( "death effect", impacts.death_effect );
			if ( xui::begin_popup( "##deathfx_popup", 220.0f ) )
			{
				xui::color_picker( "color##deathfx", impacts.death_effect_color );
				xui::end_popup( );
			}

			xui::checkbox( "scoreboard weapons", m.m_scoreboard_weapons.enabled );
			if ( xui::begin_popup( "##scoreboardeq_popup", 220.0f ) )
			{
				xui::color_picker( "color##scoreboardeq", m.m_scoreboard_weapons.color );
				xui::end_popup( );
			}

			xui::checkbox( "bullet impacts", impacts.bullet_impact_effect );
			if ( xui::begin_popup( "##bulletfx_popup", 220.0f ) )
			{
				xui::combo( "type##bulletfx", impacts.bullet_impact_effect_type.value, detail::impact_types, 3 );

				const auto type = impacts.bullet_impact_effect_type.value;
				const auto show_overlay = type == settings::misc::impacts::bullet_impact_type::overlay || type == settings::misc::impacts::bullet_impact_type::both;
				const auto show_sparks = type == settings::misc::impacts::bullet_impact_type::sparks || type == settings::misc::impacts::bullet_impact_type::both;

				if ( show_overlay )
				{
					xui::slider_float( "duration##bulletfx", impacts.bullet_impact_effect_duration, 0.1f, 5.0f, "%.1fs" );
					xui::color_picker( "fill##bulletfx", impacts.bullet_impact_effect_fill_color );
					xui::color_picker( "edge##bulletfx", impacts.bullet_impact_effect_edge_color );

					xui::checkbox( "glow##bulletfx", impacts.bullet_impact_effect_glow );
					if ( impacts.bullet_impact_effect_glow )
					{
						xui::slider_float( "glow strength##bulletfx", impacts.bullet_impact_effect_glow_strength, 0.1f, 1.0f, "%.2f" );
					}
				}

				if ( show_sparks )
				{
					xui::color_picker( "spark##bulletfx", impacts.bullet_impact_effect_color_spark );
				}

				xui::end_popup( );
			}

			xui::checkbox( "bullet tracers", impacts.bullet_tracers );
			if ( xui::begin_popup( "##tracers_popup", 220.0f ) )
			{
				xui::slider_float( "duration##tracer", impacts.bullet_tracer_duration, 0.1f, 5.0f, "%.1fs" );
				xui::color_picker( "color##tracer", impacts.bullet_tracer_color );
				xui::end_popup( );
			}

			xui::end_child( );
		}

		// ==================== COLUMN 3: REMOVALS & CAMERA, HUD & OTHER ====================
		xui::layout::set_cursor( col2_x - wx, body_y - wy );
		if ( xui::begin_child( "##misc_camera_card", col_w, c2_card1_h, true ) )
		{
			xui::text( "REMOVALS & CAMERA", tokens::col_text );
			xui::layout::separator( );

			xui::checkbox( "remove crosshair", rem.crosshair );
			xui::checkbox( "remove scope", rem.scope );
			xui::checkbox( "remove overhead", rem.overhead );
			xui::checkbox( "remove legs", rem.legs );
			xui::checkbox( "remove recoil", rem.recoil );
			xui::checkbox( "remove skybox fog", rem.skybox_fog );
			xui::checkbox( "remove 3d skybox", rem.skybox_3d );
			xui::checkbox( "remove decals", rem.decals );
			xui::checkbox( "remove smoke", rem.smoke );
			xui::slider_float( "flash alpha##flash", rem.flash_alpha, 0.0f, 100.0f, "%.0f%%" );

			xui::checkbox( "custom fov", cam.change_fov );
			if ( xui::begin_popup( "##fov_popup", 220.0f ) )
			{
				xui::slider_float( "fov", cam.fov, 60.0f, 150.0f, "%.0f" );
				xui::checkbox( "scoped fov override", cam.scoped_fov_override );
				xui::slider_float( "scoped fov", cam.scoped_fov, 10.0f, 90.0f, "%.0f" );
				xui::end_popup( );
			}

			xui::checkbox( "thirdperson", cam.thirdperson );
			if ( xui::begin_popup( "##tp_popup", 220.0f ) )
			{
				xui::slider_float( "distance", cam.thirdperson_distance, 35.0f, 200.0f, "%.0f" );
				xui::slider_float( "hull size", cam.thirdperson_hull_size, 0.0f, 20.0f, "%.0f" );
				xui::end_popup( );
			}

			xui::checkbox( "aspect ratio", cam.change_aspect_ratio );
			if ( xui::begin_popup( "##ar_popup", 220.0f ) )
			{
				xui::slider_float( "ratio##ar", cam.aspect_ratio, 1.0f, 1.78f, "%.3f" );
				xui::end_popup( );
			}

			xui::checkbox( "viewmodel adjust", vm.enabled );
			if ( xui::begin_popup( "##vm_popup", 220.0f ) )
			{
				xui::slider_float( "offset x", vm.offset_x, -10.0f, 10.0f, "%.1f" );
				xui::slider_float( "offset y", vm.offset_y, -10.0f, 10.0f, "%.1f" );
				xui::slider_float( "offset z", vm.offset_z, -10.0f, 10.0f, "%.1f" );
				xui::slider_float( "fov", vm.fov, 54.0f, 90.0f, "%.0f" );
				xui::end_popup( );
			}

			xui::end_child( );
		}

		xui::layout::set_cursor( col2_x - wx, ( body_y + c2_card1_h + gap ) - wy );
		if ( xui::begin_child( "##misc_hud_card", col_w, c2_card2_h, true ) )
		{
			xui::text( "HUD & OTHER", tokens::col_text );
			xui::layout::separator( );

			xui::checkbox( "crosshair overlay", hud.m_crosshair.enabled );
			if ( xui::begin_popup( "##xhair_popup", 220.0f ) )
			{
				xui::slider_float( "size##xhair", hud.m_crosshair.size, 0.5f, 10.0f, "%.1f" );
				xui::slider_float( "outline##xhair", hud.m_crosshair.outline, 0.0f, 4.0f, "%.1f" );
				xui::color_picker( "color##xhair", hud.m_crosshair.color );
				xui::color_picker( "outline color##xhair", hud.m_crosshair.outline_color );
				xui::end_popup( );
			}

			xui::checkbox( "scope overlay", hud.m_scope.enabled );
			if ( xui::begin_popup( "##scope_popup", 220.0f ) )
			{
				xui::slider_float( "line length", hud.m_scope.line_length, 10.0f, 500.0f, "%.0f" );
				xui::slider_float( "gap##scope", hud.m_scope.gap, 0.0f, 50.0f, "%.0f" );
				xui::slider_float( "thickness##scope", hud.m_scope.thickness, 0.5f, 5.0f, "%.2f" );
				xui::slider_float( "anim speed", hud.m_scope.anim_speed, 1.0f, 30.0f, "%.0f" );
				xui::color_picker( "color##scope", hud.m_scope.color );
				xui::checkbox( "fade in##scope", hud.m_scope.fade_in );

				xui::layout::separator( );

				xui::checkbox( "glow##scope", hud.m_scope.glow );
				xui::slider_float( "glow strength##scope", hud.m_scope.glow_strength, 0.1f, 1.0f, "%.2f" );
				xui::end_popup( );
			}

			xui::checkbox( "velocity counter", hud.m_velocity.counter );
			xui::checkbox( "velocity chart", hud.m_velocity.chart );
			if ( xui::begin_popup( "##velocity_hud_popup", 220.0f ) )
			{
				xui::color_picker( "color##velocity", hud.m_velocity.color );
				xui::slider_float( "bottom offset", hud.m_velocity.bottom_offset, 20.0f, 200.0f, "%.0f" );
				xui::slider_float( "chart width", hud.m_velocity.chart_width, 120.0f, 320.0f, "%.0f" );
				xui::slider_float( "chart height", hud.m_velocity.chart_height, 24.0f, 80.0f, "%.0f" );
				xui::end_popup( );
			}

			xui::checkbox( "hat", hud.m_hat.enabled );
			if ( xui::begin_popup( "##hat_popup", 220.0f ) )
			{
				xui::combo( "type##hat", hud.m_hat.type.value, detail::hat_types, 2 );
				xui::color_picker( "color##hat", hud.m_hat.color );
				xui::color_picker( "secondary color##hat", hud.m_hat.secondary_color );
				xui::checkbox( "glow##hat", hud.m_hat.glow );
				xui::slider_float( "glow strength##hat", hud.m_hat.glow_strength, 0.1f, 1.0f, "%.2f" );
				xui::end_popup( );
			}

			xui::checkbox( "projectile trajectory", traj.enabled );
			if ( xui::begin_popup( "##traj_popup", 220.0f ) )
			{
				xui::checkbox( "straight throw", traj.straight_throw );
				xui::color_picker( "held color", traj.held_color );
				xui::color_picker( "thrown color", traj.thrown_color );
				xui::color_picker( "will damage held color", traj.will_deal_damage_held_color );
				xui::color_picker( "will damage thrown color", traj.will_deal_damage_thrown_color );
				xui::end_popup( );
			}

			xui::checkbox( "dynamic light", dlights.enabled );
			if ( xui::begin_popup( "##dlight_popup", 220.0f ) )
			{
				xui::color_picker( "color##dl", dlights.color );
				xui::slider_float( "radius##dl", dlights.radius, 50.0f, 15000.0f, "%.0f" );
				xui::slider_float( "z offset##dl", dlights.z_offset, 0.0f, 100.0f, "%.0f" );
				xui::end_popup( );
			}

			xui::checkbox( "penetration crosshair", pen.enabled );
			if ( xui::begin_popup( "##pen_popup", 220.0f ) )
			{
				xui::checkbox( "glow##pen", pen.glow );
				xui::slider_float( "glow strength##pen", pen.glow_strength, 0.1f, 1.0f, "%.2f" );
				xui::color_picker( "can penetrate##pen", pen.can_penetrate_fill );
				xui::color_picker( "can pen outline##pen", pen.can_penetrate_outline );
				xui::color_picker( "blocked##pen", pen.blocked_fill );
				xui::color_picker( "blocked outline##pen", pen.blocked_outline );
				xui::end_popup( );
			}

			xui::checkbox( "reveal radar", m.reveal_radar );
			xui::checkbox( "preserve killfeed", m.preserve_killfeed );
			xui::checkbox( "disable game logs", m.disable_game_logs );

			xui::checkbox( "clantag", m.m_name_changer.clantag );
			xui::checkbox( "override name", m.m_name_changer.override_name );
			if ( xui::begin_popup( "##override_name_popup", 220.0f ) )
			{
				xui::text_input( "name##nc", m.m_name_changer.name.value, 32, "player name..." );
				xui::end_popup( );
			}

			xui::checkbox( "watermark", m.m_watermark.enabled );
			if ( xui::begin_popup( "##watermark_popup", 200.0f ) )
			{
				xui::checkbox( "fps##wm",      m.m_watermark.show_fps );
				xui::checkbox( "ping##wm",     m.m_watermark.show_ping );
				xui::checkbox( "time##wm",     m.m_watermark.show_time );
				xui::checkbox( "user##wm",     m.m_watermark.show_user );
				xui::checkbox( "map##wm",      m.m_watermark.show_map );
				xui::checkbox( "tick rate##wm",m.m_watermark.show_tick );
				xui::checkbox( "velocity##wm", m.m_watermark.show_velocity );
				xui::end_popup( );
			}

			xui::end_child( );
		}
	}

} // namespace rendering
