#include "md2_win32.h"

#include <windows.h>

char* win32_realpath(char const* path, bool* d_error, char* error_buffer, size_t error_buffer_size)
{
	char output[MAX_PATH];
	DWORD size_or_error = GetFullPathNameA(
		path,
		sizeof output,
		output,
		NULL);
	if (size_or_error == 0)
	{
		*d_error = true;
		snprintf(error_buffer, error_buffer_size, "Error 0x%x", GetLastError());
		return NULL;
	}
	if (size_or_error + 1 > MAX_PATH) {
		*d_error = true;
		snprintf(error_buffer, error_buffer_size, "buffer was insufficient");
		return NULL;
	}
	*d_error = false;
	return _strdup(output);
}
