#include <pch/pch.hpp>
#include <utilities/memory/memory.hpp>
#include <utilities/logging/logging.hpp>
#include <core/systems/systems.hpp>
#include <core/features/features.hpp>
#include <core/settings.hpp>
#include <protection/game_addresses.hpp>

#include "../movement.hpp"

namespace features::movement {

	void airstrafe::on_create_move( systems::input::usercmd* cmd )
	{
		const auto base = cmd->csgo_user_cmd.mutable_base( );
		if ( !base )
		{
			return;
		}

		const auto current_buttons = cmd->buttons.value;
		const bool shift_held = ( current_buttons & static_cast< std::uintptr_t >( cstypes::command_buttons::in_sprint ) ) != 0;

		const auto& prestate = systems::g_prediction.pre( );
		const bool in_air = !( prestate.flags & cstypes::entity_flags::on_ground );

		if ( shift_held && in_air )
		{
			this->rotate_to_stop( base, prestate.networked_velocity );
			return;
		}

		const auto wants_stop = features::combat::g_rage.should_stop( ) || features::misc::g_projectile_trajectory.should_stop( );

		if ( ( !settings::g_movement.airstrafe.value && !wants_stop ) || features::combat::g_rage.is_firing_this_tick( ) )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.pawn )
		{
			return;
		}

		const auto move_type = memory::read<std::uint8_t>( local.pawn + SCHEMA( "C_BaseEntity", "m_nActualMoveType"_hash ) );
		if ( move_type == cstypes::move_type::ladder || move_type == cstypes::move_type::noclip )
		{
			return;
		}

		if ( !in_air )
		{
			this->m_air_ticks = 0;
			this->m_old_yaw = ( this->m_angles.y != 0.0f || this->m_angles.x != 0.0f ) ? this->m_angles.y : base->viewangles( )->y( );
			return;
		}

		if ( current_buttons & static_cast< std::uintptr_t >( cstypes::command_buttons::in_sprint ) )
		{
			return;
		}

		this->m_air_ticks++;
		const bool is_takeoff = ( this->m_air_ticks <= 2 );

		if ( !wants_stop )
		{
			this->check_button( current_buttons, cstypes::command_buttons::in_moveleft );
			this->check_button( current_buttons, cstypes::command_buttons::in_moveright );
			this->check_button( current_buttons, cstypes::command_buttons::in_forward );
			this->check_button( current_buttons, cstypes::command_buttons::in_back );
			this->m_last_buttons = current_buttons;
		}

		if ( wants_stop )
		{
			this->rotate_to_stop( base, prestate.networked_velocity );
			return;
		}

		const auto& velocity = prestate.networked_velocity;
		const auto speed_2d = velocity.length_2d( );

		const auto current_yaw = ( this->m_angles.y != 0.0f || this->m_angles.x != 0.0f ) ? this->m_angles.y : base->viewangles( )->y( );
		auto mouse_yaw_delta = current_yaw - this->m_old_yaw;
		math::helpers::normalize_angle( mouse_yaw_delta );
		this->m_old_yaw = current_yaw;

		const bool holding_left = ( current_buttons & static_cast< std::uintptr_t >( cstypes::command_buttons::in_moveleft ) ) != 0;
		const bool holding_right = ( current_buttons & static_cast< std::uintptr_t >( cstypes::command_buttons::in_moveright ) ) != 0;

		auto yaw_offset = 0.0f;
		if ( settings::g_movement.airstrafe_fully_directional.value )
		{
			if ( this->m_last_pressed & cstypes::command_buttons::in_moveleft )
			{
				yaw_offset += 90.0f;
			}

			if ( this->m_last_pressed & cstypes::command_buttons::in_moveright )
			{
				yaw_offset -= 90.0f;
			}

			if ( this->m_last_pressed & cstypes::command_buttons::in_forward )
			{
				yaw_offset *= 0.5f;
			}
			else if ( this->m_last_pressed & cstypes::command_buttons::in_back )
			{
				yaw_offset = -yaw_offset * 0.5f + 180.0f;
			}
		}

		if ( speed_2d < 15.0f )
		{
			base->set_forwardmove( 1.0f );
			base->set_leftmove( 0.0f );
			return;
		}

