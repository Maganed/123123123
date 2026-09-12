#include <pch/pch.hpp>
#include <utilities/memory/memory.hpp>
#include <utilities/addresses/addresses.hpp>
#include <utilities/logging/logging.hpp>
#include <core/systems/systems.hpp>
#include <core/features/features.hpp>
#include <protection/game_addresses.hpp>

namespace features::combat {

	void shared::lagcomp::predict_movement( extrapolation_data& data, std::uintptr_t skip_entity ) const
	{
		if ( !addresses::globals::game_trace_manager )
		{
			return;
		}

		const auto sv_gravity = CONVAR( "sv_gravity" ) ? CONVAR( "sv_gravity" )->get<float>( ) : 800.0f;
		const auto sv_stopspeed = CONVAR( "sv_stopspeed" ) ? CONVAR( "sv_stopspeed" )->get<float>( ) : 100.0f;
		const auto sv_friction = CONVAR( "sv_friction" ) ? CONVAR( "sv_friction" )->get<float>( ) : 5.2f;

		const bool was_on_ground = ( data.flags & cstypes::entity_flags::on_ground ) != 0;

		// 1. Vertical acceleration (gravity)
		if ( was_on_ground )
		{
			data.velocity.z = 0.0f;
		}
		else
		{
			data.velocity.z -= sv_gravity * cstypes::tick_interval;
			data.velocity.z = std::clamp( data.velocity.z, -3500.0f, 3500.0f );
		}

		// 2. Ground friction when over max running speed or decelerating
		if ( was_on_ground )
		{
			const auto speed_2d = data.velocity.length_2d( );
			if ( speed_2d > 250.0f )
			{
				const auto control = std::max( speed_2d, sv_stopspeed );
				const auto drop = sv_friction * control * cstypes::tick_interval;
				const auto new_speed = std::max( speed_2d - drop, 250.0f );
				data.velocity.x *= ( new_speed / speed_2d );
				data.velocity.y *= ( new_speed / speed_2d );
			}
		}

		constexpr float k_step_size = 18.0f;
		const auto move_delta = data.velocity * cstypes::tick_interval;
		const auto move_end = data.origin + move_delta;

		// 3. Direct forward hull trace
		auto trace_result = systems::g_tracing.trace_hull(
			data.origin, move_end,
			data.obb_mins, data.obb_maxs,
			skip_entity, 0x1c3003, 4
		);

		bool stepped = false;

		// 4. Step-move (climbing stairs, curbs, ramps) if obstructed on ground
		if ( trace_result.fraction < 1.0f && was_on_ground && trace_result.normal.z < 0.7f )
		{
			const auto step_up_end = data.origin + math::vector3{ 0.0f, 0.0f, k_step_size };
			const auto up_trace = systems::g_tracing.trace_hull(
				data.origin, step_up_end,
				data.obb_mins, data.obb_maxs,
				skip_entity, 0x1c3003, 4
			);

			const auto actual_step_up = up_trace.end_pos.z - data.origin.z;
			if ( actual_step_up > 0.5f )
			{
				const auto step_fwd_end = up_trace.end_pos + move_delta;
				const auto fwd_trace = systems::g_tracing.trace_hull(
					up_trace.end_pos, step_fwd_end,
					data.obb_mins, data.obb_maxs,
					skip_entity, 0x1c3003, 4
				);

				const auto step_down_end = fwd_trace.end_pos - math::vector3{ 0.0f, 0.0f, actual_step_up + 2.0f };
				const auto down_trace = systems::g_tracing.trace_hull(
					fwd_trace.end_pos, step_down_end,
					data.obb_mins, data.obb_maxs,
					skip_entity, 0x1c3003, 4
				);

				if ( down_trace.fraction < 1.0f && down_trace.normal.z >= 0.7f )
				{
					const auto flat_dist = ( trace_result.end_pos - data.origin ).length_2d( );
					const auto step_dist = ( down_trace.end_pos - data.origin ).length_2d( );

					if ( step_dist > flat_dist )
					{
						data.origin = down_trace.end_pos;
						data.flags |= cstypes::entity_flags::on_ground;
						data.velocity.z = 0.0f;
						stepped = true;
					}
				}
			}
		}

		// 5. If not stepped and hit obstacle, slide along surface
		if ( !stepped )
		{
			if ( trace_result.fraction != 1.0f )
			{
				for ( auto i = 0; i < 2; ++i )
				{
					const auto dot = data.velocity.dot( trace_result.normal );
					data.velocity -= trace_result.normal * dot;

					const auto adjust = data.velocity.dot( trace_result.normal );
					if ( adjust < 0.0f )
					{
						data.velocity -= trace_result.normal * adjust;
					}

					const auto remaining_fraction = 1.0f - trace_result.fraction;
					const auto clip_end = trace_result.end_pos + data.velocity * ( cstypes::tick_interval * remaining_fraction );

					trace_result = systems::g_tracing.trace_hull(
						trace_result.end_pos, clip_end,
						data.obb_mins, data.obb_maxs,
						skip_entity, 0x1c3003, 4
					);

					if ( trace_result.fraction == 1.0f )
					{
						break;
					}
				}
			}

			data.origin = trace_result.end_pos;
		}

		// 6. Ground verification and downward snapping for stairs/slopes
		const auto ground_end = math::vector3{ data.origin.x, data.origin.y, data.origin.z - ( was_on_ground ? k_step_size : 2.0f ) };
		const auto ground_trace = systems::g_tracing.trace_hull(
			data.origin, ground_end,
			data.obb_mins, data.obb_maxs,
			skip_entity, 0x1c3003, 4
		);

		data.flags &= ~cstypes::entity_flags::on_ground;

		if ( ground_trace.fraction < 1.0f && ground_trace.normal.z >= 0.7f )
		{
			data.flags |= cstypes::entity_flags::on_ground;
			data.origin = ground_trace.end_pos;
			data.velocity.z = 0.0f;
		}
	}

