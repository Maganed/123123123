#include <pch/pch.hpp>
#include <utilities/memory/memory.hpp>
#include <core/systems/systems.hpp>
#include <core/features/features.hpp>
#include <core/settings.hpp>
#include <protection/game_addresses.hpp>

#include "../movement.hpp"

namespace features::movement {

	void airstrafe::on_create_move( systems::input::usercmd* cmd )
	{
		const auto base = cmd->csgo_user_cmd.mutable_base( );
		if ( !base || !base->viewangles( ) )
		{
			return;
		}

		constexpr auto forward = static_cast<std::uintptr_t>( cstypes::command_buttons::in_forward );
		constexpr auto back = static_cast<std::uintptr_t>( cstypes::command_buttons::in_back );
		constexpr auto moveleft = static_cast<std::uintptr_t>( cstypes::command_buttons::in_moveleft );
		constexpr auto moveright = static_cast<std::uintptr_t>( cstypes::command_buttons::in_moveright );
		constexpr auto sprint = static_cast<std::uintptr_t>( cstypes::command_buttons::in_sprint );
		constexpr auto movement_mask = forward | back | moveleft | moveright;

		const auto current_buttons = cmd->buttons.value;
		const auto current_yaw =
			( this->m_angles.y != 0.0f || this->m_angles.x != 0.0f )
				? this->m_angles.y
				: base->viewangles( )->y( );

		const auto& prestate = systems::g_prediction.pre( );
		const auto in_air =
			( prestate.flags & cstypes::entity_flags::on_ground ) == 0;

		// Reset all persistent steering state on the ground even while the
		// feature is disabled. Keeping button history from the previous jump was
		// responsible for stale left/right decisions on the next takeoff.
		if ( !in_air )
		{
			this->m_air_ticks = 0;
			this->m_old_yaw = current_yaw;
			this->m_last_buttons = current_buttons;
			this->m_last_pressed = current_buttons & movement_mask;
			this->m_side_switch = false;
			return;
		}

		const auto wants_stop = features::combat::g_rage.should_stop( ) ||
			features::misc::g_projectile_trajectory.should_stop( );
		if ( ( !settings::g_movement.airstrafe.value && !wants_stop ) ||
			features::combat::g_rage.is_firing_this_tick( ) )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.pawn )
		{
			return;
		}

		const auto move_type = memory::read<std::uint8_t>(
			local.pawn + SCHEMA( "C_BaseEntity", "m_nActualMoveType"_hash ) );
		if ( move_type == cstypes::move_type::ladder || move_type == cstypes::move_type::noclip )
		{
			return;
		}

		if ( wants_stop || ( settings::g_movement.airstrafe.value && ( current_buttons & sprint ) ) )
		{
			this->rotate_to_stop( base, prestate.networked_velocity );
			return;
		}

		++this->m_air_ticks;

		this->check_button( current_buttons, moveleft );
		this->check_button( current_buttons, moveright );
		this->check_button( current_buttons, forward );
		this->check_button( current_buttons, back );
		this->m_last_buttons = current_buttons;

		auto yaw_offset = 0.0f;
		if ( settings::g_movement.airstrafe_fully_directional.value )
		{
			if ( this->m_last_pressed & moveleft )
			{
				yaw_offset += 90.0f;
			}
			if ( this->m_last_pressed & moveright )
			{
				yaw_offset -= 90.0f;
			}
			if ( this->m_last_pressed & forward )
			{
				yaw_offset *= 0.5f;
			}
			else if ( this->m_last_pressed & back )
			{
				yaw_offset = -yaw_offset * 0.5f + 180.0f;
			}
		}

		const auto& velocity = prestate.networked_velocity;
		const auto speed_2d = velocity.length_2d( );
		if ( speed_2d < 15.0f )
		{
			base->set_forwardmove( 1.0f );
			base->set_leftmove( 0.0f );
			return;
		}

		auto mouse_yaw_delta = current_yaw - this->m_old_yaw;
		math::helpers::normalize_angle( mouse_yaw_delta );
		this->m_old_yaw = current_yaw;

		auto target_yaw = current_yaw + yaw_offset;
		math::helpers::normalize_angle( target_yaw );

		const auto velocity_yaw = std::atan2f( velocity.y, velocity.x ) *
			( 180.0f / std::numbers::pi_v<float> );
		auto target_delta = target_yaw - velocity_yaw;
		math::helpers::normalize_angle( target_delta );

		const auto wishspeed_cvar = CONVAR( "sv_air_max_wishspeed" );
		const auto max_wishspeed =
			wishspeed_cvar ? wishspeed_cvar->get<float>( ) : 30.0f;
		const auto cos_theta = std::clamp(
			max_wishspeed / std::max( speed_2d, 1.0f ), 0.0f, 1.0f );
		const auto ideal_angle = std::acosf( cos_theta ) *
			( 180.0f / std::numbers::pi_v<float> );

		// Use hysteresis: tiny yaw noise must not switch the strafe side every
		// command. A real mouse turn wins; otherwise steer toward the requested
		// direction and retain the previous side inside the dead zone.
		if ( std::fabsf( mouse_yaw_delta ) > 0.15f )
		{
			this->m_side_switch = mouse_yaw_delta > 0.0f;
		}
		else if ( std::fabsf( target_delta ) > 2.0f )
		{
			this->m_side_switch = target_delta > 0.0f;
		}

