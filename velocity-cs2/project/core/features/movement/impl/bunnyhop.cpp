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

		const auto& prestate = systems::g_prediction.pre( );

		// Sole ground signal: entity flags from pre-prediction state.
		// The old check_ground_probe used movement_services+56 as a pawn
		// back-pointer; that offset drifts across builds and produced spurious
		// "on ground" reads that ate the landing frame before the real landing.
		const auto on_ground =
			( prestate.flags & cstypes::entity_flags::on_ground ) != 0;

		const auto base = cmd->csgo_user_cmd.mutable_base( );

		if ( on_ground )
		{
			++this->m_ticks_on_ground;

			// Mark jump as a fresh press in all button-state fields so the
			// server sees a new rising edge.
			cmd->buttons.value         |= jump;
			cmd->buttons.value_scroll  |= jump;
			cmd->buttons.value_changed |= jump;

			if ( base )
			{
				// CS2 jump processing is driven by per-subtick timestamps, not
				// button flags.  Without an explicit subtick entry the engine
				// chooses an arbitrary moment inside the tick that is usually
				// outside the narrow landing window.
				//
				// Strategy:
				//  1. If the player's physical input already created a jump
				//     subtick, pin its timestamp to 0.0 (very start of tick).
				//  2. Otherwise allocate a fresh entry via the project's own
				//     acquire_subtick_step allocator (same path used by input.cpp
				//     for analog deltas).  This properly handles arena memory and
				//     RepT capacity, so the entry survives serialisation.

				bool found = false;
				for ( auto i = 0; i < base->subtick_moves_size( ); ++i )
				{
					if ( const auto step = base->mutable_subtick_moves( i );
						 step && step->button( ) == static_cast<std::uint64_t>( jump ) )
					{
						step->set_pressed( true );
						step->set_when( 0.0f );
						found = true;
						break;
					}
				}

				if ( !found )
				{
					if ( const auto step = systems::g_input.acquire_subtick_step(
							base->mutable_subtick_moves( ) ) )
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

		// Airborne: clear jump so the server sees a clean press edge on the
		// next landing.  Convert any existing jump subtick into a release so
		// the subtick list stays consistent with the button state.
		cmd->buttons.value         &= ~jump;
		cmd->buttons.value_scroll  &= ~jump;
		cmd->buttons.value_changed |= jump;

		if ( base )
		{
			for ( auto i = 0; i < base->subtick_moves_size( ); ++i )
			{
				if ( const auto step = base->mutable_subtick_moves( i );
					 step && step->button( ) == static_cast<std::uint64_t>( jump ) )
				{
					step->set_pressed( false );
				}
			}
		}
	}

} // namespace features::movement
