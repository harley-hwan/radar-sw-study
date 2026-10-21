//
// @file	ScoreDlg.h
// @brief	트랙 스코어 확인 창: Assigned_Flag 순서를 넣어 TrackScore, MemoryScore, TrackState 변화를 표로 확인
// @author	hwan
// @date	2026.10.21.
//
#pragma once

#include "TrackMgr.h"

#define SCORE_MAX_FLAG			40						// 넣을 수 있는 Assigned_Flag 수
#define SCORE_PRESET_NUM		5

// 트랙 스코어 확인 (과제 5): Assigned_Flag 순서를 넣으면 막 개시한 임시 트랙 (TrackScore = TRK_SCORE_INIT) 의
// TrackScore, MemoryScore, TrackState 가 스캔마다 어떻게 바뀌는지 표로 보여 줌. Core 의 f_Trk_UpdateState 를 그대로 부름.
// 시뮬레이션에서는 Assigned_Flag 가 늘 0 이어서 확정, 메모리 상태가 나오지 않음. 그 두 상태를 여기서 확인. 입력을 고치면 바로 다시 계산.
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
