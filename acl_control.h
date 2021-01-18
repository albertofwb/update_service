#pragma once
#include <windows.h> 
#include <stdio.h> 
#include <tchar.h>
#include <strsafe.h>
#include <AccCtrl.h>
#include <Aclapi.h>

// windows 服务创建的 namedpipe 普通进程没有 write 权限，需要一段神奇代码赋予普通进程 write 权限
// https://stackoverflow.com/questions/340345/web-service-cant-open-named-pipe-access-denied
void BuildSecurityAttributes(SECURITY_ATTRIBUTES& sa);