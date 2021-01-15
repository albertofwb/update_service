#pragma once
#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>

void RunNewProcess(LPTSTR lpCommandLine, LPCTSTR lpWorkDir = NULL);