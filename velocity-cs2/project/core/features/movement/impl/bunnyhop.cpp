#include <pch/pch.hpp>
#include <utilities/memory/memory.hpp>
#include <core/systems/systems.hpp>
#include <core/features/features.hpp>
#include <core/settings.hpp>

#include "../movement.hpp"
#include <protection/game_addresses.hpp>

namespace features::movement {

	void bhop::on_create_move( systems::input::usercmd* cmd )
	{
		constexpr auto jump = static_cast<std::uintptr_t>( cstypes::command_buttons::in_jump );

		if ( !settings::g_movement.bhop.value || !( cmd->buttons.value & jump ) )
		{
			this->m_ticks_on_ground = 0;
			return;
		}

		const auto auto_bhop_cvar = CONVAR( "sv_autobunnyhopping" );
		if ( auto_bhop_cvar && auto_bhop_cvar->get<bool>( ) )
		{
			this->m_ticks_on_ground = 0;
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.pawn )
		{
			this->m_ticks_on_ground = 0;
			return;
		}

		const auto move_type = memory::read<std::uint8_t>(
			local.pawn + SCHEMA( "C_BaseEntity", "m_nActualMoveType"_hash ) );
		if ( move_type == cstypes::move_type::ladder || move_type == cstypes::move_type::noclip )
		{
			this->m_ticks_on_ground = 0;
			return;
		}

		// Read ground flag directly from the entity flags field.
		// Previously we used prestate.flags from the prediction system, but
		// if that system mis-reads after an SDK refresh the flag is always 0
		// and bhop clears the jump button on EVERY tick — making it do nothing.
		const auto entity_flags = memory::read<std::uint32_t>(
			local.pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
		const auto on_ground = ( entity_flags & static_cast<std::uint32_t>( cstypes::entity_flags::on_ground ) ) != 0;

		const auto base = cmd->csgo_user_cmd.mutable_base( );

		if ( on_ground )
		{
			++this->m_ticks_on_ground;

			// Mark jump as newly pressed so the server sees a rising edge.
			// Do NOT touch value_scroll — it is for scroll-wheel input only.
			cmd->buttons.value         |= jump;
			cmd->buttons.value_changed |= jump;

			if ( base )
			{
				// The CS2 engine decides whether a jump succeeds by inspecting
				// the subtick timestamp, not just the button flags.  We need an
				// explicit jump subtick at when=0.0 (start of tick) so the engine
				// sees the press right at the landing moment.
				//
				// 1. If the engine already created a jump subtick from physical
				//    input, just pin its timestamp to 0.0.
				// 2. Otherwise inject a fresh one via acquire_subtick_step(),
				//    which is the project's own safe arena/capacity allocator.

				bool found = false;
				for ( auto i = 0; i < base->subtick_moves_size( ); ++i )
				{
					const auto step = base->mutable_subtick_moves( i );
					if ( step && step->button( ) == static_cast<std::uint64_t>( jump ) )
					{
						step->set_pressed( true );
						step->set_when( 0.0f );
						found = true;
						break;
					}
				}

				if ( !found )
				{
					const auto step = systems::g_input.acquire_subtick_step(
						base->mutable_subtick_moves( ) );
					if ( step )
					{
						step->set_button( static_cast<std::uint64_t>( jump ) );
						step->set_pressed( true );
						step->set_when( 0.0f );
					}
				}
			}

			return;
		}

		this->m_ticks_on_ground = 0;

		// Airborne — suppress jump so the server sees a clean rising edge the
		// moment we land next tick.  Also flip any lingering jump subtick to
		// released so the subtick list stays consistent with the button state.
		cmd->buttons.value         &= ~jump;
		cmd->buttons.value_changed |= jump;

		if ( base )
		{
			for ( auto i = 0; i < base->subtick_moves_size( ); ++i )
			{
				const auto step = base->mutable_subtick_moves( i );
				if ( step && step->button( ) == static_cast<std::uint64_t>( jump ) )
				{
					step->set_pressed( false );
				}
			}
		}
	}

} // namespace features::movement