		// Optimal wishdir lead angle formula: cos(theta) = sv_air_max_wishspeed / (2 * speed_2d)
		const auto cvar = CONVAR( "sv_air_max_wishspeed" );
		const auto sv_air_max_wishspeed = cvar ? cvar->get<float>( ) : 30.0f;
		const auto cos_theta = std::clamp( sv_air_max_wishspeed / ( 2.0f * speed_2d ), 0.0f, 1.0f );
		const auto ideal_angle = std::acosf( cos_theta ) * ( 180.0f / std::numbers::pi_v<float> );

		auto target_yaw = current_yaw + yaw_offset;
		math::helpers::normalize_angle( target_yaw );

		const auto velocity_yaw = std::atan2f( velocity.y, velocity.x ) * ( 180.0f / std::numbers::pi_v<float> );
		auto delta_yaw = target_yaw - velocity_yaw;
		math::helpers::normalize_angle( delta_yaw );

		const bool has_mouse_turn = ( std::fabsf( mouse_yaw_delta ) > 0.05f );

		float wish_yaw = target_yaw;

		if ( is_takeoff && !has_mouse_turn && yaw_offset == 0.0f )
		{
			// Clean straight launch off ground: no lateral pull
			wish_yaw = target_yaw;
			this->m_side_switch = false;
		}
		else if ( has_mouse_turn )
		{
			// User is steering with the mouse -> apply optimal lead angle in direction of mouse turn!
			if ( mouse_yaw_delta > 0.0f )
			{
				wish_yaw = velocity_yaw + ideal_angle;
				this->m_side_switch = true;
			}
			else
			{
				wish_yaw = velocity_yaw - ideal_angle;
				this->m_side_switch = false;
			}
		}
		else if ( settings::g_movement.airstrafe_fully_directional.value || yaw_offset != 0.0f )
		{
			// Steer towards target_yaw (crosshair + directional keys)
			if ( std::fabsf( delta_yaw ) > 1.5f )
			{
				if ( delta_yaw > 0.0f )
				{
					wish_yaw = velocity_yaw + ideal_angle;
					this->m_side_switch = true;
				}
				else
				{
					wish_yaw = velocity_yaw - ideal_angle;
					this->m_side_switch = false;
				}
			}
			else
			{
				// Aligned within 1.5 degree deadzone -> maintain target_yaw smoothly without jitter!
				wish_yaw = target_yaw;
				this->m_side_switch = false;
			}
		}
		else if ( holding_left && !holding_right )
		{
			// Holding A without fully directional -> steer left
			wish_yaw = velocity_yaw + ideal_angle;
			this->m_side_switch = true;
		}
		else if ( holding_right && !holding_left )
		{
			// Holding D without fully directional -> steer right
			wish_yaw = velocity_yaw - ideal_angle;
			this->m_side_switch = false;
		}
		else
		{
			// Straight flight: maintain full forward momentum towards target_yaw without lateral jitter!
			wish_yaw = target_yaw;
			this->m_side_switch = false;
		}

		math::helpers::normalize_angle( wish_yaw );

		auto angle_diff = wish_yaw - current_yaw;
		math::helpers::normalize_angle( angle_diff );
		const auto rot_rad = angle_diff * ( std::numbers::pi_v<float> / 180.0f );

		auto fwd = std::cosf( rot_rad );
		auto side = std::sinf( rot_rad );

		const auto max_comp = std::max( std::fabsf( fwd ), std::fabsf( side ) );
		if ( max_comp > 0.0001f )
		{
			fwd /= max_comp;
			side /= max_comp;
		}

		base->set_forwardmove( std::clamp( fwd, -1.0f, 1.0f ) );
		base->set_leftmove( std::clamp( side, -1.0f, 1.0f ) );

		cmd->buttons.value &= ~static_cast< std::uintptr_t >(
			cstypes::command_buttons::in_forward |
			cstypes::command_buttons::in_back |
			cstypes::command_buttons::in_moveleft |
			cstypes::command_buttons::in_moveright
		);

		if ( base->forwardmove( ) > 0.01f )
		{
			cmd->buttons.value |= cstypes::command_buttons::in_forward;
		}
		else if ( base->forwardmove( ) < -0.01f )
		{
			cmd->buttons.value |= cstypes::command_buttons::in_back;
		}