		auto wish_yaw = velocity_yaw +
			( this->m_side_switch ? ideal_angle : -ideal_angle );
		math::helpers::normalize_angle( wish_yaw );

		auto angle_diff = wish_yaw - current_yaw;
		math::helpers::normalize_angle( angle_diff );
		const auto rotation = angle_diff *
			( std::numbers::pi_v<float> / 180.0f );

		auto forward_move = std::cosf( rotation );
		auto side_move = std::sinf( rotation );
		const auto max_component =
			std::max( std::fabsf( forward_move ), std::fabsf( side_move ) );
		if ( max_component > 0.0001f )
		{
			forward_move /= max_component;
			side_move /= max_component;
		}

		base->set_forwardmove( std::clamp( forward_move, -1.0f, 1.0f ) );
		base->set_leftmove( std::clamp( side_move, -1.0f, 1.0f ) );

		const auto old_movement_buttons = cmd->buttons.value & movement_mask;
		cmd->buttons.value &= ~movement_mask;
		if ( base->forwardmove( ) > 0.01f )
		{
			cmd->buttons.value |= forward;
		}
		else if ( base->forwardmove( ) < -0.01f )
		{
			cmd->buttons.value |= back;
		}
		if ( base->leftmove( ) > 0.01f )
		{
			cmd->buttons.value |= moveleft;
		}
		else if ( base->leftmove( ) < -0.01f )
		{
			cmd->buttons.value |= moveright;
		}

		// Publish movement transitions along with the analog values. Previously
		// only buttonstate1 changed, so stale direction bits could survive on the
		// receiving side and fight the new analog movement.
		const auto new_movement_buttons = cmd->buttons.value & movement_mask;
		cmd->buttons.value_changed |= old_movement_buttons ^ new_movement_buttons;
	}

	void airstrafe::store_angles( )
	{
		this->m_angles = systems::g_input.get_view_angles( );
	}

	void airstrafe::check_button( std::uintptr_t current_buttons, std::uintptr_t button )
	{
		constexpr auto moveleft = static_cast<std::uintptr_t>( cstypes::command_buttons::in_moveleft );
		constexpr auto moveright = static_cast<std::uintptr_t>( cstypes::command_buttons::in_moveright );
		constexpr auto forward = static_cast<std::uintptr_t>( cstypes::command_buttons::in_forward );
		constexpr auto back = static_cast<std::uintptr_t>( cstypes::command_buttons::in_back );

		if ( current_buttons & button )
		{
			if ( button == moveleft ) this->m_last_pressed &= ~moveright;
			else if ( button == moveright ) this->m_last_pressed &= ~moveleft;
			else if ( button == forward ) this->m_last_pressed &= ~back;
			else if ( button == back ) this->m_last_pressed &= ~forward;
			this->m_last_pressed |= button;
		}
		else
		{
			this->m_last_pressed &= ~button;
		}
	}

	void airstrafe::rotate_movement(
		proto::base_usercmd_pb* base,
		float target_yaw,
		float view_yaw ) const
	{
		const auto forward_move = base->forwardmove( );
		const auto side_move = base->leftmove( );
		auto angle_diff = target_yaw - view_yaw;
		math::helpers::normalize_angle( angle_diff );
		const auto rotation = angle_diff *
			( std::numbers::pi_v<float> / 180.0f );
		const auto cos_rotation = std::cosf( rotation );
		const auto sin_rotation = std::sinf( rotation );
		base->set_forwardmove( std::clamp(
			cos_rotation * forward_move - sin_rotation * side_move, -1.0f, 1.0f ) );
		base->set_leftmove( std::clamp(
			sin_rotation * forward_move + cos_rotation * side_move, -1.0f, 1.0f ) );
	}

	void airstrafe::rotate_to_stop(
		proto::base_usercmd_pb* base,
		const math::vector3& velocity ) const
	{
		const auto speed = velocity.length_2d( );
		if ( speed < 0.1f )
		{
			base->set_forwardmove( 0.0f );
			base->set_leftmove( 0.0f );
			return;
		}

		const auto wish_yaw = std::atan2f( velocity.y, velocity.x ) *
			( 180.0f / std::numbers::pi_v<float> ) + 180.0f;
		const auto& ctx = features::combat::g_shared.ctx( );
		const auto max_speed = ( ctx.valid && ctx.weapon_vdata )
			? memory::read<float>( ctx.weapon_vdata +
				SCHEMA( "CCSWeaponBaseVData", "m_flMaxSpeed"_hash ) )
			: 250.0f;
		const auto speed_ratio = std::clamp(
			speed / std::max( max_speed, 1.0f ), 0.0f, 1.0f );
		auto angle_diff = wish_yaw - base->viewangles( )->y( );
		math::helpers::normalize_angle( angle_diff );
		const auto rotation = angle_diff *
			( std::numbers::pi_v<float> / 180.0f );
		base->set_forwardmove( std::clamp(
			std::cosf( rotation ) * speed_ratio, -1.0f, 1.0f ) );
		base->set_leftmove( std::clamp(
			std::sinf( rotation ) * speed_ratio, -1.0f, 1.0f ) );
	}

} // namespace features::movement