	std::optional<shared::lagcomp::record> shared::lagcomp::extrapolate( std::uintptr_t pawn )
	{
		if ( !settings::g_combat.m_lagcomp.extrapolation.value )
		{
			return std::nullopt;
		}

		std::shared_lock records_lock( this->m_records_mtx );

		auto it = this->m_records.find( pawn );
		if ( it == this->m_records.end( ) || it->second.empty( ) )
		{
			return std::nullopt;
		}

		const auto& latest = it->second.front( );
		if ( !latest.is_valid( ) )
		{
			return std::nullopt;
		}

		const auto net_client = addresses::globals::network_client_service;
		if ( !net_client )
		{
			return std::nullopt;
		}

		const auto tick_state = memory::call_vfunc<std::uintptr_t>( net_client, 23 );
		if ( !tick_state )
		{
			return std::nullopt;
		}

		const auto server_tick = memory::read<int>( tick_state + 892 );
		const auto delta_ticks = server_tick - latest.tick;

		if ( delta_ticks <= 0 )
		{
			return std::nullopt;
		}

		const auto max_extrap = std::clamp( settings::g_combat.m_lagcomp.max_extrapolate_ticks.value, 1, 64 );
		const auto ticks_to_extrapolate = std::min( delta_ticks, max_extrap );

		const auto velocity = memory::read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecVelocity"_hash ) );
		const auto speed = velocity.length_2d( );

		if ( speed < 0.1f )
		{
			logging::console::print( xs( "[extrap] {:x} | skip: player stationary (speed {:.2f})\n" ), pawn, speed );
			return std::nullopt;
		}

		float direction = 0.0f;
		if ( velocity.x != 0.0f || velocity.y != 0.0f )
		{
			direction = std::atan2f( velocity.y, velocity.x ) * ( 180.0f / 3.14159265f );
		}

		float direction_change = 0.0f;

		if ( it->second.size( ) > 1 )
		{
			const auto& prev = it->second[ 1 ];
			if ( prev.valid )
			{
				const auto dt = latest.simulation_time - prev.simulation_time;
				if ( dt > 0.0f )
				{
					const auto origin_delta = latest.origin - prev.origin;
					float prev_dir = 0.0f;

					if ( origin_delta.x != 0.0f || origin_delta.y != 0.0f )
					{
						prev_dir = std::atan2f( origin_delta.y, origin_delta.x ) * ( 180.0f / 3.14159265f );
					}

				auto angle_diff = direction - prev_dir;
				while ( angle_diff > 180.0f ) angle_diff -= 360.0f;
				while ( angle_diff < -180.0f ) angle_diff += 360.0f;

				if ( std::fabsf( angle_diff ) > 35.0f )
				{
					logging::console::print( xs( "[extrap] {:x} | skip: direction change too large ({:.1f} deg)\n" ), pawn, angle_diff );
					return std::nullopt;
				}

				direction_change = ( angle_diff / dt ) * cstypes::tick_interval;
				}
			}
		}

		if ( std::fabsf( direction_change ) > 6.0f )
		{
			direction_change = 0.0f;
		}

		const auto game_scene_node = memory::read<std::uintptr_t>( pawn + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
		if ( !game_scene_node )
		{
			return std::nullopt;
		}

		const auto collision = memory::read<std::uintptr_t>( pawn + SCHEMA( "C_BaseEntity", "m_pCollision"_hash ) );
		math::vector3 obb_mins{}, obb_maxs{};

		if ( collision )
		{
			obb_mins = memory::read<math::vector3>( collision + SCHEMA( "CCollisionProperty", "m_vecMins"_hash ) );
			obb_maxs = memory::read<math::vector3>( collision + SCHEMA( "CCollisionProperty", "m_vecMaxs"_hash ) );
		}
		else
		{
			obb_mins = { -16.0f, -16.0f, 0.0f };
			obb_maxs = { 16.0f, 16.0f, 72.0f };
		}

		const auto flags = memory::read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );

		extrapolation_data data{};
		data.origin = latest.origin;
		data.velocity = velocity;
		data.obb_mins = obb_mins;
		data.obb_maxs = obb_maxs;
		data.flags = flags;
		data.sim_time = latest.simulation_time;
		data.direction = direction;

		const auto initial_direction = direction;

		for ( auto i = 0; i < ticks_to_extrapolate; ++i )
		{
			data.direction += direction_change;
			while ( data.direction > 180.0f ) data.direction -= 360.0f;
			while ( data.direction < -180.0f ) data.direction += 360.0f;

			// Natural exponential decay of angular turn rate over multiple ticks
			direction_change *= 0.90f;

			const auto rad = data.direction * ( 3.14159265f / 180.0f );
			const auto current_speed = data.velocity.length_2d( );
			data.velocity.x = std::cosf( rad ) * current_speed;
			data.velocity.y = std::sinf( rad ) * current_speed;

			data.sim_time += cstypes::tick_interval;

			this->predict_movement( data, pawn );
		}

		const auto origin_delta = data.origin - latest.origin;

		if ( origin_delta.length_sqr( ) < 0.01f )
		{
			logging::console::print( xs( "[extrap] {:x} | skip: predicted origin unchanged after {} ticks\n" ), pawn, ticks_to_extrapolate );
			return std::nullopt;
		}

		record extrap_record = latest;
		extrap_record.origin = data.origin;
		extrap_record.simulation_time = data.sim_time;
		extrap_record.tick = cstypes::time_to_ticks( data.sim_time );
		extrap_record.extrapolated = true;

		// Calculate total accumulated yaw rotation
		const auto total_yaw = math::helpers::normalize_yaw( data.direction - initial_direction );
		const auto rot_quat = math::quaternion::from_euler( { 0.0f, total_yaw, 0.0f } );

		extrap_record.rotation.y = math::helpers::normalize_yaw( latest.rotation.y + total_yaw );

		for ( auto i = 0; i < extrap_record.bone_count && i < 128; ++i )
		{
			const auto rel = latest.bones[ i ].position - latest.origin;
			extrap_record.bones[ i ].position = data.origin + rot_quat.rotate_vector( rel );
		}

		const auto dist = origin_delta.length_2d( );
		logging::console::print(
			xs( "[extrap] {:x} | ok: {} ticks | delta {:.2f} u | origin ({:.1f}, {:.1f}, {:.1f})\n" ),
			pawn, ticks_to_extrapolate, dist,
			data.origin.x, data.origin.y, data.origin.z
		);

		return extrap_record;
	}

} // namespace features::combat
