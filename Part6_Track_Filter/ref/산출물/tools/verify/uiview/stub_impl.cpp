// afx_stub.h 구현: 그리기 호출 기록 (창 전체 좌표, JSON), CString 서식, Win32 함수 흉내
#include "afx_stub.h"

#include <sstream>

CWnd	*CWnd::s_Capture = nullptr;
CWnd	*CWnd::s_Focus = nullptr;
CFont	CWnd::defaultFont;

namespace rec
{
	std::vector<std::string>	g_Ops;
	double						offsetX = 0.0;
	double						offsetY = 0.0;
	int							nMessageBox = 0;

	void op(const std::string &json) { g_Ops.push_back(json); }

	std::string utf8(const wchar_t *s)
	{
		std::string o;
		for (; *s; s++)
		{
			unsigned c = (unsigned)*s;
			if (c < 0x80) { if (c == '"' || c == '\\') o += '\\'; if (c == '\n') { o += "\\n"; continue; } if (c == '\r') continue; o += (char)c; }
			else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
			else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
			else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
		}
		return o;
	}

	std::string col(COLORREF c, int a)
	{
		char b[40];
		snprintf(b, sizeof(b), "[\"#%02x%02x%02x\",%d]", GetRValue(c), GetGValue(c), GetBValue(c), a);
		return b;
	}

	// 맑은 고딕에 가까운 글자 폭 (em): 한글 0.92, 공백 0.28, 숫자 0.55, 그 밖 0.56
	double textWidth(const wchar_t *s, double sizePx)
	{
		double w = 0.0;
		for (; *s; s++)
		{
			const wchar_t c = *s;
			w += ((c >= 0x1100) ? 0.92 : ((c == L' ') ? 0.28 : ((c >= L'0' && c <= L'9') ? 0.55 : 0.56))) * sizePx;
		}
		return w;
	}
}

// MSVC 넓은 문자 서식 -> glibc: %s (넓은 문자열) -> %ls, %hs (좁은 문자열) -> %s
std::wstring CString::vfmt(const wchar_t *fmt, va_list a)
{
	std::wstring f;
	for (const wchar_t *p = fmt; *p; p++)
	{
		if (*p != L'%') { f += *p; continue; }
		std::wstring spec(L"%");
		p++;
		while (*p && wcschr(L"-+ #0123456789.*", *p)) spec += *p++;
		bool isShort = false;
		if (*p == L'h' && p[1] == L's') { isShort = true; p++; }
		if (*p == L's') spec += isShort ? L"s" : L"ls";
		else if (*p) spec += *p;
		f += spec;
		if (!*p) break;
	}
	std::vector<wchar_t> buf(8192);
	vswprintf(buf.data(), buf.size(), f.c_str(), a);
	return buf.data();
}

int MultiByteToWideChar(UINT, DWORD, const char *src, int srcLen, wchar_t *dst, int dstLen)
{
	const unsigned char	*s = (const unsigned char *)src;
	const unsigned char	*end = (srcLen < 0) ? nullptr : (s + srcLen);
	int					n = 0;

	while ((end == nullptr || s < end) && n < dstLen)
	{
		unsigned c = *s;
		if (c < 0x80) { s += 1; }
		else if ((c >> 5) == 0x6) { c = ((c & 0x1F) << 6) | (s[1] & 0x3F); s += 2; }
		else if ((c >> 4) == 0xE) { c = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); s += 3; }
		else { c = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); s += 4; }
		dst[n++] = (wchar_t)c;
		if (c == 0 && end == nullptr) break;
	}
	return n;
}

int _snprintf_s(char *buf, size_t size, size_t, const char *fmt, ...)
{
	va_list a;
	va_start(a, fmt);
	const int n = vsnprintf(buf, size, fmt, a);
	va_end(a);
	return (n >= 0 && (size_t)n < size) ? n : -1;
}

int _wfopen_s(FILE **file, const wchar_t *path, const wchar_t *mode)
{
	*file = fopen(rec::utf8(path).c_str(), rec::utf8(mode).c_str());
	return (*file != nullptr) ? 0 : 1;
}

BOOL SystemParametersInfo(UINT action, UINT, void *out, UINT)
{
	if (action == SPI_GETWORKAREA) *(CRect *)out = CRect(0, 0, 1920, 1040);
	return TRUE;
}

int AfxMessageBox(const wchar_t *text, UINT, UINT)
{
	rec::nMessageBox++;
	fprintf(stderr, "    [AfxMessageBox] %s\n", rec::utf8(text).c_str());
	return IDOK;
}

// ---------------------------------------------------------------- DC

