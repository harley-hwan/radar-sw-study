#include "pch.h"
#include "framework.h"

#include <algorithm>
namespace Gdiplus
{
	using std::min;
	using std::max;
}
#pragma warning(push, 3)
#include <gdiplus.h>
#pragma warning(pop)

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

// 공용 컨트롤, GDI+ 초기화 후 주 대화상자 실행. 닫히면 GDI+ 종료.
BOOL CPlotSimUIApp::InitInstance()
{
	INITCOMMONCONTROLSEX			st_InitCtrls;
	Gdiplus::GdiplusStartupInput	st_GdiplusInput;
	ULONG_PTR						gdiplusToken = 0U;

	st_InitCtrls.dwSize	= static_cast<DWORD>(sizeof(st_InitCtrls));
	st_InitCtrls.dwICC	= ICC_WIN95_CLASSES;
	(VOID)InitCommonControlsEx(&st_InitCtrls);

	(VOID)CWinApp::InitInstance();
	(VOID)Gdiplus::GdiplusStartup(&gdiplusToken, &st_GdiplusInput, nullptr);

	// GDI+ 종료 전에 대화상자 소멸.
	{
		CPlotSimUIDlg st_Dlg;

		m_pMainWnd = &st_Dlg;
		(VOID)st_Dlg.DoModal();
		m_pMainWnd = nullptr;
	}

	Gdiplus::GdiplusShutdown(gdiplusToken);

	return FALSE;
}
