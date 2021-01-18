#pragma once
#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <vector>

void RunNewProcess(LPTSTR lpCommandLine, LPCTSTR lpWorkDir = NULL);
BOOL ReadFromRegistry(LPTSTR lpSubKey, LPTSTR pszKeyname, LPTSTR szBuff, DWORD dwSize);
BOOL FindProcessPid(LPCTSTR ProcessName, DWORD& dwPid);