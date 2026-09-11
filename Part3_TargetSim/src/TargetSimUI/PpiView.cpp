//
// @file	PpiView.cpp
// @brief	PPI 스코프 그리기. 플랫폼에서 본 거리·방위를 극좌표로 찍는다.
//			스윕이 표적 방위를 지나는 순간에만 플롯이 찍히고, 오래된 플롯은 잔상처럼 흐려진다.
// @author	hwan
// @date	2026.09.12.
//
#include "pch.h"
#include "framework.h"
#include <cmath>
#include "PpiView.h"

#define SWEEP_PERIOD	3.0			// 안테나 한 바퀴 [s]
#define PLOT_KEEP		6			// 표적당 잔상으로 남기는 플롯 수

static const double   PI_       = 3.14159265358979;
static const COLORREF CLR_BG    = RGB(5, 14, 9);
static const COLORREF CLR_GRID  = RGB(0, 84, 40);
static const COLORREF CLR_TEXT  = RGB(96, 200, 130);
static const COLORREF CLR_SWEEP = RGB(140, 255, 170);
static const COLORREF CLR_PLOT  = RGB(215, 255, 225);

BEGIN_MESSAGE_MAP(CPpiView, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


// a 에서 b 쪽으로 f (0~1) 만큼 섞은 색
static COLORREF Mix(COLORREF a, COLORREF b, double f)
{
	auto ch = [f](int sa, int sb) { return static_cast<int>(sa + (sb - sa) * f + 0.5); };
	return RGB(ch(GetRValue(a), GetRValue(b)), ch(GetGValue(a), GetGValue(b)), ch(GetBValue(a), GetBValue(b)));
}


// 표시 거리 : 궤적 중 가장 먼 거리를 링 간격으로 올림
void CPpiView::SetPaths(const PathList *pPaths, double dDt)
{
	double dMax = 0.0;

	m_pPaths = pPaths;
	m_dDt    = dDt;
	m_nStep  = 0;
	if (pPaths != nullptr)
		for (const auto &path : *pPaths)
			for (const ST_Point &p : path)
				if (p.dRange > dMax) dMax = p.dRange;
	m_dRing  = (dMax <= 20000.0) ? 5000.0 : (dMax <= 50000.0) ? 10000.0 : 20000.0;
	m_dScale = std::ceil(dMax / m_dRing) * m_dRing;
	if (m_dScale < m_dRing) m_dScale = 3.0 * m_dRing;
	Invalidate(FALSE);
}

BOOL CPpiView::OnEraseBkgnd(CDC *)
{
	return TRUE;
}

void CPpiView::OnPaint()
{
	CPaintDC dc(this);
	CRect    rc;
	CDC      memDC;
	CBitmap  bmp;

	GetClientRect(&rc);
	memDC.CreateCompatibleDC(&dc);
	bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
	CBitmap *pOld = memDC.SelectObject(&bmp);
	Draw(memDC, rc);
	dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
	memDC.SelectObject(pOld);
}


void CPpiView::Draw(CDC &dc, const CRect &rc)
{
	const int    cx = rc.CenterPoint().x, cy = rc.CenterPoint().y;
	const int    R  = ((rc.Width() < rc.Height()) ? rc.Width() : rc.Height()) / 2 - 26;
	const int    nLast  = ((m_pPaths != nullptr) && !m_pPaths->empty()) ? static_cast<int>((*m_pPaths)[0].size()) - 1 : -1;
	const int    nStep  = (nLast < 0) ? 0 : (m_nStep < 0) ? 0 : (m_nStep > nLast) ? nLast : m_nStep;
	const double t      = nStep * m_dDt;
	const double dSweep = std::fmod(t, SWEEP_PERIOD) / SWEEP_PERIOD * 360.0;		// 북에서 시계방향 [deg]
	CFont   fontMono, fontUi;
	CString str;

	// 방위(북 0, 시계방향, deg) 와 반지름(px) → 화면 좌표
	auto PT = [&](double dDeg, double r)
	{
		return CPoint(cx + static_cast<int>(r * std::sin(dDeg * PI_ / 180.0) + 0.5),
		              cy - static_cast<int>(r * std::cos(dDeg * PI_ / 180.0) + 0.5));
	};

	fontMono.CreatePointFont(80, _T("Consolas"));
	fontUi.CreatePointFont(85, _T("Segoe UI"));
	CFont *pOldFont = dc.SelectObject(&fontMono);
	dc.FillSolidRect(rc, CLR_BG);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(CLR_TEXT);

	// 스윕 잔광 : 6 도짜리 부채꼴 8 장, 뒤로 갈수록 어둡게
	{
		CPen *pOldPen = static_cast<CPen *>(dc.SelectStockObject(NULL_PEN));
		for (int j = 7; j >= 0; j--)
		{
			CBrush  brush(Mix(CLR_BG, CLR_SWEEP, 0.30 * (1.0 - j / 8.0)));
			CBrush *pOldBrush = dc.SelectObject(&brush);
			CPoint  apt[6];
			apt[0] = CPoint(cx, cy);
			for (int m = 0; m <= 4; m++)
				apt[1 + m] = PT(dSweep - (j + 1) * 6.0 + m * 1.5, R);
			dc.Polygon(apt, 6);
			dc.SelectObject(pOldBrush);
		}
		dc.SelectObject(pOldPen);
	}

	// 거리 링 · 방위 눈금
	{
		CPen    penGrid(PS_SOLID, 1, CLR_GRID), penDim(PS_SOLID, 1, Mix(CLR_BG, CLR_GRID, 0.5));
		CPen   *pOldPen   = dc.SelectObject(&penDim);
		CBrush *pOldBrush = static_cast<CBrush *>(dc.SelectStockObject(NULL_BRUSH));

		for (int a = 0; a < 360; a += 30)
		{
			dc.MoveTo(cx, cy);
			dc.LineTo(PT(a, R));
		}
		dc.SelectObject(&penGrid);
		for (double r = m_dRing; r <= m_dScale + 1.0; r += m_dRing)
		{
			const int rp = static_cast<int>(r / m_dScale * R + 0.5);
			dc.Ellipse(cx - rp, cy - rp, cx + rp + 1, cy + rp + 1);
			str.Format(_T("%g km"), r / 1000.0);
			dc.TextOut(cx + 4, cy - rp + 2, str);
		}
		for (int a = 0; a < 360; a += 10)
		{
			const bool bMajor = (a % 30) == 0;
			dc.MoveTo(PT(a, R));
			dc.LineTo(PT(a, R + (bMajor ? 8 : 4)));
			if (bMajor)
			{
				const CPoint c = PT(a, R + 17);
				str.Format(_T("%03d"), a);
				dc.DrawText(str, CRect(c.x - 14, c.y - 7, c.x + 14, c.y + 7), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}
		}
		dc.SelectObject(pOldPen);
		dc.SelectObject(pOldBrush);
	}

	// 스윕 선
	{
		CPen  penSweep(PS_SOLID, 2, CLR_SWEEP);
		CPen *pOldPen = dc.SelectObject(&penSweep);
		dc.MoveTo(cx, cy);
		dc.LineTo(PT(dSweep, R));
		dc.SelectObject(pOldPen);
	}

	// 플롯 : 최근 PLOT_KEEP 바퀴 동안 스윕이 표적 방위를 지난 시각의 위치. 방금 것은 크고 밝게, 오래된 것은 작고 어둡게
	if (nLast >= 0)
	{
		const int kNow = static_cast<int>(std::floor(t / SWEEP_PERIOD));

		for (size_t i = 1; i < m_pPaths->size(); i++)
		{
			const auto &path = (*m_pPaths)[i];
			auto azAt = [&](double tt)
			{
				int s = static_cast<int>(tt / m_dDt + 0.5);
				s = (s < 0) ? 0 : (s > nLast) ? nLast : s;
				return path[s].dAz;
			};
			bool bLatest = true;

			for (int k = kNow; (k >= 0) && (k > kNow - PLOT_KEEP); k--)
			{
				// k 번째 바퀴에서 스윕이 표적 방위에 닿는 시각. 그 사이 방위가 조금 움직이니 한 번 보정
				double tc = k * SWEEP_PERIOD + azAt(k * SWEEP_PERIOD) / 360.0 * SWEEP_PERIOD;
				tc = k * SWEEP_PERIOD + azAt(tc) / 360.0 * SWEEP_PERIOD;
				if (tc > t + 1e-9)
					continue;

				int s = static_cast<int>(tc / m_dDt + 0.5);
				if (s > nLast) s = nLast;
				const ST_Point &p = path[s];
				const double    f = 1.0 - (t - tc) / (PLOT_KEEP * SWEEP_PERIOD);		// 1 방금 ~ 0 사라지기 직전
				const CPoint    c = PT(p.dAz, p.dRange / m_dScale * R);
				const int       rad = bLatest ? 5 : 3;
				CBrush  brush(Mix(CLR_BG, CLR_PLOT, 0.15 + 0.85 * f));
				CBrush *pOldBrush = dc.SelectObject(&brush);
				CPen   *pOldPen   = static_cast<CPen *>(dc.SelectStockObject(NULL_PEN));

				dc.Ellipse(c.x - rad, c.y - rad, c.x + rad + 1, c.y + rad + 1);
				dc.SelectObject(pOldPen);
				dc.SelectObject(pOldBrush);
				if (bLatest)
				{
					dc.SetTextColor(CLR_PLOT);
					str.Format(_T("T%d"), static_cast<int>(i));
					dc.TextOut(c.x + 8, c.y - 15, str);
					dc.SetTextColor(CLR_TEXT);
					str.Format(_T("%.1fkm %.1f°"), p.dRange / 1000.0, p.dEl);
					dc.TextOut(c.x + 8, c.y - 1, str);
					bLatest = false;
				}
			}
		}
	}

	// 플랫폼 (가운데)
	{
		CPen    penPf(PS_SOLID, 1, CLR_SWEEP);
		CPen   *pOldPen   = dc.SelectObject(&penPf);
		CBrush *pOldBrush = static_cast<CBrush *>(dc.SelectStockObject(NULL_BRUSH));
		dc.Ellipse(cx - 5, cy - 5, cx + 6, cy + 6);
		dc.MoveTo(cx - 9, cy); dc.LineTo(cx + 10, cy);
		dc.MoveTo(cx, cy - 9); dc.LineTo(cx, cy + 10);
		dc.SelectObject(pOldPen);
		dc.SelectObject(pOldBrush);
	}

	// 구석 정보
	str.Format(_T("t = %5.1f s"), t);
	dc.TextOut(rc.left + 8, rc.top + 6, str);
	str.Format(_T("SWEEP %.0f s/rev"), SWEEP_PERIOD);
	dc.TextOut(rc.left + 8, rc.bottom - 20, str);
	str.Format(_T("RANGE %g km"), m_dScale / 1000.0);
	dc.DrawText(str, CRect(rc.right - 140, rc.bottom - 20, rc.right - 8, rc.bottom - 4), DT_RIGHT | DT_SINGLELINE);
	if (nLast >= 0)
		str.Format(_T("TARGETS %d"), static_cast<int>(m_pPaths->size()) - 1);
	else
		str = _T("");
	dc.DrawText(str, CRect(rc.right - 140, rc.top + 6, rc.right - 8, rc.top + 22), DT_RIGHT | DT_SINGLELINE);
	if (nLast < 0)
	{
		dc.SelectObject(&fontUi);
		dc.DrawText(_T("[실행] 을 누르면 스윕이 돌기 시작합니다"), CRect(rc.left, cy + R / 2, rc.right, cy + R / 2 + 20), DT_CENTER | DT_SINGLELINE);
	}
	dc.SelectObject(pOldFont);
}
