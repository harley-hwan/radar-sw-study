// 트랙 수명 그림 검증용: TargetSimUI 의 CTrackLifeView 가 쓰는 MFC · Win32 · GDI+ 를 리눅스에서 흉내 냄.
// 그리기 호출은 기록만 함 (JSON). 오버로드 묶음은 GDI+ 와 같게 둬 모호한 호출이 여기서도 걸리게 함. 실제 빌드는 Visual Studio.
#pragma once
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

typedef int BOOL;
typedef long LONG;
typedef unsigned long DWORD;
typedef DWORD COLORREF;
typedef unsigned long long UINT64;
typedef unsigned char BYTE;
typedef unsigned char UINT8;
typedef short INT16_T;
typedef wchar_t WCHAR;
typedef const wchar_t *LPCTSTR;
typedef void *HDC;
typedef unsigned int UINT;
#define TRUE 1
#define FALSE 0
#define _T(x) L##x
#define WHEEL_DELTA 120
#define SRCCOPY 0x00CC0020
#define UNREFERENCED_PARAMETER(p) ((void)(p))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((DWORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(c) ((BYTE)(c))
#define GetGValue(c) ((BYTE)(((c) >> 8) & 0xFF))
#define GetBValue(c) ((BYTE)(((c) >> 16) & 0xFF))
#define afx_msg
#define DECLARE_MESSAGE_MAP()
#define BEGIN_MESSAGE_MAP(c, b)
#define END_MESSAGE_MAP()
#define ON_WM_PAINT()
#define ON_WM_ERASEBKGND()
#define ON_WM_SIZE()
#define ON_WM_LBUTTONDBLCLK()
static inline int MulDiv(int a, int b, int c) { return (int)std::lround((double)a * b / c); }

class CString
{
public:
	CString() {}
	explicit CString(const wchar_t *s) : w(s) {}
	CString &operator=(const wchar_t *s) { w = s; return *this; }
	CString &operator+=(const wchar_t *s) { w += s; return *this; }
	CString &operator+=(const CString &s) { w += s.w; return *this; }
	bool operator==(const CString &o) const { return w == o.w; }
	bool operator!=(const CString &o) const { return w != o.w; }
	const wchar_t *GetString() const { return w.c_str(); }
	int IsEmpty() const { return w.empty() ? 1 : 0; }
	void Format(const wchar_t *fmt, ...) { va_list a; va_start(a, fmt); w = vfmt(fmt, a); va_end(a); }
	void AppendFormat(const wchar_t *fmt, ...) { va_list a; va_start(a, fmt); w += vfmt(fmt, a); va_end(a); }
	std::wstring w;
private:
	static std::wstring vfmt(const wchar_t *fmt, va_list a)
	{
		std::wstring f(fmt);                                           // MSVC wide printf: %s = wide string; glibc needs %ls
		for (size_t p = 0; (p = f.find(L"%s", p)) != std::wstring::npos; p += 3) f.replace(p, 2, L"%ls");
		wchar_t buf[1024];
		vswprintf(buf, 1024, f.c_str(), a);
		return buf;
	}
};

struct CPoint { LONG x, y; CPoint(LONG a = 0, LONG b = 0) : x(a), y(b) {} };
struct CRect
{
	LONG left, top, right, bottom;
	CRect(LONG l = 0, LONG t = 0, LONG r = 0, LONG b = 0) : left(l), top(t), right(r), bottom(b) {}
	int Width() const { return (int)(right - left); }
	int Height() const { return (int)(bottom - top); }
	void SetRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
	BOOL PtInRect(CPoint p) const { return (p.x >= left && p.x < right && p.y >= top && p.y < bottom) ? TRUE : FALSE; }
};

class CBitmap { public: BOOL CreateCompatibleBitmap(class CDC *, int, int) { return TRUE; } };
class CDC
{
public:
	BOOL CreateCompatibleDC(CDC *) { return TRUE; }
	void FillSolidRect(const CRect *, COLORREF) {}
	CBitmap *SelectObject(CBitmap *b) { return b; }
	BOOL BitBlt(int, int, int, int, CDC *, int, int, DWORD) { return TRUE; }
	HDC GetSafeHdc() { return nullptr; }
};
class CWnd
{
public:
	void Invalidate(BOOL = TRUE) { invalidated++; }
	void GetClientRect(CRect *r) const { *r = CRect(0, 0, clientW, clientH); }
	void ScreenToClient(CPoint *) const {}
	int invalidated = 0, clientW = 800, clientH = 400;
};
class CPaintDC : public CDC { public: explicit CPaintDC(CWnd *) {} };
class CStatic : public CWnd
{
protected:
	void OnSize(UINT, int, int) {}
	void OnLButtonDblClk(UINT, CPoint) {}
};

// ---------------------------------------------------------------- GDI+
namespace rec { void op(const std::string &json); std::string utf8(const wchar_t *s); }

namespace Gdiplus
{
	typedef float REAL;
	typedef int INT;
	enum StringAlignment { StringAlignmentNear = 0, StringAlignmentCenter = 1, StringAlignmentFar = 2 };
	enum SmoothingMode { SmoothingModeNone = 3, SmoothingModeAntiAlias = 4 };
	enum TextRenderingHint { TextRenderingHintClearTypeGridFit = 5 };
	enum FontStyle { FontStyleRegular = 0, FontStyleBold = 1 };
	enum Unit { UnitPixel = 2 };
	enum StringFormatFlags { StringFormatFlagsNoWrap = 0x1000 };
	enum Status { Ok = 0 };

