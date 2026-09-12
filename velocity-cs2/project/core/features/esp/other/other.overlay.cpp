#include <pch/pch.hpp>
#include <utilities/memory/memory.hpp>
#include <utilities/addresses/addresses.hpp>
#include <utilities/steam/steam.hpp>
#include <core/rendering/rendering.hpp>
#include <core/settings.hpp>
#include <core/features/features.hpp>

namespace features::esp::other {

	namespace detail {

		struct avatar_cache
		{
			struct entry
			{
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture{};
				bool attempted{};
			};

			std::unordered_map<std::uintptr_t, entry> m_entries{};

			[[nodiscard]] ID3D11ShaderResourceView* get( std::uintptr_t steam_id )
			{
				auto it = this->m_entries.find( steam_id );
				if ( it != this->m_entries.end( ) )
				{
					return it->second.texture.Get( );
				}

				auto& e = this->m_entries[ steam_id ];
				e.attempted = true;

				const auto image_handle = steam::friends::get_medium_friend_avatar( steam_id );
				if ( image_handle <= 0 )
				{
					return nullptr;
				}

				std::uint32_t w{}, h{};
				if ( !steam::utils::get_image_size( image_handle, &w, &h ) || !w || !h )
				{
					return nullptr;
				}

				std::vector<std::uint8_t> rgba( w * h * 4 );
				if ( !steam::utils::get_image_rgba( image_handle, rgba.data( ), static_cast< int >( rgba.size( ) ) ) )
				{
					return nullptr;
				}

				e.texture = xdraw::create_srv_from_rgba( rgba.data( ), static_cast< int >( w ), static_cast< int >( h ) );
				return e.texture.Get( );
			}

			void clear( )
			{
				this->m_entries.clear( );
			}
		};

