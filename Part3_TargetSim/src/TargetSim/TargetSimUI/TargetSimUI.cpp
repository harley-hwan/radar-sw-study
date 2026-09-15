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

CTargetSimUIApp g_TargetSimApp;

BOOL CTargetSimUIApp::InitInstance()
{
	INITCOMMONCONTROLSEX st_InitCtrls;
	st_InitCtrls.dwSize = static_cast<DWORD>(sizeof(st_InitCtrls));
	st_InitCtrls.dwICC = ICC_WIN95_CLASSES;
	(VOID)InitCommonControlsEx(&st_InitCtrls);

	(VOID)CWinApp::InitInstance();

	SetRegistryKey(_T("RadarSwStudy"));

	// 대화 상자 객체가 설정·상태 사본을 들고 있어 약 45 KB 라 스택 대신 힙에 둔다.
	CTargetSimUIDlg *st_Dlg = new CTargetSimUIDlg();
	m_pMainWnd = st_Dlg;

	const INT_PTR dialogResult = st_Dlg->DoModal();

	if (dialogResult == -1)
	{
		TRACE(traceAppMsg, 0, "경고: 대화 상자를 만들지 못했습니다.\n");
	}

	// 대화 상자가 닫혔으므로 메시지 펌프를 돌리지 않고 종료한다.
	m_pMainWnd = nullptr;
	delete st_Dlg;

	return FALSE;
}
