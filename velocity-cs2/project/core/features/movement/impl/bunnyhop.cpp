#include <pch/pch.hpp>
#include <utilities/memory/memory.hpp>
#include <core/systems/systems.hpp>
#include <core/features/features.hpp>
#include <core/settings.hpp>

#include "../movement.hpp"
#include <protection/game_addresses.hpp>

namespace features::movement {

	namespace {

		[[nodiscard]] bool check_ground_probe(
			std::uintptr_t local_pawn,
			std::uintptr_t movement_services,
			const systems::prediction::state& prestate,
			float distance = 2.0f )
		{
			const auto mins = memory::read<math::vector3>( local_pawn + SCHEMA( "C_BaseModelEntity", "m_Collision"_hash ) + SCHEMA( "CCollisionProperty", "m_vecMins"_hash ) );
			const auto maxs = memory::read<math::vector3>( local_pawn + SCHEMA( "C_BaseModelEntity", "m_Collision"_hash ) + SCHEMA( "CCollisionProperty", "m_vecMaxs"_hash ) );

			auto trace_mask{ 0ull };
			{
				const auto pawn_ptr = memory::read<std::uintptr_t>( movement_services + 56 );
				trace_mask = memory::read<std::uintptr_t>( pawn_ptr + 0xd48 );

				if ( !pawn_ptr || ( memory::read<std::uint32_t>( pawn_ptr + 0x3f8 ) & 0x10 ) )
				{
					trace_mask |= 0x20;
				}
			}

			const auto filter = systems::g_tracing.make_player_movement_filter( local_pawn, trace_mask, 11 );
			const auto standable_convar = CONVAR ("sv_standable_normal");
			const auto sv_standable_normal = standable_convar ? standable_convar->get<float>( ) : 0.7f;

			const math::vector3 trace_start = prestate.networked_origin;
			math::vector3 trace_end = trace_start;
			trace_end.z -= distance;

			const auto result = systems::g_tracing.trace_player_bbox( trace_start, trace_end, { mins, maxs }, filter, movement_services );
			return ( result.fraction < 1.0f && result.normal.z >= sv_standable_normal );
		}

	} // namespace

	void bhop::on_create_move( systems::input::usercmd* cmd )
	{
		if ( !settings::g_movement.bhop.value )
		{
			this->m_ticks_on_ground = 0;
			return;
		}

		const auto auto_bhop_cvar = CONVAR ("sv_autobunnyhopping");
		if ( auto_bhop_cvar && auto_bhop_cvar->get<bool>( ) )
		{
			this->m_ticks_on_ground = 0;
			return;
		}

		if ( !( cmd->buttons.value & cstypes::command_buttons::in_jump ) )
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

		const auto move_type = memory::read<std::uint8_t>( local.pawn + SCHEMA( "C_BaseEntity", "m_nActualMoveType"_hash ) );
		if ( move_type == cstypes::move_type::ladder || move_type == cstypes::move_type::noclip )
		{
			this->m_ticks_on_ground = 0;
			return;
		}

		const auto movement_services = memory::read<std::uintptr_t>( local.pawn + SCHEMA( "C_BasePlayerPawn", "m_pMovementServices"_hash ) );
		if ( !movement_services )
		{
			this->m_ticks_on_ground = 0;
			return;
		}

		const auto& prestate = systems::g_prediction.pre( );
		const auto velocity = prestate.networked_velocity;
		const bool has_ground_flag = ( prestate.flags & cstypes::entity_flags::on_ground ) != 0;

		// When ascending with vertical velocity > 50 u/s, player is guaranteed airborne (just jumped)
		const bool is_ascending = ( velocity.z > 50.0f );

		// In CS2, ground snap happens at 2.0 units downward when falling or level
		const bool on_ground = !is_ascending && ( has_ground_flag || ( velocity.z <= 0.0f && check_ground_probe( local.pawn, movement_services, prestate, 2.0f ) ) );

		if ( on_ground )
		{
			this->m_ticks_on_ground++;

			// On landing tick (tick 1 on ground): send fresh jump pulse on value and scroll!
			if ( this->m_ticks_on_ground == 1 )
			{
				cmd->buttons.value |= cstypes::command_buttons::in_jump;
				cmd->buttons.value_scroll |= cstypes::command_buttons::in_jump;
				cmd->buttons.value_changed |= cstypes::command_buttons::in_jump;
			}
			else
			{
				// Standing still / delayed jump: simulate rapid mousewheel scroll (alternate ticks)
				if ( this->m_ticks_on_ground % 2 == 1 )
				{
					cmd->buttons.value |= cstypes::command_buttons::in_jump;
					cmd->buttons.value_scroll |= cstypes::command_buttons::in_jump;
					cmd->buttons.value_changed |= cstypes::command_buttons::in_jump;
				}
				else
				{
					cmd->buttons.value &= ~cstypes::command_buttons::in_jump;
					cmd->buttons.value_scroll &= ~cstypes::command_buttons::in_jump;
					cmd->buttons.value_changed |= cstypes::command_buttons::in_jump;
				}
			}
			return;
		}

		// Player is in the air:
		this->m_ticks_on_ground = 0;

		// Strip jump button in the air so m_afButtonLast on the server is guaranteed to be 0!
		cmd->buttons.value &= ~cstypes::command_buttons::in_jump;
		cmd->buttons.value_scroll &= ~cstypes::command_buttons::in_jump;

		if ( const auto base = cmd->csgo_user_cmd.mutable_base( ) )
		{
			for ( auto i = 0; i < base->subtick_moves_size( ); ++i )
			{
				if ( const auto step = base->mutable_subtick_moves( i ) )
				{
					if ( step->button( ) == static_cast< std::uint64_t >( cstypes::command_buttons::in_jump ) )
					{
						step->set_button( 0 );
						step->set_pressed( false );
					}
				}
			}
		}
	}

} // namespace features::movement