		constexpr unsigned char spectator_icon[ 1030 ]
		{
			0x3C, 0x73, 0x76, 0x67, 0x20, 0x77, 0x69, 0x64, 0x74, 0x68, 0x3D, 0x22,
			0x31, 0x32, 0x22, 0x20, 0x68, 0x65, 0x69, 0x67, 0x68, 0x74, 0x3D, 0x22,
			0x31, 0x32, 0x22, 0x20, 0x76, 0x69, 0x65, 0x77, 0x42, 0x6F, 0x78, 0x3D,
			0x22, 0x30, 0x20, 0x30, 0x20, 0x31, 0x32, 0x20, 0x31, 0x32, 0x22, 0x20,
			0x66, 0x69, 0x6C, 0x6C, 0x3D, 0x22, 0x6E, 0x6F, 0x6E, 0x65, 0x22, 0x20,
			0x78, 0x6D, 0x6C, 0x6E, 0x73, 0x3D, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3A,
			0x2F, 0x2F, 0x77, 0x77, 0x77, 0x2E, 0x77, 0x33, 0x2E, 0x6F, 0x72, 0x67,
			0x2F, 0x32, 0x30, 0x30, 0x30, 0x2F, 0x73, 0x76, 0x67, 0x22, 0x3E, 0x0D,
			0x0A, 0x3C, 0x67, 0x20, 0x63, 0x6C, 0x69, 0x70, 0x2D, 0x70, 0x61, 0x74,
			0x68, 0x3D, 0x22, 0x75, 0x72, 0x6C, 0x28, 0x23, 0x63, 0x6C, 0x69, 0x70,
			0x30, 0x5F, 0x31, 0x35, 0x35, 0x5F, 0x32, 0x32, 0x33, 0x29, 0x22, 0x3E,
			0x0D, 0x0A, 0x3C, 0x70, 0x61, 0x74, 0x68, 0x20, 0x64, 0x3D, 0x22, 0x4D,
			0x35, 0x20, 0x36, 0x43, 0x35, 0x20, 0x36, 0x2E, 0x32, 0x36, 0x35, 0x32,
			0x32, 0x20, 0x35, 0x2E, 0x31, 0x30, 0x35, 0x33, 0x36, 0x20, 0x36, 0x2E,
			0x35, 0x31, 0x39, 0x35, 0x37, 0x20, 0x35, 0x2E, 0x32, 0x39, 0x32, 0x38,
			0x39, 0x20, 0x36, 0x2E, 0x37, 0x30, 0x37, 0x31, 0x31, 0x43, 0x35, 0x2E,
			0x34, 0x38, 0x30, 0x34, 0x33, 0x20, 0x36, 0x2E, 0x38, 0x39, 0x34, 0x36,
			0x34, 0x20, 0x35, 0x2E, 0x37, 0x33, 0x34, 0x37, 0x38, 0x20, 0x37, 0x20,
			0x36, 0x20, 0x37, 0x43, 0x36, 0x2E, 0x32, 0x36, 0x35, 0x32, 0x32, 0x20,
			0x37, 0x20, 0x36, 0x2E, 0x35, 0x31, 0x39, 0x35, 0x37, 0x20, 0x36, 0x2E,
			0x38, 0x39, 0x34, 0x36, 0x34, 0x20, 0x36, 0x2E, 0x37, 0x30, 0x37, 0x31,
			0x31, 0x20, 0x36, 0x2E, 0x37, 0x30, 0x37, 0x31, 0x31, 0x43, 0x36, 0x2E,
			0x38, 0x39, 0x34, 0x36, 0x34, 0x20, 0x36, 0x2E, 0x35, 0x31, 0x39, 0x35,
			0x37, 0x20, 0x37, 0x20, 0x36, 0x2E, 0x32, 0x36, 0x35, 0x32, 0x32, 0x20,
			0x37, 0x20, 0x36, 0x43, 0x37, 0x20, 0x35, 0x2E, 0x37, 0x33, 0x34, 0x37,
			0x38, 0x20, 0x36, 0x2E, 0x38, 0x39, 0x34, 0x36, 0x34, 0x20, 0x35, 0x2E,
			0x34, 0x38, 0x30, 0x34, 0x33, 0x20, 0x36, 0x2E, 0x37, 0x30, 0x37, 0x31,
			0x31, 0x20, 0x35, 0x2E, 0x32, 0x39, 0x32, 0x38, 0x39, 0x43, 0x36, 0x2E,
			0x35, 0x31, 0x39, 0x35, 0x37, 0x20, 0x35, 0x2E, 0x31, 0x30, 0x35, 0x33,
			0x36, 0x20, 0x36, 0x2E, 0x32, 0x36, 0x35, 0x32, 0x32, 0x20, 0x35, 0x20,
			0x36, 0x20, 0x35, 0x43, 0x35, 0x2E, 0x37, 0x33, 0x34, 0x37, 0x38, 0x20,
			0x35, 0x20, 0x35, 0x2E, 0x34, 0x38, 0x30, 0x34, 0x33, 0x20, 0x35, 0x2E,
			0x31, 0x30, 0x35, 0x33, 0x36, 0x20, 0x35, 0x2E, 0x32, 0x39, 0x32, 0x38,
			0x39, 0x20, 0x35, 0x2E, 0x32, 0x39, 0x32, 0x38, 0x39, 0x43, 0x35, 0x2E,
			0x31, 0x30, 0x35, 0x33, 0x36, 0x20, 0x35, 0x2E, 0x34, 0x38, 0x30, 0x34,
			0x33, 0x20, 0x35, 0x20, 0x35, 0x2E, 0x37, 0x33, 0x34, 0x37, 0x38, 0x20,
			0x35, 0x20, 0x36, 0x5A, 0x22, 0x20, 0x73, 0x74, 0x72, 0x6F, 0x6B, 0x65,
			0x3D, 0x22, 0x23, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x22, 0x20, 0x73,
			0x74, 0x72, 0x6F, 0x6B, 0x65, 0x2D, 0x6C, 0x69, 0x6E, 0x65, 0x63, 0x61,
			0x70, 0x3D, 0x22, 0x72, 0x6F, 0x75, 0x6E, 0x64, 0x22, 0x20, 0x73, 0x74,
			0x72, 0x6F, 0x6B, 0x65, 0x2D, 0x6C, 0x69, 0x6E, 0x65, 0x6A, 0x6F, 0x69,
			0x6E, 0x3D, 0x22, 0x72, 0x6F, 0x75, 0x6E, 0x64, 0x22, 0x2F, 0x3E, 0x0D,
			0x0A, 0x3C, 0x70, 0x61, 0x74, 0x68, 0x20, 0x64, 0x3D, 0x22, 0x4D, 0x37,
			0x2E, 0x35, 0x31, 0x35, 0x20, 0x38, 0x2E, 0x37, 0x33, 0x39, 0x43, 0x37,
			0x2E, 0x30, 0x32, 0x39, 0x32, 0x34, 0x20, 0x38, 0x2E, 0x39, 0x31, 0x34,
			0x32, 0x36, 0x20, 0x36, 0x2E, 0x35, 0x31, 0x36, 0x34, 0x31, 0x20, 0x39,
			0x2E, 0x30, 0x30, 0x32, 0x36, 0x31, 0x20, 0x36, 0x20, 0x39, 0x43, 0x34,
			0x2E, 0x32, 0x20, 0x39, 0x20, 0x32, 0x2E, 0x37, 0x20, 0x38, 0x20, 0x31,
			0x2E, 0x35, 0x20, 0x36, 0x43, 0x32, 0x2E, 0x37, 0x20, 0x34, 0x20, 0x34,
			0x2E, 0x32, 0x20, 0x33, 0x20, 0x36, 0x20, 0x33, 0x43, 0x37, 0x2E, 0x38,
			0x20, 0x33, 0x20, 0x39, 0x2E, 0x33, 0x20, 0x34, 0x20, 0x31, 0x30, 0x2E,
			0x35, 0x20, 0x36, 0x43, 0x31, 0x30, 0x2E, 0x34, 0x35, 0x37, 0x38, 0x20,
			0x36, 0x2E, 0x30, 0x37, 0x30, 0x33, 0x35, 0x20, 0x31, 0x30, 0x2E, 0x34,
			0x31, 0x34, 0x38, 0x20, 0x36, 0x2E, 0x31, 0x34, 0x30, 0x31, 0x39, 0x20,
			0x31, 0x30, 0x2E, 0x33, 0x37, 0x31, 0x20, 0x36, 0x2E, 0x32, 0x30, 0x39,
			0x35, 0x22, 0x20, 0x73, 0x74, 0x72, 0x6F, 0x6B, 0x65, 0x3D, 0x22, 0x23,
			0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x22, 0x20, 0x73, 0x74, 0x72, 0x6F,
			0x6B, 0x65, 0x2D, 0x6C, 0x69, 0x6E, 0x65, 0x63, 0x61, 0x70, 0x3D, 0x22,
			0x72, 0x6F, 0x75, 0x6E, 0x64, 0x22, 0x20, 0x73, 0x74, 0x72, 0x6F, 0x6B,
			0x65, 0x2D, 0x6C, 0x69, 0x6E, 0x65, 0x6A, 0x6F, 0x69, 0x6E, 0x3D, 0x22,
			0x72, 0x6F, 0x75, 0x6E, 0x64, 0x22, 0x2F, 0x3E, 0x0D, 0x0A, 0x3C, 0x70,
			0x61, 0x74, 0x68, 0x20, 0x64, 0x3D, 0x22, 0x4D, 0x39, 0x2E, 0x35, 0x20,
			0x38, 0x56, 0x39, 0x2E, 0x35, 0x22, 0x20, 0x73, 0x74, 0x72, 0x6F, 0x6B,
			0x65, 0x3D, 0x22, 0x23, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x22, 0x20,
			0x73, 0x74, 0x72, 0x6F, 0x6B, 0x65, 0x2D, 0x6C, 0x69, 0x6E, 0x65, 0x63,
			0x61, 0x70, 0x3D, 0x22, 0x72, 0x6F, 0x75, 0x6E, 0x64, 0x22, 0x20, 0x73,
			0x74, 0x72, 0x6F, 0x6B, 0x65, 0x2D, 0x6C, 0x69, 0x6E, 0x65, 0x6A, 0x6F,
			0x69, 0x6E, 0x3D, 0x22, 0x72, 0x6F, 0x75, 0x6E, 0x64, 0x22, 0x2F, 0x3E,
			0x0D, 0x0A, 0x3C, 0x70, 0x61, 0x74, 0x68, 0x20, 0x64, 0x3D, 0x22, 0x4D,
			0x39, 0x2E, 0x35, 0x20, 0x31, 0x31, 0x56, 0x31, 0x31, 0x2E, 0x30, 0x30,
			0x35, 0x22, 0x20, 0x73, 0x74, 0x72, 0x6F, 0x6B, 0x65, 0x3D, 0x22, 0x23,
			0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x22, 0x20, 0x73, 0x74, 0x72, 0x6F,
			0x6B, 0x65, 0x2D, 0x6C, 0x69, 0x6E, 0x65, 0x63, 0x61, 0x70, 0x3D, 0x22,
			0x72, 0x6F, 0x75, 0x6E, 0x64, 0x22, 0x20, 0x73, 0x74, 0x72, 0x6F, 0x6B,
			0x65, 0x2D, 0x6C, 0x69, 0x6E, 0x65, 0x6A, 0x6F, 0x69, 0x6E, 0x3D, 0x22,
			0x72, 0x6F, 0x75, 0x6E, 0x64, 0x22, 0x2F, 0x3E, 0x0D, 0x0A, 0x3C, 0x2F,
			0x67, 0x3E, 0x0D, 0x0A, 0x3C, 0x64, 0x65, 0x66, 0x73, 0x3E, 0x0D, 0x0A,
			0x3C, 0x63, 0x6C, 0x69, 0x70, 0x50, 0x61, 0x74, 0x68, 0x20, 0x69, 0x64,
			0x3D, 0x22, 0x63, 0x6C, 0x69, 0x70, 0x30, 0x5F, 0x31, 0x35, 0x35, 0x5F,
			0x32, 0x32, 0x33, 0x22, 0x3E, 0x0D, 0x0A, 0x3C, 0x72, 0x65, 0x63, 0x74,
			0x20, 0x77, 0x69, 0x64, 0x74, 0x68, 0x3D, 0x22, 0x31, 0x32, 0x22, 0x20,
			0x68, 0x65, 0x69, 0x67, 0x68, 0x74, 0x3D, 0x22, 0x31, 0x32, 0x22, 0x20,
			0x66, 0x69, 0x6C, 0x6C, 0x3D, 0x22, 0x77, 0x68, 0x69, 0x74, 0x65, 0x22,
			0x2F, 0x3E, 0x0D, 0x0A, 0x3C, 0x2F, 0x63, 0x6C, 0x69, 0x70, 0x50, 0x61,
			0x74, 0x68, 0x3E, 0x0D, 0x0A, 0x3C, 0x2F, 0x64, 0x65, 0x66, 0x73, 0x3E,
			0x0D, 0x0A, 0x3C, 0x2F, 0x73, 0x76, 0x67, 0x3E, 0x0D, 0x0A
		};

	} // namespace detail

