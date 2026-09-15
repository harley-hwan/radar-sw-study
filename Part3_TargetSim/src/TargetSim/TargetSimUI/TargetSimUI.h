#pragma once

#ifndef __AFXWIN_H__
	#error "PCH 를 먼저 포함해야 합니다. 이 파일보다 pch.h 를 앞에 두십시오."
#endif

#include "Resource.h"

class CTargetSimUIApp : public CWinApp
{
public:
	CTargetSimUIApp() noexcept;

	virtual BOOL InitInstance() override;

	DECLARE_MESSAGE_MAP()
};

extern CTargetSimUIApp theApp;
