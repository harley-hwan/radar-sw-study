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

#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CTargetSimUIApp, CWinApp)
END_MESSAGE_MAP()

// 응용 프로그램 객체.
CTargetSimUIApp::CTargetSimUIApp() noexcept
{
}

CTargetSimUIApp g_TargetSimApp;

// 공용 컨트롤과 GDI+ 를 올리고 주 대화상자를 띄운다. 닫히면 GDI+ 를 내린다.
BOOL CTargetSimUIApp::InitInstance()
{
	INITCOMMONCONTROLSEX			st_InitCtrls;
	Gdiplus::GdiplusStartupInput	st_GdiplusInput;
	ULONG_PTR						gdiplusToken = 0U;

	st_InitCtrls.dwSize	= static_cast<DWORD>(sizeof(st_InitCtrls));
	st_InitCtrls.dwICC	= ICC_WIN95_CLASSES;
	(VOID)InitCommonControlsEx(&st_InitCtrls);

	(VOID)CWinApp::InitInstance();
	(VOID)Gdiplus::GdiplusStartup(&gdiplusToken, &st_GdiplusInput, nullptr);

	// 대화상자가 GDI+ 를 내리기 전에 사라지도록 블록 안에 둔다.
	{
		CTargetSimUIDlg st_Dlg;

		m_pMainWnd = &st_Dlg;
		(VOID)st_Dlg.DoModal();
		m_pMainWnd = nullptr;
	}

	Gdiplus::GdiplusShutdown(gdiplusToken);

	return FALSE;
}
