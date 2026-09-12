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
			const auto pawn_ptr = memory::read<std::uintptr_t>( movement_services + 56 );
			if ( !pawn_ptr )
			{
				return false;
			}

			const auto collision = local_pawn + SCHEMA( "C_BaseModelEntity", "m_Collision"_hash );
			const auto mins = memory::read<math::vector3>( collision + SCHEMA( "CCollisionProperty", "m_vecMins"_hash ) );
			const auto maxs = memory::read<math::vector3>( collision + SCHEMA( "CCollisionProperty", "m_vecMaxs"_hash ) );

			auto trace_mask = memory::read<std::uintptr_t>( pawn_ptr + 0xd48 );
			if ( memory::read<std::uint32_t>( pawn_ptr + 0x3f8 ) & 0x10 )
			{
				trace_mask |= 0x20;
			}

			const auto filter = systems::g_tracing.make_player_movement_filter( local_pawn, trace_mask, 11 );
			const auto standable_convar = CONVAR( "sv_standable_normal" );
			const auto standable_normal = standable_convar ? standable_convar->get<float>( ) : 0.7f;

			const auto trace_start = prestate.networked_origin;
			auto trace_end = trace_start;
			trace_end.z -= distance;

			const auto result = systems::g_tracing.trace_player_bbox(
				trace_start,
				trace_end,
				{ mins, maxs },
				filter,
				movement_services );

			return result.fraction < 1.0f && result.normal.z >= standable_normal;
		}

	} // namespace

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

		const auto movement_services = memory::read<std::uintptr_t>(
			local.pawn + SCHEMA( "C_BasePlayerPawn", "m_pMovementServices"_hash ) );
		if ( !movement_services )
		{
			this->m_ticks_on_ground = 0;
			return;
		}

		const auto& prestate = systems::g_prediction.pre( );
		const auto has_ground_flag =
			( prestate.flags & cstypes::entity_flags::on_ground ) != 0;
		const auto falling_or_level = prestate.networked_velocity.z <= 0.0f;
		const auto on_ground = has_ground_flag ||
			( falling_or_level && check_ground_probe( local.pawn, movement_services, prestate ) );

		if ( on_ground )
		{
			++this->m_ticks_on_ground;

			// Emit a fresh press on every detected landing. The airborne command
			// below always emits the matching release transition.
			cmd->buttons.value |= jump;
			cmd->buttons.value_scroll |= jump;
			cmd->buttons.value_changed |= jump;
			return;
		}

		this->m_ticks_on_ground = 0;

		// A held physical key must become a released command while airborne;
		// otherwise the server keeps the previous jump state and the next landing
		// does not see a new press edge.
		cmd->buttons.value &= ~jump;
		cmd->buttons.value_scroll &= ~jump;
		cmd->buttons.value_changed |= jump;

		if ( const auto base = cmd->csgo_user_cmd.mutable_base( ) )
		{
			for ( auto i = 0; i < base->subtick_moves_size( ); ++i )
			{
				if ( const auto step = base->mutable_subtick_moves( i );
					step && step->button( ) == static_cast<std::uint64_t>( jump ) )
				{
					// Keep the button identity and convert any airborne jump event
					// into a release instead of leaving an invalid button-0 step.
					step->set_pressed( false );
				}
			}
		}
	}

} // namespace features::movement
