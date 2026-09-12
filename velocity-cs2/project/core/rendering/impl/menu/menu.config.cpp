#include <pch/pch.hpp>
#include <utilities/math/math.hpp>
#include <core/settings.hpp>

#include "../../rendering.hpp"

namespace rendering {

	namespace config_meta {

		struct entry {
			std::string created{};
			std::string modified{};
		};

		inline constexpr wchar_t k_meta_key[]{ L"Software\\vertex\\configs_meta" };

		inline std::string get_current_timestamp( )
		{
			SYSTEMTIME st{};
			GetLocalTime( &st );
			char buf[ 32 ]{};
			std::snprintf( buf, sizeof( buf ), "%02d.%02d.%04d %02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute );
			return buf;
		}

		inline entry get( const std::wstring& wname )
		{
			entry e{};
			HKEY hkey{};
			if ( RegOpenKeyExW( HKEY_CURRENT_USER, k_meta_key, 0, KEY_QUERY_VALUE, &hkey ) == ERROR_SUCCESS )
			{
				char buf[ 128 ]{};
				DWORD size = sizeof( buf );
				if ( RegQueryValueExW( hkey, wname.c_str( ), nullptr, nullptr, reinterpret_cast< LPBYTE >( buf ), &size ) == ERROR_SUCCESS )
				{
					std::string s( buf, size ? ( size - 1 ) : 0 );
					const auto sep = s.find( '|' );
					if ( sep != std::string::npos )
					{
						e.created = s.substr( 0, sep );
						e.modified = s.substr( sep + 1 );
					}
					else
					{
						e.created = s;
						e.modified = s;
					}
				}
				RegCloseKey( hkey );
			}

			if ( e.created.empty( ) )
			{
				const auto now = get_current_timestamp( );
				e.created = now;
				e.modified = now;

				HKEY write_key{};
				if ( RegCreateKeyExW( HKEY_CURRENT_USER, k_meta_key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &write_key, nullptr ) == ERROR_SUCCESS )
				{
					const std::string val = e.created + "|" + e.modified;
					RegSetValueExW( write_key, wname.c_str( ), 0, REG_SZ, reinterpret_cast< const BYTE* >( val.c_str( ) ), static_cast< DWORD >( val.size( ) + 1 ) );
					RegCloseKey( write_key );
				}
			}

			return e;
		}

		inline void save_update( const std::wstring& wname, bool is_new = false )
		{
			HKEY hkey{};
			if ( RegCreateKeyExW( HKEY_CURRENT_USER, k_meta_key, 0, nullptr, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &hkey, nullptr ) != ERROR_SUCCESS )
			{
				return;
			}

			entry cur{};
			if ( !is_new )
			{
				cur = get( wname );
			}
			else
			{
				cur.created = get_current_timestamp( );
			}

			cur.modified = get_current_timestamp( );
			if ( cur.created.empty( ) )
			{
				cur.created = cur.modified;
			}

			const std::string val = cur.created + "|" + cur.modified;
			RegSetValueExW( hkey, wname.c_str( ), 0, REG_SZ, reinterpret_cast< const BYTE* >( val.c_str( ) ), static_cast< DWORD >( val.size( ) + 1 ) );
			RegCloseKey( hkey );
		}

		inline void remove( const std::wstring& wname )
		{
			HKEY hkey{};
			if ( RegOpenKeyExW( HKEY_CURRENT_USER, k_meta_key, 0, KEY_SET_VALUE, &hkey ) == ERROR_SUCCESS )
			{
				RegDeleteValueW( hkey, wname.c_str( ) );
				RegCloseKey( hkey );
			}
		}

	} // namespace config_meta

	namespace detail {

		struct config_item {
			std::wstring wname{};
			std::string name{};
			std::string created{};
			std::string modified{};
		};

		std::string search_buf{};
		std::vector<config_item> config_items{};
		auto selected{ -1 };
		auto needs_refresh{ true };
		auto confirm_delete{ false };
		auto confirm_timer{ 0.0f };

		// Modal state
		bool create_modal_open{ false };
		bool create_modal_just_opened{ false };
		std::string new_cfg_name{};

		static inline void wide_to_utf8( const std::wstring& wide, char* out, int out_size )
		{
			WideCharToMultiByte( CP_UTF8, 0, wide.c_str( ), -1, out, out_size, nullptr, nullptr );
		}

