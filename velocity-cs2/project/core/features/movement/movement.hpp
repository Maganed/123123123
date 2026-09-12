#pragma once

#include <core/systems/systems.hpp>

namespace xdraw {
	struct draw_list;
}

namespace features::movement {

	class bhop
	{
	public:
		void on_create_move( systems::input::usercmd* cmd );

	private:
		int m_ticks_on_ground{ 0 };
	};

	class airstrafe
	{
	public:
		void on_create_move( systems::input::usercmd* cmd );
		void store_angles( );

	private:
		void check_button( std::uintptr_t current_buttons, std::uintptr_t button );
		void rotate_movement( proto::base_usercmd_pb* base, float target_yaw, float view_yaw ) const;
		void rotate_to_stop( proto::base_usercmd_pb* base, const math::vector3& velocity ) const;

		std::uintptr_t m_last_buttons{};
		std::uintptr_t m_last_pressed{};
		bool m_side_switch{};
		math::vector3 m_angles{};
		float m_old_yaw{};
		int m_air_ticks{};
	};

	class jumpbug
	{
	public:
		void on_create_move( systems::input::usercmd* cmd );
		[[nodiscard]] bool active_this_tick( ) const { return false; }
		[[nodiscard]] float landing_fraction( ) const { return 1.0f; }
	};

	class fastladder
	{
	public:
		void on_create_move( systems::input::usercmd* cmd ) const;
	};

	class edgejump
	{
	public:
		void on_create_move( systems::input::usercmd* cmd ) const;
	};

	class edgestop
	{
	public:
		void on_create_move( systems::input::usercmd* cmd ) const;
	};

	class edgebug
	{
	public:
		void on_create_move( systems::input::usercmd* cmd );
		void on_render( xdraw::draw_list& draw_list );

		[[nodiscard]] bool active_this_tick( ) const { return false; }
	};

	class slowwalk
	{
	public:
		void on_create_move( systems::input::usercmd* cmd ) const;
	};

	class test_strafer
	{
	public:
		void on_create_move( systems::input::usercmd* cmd );
		[[nodiscard]] bool is_active( ) const { return false; }
		[[nodiscard]] bool handled_this_tick( ) const { return false; }
	};

} // namespace features::movement