	class Color { public: Color(BYTE a, BYTE r, BYTE g, BYTE b) : A(a), R(r), G(g), B(b) {} BYTE A, R, G, B; };
	struct PointF { REAL X, Y; PointF(REAL x = 0, REAL y = 0) : X(x), Y(y) {} };
	struct Point { INT X, Y; };
	struct RectF { REAL X, Y, Width, Height; RectF(REAL x = 0, REAL y = 0, REAL w = 0, REAL h = 0) : X(x), Y(y), Width(w), Height(h) {} };
	struct Rect { INT X, Y, Width, Height; Rect(INT x = 0, INT y = 0, INT w = 0, INT h = 0) : X(x), Y(y), Width(w), Height(h) {} };
	class Brush { public: Color c{255, 0, 0, 0}; };
	class SolidBrush : public Brush { public: explicit SolidBrush(const Color &col) { c = col; } };
	class Pen { public: Pen(const Color &col, REAL w = 1.0F) : c(col), width(w) {} Status SetDashPattern(const REAL *, INT) { return Ok; } Color c; REAL width; };
	class FontFamily { public: explicit FontFamily(const WCHAR *) {} };
	class Font { public: Font(const FontFamily *, REAL s, INT style, Unit) : size(s), bold(style & FontStyleBold) {} REAL size; INT bold; };
	class StringFormat
	{
	public:
		Status SetAlignment(StringAlignment a) { ax = a; return Ok; }
		Status SetLineAlignment(StringAlignment a) { ay = a; return Ok; }
		Status SetFormatFlags(INT) { return Ok; }
		StringAlignment ax = StringAlignmentNear, ay = StringAlignmentNear;
	};

	std::string col(const Color &c);
	REAL textWidth(const WCHAR *s, REAL size);

	class Graphics
	{
	public:
		explicit Graphics(HDC) {}
		Status SetSmoothingMode(SmoothingMode m) { mode = m; return Ok; }
		SmoothingMode GetSmoothingMode() const { return mode; }
		Status SetTextRenderingHint(TextRenderingHint) { return Ok; }
		Status SetClip(const Rect &) { return Ok; }
		Status ResetClip() { return Ok; }
		Status FillRectangle(const Brush *b, const RectF &r) { return fill(b, r.X, r.Y, r.Width, r.Height); }
		Status FillRectangle(const Brush *b, REAL x, REAL y, REAL w, REAL h) { return fill(b, x, y, w, h); }
		Status FillRectangle(const Brush *b, const Rect &r) { return fill(b, (REAL)r.X, (REAL)r.Y, (REAL)r.Width, (REAL)r.Height); }
		Status FillRectangle(const Brush *b, INT x, INT y, INT w, INT h) { return fill(b, (REAL)x, (REAL)y, (REAL)w, (REAL)h); }
		Status FillRectangles(const Brush *b, const RectF *r, INT n) { for (INT i = 0; i < n; i++) fill(b, r[i].X, r[i].Y, r[i].Width, r[i].Height); return Ok; }
		Status DrawLine(const Pen *p, REAL x1, REAL y1, REAL x2, REAL y2) { return line(p, x1, y1, x2, y2); }
		Status DrawLine(const Pen *p, const PointF &a, const PointF &b) { return line(p, a.X, a.Y, b.X, b.Y); }
		Status DrawLine(const Pen *p, INT x1, INT y1, INT x2, INT y2) { return line(p, (REAL)x1, (REAL)y1, (REAL)x2, (REAL)y2); }
		Status DrawLine(const Pen *p, const Point &a, const Point &b) { return line(p, (REAL)a.X, (REAL)a.Y, (REAL)b.X, (REAL)b.Y); }
		Status DrawRectangle(const Pen *p, const RectF &r) { return box(p, r.X, r.Y, r.Width, r.Height); }
		Status DrawRectangle(const Pen *p, REAL x, REAL y, REAL w, REAL h) { return box(p, x, y, w, h); }
		Status DrawRectangle(const Pen *p, const Rect &r) { return box(p, (REAL)r.X, (REAL)r.Y, (REAL)r.Width, (REAL)r.Height); }
		Status DrawRectangle(const Pen *p, INT x, INT y, INT w, INT h) { return box(p, (REAL)x, (REAL)y, (REAL)w, (REAL)h); }
		Status MeasureString(const WCHAR *s, INT, const Font *f, const PointF &, RectF *out) const
		{
			*out = RectF(0, 0, textWidth(s, f->size), f->size * 1.33F);
			return Ok;
		}
		Status DrawString(const WCHAR *s, INT, const Font *f, const PointF &o, const StringFormat *fmt, const Brush *b);
	private:
		Status fill(const Brush *b, REAL x, REAL y, REAL w, REAL h);
		Status line(const Pen *p, REAL x1, REAL y1, REAL x2, REAL y2);
		Status box(const Pen *p, REAL x, REAL y, REAL w, REAL h);
		SmoothingMode mode = SmoothingModeAntiAlias;
	};
}
