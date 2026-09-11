//
// @file	TargetSimUI.cpp
// @brief	앱 초기화 (MFC 마법사 코드 기반, Part2 CoordUI 와 같은 뼈대)
// @author	hwan
// @date	2026.09.11.
//
#include "pch.h"
#include "framework.h"
#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTargetSimUIApp

BEGIN_MESSAGE_MAP(CTargetSimUIApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


CTargetSimUIApp::CTargetSimUIApp()
{
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
}


CTargetSimUIApp theApp;


BOOL CTargetSimUIApp::InitInstance()
{
	INITCOMMONCONTROLSEX st_InitCtrls;
	st_InitCtrls.dwSize = sizeof(st_InitCtrls);
	st_InitCtrls.dwICC  = ICC_WIN95_CLASSES;	// 슬라이더, 리스트뷰
	InitCommonControlsEx(&st_InitCtrls);

	CWinApp::InitInstance();

	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	SetRegistryKey(_T("TargetSim"));

	CShellManager *pShellManager = new CShellManager;

	CTargetSimUIDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
	}

	m_pMainWnd = nullptr;

	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	return FALSE;
}