	void overlay::on_render( xdraw::draw_list& draw_list )
	{
		this->add_spectators( draw_list );
		this->add_bomb( draw_list );
	}

	void overlay::add_bomb( xdraw::draw_list& draw_list )
	{
		const auto local = systems::g_local.get( );
		if ( !local.is_valid( ) || !systems::g_entities.exists( local.view_controller( ) ) )
		{
			return;
		}

		const auto planted_c4 = memory::read<std::uintptr_t>( addresses::globals::planted_c4 );
		const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars );

		if ( !planted_c4 || !global_vars )
		{
			return;
		}

		const auto current_time = memory::read<float>( global_vars + 0x30 );
		const auto blow_time = memory::read<float>( planted_c4 + SCHEMA( "C_PlantedC4", "m_flC4Blow"_hash ) );
		const auto has_exploded = memory::read<bool>( planted_c4 + SCHEMA( "C_PlantedC4", "m_bHasExploded"_hash ) );
		const auto bomb_defused = memory::read<bool>( planted_c4 + SCHEMA( "C_PlantedC4", "m_bBombDefused"_hash ) );

		if ( bomb_defused )
		{
			return;
		}

		const auto time_remaining = blow_time - current_time;
		const auto is_exploding = has_exploded || time_remaining <= 0.0f;

