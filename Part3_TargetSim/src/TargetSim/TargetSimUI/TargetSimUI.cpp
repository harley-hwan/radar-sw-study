#include "pch.h"
#include "framework.h"
#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CTargetSimUIApp, CWinApp)
END_MESSAGE_MAP()

CTargetSimUIApp::CTargetSimUIApp() noexcept
{
}

CTargetSimUIApp theApp;

BOOL CTargetSimUIApp::InitInstance()
{
	INITCOMMONCONTROLSEX initCtrls;
	initCtrls.dwSize = sizeof(initCtrls);
	initCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&initCtrls);

	CWinApp::InitInstance();

	SetRegistryKey(_T("RadarSwStudy"));

	CTargetSimUIDlg dlg;
	m_pMainWnd = &dlg;

	const INT_PTR response = dlg.DoModal();

	if (response == -1)
	{
		TRACE(traceAppMsg, 0, "경고: 대화 상자를 만들지 못했습니다.\n");
	}

	// 대화 상자가 닫혔으므로 메시지 펌프를 돌리지 않고 종료한다.
	m_pMainWnd = nullptr;

	return FALSE;
}
