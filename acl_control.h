#pragma once
#include <windows.h> 
#include <stdio.h> 
#include <tchar.h>
#include <strsafe.h>
#include <AccCtrl.h>
#include <Aclapi.h>

// windows ���񴴽��� namedpipe ��ͨ����û�� write Ȩ�ޣ���Ҫһ��������븳����ͨ���� write Ȩ��
// https://stackoverflow.com/questions/340345/web-service-cant-open-named-pipe-access-denied
void BuildSecurityAttributes(SECURITY_ATTRIBUTES& sa);