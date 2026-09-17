#include "pch.h"
#include "framework.h"

// gdiplus.h 는 min·max 매크로를 쓰는데 framework.h 가 NOMINMAX 를 걸어 두었다.
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

	// 궤적 그림이 GDI+ 로 부드러운 선을 그린다. 대화상자가 살아 있는 동안만 켜 둔다.
	if (Gdiplus::GdiplusStartup(&gdiplusToken, &st_GdiplusInput, nullptr) == Gdiplus::Ok)
	{
		// 대화 상자 객체가 설정·상태 사본을 들고 있어 수십 KB 라 스택 대신 힙에 둔다.
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

		Gdiplus::GdiplusShutdown(gdiplusToken);
	}

	return FALSE;
}