		if ( is_exploding && time_remaining < -2.0f )
		{
			return;
		}

		const auto bomb_site = memory::read<int>( planted_c4 + SCHEMA( "C_PlantedC4", "m_nBombSite"_hash ) );
		const auto being_defused = memory::read<bool>( planted_c4 + SCHEMA( "C_PlantedC4", "m_bBeingDefused"_hash ) );
		const auto timer_length = memory::read<float>( planted_c4 + SCHEMA( "C_PlantedC4", "m_flTimerLength"_hash ) );

		const auto calculate_bomb_damage = [ & ]( ) -> float
			{
				const auto view_pawn = local.view_pawn( );
				if ( !view_pawn )
				{
					return 0.0f;
				}

				const auto c4_scene_node = memory::read<std::uintptr_t>( planted_c4 + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
				const auto pawn_scene_node = memory::read<std::uintptr_t>( view_pawn + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );

				if ( !c4_scene_node || !pawn_scene_node )
				{
					return 0.0f;
				}

				const auto c4_origin = memory::read<math::vector3>( c4_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );
				const auto pawn_origin = memory::read<math::vector3>( pawn_scene_node + SCHEMA( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );

				const auto distance = ( c4_origin - pawn_origin ).length( );

				constexpr auto default_damage{ 650.0f };
				constexpr auto default_radius{ 2275.0f };

				const auto sigma = default_radius / 3.0f;
				auto damage = default_damage * std::exp( -( distance * distance ) / ( 2.0f * sigma * sigma ) );

				const auto armor = memory::read<int>( view_pawn + SCHEMA( "C_CSPlayerPawn", "m_ArmorValue"_hash ) );

				if ( armor > 0 )
				{
					constexpr auto armor_ratio = 0.5f;
					constexpr auto armor_bonus = 0.5f;

					auto armor_absorbed = damage * armor_ratio;
					auto armor_cost = ( damage - armor_absorbed ) * armor_bonus;

					if ( armor_cost > static_cast< float >( armor ) )
					{
						armor_cost = static_cast< float >( armor ) * ( 1.0f / armor_bonus );
						armor_absorbed = damage - armor_cost;
					}

					damage = armor_absorbed;
				}

				return std::floor( damage );
			}( );

		const auto [screen_w, screen_h] = xdraw::viewport_size( );
		const auto& s = xui::ctx( ).style;

		constexpr auto h{ 26.0f };
		constexpr auto top_offset{ 175.0f };
		const auto r = h * 0.5f;
		constexpr auto inner_h{ 21.0f };
		const auto inner_r = inner_h * 0.5f;
		const auto inner_pad = ( h - inner_h ) * 0.5f;
		constexpr auto text_pad_x{ 8.0f };
		constexpr auto text_nudge{ 0.5f };
		constexpr auto section_spacing{ 3.0f };

		auto timer_color = [ & ]( ) -> xdraw::color
			{
				if ( is_exploding )
				{
					return { 255, 100, 100, 255 };
				}

				const auto frac = timer_length > 0.0f ? time_remaining / timer_length : 1.0f;

				if ( frac > 0.5f )
				{
					return tokens::col_accent;
				}
				else if ( frac > 0.2f )
				{
					const auto t = ( frac - 0.2f ) / 0.3f;

					return
					{
						static_cast< std::uint8_t >( 255 ),
						static_cast< std::uint8_t >( 200 + static_cast< int >( ( tokens::col_accent.g - 200 ) * t ) ),
						static_cast< std::uint8_t >( 140 + static_cast< int >( ( tokens::col_accent.b - 140 ) * t ) ),
						255
					};
				}
				else
				{
					const auto t = frac / 0.2f;

					return
					{
						255,
						static_cast< std::uint8_t >( 120 + static_cast< int >( 80 * t ) ),
						static_cast< std::uint8_t >( 100 + static_cast< int >( 40 * t ) ),
						255
					};
				}
			}( );

		const auto site_label = bomb_site == 0 ? "A plant" : "B plant";
		const auto [site_tw, site_th] = xdraw::measure_text( site_label );
		const auto site_pill_w = site_tw + text_pad_x * 2.0f;

		const auto damage = static_cast< int >( calculate_bomb_damage );
		const auto view_pawn = local.view_pawn( );
		const auto health = view_pawn ? memory::read<int>( view_pawn + SCHEMA( "C_BaseEntity", "m_iHealth"_hash ) ) : 0;
		const auto will_kill = health <= damage;

		char health_buf[ 16 ]{};
		std::snprintf( health_buf, sizeof( health_buf ), "%+d", -damage );

		const auto [health_vw, health_vh] = xdraw::measure_text( health_buf );
		const auto [health_uw, health_uh] = xdraw::measure_text( " health" );
		const auto health_pill_w = health_vw + health_uw + text_pad_x * 2.0f;
		const auto health_col = will_kill ? xdraw::color{ 255, 120, 120, 255 } : xdraw::color{ 160, 210, 140, 255 };

		char timer_buf[ 16 ]{};
		const char* timer_unit{};

		if ( is_exploding )
		{
			strncpy_s( timer_buf, sizeof( timer_buf ), "0.0s", _TRUNCATE );
			timer_unit = " exploding";
		}
		else
		{
			std::snprintf( timer_buf, sizeof( timer_buf ), "%.1fs", time_remaining );
			timer_unit = being_defused ? " defusing" : " explosion";
		}

		const auto [timer_vw, timer_vh] = xdraw::measure_text( timer_buf );
		const auto [timer_uw, timer_uh] = xdraw::measure_text( timer_unit );
		const auto timer_pill_w = timer_vw + timer_uw + text_pad_x * 2.0f;

		const auto total_w = inner_pad + site_pill_w + section_spacing + health_pill_w + section_spacing + timer_pill_w + inner_pad;
		const auto x = ( static_cast< float >( screen_w ) - total_w ) * 0.5f;
		const auto y = top_offset;

		draw_list.rect_filled_blurred( x, y, total_w, h, xdraw::corner_radius{ r } );
		draw_list.rect_filled( x, y, total_w, h, tokens::col_dark.alpha( 230 ), xdraw::corner_radius{ r } );
		draw_list.rect( x, y, total_w, h, tokens::col_elevated.alpha( 170 ), xdraw::corner_radius{ r }, 1.0f );

		auto cx = x + inner_pad;

		// Site badge: accent fill with dark text
		draw_list.rect_filled( cx, y + inner_pad, site_pill_w, inner_h, tokens::col_accent, xdraw::corner_radius{ inner_r } );
		draw_list.text( cx + text_pad_x, y + ( h - site_th ) * 0.5f + text_nudge, site_label, tokens::col_dark );
		cx += site_pill_w + section_spacing;

		// Health chip
		draw_list.rect_filled( cx, y + inner_pad, health_pill_w, inner_h, tokens::col_card, xdraw::corner_radius{ inner_r } );
		draw_list.rect( cx, y + inner_pad, health_pill_w, inner_h, tokens::col_elevated.alpha( 130 ), xdraw::corner_radius{ inner_r }, 1.0f );
		draw_list.text( cx + text_pad_x, y + ( h - health_vh ) * 0.5f + text_nudge, health_buf, health_col );
		draw_list.text( cx + text_pad_x + health_vw, y + ( h - health_uh ) * 0.5f + text_nudge, " health", tokens::col_text_dim );
		cx += health_pill_w + section_spacing;

		// Timer chip
		draw_list.rect_filled( cx, y + inner_pad, timer_pill_w, inner_h, tokens::col_card, xdraw::corner_radius{ inner_r } );
		draw_list.rect( cx, y + inner_pad, timer_pill_w, inner_h, tokens::col_elevated.alpha( 130 ), xdraw::corner_radius{ inner_r }, 1.0f );
		draw_list.text( cx + text_pad_x, y + ( h - timer_vh ) * 0.5f + text_nudge, timer_buf, timer_color );
		draw_list.text( cx + text_pad_x + timer_vw, y + ( h - timer_uh ) * 0.5f + text_nudge, timer_unit, tokens::col_text_dim );
	}

	void overlay::add_spectators( xdraw::draw_list& draw_list )
	{
		const auto local = systems::g_local.get( );
		if ( !local.is_valid( ) || !systems::g_entities.exists( local.view_controller( ) ) )
		{
			return;
		}

		const auto game_rules = memory::read<std::uintptr_t>( addresses::globals::game_rules );
		if ( !game_rules || memory::read<int>( game_rules + SCHEMA( "C_CSGameRules", "m_gamePhase"_hash ) ) >= 4 )
		{
			return;
		}

		const auto local_controller = local.controller;
		const auto view_controller = local.view_controller( );
		const auto view_pawn = local.view_pawn( );
		if ( !view_pawn )
		{
			return;
		}

		const auto [screen_w, screen_h] = xdraw::viewport_size( );
		const auto& s = xui::ctx( ).style;

		struct spectator_entry
		{
			char name[ 128 ];
			std::uintptr_t steam_id;
		};

		spectator_entry entries[ 32 ]{};
		auto count{ 0 };

		for ( const auto& player : systems::g_entities.get_by_type( systems::entities::type::player ) )
		{
			if ( player.ptr == view_controller || player.ptr == local_controller || count >= 32 )
			{
				continue;
			}

			if ( memory::read<bool>( player.ptr + SCHEMA( "CCSPlayerController", "m_bPawnIsAlive"_hash ) ) )
			{
				continue;
			}

			const auto obs_pawn_handle = memory::read<std::uint32_t>( player.ptr + SCHEMA( "CCSPlayerController", "m_hObserverPawn"_hash ) );
			if ( !obs_pawn_handle || obs_pawn_handle == 0xffffffff )
			{
				continue;
			}

			const auto obs_pawn = systems::g_entities.lookup( obs_pawn_handle );
			if ( !obs_pawn )
			{
				continue;
			}

			const auto observer_services = memory::safe_read<std::uintptr_t>( obs_pawn + SCHEMA( "C_BasePlayerPawn", "m_pObserverServices"_hash ) ).value_or( 0 );
			if ( !observer_services || ( observer_services >> 48 ) != 0 )
			{
				continue;
			}

			const auto observer_target_handle = memory::safe_read<std::uint32_t>( observer_services + SCHEMA( "CPlayer_ObserverServices", "m_hObserverTarget"_hash ) ).value_or( 0 );
			if ( !observer_target_handle )
			{
				continue;
			}

			const auto observer_target = systems::g_entities.lookup( observer_target_handle );
			if ( observer_target != view_pawn )
			{
				continue;
			}

			const auto name_ptr = memory::read<std::uintptr_t>( player.ptr + SCHEMA( "CCSPlayerController", "m_sSanitizedPlayerName"_hash ) );
			if ( !name_ptr )
			{
				continue;
			}

			auto name = memory::read_string( name_ptr, 127 );
			std::ranges::transform( name, name.begin( ), [ ]( unsigned char c ) { return std::tolower( c ); } );

			auto& e = entries[ count++ ];
			strncpy_s( e.name, name.c_str( ), sizeof( e.name ) - 1 );

			e.name[ sizeof( e.name ) - 1 ] = '\0';
			e.steam_id = memory::read<std::uintptr_t>( player.ptr + SCHEMA( "CBasePlayerController", "m_steamID"_hash ) );
		}

		const bool menu_open = rendering::g_menu.is_open( );

		if ( count <= 0 && !menu_open )
		{
			return;
		}

		static detail::avatar_cache avatars{};

		static auto icon_w_px = 0, icon_h_px = 0;
		static const auto eye_icon = xdraw::load_svg( std::span<const std::byte>( reinterpret_cast< const std::byte* >( detail::spectator_icon ), sizeof( detail::spectator_icon ) ), 1.0f, &icon_w_px, &icon_h_px );

		constexpr auto header_h{ 26.0f };
		constexpr auto row_h{ 22.0f };
		constexpr auto row_gap{ 3.0f };
		constexpr auto pad_y{ 5.0f };
		constexpr auto card_r{ 8.0f };
		constexpr auto inner_r{ 5.0f };
		constexpr auto icon_box_size{ 18.0f };

		const auto effective_count = ( count == 0 && menu_open ) ? 1 : count;
		const auto total_h = header_h + pad_y + ( static_cast< float >( effective_count ) * ( row_h + row_gap ) ) + 2.0f;

		// Dynamic card width based on spectator names
		auto max_w = 180.0f;
		for ( auto i = 0; i < count; ++i )
		{
			const auto [nw, nh] = xdraw::measure_text( entries[ i ].name );
			const auto has_avatar = avatars.get( entries[ i ].steam_id ) != nullptr;
			const auto item_w = nw + ( has_avatar ? 24.0f : 0.0f ) + 32.0f;
			if ( item_w > max_w )
				max_w = item_w;
		}
		const auto card_w = std::min( max_w, 260.0f );

		auto& cfg_x = settings::g_misc.m_widgets.spectators_x;
		auto& cfg_y = settings::g_misc.m_widgets.spectators_y;

		if ( cfg_x.value < 0.0f || cfg_y.value < 0.0f )
		{
			cfg_x = static_cast< float >( screen_w ) - card_w - 14.0f;
			cfg_y = ( static_cast< float >( screen_h ) - total_h ) * 0.5f;
		}

		static bool s_spec_dragging{ false };
		static float s_spec_drag_off_x{ 0.0f };
		static float s_spec_drag_off_y{ 0.0f };

		auto& input = xui::ctx( ).input;
		const auto cur_x = std::clamp( cfg_x.value, 4.0f, std::max( 0.0f, static_cast< float >( screen_w ) - card_w - 4.0f ) );
		const auto cur_y = std::clamp( cfg_y.value, 4.0f, std::max( 0.0f, static_cast< float >( screen_h ) - total_h - 4.0f ) );

		const xui::rect header_rect{ cur_x, cur_y, card_w, header_h };
		const auto header_hovered = menu_open && input.in_rect( header_rect );

		if ( menu_open )
		{
			if ( input.mouse_clicked && header_hovered )
			{
				s_spec_dragging = true;
				s_spec_drag_off_x = input.mouse_x - cur_x;
				s_spec_drag_off_y = input.mouse_y - cur_y;
			}

			if ( s_spec_dragging )
			{
				if ( !input.mouse_down )
				{
					s_spec_dragging = false;
				}
				else
				{
					auto new_x = input.mouse_x - s_spec_drag_off_x;
					auto new_y = input.mouse_y - s_spec_drag_off_y;

					const auto max_x = std::max( 0.0f, static_cast< float >( screen_w ) - card_w - 4.0f );
					const auto max_y = std::max( 0.0f, static_cast< float >( screen_h ) - total_h - 4.0f );

					new_x = std::clamp( new_x, 4.0f, max_x );
					new_y = std::clamp( new_y, 4.0f, max_y );

					cfg_x = new_x;
					cfg_y = new_y;
				}
			}
		}
		else
		{
			s_spec_dragging = false;
		}

		// Outer card
		draw_list.rect_filled_blurred( cur_x, cur_y, card_w, total_h, xdraw::corner_radius{ card_r } );
		draw_list.rect_filled( cur_x, cur_y, card_w, total_h, tokens::col_dark.alpha( 230 ), xdraw::corner_radius{ card_r } );

		const auto border_col = s_spec_dragging ? tokens::col_accent.alpha( 220 )
			: ( header_hovered ? tokens::col_accent.alpha( 160 )
			: tokens::col_elevated.alpha( 170 ) );
		draw_list.rect( cur_x, cur_y, card_w, total_h, border_col, xdraw::corner_radius{ card_r }, 1.0f );

		// Header accent icon badge
		const auto icon_box_x = cur_x + 5.0f;
		const auto icon_box_y = cur_y + ( header_h - icon_box_size ) * 0.5f;
		draw_list.rect_filled( icon_box_x, icon_box_y, icon_box_size, icon_box_size, tokens::col_accent, xdraw::corner_radius{ inner_r } );

		if ( eye_icon )
		{
			constexpr auto icon_draw{ 11.0f };
			const auto ix = std::floor( icon_box_x + ( icon_box_size - icon_draw ) * 0.5f );
			const auto iy = std::floor( icon_box_y + ( icon_box_size - icon_draw ) * 0.5f );
			draw_list.image( ix, iy, icon_draw, icon_draw, eye_icon.Get( ), tokens::col_dark );
		}

		// Title "spectators"
		const auto [header_tw, header_th] = xdraw::measure_text( "spectators" );
		draw_list.text( cur_x + 28.0f, cur_y + ( header_h - header_th ) * 0.5f + 0.5f, "spectators", tokens::col_text );

		// Right side count
		if ( count > 0 )
		{
			char count_buf[ 8 ];
			std::snprintf( count_buf, sizeof( count_buf ), "%d", count );
			const auto [cw, ch] = xdraw::measure_text( count_buf );
			const auto cbx = cur_x + card_w - 7.0f - ( cw + 10.0f );
			const auto cby = cur_y + ( header_h - 16.0f ) * 0.5f;
			draw_list.rect_filled( cbx, cby, cw + 10.0f, 16.0f, tokens::col_card.alpha( 200 ), xdraw::corner_radius{ 4.0f } );
			draw_list.rect( cbx, cby, cw + 10.0f, 16.0f, tokens::col_elevated.alpha( 180 ), xdraw::corner_radius{ 4.0f }, 1.0f );
			draw_list.text( cbx + 5.0f, cby + ( 16.0f - ch ) * 0.5f + 0.5f, count_buf, tokens::col_accent );
		}

		// Separator line
		draw_list.line( cur_x + 5.0f, cur_y + header_h, cur_x + card_w - 5.0f, cur_y + header_h, tokens::col_elevated.alpha( 120 ), 1.0f );

		// Rows
		const auto row_start_y = cur_y + header_h + pad_y;

		if ( count == 0 && menu_open )
		{
			draw_list.rect_filled( cur_x + 4.0f, row_start_y, card_w - 8.0f, row_h, tokens::col_card.alpha( 140 ), xdraw::corner_radius{ 4.0f } );
			const auto [pw, ph] = xdraw::measure_text( "no spectators" );
			draw_list.text( cur_x + 8.0f, row_start_y + ( row_h - ph ) * 0.5f + 0.5f, "no spectators", tokens::col_text_dim.alpha( 160 ) );
		}
		else
		{
			for ( auto i = 0; i < count; ++i )
			{
				const auto& e = entries[ i ];
				const auto row_y = row_start_y + static_cast< float >( i ) * ( row_h + row_gap );

				// Row background
				draw_list.rect_filled( cur_x + 4.0f, row_y, card_w - 8.0f, row_h, tokens::col_card.alpha( 140 ), xdraw::corner_radius{ 4.0f } );

				auto text_offset_x = cur_x + 8.0f;
				const auto avatar_tex = avatars.get( e.steam_id );
				if ( avatar_tex )
				{
					constexpr auto avatar_size{ 16.0f };
					const auto ay = row_y + ( row_h - avatar_size ) * 0.5f;
					draw_list.image( text_offset_x, ay, avatar_size, avatar_size, avatar_tex, xdraw::corner_radius{ 3.0f }, xdraw::color{ 255, 255, 255, 255 } );
					text_offset_x += avatar_size + 6.0f;
				}

				const auto [nw, nh] = xdraw::measure_text( e.name );
				draw_list.text( text_offset_x, row_y + ( row_h - nh ) * 0.5f + 0.5f, e.name, tokens::col_text );
			}
		}
	}

} // namespace features::esp::other
