/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   OS dependant file system & system helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>
*/

#include "common/common_pch.h"

#if defined(SYS_WINDOWS)

#include <io.h>
#include <windows.h>
#include <winreg.h>
#include <direct.h>
#include <shlobj.h>
#include <sys/timeb.h>

#include "common/error.h"
#include "common/fs_sys_helpers.h"
#include "common/strings/editing.h"
#include "common/strings/utf8.h"

namespace mtx { namespace sys {

int64_t
get_current_time_millis() {
  struct _timeb tb;
  _ftime(&tb);

  return (int64_t)tb.time * 1000 + tb.millitm;
}

bool
get_registry_key_value(const std::string &key,
                       const std::string &value_name,
                       std::string &value) {
  std::vector<std::string> key_parts = split(key, "\\", 2);
  HKEY hkey;
  HKEY hkey_base = key_parts[0] == "HKEY_CURRENT_USER" ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  DWORD error    = RegOpenKeyExA(hkey_base, key_parts[1].c_str(), 0, KEY_READ, &hkey);

  if (ERROR_SUCCESS != error)
    return false;

  bool ok        = false;
  DWORD data_len = 0;
  DWORD dwDisp;
  if (ERROR_SUCCESS == RegQueryValueExA(hkey, value_name.c_str(), nullptr, &dwDisp, nullptr, &data_len)) {
    char *data = new char[data_len + 1];
    memset(data, 0, data_len + 1);

    if (ERROR_SUCCESS == RegQueryValueExA(hkey, value_name.c_str(), nullptr, &dwDisp, (BYTE *)data, &data_len)) {
      value = data;
      ok    = true;
    }

    delete []data;
  }

  RegCloseKey(hkey);

  return ok;
}

void
set_environment_variable(const std::string &key,
                         const std::string &value) {
  SetEnvironmentVariableA(key.c_str(), value.c_str());
  std::string env_buf = (boost::format("%1%=%2%") % key % value).str();
  _putenv(env_buf.c_str());
}

std::string
get_environment_variable(const std::string &key) {
  auto size   = 100u;
  auto buffer = memory_c::alloc(size);

  while (true) {
    auto required_size = GetEnvironmentVariableA(key.c_str(), reinterpret_cast<char *>(buffer->get_buffer()), size);
    if (required_size < size) {
      buffer->get_buffer()[required_size] = 0;
      break;
    }

    size = required_size;
  }

  return reinterpret_cast<char *>(buffer->get_buffer());
}

unsigned int
get_windows_version() {
  OSVERSIONINFO os_version_info;

  memset(&os_version_info, 0, sizeof(OSVERSIONINFO));
  os_version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

  if (!GetVersionEx(&os_version_info))
    return WINDOWS_VERSION_UNKNOWN;

  return (os_version_info.dwMajorVersion << 16) | os_version_info.dwMinorVersion;
}

bfs::path
get_application_data_folder() {
  wchar_t szPath[MAX_PATH];

  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, szPath)))
    return bfs::path{to_utf8(std::wstring(szPath))} / "mkvtoolnix";

  return bfs::path{};
}

int
system(std::string const &command) {
  std::wstring wcommand = to_wide(command);
  auto mem              = memory_c::clone(wcommand.c_str(), (wcommand.length() + 1) * sizeof(wchar_t));

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(STARTUPINFOW));
  memset(&pi, 0, sizeof(PROCESS_INFORMATION));

  auto result = ::CreateProcessW(NULL,                                           // application name (use only cmd line)
                                 reinterpret_cast<wchar_t *>(mem->get_buffer()), // full command line
                                 NULL,                                           // security attributes: defaults for both
                                 NULL,                                           //   the process and its main thread
                                 FALSE,                                          // inherit handles if we use pipes
                                 CREATE_NO_WINDOW,                               // process creation flags
                                 NULL,                                           // environment (use the same)
                                 NULL,                                           // current directory (use the same)
                                 &si,                                            // startup info (unused here)
                                 &pi                                             // process info
                                 );

  // Wait until child process exits.
  WaitForSingleObject(pi.hProcess, INFINITE);

  // Close process and thread handles.
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return !result ? -1 : 0;

}

bfs::path
get_current_exe_path(std::string const &) {
  std::wstring file_name;
  file_name.resize(4000);

  while (true) {
    memset(&file_name[0], 0, file_name.size() * sizeof(std::wstring::value_type));
    auto size = GetModuleFileNameW(nullptr, &file_name[0], file_name.size() - 1);
    if (size) {
      file_name.resize(size);
      break;
    }

    file_name.resize(file_name.size() + 4000);
  }

  return bfs::absolute(bfs::path{to_utf8(file_name)}).parent_path();
}

}}

#endif  // SYS_WINDOWS