		static inline std::wstring utf8_to_wide( const std::string& utf8 )
		{
			wchar_t buf[ 128 ]{};
			MultiByteToWideChar( CP_UTF8, 0, utf8.c_str( ), -1, buf, 128 );
			return buf;
		}

		static inline bool config_matches_search( const std::string& name )
		{
			if ( detail::search_buf.empty( ) )
			{
				return true;
			}

			std::string lower_name{ name };
			std::string lower_search{ detail::search_buf };

			for ( auto& c : lower_name )
			{
				c = static_cast< char >( std::tolower( c ) );
			}

			for ( auto& c : lower_search )
			{
				c = static_cast< char >( std::tolower( c ) );
			}

			return lower_name.find( lower_search ) != std::string::npos;
		}

		static inline std::string selected_name( )
		{
			if ( detail::selected < 0 || detail::selected >= static_cast< int >( detail::config_items.size( ) ) )
			{
				return {};
			}

			return detail::config_items[ detail::selected ].name;
		}

	} // namespace detail

	void menu::draw_config( float group_w )
	{
		( void )group_w;

		if ( detail::needs_refresh )
		{
			const auto raw_list = config::registry::list( );
			detail::config_items.clear( );
			detail::config_items.reserve( raw_list.size( ) );

			for ( const auto& wname : raw_list )
			{
				char narrow[ 128 ]{};
				detail::wide_to_utf8( wname, narrow, sizeof( narrow ) );

				const auto meta = config_meta::get( wname );
				detail::config_items.push_back( { wname, narrow, meta.created, meta.modified } );
			}

			detail::needs_refresh = false;

			if ( detail::selected >= static_cast< int >( detail::config_items.size( ) ) )
			{
				detail::selected = -1;
			}
		}

		const auto dt = xdraw::delta_time( );

		if ( detail::confirm_delete )
		{
			detail::confirm_timer += dt;

			if ( detail::confirm_timer > 3.0f )
			{
				detail::confirm_delete = false;
			}
		}

		auto& dl = xui::draw::current( );
		const auto& s = xui::ctx( ).style;
		const auto& input = xui::ctx( ).input;

		xui::layout::set_cursor( this->m_body_x - this->m_x, this->m_body_y - this->m_y );

		if ( !xui::begin_child( "##cfg_panel", this->m_body_w, this->m_body_h, false ) )
		{
			return;
		}

		xui::text_input( "##cfg_search", detail::search_buf, 64, "search configs..." );

		constexpr auto btn_h{ 28.0f };
		const auto [ avail_w, avail_h ] = xui::layout::avail( );
		const auto list_h = std::max( 80.0f, avail_h - btn_h - s.item_spacing_y );

		if ( xui::begin_child( "##cfg_list", avail_w, list_h, true ) )
		{
			const auto row_w = xui::layout::avail( ).first;
			constexpr auto row_h{ 40.0f };
			auto visible_rows{ 0 };

			for ( auto i = 0; i < static_cast< int >( detail::config_items.size( ) ); ++i )
			{
				const auto& item = detail::config_items[ i ];

				if ( !detail::config_matches_search( item.name ) )
				{
					continue;
				}

				const auto row = xui::layout::item( row_w, row_h );
				const auto is_selected = ( detail::selected == i );
				const auto is_hovered = input.in_rect( row ) && !detail::create_modal_open;

				if ( is_hovered && input.mouse_clicked && !xui::ctx( ).overlay_blocking( ) && !detail::create_modal_open )
				{
					detail::selected = i;
					detail::confirm_delete = false;
					config::registry::load( item.wname );
					settings::finalize_binds( );
				}

				const auto hover_anim = xui::anim::lerp( xui::fnv1a( "cfgrow" ) + i, is_hovered ? 1.0f : 0.0f, 14.0f );
				const auto sel_anim = xui::anim::lerp( xui::fnv1a( "cfgsel" ) + i, is_selected ? 1.0f : 0.0f, 10.0f );

				if ( sel_anim > 0.01f )
				{
					dl.rect_filled( row.x, row.y, row.w, row.h, tokens::col_accent.alpha( static_cast< std::uint8_t >( 70.0f * sel_anim ) ), xdraw::corner_radius{ 6.0f } );
				}
				else if ( hover_anim > 0.01f )
				{
					dl.rect_filled( row.x, row.y, row.w, row.h, tokens::col_elevated.alpha( static_cast< std::uint8_t >( 255.0f * hover_anim * 0.5f ) ), xdraw::corner_radius{ 6.0f } );
				}

				// Left: Config Name
				const auto [ tw, th ] = xdraw::measure_text( item.name );
				const auto text_col = is_selected
					? xui::lerp( tokens::col_text, tokens::col_accent, sel_anim )
					: xui::lerp( tokens::col_text_dim, tokens::col_text, hover_anim );

				dl.text( row.x + 14.0f, row.y + ( row.h - th ) * 0.5f, item.name, text_col );

				// Right: Creation Date (Top) and Last Modified Date (Bottom)
				const std::string created_str = "created: " + item.created;
				const std::string modified_str = "modified: " + item.modified;

				const auto [ cw, ch ] = xdraw::measure_text( created_str );
				const auto [ mw, mh ] = xdraw::measure_text( modified_str );

				constexpr auto right_pad = 14.0f;
				const auto created_x = row.x + row.w - right_pad - cw;
				const auto modified_x = row.x + row.w - right_pad - mw;

				const auto date_col = is_selected
					? tokens::col_text.alpha( 210 )
					: tokens::col_text_dim.alpha( 170 );

				dl.text( created_x, row.y + 6.0f, created_str, date_col );
				dl.text( modified_x, row.y + 22.0f, modified_str, date_col );

				visible_rows++;
			}

			if ( visible_rows == 0 )
			{
				const auto row = xui::layout::item( row_w, 28.0f );
				dl.text( row.x + 10.0f, row.y + 6.0f, detail::config_items.empty( ) ? "no configs found" : "no matches", tokens::col_text_dim );
			}

			xui::end_child( );
		}

		// --- Bottom Action Bar: [ create ] [ save ] [ delete ] ---
		const auto btn_w = ( avail_w - s.item_spacing_x * 2.0f ) / 3.0f;
		const auto has_selection = detail::selected >= 0 && detail::selected < static_cast< int >( detail::config_items.size( ) );
		const auto save_name = has_selection ? detail::selected_name( ) : detail::search_buf;
		const auto can_save = !save_name.empty( );

		const auto create_clicked = xui::button( "create", btn_w, btn_h );
		if ( !detail::create_modal_open && create_clicked )
		{
			detail::create_modal_open = true;
			detail::create_modal_just_opened = true;
			detail::new_cfg_name.clear( );
		}

		xui::layout::same_line( );

		const auto save_clicked = xui::button( "save", btn_w, btn_h );
		if ( !detail::create_modal_open && save_clicked && can_save )
		{
			const auto wname = detail::utf8_to_wide( save_name );
			config::registry::save( wname );
			config_meta::save_update( wname, false );
			detail::needs_refresh = true;
		}

		xui::layout::same_line( );

		if ( detail::confirm_delete )
		{
			const auto confirm_clicked = xui::button( "confirm", btn_w, btn_h );
			if ( !detail::create_modal_open && confirm_clicked && has_selection )
			{
				const auto& wname = detail::config_items[ detail::selected ].wname;
				config::registry::remove( wname );
				config_meta::remove( wname );
				detail::selected = -1;
				detail::needs_refresh = true;
				detail::confirm_delete = false;
			}
		}
		else
		{
			const auto delete_clicked = xui::button( "delete", btn_w, btn_h );
			if ( !detail::create_modal_open && delete_clicked && has_selection )
			{
				detail::confirm_delete = true;
				detail::confirm_timer = 0.0f;
			}
		}

		// --- Create Config Mini-Panel Modal ---
		if ( detail::create_modal_open )
		{
			// Dim overlay over the entire config area
			dl.rect_filled( this->m_body_x, this->m_body_y, this->m_body_w, this->m_body_h, xdraw::color{ 8, 7, 16, 215 }, xdraw::corner_radius{ 0.0f } );

			constexpr auto modal_w = 320.0f;
			constexpr auto modal_h = 120.0f;
			const auto modal_rel_x = ( this->m_body_w - modal_w ) * 0.5f;
			const auto modal_rel_y = ( this->m_body_h - modal_h ) * 0.5f;
			const auto modal_x = this->m_body_x + modal_rel_x;
			const auto modal_y = this->m_body_y + modal_rel_y;
			const auto modal_rect = xui::rect{ modal_x, modal_y, modal_w, modal_h };

			// Outer border / glow
			dl.rect_filled( modal_x - 1.0f, modal_y - 1.0f, modal_w + 2.0f, modal_h + 2.0f, tokens::col_accent.alpha( 45 ), xdraw::corner_radius{ tokens::card_rounding + 1.0f } );
			dl.rect( modal_x, modal_y, modal_w, modal_h, tokens::col_elevated.alpha( 220 ), xdraw::corner_radius{ tokens::card_rounding }, 1.0f );

			// Check key presses: Enter or Esc
			auto enter_pressed = false;
			auto esc_pressed = false;
			for ( const auto vk : input.key_presses( ) )
			{
				if ( vk == VK_RETURN )
				{
					enter_pressed = true;
				}
				else if ( vk == VK_ESCAPE )
				{
					esc_pressed = true;
				}
			}

			if ( !enter_pressed && ( GetAsyncKeyState( VK_RETURN ) & 1 ) )
			{
				enter_pressed = true;
			}
			if ( !esc_pressed && ( GetAsyncKeyState( VK_ESCAPE ) & 1 ) )
			{
				esc_pressed = true;
			}

			if ( esc_pressed )
			{
				detail::create_modal_open = false;
				detail::new_cfg_name.clear( );
				xui::ctx( ).active_text_input = xui::null_id;
			}
			else if ( !detail::create_modal_just_opened && input.mouse_clicked && !input.in_rect( modal_rect ) )
			{
				// Clicked outside modal to cancel
				detail::create_modal_open = false;
				detail::new_cfg_name.clear( );
				xui::ctx( ).active_text_input = xui::null_id;
			}

			// Autofocus on first frame
			if ( detail::create_modal_just_opened )
			{
				detail::create_modal_just_opened = false;
				xui::ctx( ).active_text_input = xui::make_id( "##new_cfg_name" );
			}

			xui::layout::set_cursor( modal_rel_x, modal_rel_y );

			// Save and style child container
			auto& style = xui::ctx( ).style;
			const auto saved_child_bg = style.child_bg;
			const auto saved_child_border = style.child_border;
			style.child_bg = tokens::col_card;
			style.child_border = xdraw::color{ 0, 0, 0, 0 };

			if ( xui::begin_child( "##cfg_create_box", modal_w, modal_h, false ) )
			{
				const auto box_avail_w = xui::layout::avail( ).first;

				// Header: Title and Close Cross '✕'
				const auto title_row = xui::layout::item( box_avail_w, 20.0f );
				dl.text( title_row.x, title_row.y + 2.0f, "CREATE CONFIG", tokens::col_accent );

				const auto close_rect = xui::rect{ title_row.x + title_row.w - 18.0f, title_row.y + 1.0f, 18.0f, 18.0f };
				const auto close_hovered = input.in_rect( close_rect );
				if ( close_hovered && input.mouse_clicked )
				{
					detail::create_modal_open = false;
					detail::new_cfg_name.clear( );
					xui::ctx( ).active_text_input = xui::null_id;
				}
				dl.text( close_rect.x + 4.0f, close_rect.y + 1.0f, "✕", close_hovered ? tokens::col_accent : tokens::col_text_dim );

				// Text input for config name
				xui::text_input( "##new_cfg_name", detail::new_cfg_name, 64, "config name..." );

				// Bottom Buttons: [ cancel ] [ ok ]
				const auto cur_avail_w = xui::layout::avail( ).first;
				const auto modal_btn_w = ( cur_avail_w - s.item_spacing_x ) * 0.5f;
				constexpr auto modal_btn_h = 26.0f;

				if ( xui::button( "cancel", modal_btn_w, modal_btn_h ) )
				{
					detail::create_modal_open = false;
					detail::new_cfg_name.clear( );
					xui::ctx( ).active_text_input = xui::null_id;
				}

				xui::layout::same_line( );

				const auto can_create = !detail::new_cfg_name.empty( );
				const auto ok_clicked = xui::button( "ok", modal_btn_w, modal_btn_h );

				if ( ( enter_pressed || ok_clicked ) && can_create )
				{
					const auto wname = detail::utf8_to_wide( detail::new_cfg_name );
					config::registry::save( wname );
					config_meta::save_update( wname, true );
					detail::needs_refresh = true;
					detail::create_modal_open = false;
					detail::new_cfg_name.clear( );
					xui::ctx( ).active_text_input = xui::null_id;
				}

				xui::end_child( );
			}

			style.child_bg = saved_child_bg;
			style.child_border = saved_child_border;
		}

		xui::end_child( );
	}

} // namespace rendering
