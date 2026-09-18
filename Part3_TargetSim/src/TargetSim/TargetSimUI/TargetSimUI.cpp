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

CTargetSimUIApp::CTargetSimUIApp() noexcept
{
}

CTargetSimUIApp g_TargetSimApp;

BOOL CTargetSimUIApp::InitInstance()
{
	INITCOMMONCONTROLSEX		st_InitCtrls;
	Gdiplus::GdiplusStartupInput	st_GdiplusInput;
	ULONG_PTR					gdiplusToken = 0U;

	st_InitCtrls.dwSize = static_cast<DWORD>(sizeof(st_InitCtrls));
	st_InitCtrls.dwICC = ICC_WIN95_CLASSES;
	(VOID)InitCommonControlsEx(&st_InitCtrls);

	(VOID)CWinApp::InitInstance();

	SetRegistryKey(_T("RadarSwStudy"));

	if (Gdiplus::GdiplusStartup(&gdiplusToken, &st_GdiplusInput, nullptr) == Gdiplus::Ok)
	{
		// 대화상자 객체가 수십 KB 라 힙에 둔다.
		CTargetSimUIDlg *st_Dlg = new CTargetSimUIDlg();
		m_pMainWnd = st_Dlg;

		const INT_PTR dialogResult = st_Dlg->DoModal();

		if (dialogResult == -1)
		{
			TRACE(traceAppMsg, 0, "경고: 대화 상자를 만들지 못했습니다.\n");
		}

		m_pMainWnd = nullptr;
		delete st_Dlg;

		Gdiplus::GdiplusShutdown(gdiplusToken);
	}

	return FALSE;
}
