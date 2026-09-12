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

		// Use entity flags as the sole authoritative ground signal.
		// The old check_ground_probe helper read movement_services+56 as a
		// pawn back-pointer; that offset drifts across builds and produced
		// spurious "on ground" decisions that ate landing frames.
		const auto on_ground =
			( prestate.flags & cstypes::entity_flags::on_ground ) != 0;

		const auto base = cmd->csgo_user_cmd.mutable_base( );

		if ( on_ground )
		{
			++this->m_ticks_on_ground;

			// Mark the jump button as freshly pressed in all button-state
			// fields so the server sees a new edge.
			cmd->buttons.value        |= jump;
			cmd->buttons.value_scroll |= jump;
			cmd->buttons.value_changed |= jump;

			// CS2 uses per-subtick timestamps for jump processing.
			// Without an explicit subtick step the engine picks an arbitrary
			// moment inside the tick, which often misses the landing window.
			// Inject a press at when=0.0 (very first subtick) and, if the
			// player also sent a step from physical input, replace it.
			if ( base )
			{
				bool replaced = false;
				for ( auto i = 0; i < base->subtick_moves_size( ); ++i )
				{
					const auto step = base->mutable_subtick_moves( i );
					if ( step && step->button( ) == static_cast<std::uint64_t>( jump ) )
					{
						// Take over the existing slot: force press at tick start.
						step->set_pressed( true );
						step->set_when( 0.0f );
						replaced = true;
						break;
					}
				}

				if ( !replaced )
				{
					// No existing jump subtick — synthesise one.
					auto* step = base->add_subtick_moves( );
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

		// Airborne: release the jump button so the server sees a clean
		// press edge on the next landing.  Convert any airborne subtick
		// jump steps into releases.
		cmd->buttons.value        &= ~jump;
		cmd->buttons.value_scroll &= ~jump;
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
