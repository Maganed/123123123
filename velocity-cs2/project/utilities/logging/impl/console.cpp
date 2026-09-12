#include <pch/pch.hpp>
#include <utilities/diag.hpp>
#include <cstdio>
#include <cstring>
#include "../logging.hpp"

namespace logging::console {

	static FILE* s_fp_out = nullptr;
	static FILE* s_fp_err = nullptr;
	static FILE* s_fp_in  = nullptr;
	static HANDLE s_console_handle = nullptr;
	static bool s_allocated = false;

	bool initialize () {
		if (AllocConsole()) {
			s_allocated = true;
		} else {
			AttachConsole(ATTACH_PARENT_PROCESS);
		}

		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);

		s_console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (s_console_handle != INVALID_HANDLE_VALUE && s_console_handle != nullptr) {
			DWORD mode = 0;
			if (GetConsoleMode(s_console_handle, &mode)) {
				SetConsoleMode(s_console_handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
			}
		}

		freopen_s(&s_fp_out, "CONOUT$", "w", stdout);
		freopen_s(&s_fp_err, "CONOUT$", "w", stderr);
		freopen_s(&s_fp_in,  "CONIN$",  "r", stdin);

		SetConsoleTitleA("Velocity CS2 - Debug Console");

		print_raw("\n==================================================================");
		print_raw("              VELOCITY CS2 - INITIALIZATION CONSOLE              ");
		print_raw("==================================================================\n");

		return true;
	}

	void shutdown () {
		if (s_fp_out) { fclose(s_fp_out); s_fp_out = nullptr; }
		if (s_fp_err) { fclose(s_fp_err); s_fp_err = nullptr; }
		if (s_fp_in)  { fclose(s_fp_in);  s_fp_in  = nullptr; }

		if (s_allocated) {
			FreeConsole();
			s_allocated = false;
		}
		s_console_handle = nullptr;
	}

	void print_raw (const char* text) {
		if ( !text ) {
			return;
		}

		const bool was_emitting = emitting;
		emitting = true;

		diag::write( diag::level::info, text );

		if (s_console_handle && s_console_handle != INVALID_HANDLE_VALUE) {
			DWORD written = 0;
			const auto len = static_cast<DWORD>(std::strlen(text));
			WriteConsoleA(s_console_handle, text, len, &written, nullptr);
			if (len == 0 || text[len - 1] != '\n') {
				WriteConsoleA(s_console_handle, "\n", 1, &written, nullptr);
			}
		} else {
			std::printf("%s\n", text);
			std::fflush(stdout);
		}

		emitting = was_emitting;
	}

} // namespace logging::console
