// 트랙 스코어 확인 창
#pragma once

#include "TrackMgr.h"

#define SCORE_MAX_FLAG			40						// 넣을 수 있는 Assigned_Flag 수
#define SCORE_PRESET_NUM		5

// Assigned_Flag 순서를 넣으면 f_Trk_UpdateState 로 상태 변화를 표로 보임
// (시뮬레이션은 Flag 가 늘 0 이라 확정 · 메모리는 여기서 확인)
class CScoreDlg : public CDialogEx
{
public:
	explicit CScoreDlg(CWnd *st_Parent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SCORE_DIALOG };
#endif

protected:
	VOID	DoDataExchange(CDataExchange *st_Dx) override;
	BOOL	OnInitDialog(VOID) override;

	afx_msg VOID	f_OnFlagsChanged(VOID);
	afx_msg VOID	f_OnPresetChanged(VOID);
	afx_msg VOID	f_OnListCustomDraw(NMHDR *st_Hdr, LRESULT *pt_Result);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_Run(VOID);

	CListCtrl	st_List;
	CComboBox	st_Preset;
	INT32		nRowState[SCORE_MAX_FLAG + 1];			// 줄마다 트랙 상태 (줄 바탕색)
	INT32		nRowNum;
};