void CDC::FillSolidRect(const CRect *r, COLORREF c)
{
	char t[200];
	snprintf(t, sizeof(t), "{\"op\":\"fill\",\"x\":%.1f,\"y\":%.1f,\"w\":%d,\"h\":%d,\"c\":%s}", rec::offsetX + r->left, rec::offsetY + r->top, r->Width(), r->Height(),
		rec::col(c, 255).c_str());
	rec::op(t);
}

int CDC::FrameRect(const CRect *r, CBrush *b)
{
	char t[200];
	snprintf(t, sizeof(t), "{\"op\":\"box\",\"x\":%.1f,\"y\":%.1f,\"w\":%d,\"h\":%d,\"c\":%s,\"lw\":1}", rec::offsetX + r->left, rec::offsetY + r->top, r->Width() - 1,
		r->Height() - 1, rec::col(b->color, 255).c_str());
	rec::op(t);
	return 1;
}

BOOL CDC::RoundRect(const CRect *r, POINT p)
{
	char t[240];
	snprintf(t, sizeof(t), "{\"op\":\"rbox\",\"x\":%.1f,\"y\":%.1f,\"w\":%d,\"h\":%d,\"r\":%ld,\"fill\":%s,\"c\":%s}", rec::offsetX + r->left, rec::offsetY + r->top,
		r->Width() - 1, r->Height() - 1, (long)p.x / 2, rec::col(curBrush ? curBrush->color : 0xFFFFFF, 255).c_str(), rec::col(curPen ? curPen->color : 0, 255).c_str());
	rec::op(t);
	return TRUE;
}

// ---------------------------------------------------------------- 대화상자

BOOL CDialogEx::OnInitDialog()
{
	CDataExchange dx;
	dx.dlg = this;
	DoDataExchange(&dx);
	return TRUE;
}

void CDialogEx::StubAddControl(int ctrlId, int ctrlKind, const wchar_t *ctrlText)
{
	CWnd *w = new CWnd();
	w->id = ctrlId;
	w->kind = ctrlKind;
	w->text = ctrlText;
	owned.push_back(w);
	controls[ctrlId] = w;
}

void CDialogEx::StubBind(int ctrlId, CWnd &ctrl)
{
	auto it = controls.find(ctrlId);
	if (it != controls.end() && it->second != &ctrl)
	{
		ctrl.kind = it->second->kind;
		if (ctrl.text.IsEmpty()) ctrl.text = it->second->text;
	}
	ctrl.id = ctrlId;
	controls[ctrlId] = &ctrl;
}

CWnd *CDialogEx::GetDlgItem(int ctrlId)
{
	auto it = controls.find(ctrlId);
	if (it == controls.end())
	{
		fprintf(stderr, "    [GetDlgItem] unknown control id %d\n", ctrlId);
		StubAddControl(ctrlId, 0, L"");
		it = controls.find(ctrlId);
	}
	return it->second;
}

void DDX_Control(CDataExchange *dx, int ctrlId, CWnd &ctrl)
{
	dx->dlg->StubBind(ctrlId, ctrl);
}

// ---------------------------------------------------------------- GDI+

namespace Gdiplus
{
	static std::string gcol(const Color &c)
	{
		char b[40];
		snprintf(b, sizeof(b), "[\"#%02x%02x%02x\",%d]", c.R, c.G, c.B, c.A);
		return b;
	}

	Status Graphics::SetClip(const Rect &r, CombineMode)
	{
		char t[160];
		snprintf(t, sizeof(t), "{\"op\":\"clip\",\"x\":%.1f,\"y\":%.1f,\"w\":%d,\"h\":%d}", rec::offsetX + r.X, rec::offsetY + r.Y, r.Width, r.Height);
		rec::op(t);
		return Ok;
	}

	Status Graphics::SetClip(const RectF &r, CombineMode)
	{
		char t[160];
		snprintf(t, sizeof(t), "{\"op\":\"clip\",\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f}", rec::offsetX + r.X, rec::offsetY + r.Y, r.Width, r.Height);
		rec::op(t);
		return Ok;
	}

	Status Graphics::ResetClip()
	{
		rec::op("{\"op\":\"noclip\"}");
		return Ok;
	}

	Status Graphics::fill(const Brush *b, REAL x, REAL y, REAL w, REAL h)
	{
		char t[200];
		snprintf(t, sizeof(t), "{\"op\":\"fill\",\"x\":%.2f,\"y\":%.2f,\"w\":%.2f,\"h\":%.2f,\"c\":%s}", rec::offsetX + x, rec::offsetY + y, w, h, gcol(b->c).c_str());
		rec::op(t);
		return Ok;
	}

