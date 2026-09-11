//
// @file	MapView.cpp
// @brief	궤적 지도 그리기. 축 비율을 1/cos(위도) 로 맞춰 선회 원이 찌그러지지 않게 한다
// @author	hwan
// @date	2026.09.12.
//
#include "pch.h"
#include "framework.h"
#include <cmath>
#include "MapView.h"

static const COLORREF s_aColor[] = { RGB(0x2B, 0x57, 0xA6), RGB(0x2F, 0x85, 0x5A), RGB(0xC2, 0x52, 0x1C),
                                     RGB(0x7B, 0x4F, 0xA3), RGB(0xB8, 0x86, 0x0B), RGB(0x2A, 0x9D, 0x8F) };

BEGIN_MESSAGE_MAP(CMapView, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


COLORREF CMapView::ColorOf(int nIdx)
{
	return s_aColor[nIdx % (sizeof(s_aColor) / sizeof(s_aColor[0]))];
}

BOOL CMapView::OnEraseBkgnd(CDC *)
{
	return TRUE;
}

void CMapView::OnPaint()
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


void CMapView::Draw(CDC &dc, const CRect &rc)
{
	CRect rcPlot(rc.left + 50, rc.top + 8, rc.right - 10, rc.bottom - 24);
	CFont font;
	font.CreatePointFont(85, _T("Segoe UI"));
	CFont *pOldFont = dc.SelectObject(&font);

	dc.FillSolidRect(rc, RGB(255, 255, 255));
	dc.FillSolidRect(rcPlot, RGB(0xF5, 0xF8, 0xFC));
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(RGB(0x5A, 0x64, 0x78));

	if ((m_pPaths == nullptr) || m_pPaths->empty() || (*m_pPaths)[0].empty())
	{
		dc.DrawText(_T("[실행] 을 누르면 궤적이 그려집니다"), rcPlot, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		dc.SelectObject(pOldFont);
		return;
	}

	// 범위. 위도 1 도가 경도 1 도보다 1/cos(위도) 배 길다
	double dLonMin = 1e9, dLonMax = -1e9, dLatMin = 1e9, dLatMax = -1e9;
	for (const auto &path : *m_pPaths)
	{
		for (const ST_Point &p : path)
		{
			if (p.dLon < dLonMin) dLonMin = p.dLon;
			if (p.dLon > dLonMax) dLonMax = p.dLon;
			if (p.dLat < dLatMin) dLatMin = p.dLat;
			if (p.dLat > dLatMax) dLatMax = p.dLat;
		}
	}
	const double dLatC = 0.5 * (dLatMin + dLatMax), dLonC = 0.5 * (dLonMin + dLonMax);
	const double dAsp  = 1.0 / std::cos(dLatC * 3.14159265358979 / 180.0);
	const double dW = rcPlot.Width(), dH = rcPlot.Height();
	double dLonSpan = (dLonMax - dLonMin) * 1.3 + 0.01;
	double dLatSpan = (dLatMax - dLatMin) * 1.3 + 0.01;
	if (dLatSpan * dAsp * dW / dH > dLonSpan)
		dLonSpan = dLatSpan * dAsp * dW / dH;
	else
		dLatSpan = dLonSpan / dAsp * dH / dW;
	const double dLonLo = dLonC - dLonSpan / 2, dLatLo = dLatC - dLatSpan / 2;

	auto X = [&](double dLon) { return rcPlot.left + static_cast<int>((dLon - dLonLo) / dLonSpan * dW + 0.5); };
	auto Y = [&](double dLat) { return rcPlot.bottom - static_cast<int>((dLat - dLatLo) / dLatSpan * dH + 0.5); };

	// 격자 (0.05 도)
	{
		CPen  pen(PS_SOLID, 1, RGB(0xE0, 0xE7, 0xF1));
		CPen *pOldPen = dc.SelectObject(&pen);
		CString str;
		for (double v = std::ceil(dLonLo / 0.05) * 0.05; v < dLonLo + dLonSpan; v += 0.05)
		{
			dc.MoveTo(X(v), rcPlot.top);
			dc.LineTo(X(v), rcPlot.bottom);
			str.Format(_T("%.2f"), v);
			dc.DrawText(str, CRect(X(v) - 30, rcPlot.bottom + 4, X(v) + 30, rcPlot.bottom + 20), DT_CENTER | DT_SINGLELINE);
		}
		for (double v = std::ceil(dLatLo / 0.05) * 0.05; v < dLatLo + dLatSpan; v += 0.05)
		{
			dc.MoveTo(rcPlot.left, Y(v));
			dc.LineTo(rcPlot.right, Y(v));
			str.Format(_T("%.2f"), v);
			dc.DrawText(str, CRect(rc.left, Y(v) - 8, rcPlot.left - 4, Y(v) + 8), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
		}
		dc.SelectObject(pOldPen);
	}

	// 궤적 : 전체는 가늘게, 지금 시각까지는 굵게, 지금 위치는 점
	const int nLast = static_cast<int>((*m_pPaths)[0].size()) - 1;
	const int nStep = (m_nStep < 0) ? 0 : (m_nStep > nLast) ? nLast : m_nStep;
	for (size_t i = 0; i < m_pPaths->size(); i++)
	{
		const auto &path = (*m_pPaths)[i];
		const COLORREF col = ColorOf(static_cast<int>(i));
		CPen  penThin(PS_SOLID, 1, col), penThick(PS_SOLID, 3, col);
		CPen *pOldPen = dc.SelectObject(&penThin);

		dc.MoveTo(X(path[0].dLon), Y(path[0].dLat));
		for (size_t k = 1; k < path.size(); k++)
			dc.LineTo(X(path[k].dLon), Y(path[k].dLat));
		dc.SelectObject(&penThick);
		dc.MoveTo(X(path[0].dLon), Y(path[0].dLat));
		for (int k = 1; k <= nStep; k++)
			dc.LineTo(X(path[k].dLon), Y(path[k].dLat));

		CBrush  brush(col);
		CBrush *pOldBrush = dc.SelectObject(&brush);
		const int x = X(path[nStep].dLon), y = Y(path[nStep].dLat);
		if (i == 0)
		{
			POINT apt[3] = { { x, y - 8 }, { x - 7, y + 6 }, { x + 7, y + 6 } };
			dc.SelectStockObject(WHITE_PEN);
			dc.Polygon(apt, 3);
		}
		else
		{
			// 지금 위치 + 진행 방향(yaw). 북이 위, 시계방향이 +
			const double dYaw = path[nStep].dYaw * 3.14159265358979 / 180.0;
			CPen penHead(PS_SOLID, 2, col);
			dc.SelectObject(&penHead);
			dc.MoveTo(x, y);
			dc.LineTo(x + static_cast<int>(18.0 * std::sin(dYaw)), y - static_cast<int>(18.0 * std::cos(dYaw)));
			dc.SelectStockObject(WHITE_PEN);
			dc.Ellipse(x - 5, y - 5, x + 6, y + 6);
		}
		dc.SelectObject(pOldBrush);
		dc.SelectObject(pOldPen);
	}

	// 범례
	CString str;
	for (size_t i = 0; i < m_pPaths->size(); i++)
	{
		const int y = rcPlot.top + 8 + static_cast<int>(i) * 16;
		dc.FillSolidRect(rcPlot.left + 8, y + 3, 10, 10, ColorOf(static_cast<int>(i)));
		if (i == 0) str = _T("플랫폼"); else str.Format(_T("표적 %d"), static_cast<int>(i));
		dc.TextOut(rcPlot.left + 24, y, str);
	}
	str.Format(_T("t = %.1f s"), nStep * 0.1);
	dc.DrawText(str, CRect(rcPlot.left, rcPlot.top + 8, rcPlot.right - 8, rcPlot.top + 24), DT_RIGHT | DT_SINGLELINE);

	dc.SelectObject(pOldFont);
}
