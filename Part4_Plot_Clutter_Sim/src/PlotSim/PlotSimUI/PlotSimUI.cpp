#include "pch.h"
#include "framework.h"

#include "PlotSimUI.h"
#include "PlotSimUIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CPlotSimUIApp, CWinApp)
END_MESSAGE_MAP()

// 응용 프로그램 객체.
CPlotSimUIApp::CPlotSimUIApp() noexcept
{
}

CPlotSimUIApp g_PlotSimApp;

// 주 대화상자 실행. 닫히면 끝냄.
BOOL CPlotSimUIApp::InitInstance()
{
	if (AllocConsole())
	{
		FILE* fp;
		freopen_s(&fp, "CONOUT$", "w", stdout);

		printf("Console Window Initialized. \n");
	}

	CPlotSimUIDlg st_Dlg;

	(VOID)CWinApp::InitInstance();
	m_pMainWnd = &st_Dlg;
	(VOID)st_Dlg.DoModal();

	return FALSE;
}