	Status Graphics::line(const Pen *p, REAL x1, REAL y1, REAL x2, REAL y2)
	{
		char t[240];
		snprintf(t, sizeof(t), "{\"op\":\"line\",\"x1\":%.2f,\"y1\":%.2f,\"x2\":%.2f,\"y2\":%.2f,\"c\":%s,\"lw\":%.2f,\"dash\":%d}", rec::offsetX + x1, rec::offsetY + y1,
			rec::offsetX + x2, rec::offsetY + y2, gcol(p->c).c_str(), p->width, p->dash);
		rec::op(t);
		return Ok;
	}

	Status Graphics::box(const Pen *p, REAL x, REAL y, REAL w, REAL h)
	{
		char t[240];
		snprintf(t, sizeof(t), "{\"op\":\"box\",\"x\":%.2f,\"y\":%.2f,\"w\":%.2f,\"h\":%.2f,\"c\":%s,\"lw\":%.2f,\"dash\":%d}", rec::offsetX + x, rec::offsetY + y, w, h,
			gcol(p->c).c_str(), p->width, p->dash);
		rec::op(t);
		return Ok;
	}

	Status Graphics::ellipse(const Color *fillColor, const Pen *p, REAL x, REAL y, REAL w, REAL h)
	{
		char t[260];
		snprintf(t, sizeof(t), "{\"op\":\"ellipse\",\"x\":%.2f,\"y\":%.2f,\"w\":%.2f,\"h\":%.2f,\"fill\":%s,\"c\":%s,\"lw\":%.2f}", rec::offsetX + x, rec::offsetY + y, w, h,
			fillColor ? gcol(*fillColor).c_str() : "null", p ? gcol(p->c).c_str() : "null", p ? p->width : 0.0F);
		rec::op(t);
		return Ok;
	}

	static std::string points(const PointF *pt, INT n)
	{
		std::ostringstream s;
		s.setf(std::ios::fixed);
		s.precision(2);
		s << "[";
		for (INT i = 0; i < n; i++) s << (i ? "," : "") << "[" << (rec::offsetX + pt[i].X) << "," << (rec::offsetY + pt[i].Y) << "]";
		s << "]";
		return s.str();
	}

	Status Graphics::polygon(const Color *fillColor, const Pen *p, const PointF *pt, INT n)
	{
		std::ostringstream t;
		t << "{\"op\":\"polygon\",\"pts\":" << points(pt, n) << ",\"fill\":" << (fillColor ? gcol(*fillColor) : "null") << ",\"c\":" << (p ? gcol(p->c) : "null")
		  << ",\"lw\":" << (p ? p->width : 0.0F) << "}";
		rec::op(t.str());
		return Ok;
	}

	Status Graphics::DrawLines(const Pen *p, const PointF *pt, INT n)
	{
		std::ostringstream t;
		t << "{\"op\":\"poly\",\"pts\":" << points(pt, n) << ",\"c\":" << gcol(p->c) << ",\"lw\":" << p->width << ",\"dash\":" << p->dash << "}";
		rec::op(t.str());
		return Ok;
	}

	Status Graphics::DrawLines(const Pen *p, const Point *pt, INT n)
	{
		std::vector<PointF> f;
		for (INT i = 0; i < n; i++) f.push_back(PointF((REAL)pt[i].X, (REAL)pt[i].Y));
		return DrawLines(p, f.data(), n);
	}

	Status Graphics::FillPolygon(const Brush *b, const Point *pt, INT n)
	{
		std::vector<PointF> f;
		for (INT i = 0; i < n; i++) f.push_back(PointF((REAL)pt[i].X, (REAL)pt[i].Y));
		return polygon(&b->c, nullptr, f.data(), n);
	}

	Status Graphics::DrawPolygon(const Pen *p, const Point *pt, INT n)
	{
		std::vector<PointF> f;
		for (INT i = 0; i < n; i++) f.push_back(PointF((REAL)pt[i].X, (REAL)pt[i].Y));
		return polygon(nullptr, p, f.data(), n);
	}

	Status Graphics::DrawString(const WCHAR *s, INT, const Font *f, const PointF &o, const StringFormat *fmt, const Brush *b)
	{
		std::ostringstream t;
		t << "{\"op\":\"text\",\"s\":\"" << rec::utf8(s) << "\",\"x\":" << (rec::offsetX + o.X) << ",\"y\":" << (rec::offsetY + o.Y) << ",\"ax\":" << (fmt ? fmt->ax : 0)
		  << ",\"ay\":" << (fmt ? fmt->ay : 0) << ",\"size\":" << f->size << ",\"bold\":" << f->bold << ",\"w\":" << (rec::textWidth(s, f->size) + (0.33 * f->size))
		  << ",\"c\":" << gcol(b->c) << "}";
		rec::op(t.str());
		return Ok;
	}

	Status Graphics::DrawString(const WCHAR *s, INT n, const Font *f, const PointF &o, const Brush *b)
	{
		return DrawString(s, n, f, o, nullptr, b);
	}
}