		// In CS2: positive leftmove corresponds to IN_MOVELEFT, negative to IN_MOVERIGHT
		if ( base->leftmove( ) > 0.01f )
		{
			cmd->buttons.value |= cstypes::command_buttons::in_moveleft;
		}
		else if ( base->leftmove( ) < -0.01f )
		{
			cmd->buttons.value |= cstypes::command_buttons::in_moveright;
		}
	}

	void airstrafe::store_angles( )
	{
		this->m_angles = systems::g_input.get_view_angles( );
	}

	void airstrafe::check_button( std::uintptr_t current_buttons, std::uintptr_t button )
	{
		constexpr auto moveleft = static_cast< std::uintptr_t >( cstypes::command_buttons::in_moveleft );
		constexpr auto moveright = static_cast< std::uintptr_t >( cstypes::command_buttons::in_moveright );
		constexpr auto forward = static_cast< std::uintptr_t >( cstypes::command_buttons::in_forward );
		constexpr auto back = static_cast< std::uintptr_t >( cstypes::command_buttons::in_back );

		if ( current_buttons & button && ( !( this->m_last_buttons & button ) || ( button & moveleft && !( this->m_last_pressed & moveright ) ) || ( button & moveright && !( this->m_last_pressed & moveleft ) ) || ( button & forward && !( this->m_last_pressed & back ) ) || ( button & back && !( this->m_last_pressed & forward ) ) ) )
		{
			if ( button & moveleft )
			{
				this->m_last_pressed &= ~moveright;
			}
			else if ( button & moveright )
			{
				this->m_last_pressed &= ~moveleft;
			}
			else if ( button & forward )
			{
				this->m_last_pressed &= ~back;
			}
			else if ( button & back )
			{
				this->m_last_pressed &= ~forward;
			}

			this->m_last_pressed |= button;
		}
		else if ( !( current_buttons & button ) )
		{
			this->m_last_pressed &= ~button;
		}
	}

	void airstrafe::rotate_movement( proto::base_usercmd_pb* base, float target_yaw, float view_yaw ) const
	{
		const auto forward_move = base->forwardmove( );
		const auto side_move = base->leftmove( );

		auto angle_diff = target_yaw - view_yaw;
		math::helpers::normalize_angle( angle_diff );
		const auto rotation = angle_diff * ( std::numbers::pi_v<float> / 180.0f );
		const auto cos_rot = std::cosf( rotation );
		const auto sin_rot = std::sinf( rotation );

		const auto corrected_forward = cos_rot * forward_move - sin_rot * side_move;
		const auto corrected_side = sin_rot * forward_move + cos_rot * side_move;

		base->set_forwardmove( std::clamp( corrected_forward, -1.0f, 1.0f ) );
		base->set_leftmove( std::clamp( corrected_side, -1.0f, 1.0f ) );
	}

	void airstrafe::rotate_to_stop( proto::base_usercmd_pb* base, const math::vector3& velocity ) const
	{
		const auto speed = velocity.length_2d( );
		if ( speed < 0.1f )
		{
			base->set_forwardmove( 0.0f );
			base->set_leftmove( 0.0f );
			return;
		}

		const auto wish_yaw = std::atan2f( velocity.y, velocity.x ) * ( 180.0f / std::numbers::pi_v<float> ) + 180.0f;

		const auto& ctx = features::combat::g_shared.ctx( );
		const auto max_speed = ( ctx.valid && ctx.weapon_vdata ) ? memory::read<float>( ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flMaxSpeed"_hash ) ) : 250.0f;

		const auto speed_ratio = std::clamp( speed / max_speed, 0.0f, 1.0f );

		auto angle_diff = wish_yaw - base->viewangles( )->y( );
		math::helpers::normalize_angle( angle_diff );
		const auto rotation = angle_diff * ( std::numbers::pi_v<float> / 180.0f );

		base->set_forwardmove( std::clamp( std::cosf( rotation ) * speed_ratio, -1.0f, 1.0f ) );
		base->set_leftmove( std::clamp( std::sinf( rotation ) * speed_ratio, -1.0f, 1.0f ) );
	}

} // namespace features::movement
