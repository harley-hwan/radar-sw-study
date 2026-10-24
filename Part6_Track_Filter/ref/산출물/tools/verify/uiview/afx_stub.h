// TargetSimUI 검증용: 화면 코드가 쓰는 MFC · Win32 · GDI+ 를 리눅스에서 흉내 냄 (실제 빌드는 Visual Studio).
// 컨트롤은 위치 · 글자 · 보임만 기억하고, 그리기 호출은 기록만 함 (rec::). 오버로드 묶음은 GDI+ 와 같게 둬 모호한 호출이 여기서도 걸리게 함.
#pragma once
#define __AFXWIN_H__

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------- Win32 형, 상수
typedef int BOOL;
typedef long LONG;
typedef unsigned long DWORD;
typedef DWORD COLORREF;
typedef unsigned char BYTE;
typedef unsigned char UINT8;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned long long UINT64;
typedef wchar_t WCHAR;
typedef wchar_t TCHAR;
typedef const wchar_t *LPCTSTR;
typedef wchar_t *LPTSTR;
typedef const char *LPCSTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t INT_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef intptr_t LRESULT;
typedef void *HDC;
typedef void *HWND;
typedef void *HGDIOBJ;
typedef struct HBRUSH__ { int unused; } *HBRUSH;

#define TRUE					1
#define FALSE					0
#define _T(x)					L##x
#define _tcstod					wcstod
#define _TRUNCATE				((size_t)-1)
#define WHEEL_DELTA				120
#define SRCCOPY					0x00CC0020
#define UNREFERENCED_PARAMETER(p) ((void)(p))
#define RGB(r, g, b)			((COLORREF)(((BYTE)(r) | ((DWORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(c)			((BYTE)(c))
#define GetGValue(c)			((BYTE)(((c) >> 8) & 0xFF))
#define GetBValue(c)			((BYTE)(((c) >> 16) & 0xFF))
#define DEBUG_NEW				new

#define BST_UNCHECKED			0
#define BST_CHECKED				1
#define SW_HIDE					0
#define SW_SHOWNA				8
#define SWP_NOZORDER			0x0004
#define SWP_NOACTIVATE			0x0010
#define SIZE_MINIMIZED			1
#define SPI_GETWORKAREA			0x0030
#define LOGPIXELSX				88
#define FW_BOLD					700
#define CTLCOLOR_BTN			3
#define CTLCOLOR_DLG			4
#define CTLCOLOR_STATIC			6
#define PS_SOLID				0
#define IDOK					1
#define IDCANCEL				2
#define MB_OK					0x00
#define MB_ICONWARNING			0x30
#define CP_UTF8					65001
#define LVIF_TEXT				0x0001
#define LVN_GETDISPINFO			(-150)
#define NM_CUSTOMDRAW			(-12)
#define LVCFMT_LEFT				0
#define LVCFMT_RIGHT			1
#define LVS_EX_GRIDLINES		0x0001
#define LVS_EX_FULLROWSELECT	0x0020
#define LVS_EX_DOUBLEBUFFER		0x00010000
#define LVSICF_NOINVALIDATEALL	0x0001
#define LVSICF_NOSCROLL			0x0002
#define OFN_OVERWRITEPROMPT		0x0002
#define OFN_HIDEREADONLY		0x0004
#define OFN_NOCHANGEDIR			0x0008
#define OFN_PATHMUSTEXIST		0x0800
#define CDDS_PREPAINT			0x0001
#define CDDS_ITEMPREPAINT		0x10001
#define CDRF_DODEFAULT			0x0000
#define CDRF_NOTIFYITEMDRAW		0x0020
#define ICC_WIN95_CLASSES		0x00FF
#define WM_SIZE					0x0005

// 메시지 맵은 쓰지 않음 (검증 프로그램이 처리 함수를 직접 부름)
#define afx_msg
#define DECLARE_MESSAGE_MAP()
#define BEGIN_MESSAGE_MAP(c, b)
#define END_MESSAGE_MAP()
#define ON_WM_PAINT()
#define ON_WM_ERASEBKGND()
#define ON_WM_SIZE()
#define ON_WM_LBUTTONDOWN()
#define ON_WM_LBUTTONUP()
#define ON_WM_LBUTTONDBLCLK()
#define ON_WM_MOUSEMOVE()
#define ON_WM_CAPTURECHANGED()
#define ON_WM_CTLCOLOR()
#define ON_WM_GETMINMAXINFO()
#define ON_WM_CLOSE()
#define ON_WM_DESTROY()
#define ON_WM_TIMER()
#define ON_WM_HSCROLL()
#define ON_WM_MOUSEWHEEL()
#define ON_BN_CLICKED(id, fn)
#define ON_CBN_SELCHANGE(id, fn)
#define ON_EN_CHANGE(id, fn)
#define ON_NOTIFY(code, id, fn)

static inline int MulDiv(int a, int b, int c) { return (int)std::lround((double)a * b / c); }
int MultiByteToWideChar(UINT codePage, DWORD flags, const char *src, int srcLen, wchar_t *dst, int dstLen);
int _snprintf_s(char *buf, size_t size, size_t count, const char *fmt, ...);
int _wfopen_s(FILE **file, const wchar_t *path, const wchar_t *mode);

// ---------------------------------------------------------------- 기록 (그리기, 컨트롤)
namespace rec
{
	void op(const std::string &json);
	std::string utf8(const wchar_t *s);
	std::string col(COLORREF c, int alpha);
	extern double offsetX;							// 지금 그리는 창의 자리 (창 전체 그림 좌표로 옮김)
	extern double offsetY;
	double textWidth(const wchar_t *s, double sizePx);	// 맑은 고딕에 가까운 글자 폭 [px]
}

// ---------------------------------------------------------------- MFC 기본 형
class CString
{
public:
	CString() {}
	CString(const CString &o) : w(o.w) {}
	explicit CString(const wchar_t *s) : w(s ? s : L"") {}
	CString &operator=(const CString &o) { w = o.w; return *this; }
	CString &operator=(const wchar_t *s) { w = s ? s : L""; return *this; }
	CString &operator+=(const wchar_t *s) { w += s; return *this; }
	CString &operator+=(const CString &s) { w += s.w; return *this; }
	bool operator==(const CString &o) const { return w == o.w; }
	bool operator!=(const CString &o) const { return w != o.w; }
	wchar_t operator[](int i) const { return w[(size_t)i]; }
	operator const wchar_t *() const { return w.c_str(); }
	const wchar_t *GetString() const { return w.c_str(); }
	int GetLength() const { return (int)w.size(); }
	int IsEmpty() const { return w.empty() ? 1 : 0; }
	void Empty() { w.clear(); }
	CString &Trim()
	{
		size_t a = w.find_first_not_of(L" \t\r\n"), b = w.find_last_not_of(L" \t\r\n");
		w = (a == std::wstring::npos) ? L"" : w.substr(a, b - a + 1);
		return *this;
	}
	CString Tokenize(const wchar_t *delim, int &pos) const
	{
		size_t start = (size_t)pos;
		while (start < w.size() && wcschr(delim, w[start])) start++;
		if (pos < 0 || start >= w.size()) { pos = -1; return CString(); }
		size_t end = start;
		while (end < w.size() && !wcschr(delim, w[end])) end++;
		pos = (int)end;
		return CString(w.substr(start, end - start).c_str());
	}
	void Format(const wchar_t *fmt, ...) { va_list a; va_start(a, fmt); w = vfmt(fmt, a); va_end(a); }
	void AppendFormat(const wchar_t *fmt, ...) { va_list a; va_start(a, fmt); w += vfmt(fmt, a); va_end(a); }
	std::wstring w;
private:
	static std::wstring vfmt(const wchar_t *fmt, va_list a);
};

struct POINT { LONG x, y; };
struct CPoint : public POINT { CPoint(LONG a = 0, LONG b = 0) { x = a; y = b; } };
struct CSize { LONG cx, cy; };
struct CRect
{
	LONG left, top, right, bottom;
	CRect(LONG l = 0, LONG t = 0, LONG r = 0, LONG b = 0) : left(l), top(t), right(r), bottom(b) {}
	int Width() const { return (int)(right - left); }
	int Height() const { return (int)(bottom - top); }
	void SetRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
	void InflateRect(int dx, int dy) { left -= dx; top -= dy; right += dx; bottom += dy; }
	BOOL PtInRect(CPoint p) const { return (p.x >= left && p.x < right && p.y >= top && p.y < bottom) ? TRUE : FALSE; }
};

struct LOGFONT { LONG lfHeight; LONG lfWeight; };
struct TEXTMETRIC { LONG tmHeight; };
struct MINMAXINFO { POINT ptReserved; POINT ptMaxSize; POINT ptMaxPosition; POINT ptMinTrackSize; POINT ptMaxTrackSize; };
struct NMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; };
struct LVITEM { UINT mask; int iItem; int iSubItem; UINT state; UINT stateMask; LPTSTR pszText; int cchTextMax; };
struct NMLVDISPINFO { NMHDR hdr; LVITEM item; };
struct NMCUSTOMDRAW { NMHDR hdr; DWORD dwDrawStage; HDC hdc; CRect rc; DWORD_PTR dwItemSpec; UINT uItemState; };
struct NMLVCUSTOMDRAW { NMCUSTOMDRAW nmcd; COLORREF clrText; COLORREF clrTextBk; };
struct INITCOMMONCONTROLSEX { DWORD dwSize; DWORD dwICC; };
static inline BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX *) { return TRUE; }
static inline BOOL AdjustWindowRectEx(CRect *, DWORD, BOOL, DWORD) { return TRUE; }
BOOL SystemParametersInfo(UINT action, UINT param, void *out, UINT winIni);

class CFont
{
public:
	BOOL CreateFontIndirect(const LOGFONT *lf) { log = *lf; return TRUE; }
	int GetLogFont(LOGFONT *lf) { *lf = log; return 1; }
	LOGFONT log = { -12, 400 };
};
class CBrush
{
public:
	CBrush() {}
	explicit CBrush(COLORREF c) : color(c) {}
	BOOL CreateSolidBrush(COLORREF c) { color = c; return TRUE; }
	HGDIOBJ GetSafeHandle() const { return (HGDIOBJ)this; }
	COLORREF color = 0;
};
class CPen { public: CPen(int, int, COLORREF c) : color(c) {} COLORREF color; };
class CBitmap { public: BOOL CreateCompatibleBitmap(class CDC *, int, int) { return TRUE; } };
class CScrollBar;

// ---------------------------------------------------------------- 창
class CDataExchange { public: class CDialogEx *dlg = nullptr; };

class CWnd
{
public:
	virtual ~CWnd() {}
	BOOL SetWindowPos(const CWnd *, int x, int y, int cx, int cy, UINT) { rect = CRect(x, y, x + cx, y + cy); return TRUE; }
	BOOL ShowWindow(int cmd) { BOOL was = visible; visible = (cmd != SW_HIDE) ? TRUE : FALSE; return was; }
	BOOL EnableWindow(BOOL e) { BOOL was = !enabled; enabled = e; return was; }
	BOOL IsWindowVisible() const { return visible; }
	void GetWindowText(CString &s) const { s = text; }
	void SetWindowText(const wchar_t *s) { text = s; }
	void SetFont(CFont *f, BOOL = TRUE) { font = f; }
	CFont *GetFont() const { return font ? font : &defaultFont; }
	void GetClientRect(CRect *r) const { *r = CRect(0, 0, rect.Width(), rect.Height()); }
	void GetWindowRect(CRect *r) const { *r = rect; }
	void ScreenToClient(CPoint *p) const { if (!isTop) { p->x -= rect.left; p->y -= rect.top; } }
	void ScreenToClient(CRect *r) const { if (!isTop) { r->left -= rect.left; r->right -= rect.left; r->top -= rect.top; r->bottom -= rect.top; } }
	void Invalidate(BOOL = TRUE) { invalidated++; }
	HWND GetSafeHwnd() const { return (HWND)this; }
	int GetDlgCtrlID() const { return id; }
	CWnd *SetCapture() { CWnd *old = s_Capture; s_Capture = this; return old; }
	static CWnd *GetCapture() { return s_Capture; }
	static CWnd *GetFocus() { return s_Focus; }
	DWORD GetStyle() const { return 0; }
	DWORD GetExStyle() const { return 0; }
	virtual CWnd *GetDlgItem(int ctrlId) { (void)ctrlId; return nullptr; }

	// 기본 처리 함수 (MFC CWnd 의 Default 처리 자리)
	void OnSize(UINT, int, int) {}
	void OnLButtonDown(UINT, CPoint) {}
	void OnLButtonUp(UINT, CPoint) {}
	void OnLButtonDblClk(UINT, CPoint) {}
	void OnMouseMove(UINT, CPoint) {}
	void OnCaptureChanged(CWnd *) {}

	int			id = 0;
	int			kind = 0;							// 검증 그림용 컨트롤 종류 (harness 가 rc 에서 채움)
	CRect		rect;
	CString		text;
	BOOL		visible = TRUE;
	BOOL		enabled = TRUE;
	BOOL		isTop = FALSE;
	CFont		*font = nullptr;
	int			invalidated = 0;
	static CWnd		*s_Capture;
	static CWnd		*s_Focus;
	static CFont	defaultFont;
};
static inline BOOL ReleaseCapture() { CWnd::s_Capture = nullptr; return TRUE; }

class CStatic : public CWnd {};
class CScrollBar : public CWnd {};
class CSliderCtrl : public CWnd
{
public:
	void SetRange(int lo, int hi, BOOL = FALSE) { minPos = lo; maxPos = hi; }
	void SetLineSize(int) {}
	void SetPageSize(int) {}
	void SetPos(int p) { pos = p; }
	int GetPos() const { return pos; }
	int minPos = 0, maxPos = 0, pos = 0;
};
class CComboBox : public CWnd
{
public:
	int AddString(const wchar_t *s) { items.push_back(CString(s)); return (int)items.size() - 1; }
	int SetCurSel(int n) { sel = n; text = (n >= 0 && n < (int)items.size()) ? items[(size_t)n] : CString(); return n; }
	int GetCurSel() const { return sel; }
	// 펼침 목록 콤보는 창 높이 = 닫힌 칸 높이 (SetWindowPos 에 준 높이는 펼친 목록까지). 9 pt 글꼴이면 24 px
	void GetWindowRect(CRect *r) const { *r = rect; r->bottom = r->top + 24; }
	std::vector<CString> items;
	int sel = -1;
};
class CListCtrl : public CWnd
{
public:
	DWORD SetExtendedStyle(DWORD s) { DWORD o = exStyle; exStyle = s; return o; }
	int InsertColumn(int n, const wchar_t *name, int fmt, int width) { columns.push_back({ CString(name), fmt, width }); return n; }
	int GetStringWidth(const wchar_t *s) const { return (int)std::lround(rec::textWidth(s, 12.0)); }
	BOOL SetItemCountEx(int n, DWORD) { itemCount = n; return TRUE; }
	BOOL DeleteAllItems() { cells.clear(); itemCount = 0; return TRUE; }
	int InsertItem(int n, const wchar_t *s) { cells[{ n, 0 }] = CString(s); itemCount = std::max(itemCount, n + 1); return n; }
	BOOL SetItemText(int n, int c, const wchar_t *s) { cells[{ n, c }] = CString(s); return TRUE; }
	struct Column { CString name; int fmt; int width; };
	std::vector<Column> columns;
	std::map<std::pair<int, int>, CString> cells;
	DWORD exStyle = 0;
	int itemCount = 0;
};

// ---------------------------------------------------------------- DC
class CDC
{
public:
	virtual ~CDC() {}
	BOOL CreateCompatibleDC(CDC *) { return TRUE; }
	CBitmap *SelectObject(CBitmap *b) { return b; }
	CFont *SelectObject(CFont *f) { CFont *o = curFont; curFont = f; return o; }
	CPen *SelectObject(CPen *p) { CPen *o = curPen; curPen = p; return o; }
	CBrush *SelectObject(CBrush *b) { CBrush *o = curBrush; curBrush = b; return o; }
	BOOL BitBlt(int, int, int, int, CDC *, int, int, DWORD) { return TRUE; }
	HDC GetSafeHdc() { return (HDC)this; }
	int GetDeviceCaps(int) const { return 96; }
	BOOL GetTextMetrics(TEXTMETRIC *m) const { m->tmHeight = 16; return TRUE; }
	CSize GetTextExtent(const CString &s) const { CSize z; z.cx = (LONG)std::lround(rec::textWidth(s.GetString(), 12.0)); z.cy = 16; return z; }
	COLORREF SetTextColor(COLORREF c) { COLORREF o = textColor; textColor = c; return o; }
	COLORREF SetBkColor(COLORREF c) { COLORREF o = bkColor; bkColor = c; return o; }
	void FillSolidRect(const CRect *r, COLORREF c);
	void FillSolidRect(int x, int y, int cx, int cy, COLORREF c) { CRect r(x, y, x + cx, y + cy); FillSolidRect(&r, c); }
	int FrameRect(const CRect *r, CBrush *b);
	BOOL RoundRect(const CRect *r, POINT);
	CFont *curFont = nullptr;
	CPen *curPen = nullptr;
	CBrush *curBrush = nullptr;
	COLORREF textColor = 0;
	COLORREF bkColor = 0xFFFFFF;
};
class CPaintDC : public CDC { public: explicit CPaintDC(CWnd *) {} };
class CClientDC : public CDC { public: explicit CClientDC(CWnd *) {} };

// ---------------------------------------------------------------- 대화상자, 응용 프로그램
class CDialogEx : public CWnd
{
public:
	CDialogEx(UINT dialogId, CWnd * = nullptr) : idd(dialogId) { isTop = TRUE; }
	virtual void DoDataExchange(CDataExchange *) {}
	virtual BOOL OnInitDialog();
	virtual void OnOK() {}
	virtual void OnCancel() {}
	intptr_t DoModal() { return IDCANCEL; }
	void EndDialog(int r) { result = r; }
	CWnd *GetDlgItem(int ctrlId) override;
	void SetDlgItemText(int ctrlId, const wchar_t *s) { GetDlgItem(ctrlId)->text = s; }
	int GetDlgItemText(int ctrlId, CString &s) { s = GetDlgItem(ctrlId)->text; return s.GetLength(); }
	void CheckDlgButton(int ctrlId, UINT c) { GetDlgItem(ctrlId)->kind = (GetDlgItem(ctrlId)->kind & 0xFF) | (c ? 0x100 : 0); }
	UINT IsDlgButtonChecked(int ctrlId) { return (GetDlgItem(ctrlId)->kind & 0x100) ? BST_CHECKED : BST_UNCHECKED; }
	void GotoDlgCtrl(CWnd *w) { s_Focus = w; }
	UINT_PTR SetTimer(UINT_PTR timerId, UINT, void *) { timers[timerId] = 1; return timerId; }
	BOOL KillTimer(UINT_PTR timerId) { timers.erase(timerId); return TRUE; }

	void OnSize(UINT, int, int) {}
	void OnGetMinMaxInfo(MINMAXINFO *) {}
	HBRUSH OnCtlColor(CDC *, CWnd *, UINT) { return nullptr; }
	void OnDestroy() {}
	void OnTimer(UINT_PTR) {}
	void OnHScroll(UINT, UINT, CScrollBar *) {}
	BOOL OnMouseWheel(UINT, short, CPoint) { return TRUE; }

	// 검증 프로그램이 rc 의 컨트롤을 미리 넣고 (id -> 종류, 글자), DDX_Control 이 멤버 컨트롤로 바꿔 넣음
	void StubAddControl(int ctrlId, int ctrlKind, const wchar_t *ctrlText);
	void StubBind(int ctrlId, CWnd &ctrl);
	std::map<int, CWnd *> controls;
	std::vector<CWnd *> owned;
	std::map<UINT_PTR, int> timers;
	UINT idd;
	int result = 0;
};
void DDX_Control(CDataExchange *dx, int ctrlId, CWnd &ctrl);

class CWinApp
{
public:
	virtual ~CWinApp() {}
	virtual BOOL InitInstance() { return TRUE; }
	CWnd *m_pMainWnd = nullptr;
};

class CFileDialog : public CWnd
{
public:
	CFileDialog(BOOL, const wchar_t *, const wchar_t *name, DWORD, const wchar_t *, CWnd *) : path(name) {}
	intptr_t DoModal() { return IDCANCEL; }
	CString GetPathName() const { return path; }
	CString path;
};
int AfxMessageBox(const wchar_t *text, UINT type = MB_OK, UINT help = 0);

// ---------------------------------------------------------------- GDI+
namespace Gdiplus
{
	typedef float REAL;
	typedef int INT;
	enum StringAlignment { StringAlignmentNear = 0, StringAlignmentCenter = 1, StringAlignmentFar = 2 };
	enum SmoothingMode { SmoothingModeNone = 3, SmoothingModeAntiAlias = 4 };
	enum TextRenderingHint { TextRenderingHintClearTypeGridFit = 5 };
	enum FontStyle { FontStyleRegular = 0, FontStyleBold = 1 };
	enum Unit { UnitWorld = 0, UnitPixel = 2, UnitPoint = 3 };
	enum StringFormatFlags { StringFormatFlagsNoWrap = 0x1000 };
	enum LineJoin { LineJoinMiter = 0, LineJoinRound = 2 };
	enum CombineMode { CombineModeReplace = 0 };
	enum FillMode { FillModeAlternate = 0 };
	enum Status { Ok = 0 };

	class Color { public: Color(BYTE a, BYTE r, BYTE g, BYTE b) : A(a), R(r), G(g), B(b) {} BYTE A, R, G, B; };
	struct PointF { REAL X, Y; PointF(REAL x = 0, REAL y = 0) : X(x), Y(y) {} };
	struct Point { INT X, Y; Point(INT x = 0, INT y = 0) : X(x), Y(y) {} };
	struct RectF { REAL X, Y, Width, Height; RectF(REAL x = 0, REAL y = 0, REAL w = 0, REAL h = 0) : X(x), Y(y), Width(w), Height(h) {} };
	struct Rect { INT X, Y, Width, Height; Rect(INT x = 0, INT y = 0, INT w = 0, INT h = 0) : X(x), Y(y), Width(w), Height(h) {} };
	class Brush { public: virtual ~Brush() {} Color c{ 255, 0, 0, 0 }; };
	class SolidBrush : public Brush { public: explicit SolidBrush(const Color &col) { c = col; } };
	class Pen
	{
	public:
		Pen(const Color &col, REAL w = 1.0F) : c(col), width(w) {}
		Status SetDashPattern(const REAL *, INT) { dash = 1; return Ok; }
		Status SetLineJoin(LineJoin) { return Ok; }
		Color c;
		REAL width;
		int dash = 0;
	};
	class FontFamily { public: explicit FontFamily(const WCHAR *, const void * = nullptr) {} };
	class Font { public: Font(const FontFamily *, REAL s, INT style = FontStyleRegular, Unit = UnitPoint) : size(s), bold(style & FontStyleBold) {} REAL size; INT bold; };
	class StringFormat
	{
	public:
		Status SetAlignment(StringAlignment a) { ax = a; return Ok; }
		Status SetLineAlignment(StringAlignment a) { ay = a; return Ok; }
		Status SetFormatFlags(INT) { return Ok; }
		StringAlignment ax = StringAlignmentNear, ay = StringAlignmentNear;
	};
	struct GdiplusStartupInput { int version = 1; };
	static inline Status GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *, void *) { *token = 1; return Ok; }
	static inline void GdiplusShutdown(ULONG_PTR) {}

	class Graphics
	{
	public:
		explicit Graphics(HDC) {}
		Status SetSmoothingMode(SmoothingMode m) { mode = m; return Ok; }
		SmoothingMode GetSmoothingMode() const { return mode; }
		Status SetTextRenderingHint(TextRenderingHint) { return Ok; }
		Status SetClip(const Rect &r, CombineMode = CombineModeReplace);
		Status SetClip(const RectF &r, CombineMode = CombineModeReplace);
		Status ResetClip();
		Status FillRectangle(const Brush *b, const RectF &r) { return fill(b, r.X, r.Y, r.Width, r.Height); }
		Status FillRectangle(const Brush *b, REAL x, REAL y, REAL w, REAL h) { return fill(b, x, y, w, h); }
		Status FillRectangle(const Brush *b, const Rect &r) { return fill(b, (REAL)r.X, (REAL)r.Y, (REAL)r.Width, (REAL)r.Height); }
		Status FillRectangle(const Brush *b, INT x, INT y, INT w, INT h) { return fill(b, (REAL)x, (REAL)y, (REAL)w, (REAL)h); }
		Status FillRectangles(const Brush *b, const RectF *r, INT n) { for (INT i = 0; i < n; i++) fill(b, r[i].X, r[i].Y, r[i].Width, r[i].Height); return Ok; }
		Status FillRectangles(const Brush *b, const Rect *r, INT n) { for (INT i = 0; i < n; i++) fill(b, (REAL)r[i].X, (REAL)r[i].Y, (REAL)r[i].Width, (REAL)r[i].Height); return Ok; }
		Status DrawLine(const Pen *p, REAL x1, REAL y1, REAL x2, REAL y2) { return line(p, x1, y1, x2, y2); }
		Status DrawLine(const Pen *p, const PointF &a, const PointF &b) { return line(p, a.X, a.Y, b.X, b.Y); }
		Status DrawLine(const Pen *p, INT x1, INT y1, INT x2, INT y2) { return line(p, (REAL)x1, (REAL)y1, (REAL)x2, (REAL)y2); }
		Status DrawLine(const Pen *p, const Point &a, const Point &b) { return line(p, (REAL)a.X, (REAL)a.Y, (REAL)b.X, (REAL)b.Y); }
		Status DrawLines(const Pen *p, const PointF *pt, INT n);
		Status DrawLines(const Pen *p, const Point *pt, INT n);
		Status DrawRectangle(const Pen *p, const RectF &r) { return box(p, r.X, r.Y, r.Width, r.Height); }
		Status DrawRectangle(const Pen *p, REAL x, REAL y, REAL w, REAL h) { return box(p, x, y, w, h); }
		Status DrawRectangle(const Pen *p, const Rect &r) { return box(p, (REAL)r.X, (REAL)r.Y, (REAL)r.Width, (REAL)r.Height); }
		Status DrawRectangle(const Pen *p, INT x, INT y, INT w, INT h) { return box(p, (REAL)x, (REAL)y, (REAL)w, (REAL)h); }
		Status FillEllipse(const Brush *b, const RectF &r) { return ellipse(&b->c, nullptr, r.X, r.Y, r.Width, r.Height); }
		Status FillEllipse(const Brush *b, REAL x, REAL y, REAL w, REAL h) { return ellipse(&b->c, nullptr, x, y, w, h); }
		Status FillEllipse(const Brush *b, const Rect &r) { return ellipse(&b->c, nullptr, (REAL)r.X, (REAL)r.Y, (REAL)r.Width, (REAL)r.Height); }
		Status FillEllipse(const Brush *b, INT x, INT y, INT w, INT h) { return ellipse(&b->c, nullptr, (REAL)x, (REAL)y, (REAL)w, (REAL)h); }
		Status DrawEllipse(const Pen *p, const RectF &r) { return ellipse(nullptr, p, r.X, r.Y, r.Width, r.Height); }
		Status DrawEllipse(const Pen *p, REAL x, REAL y, REAL w, REAL h) { return ellipse(nullptr, p, x, y, w, h); }
		Status DrawEllipse(const Pen *p, const Rect &r) { return ellipse(nullptr, p, (REAL)r.X, (REAL)r.Y, (REAL)r.Width, (REAL)r.Height); }
		Status DrawEllipse(const Pen *p, INT x, INT y, INT w, INT h) { return ellipse(nullptr, p, (REAL)x, (REAL)y, (REAL)w, (REAL)h); }
		Status FillPolygon(const Brush *b, const PointF *pt, INT n) { return polygon(&b->c, nullptr, pt, n); }
		Status FillPolygon(const Brush *b, const PointF *pt, INT n, FillMode) { return polygon(&b->c, nullptr, pt, n); }
		Status FillPolygon(const Brush *b, const Point *pt, INT n);
		Status DrawPolygon(const Pen *p, const PointF *pt, INT n) { return polygon(nullptr, p, pt, n); }
		Status DrawPolygon(const Pen *p, const Point *pt, INT n);
		Status MeasureString(const WCHAR *s, INT, const Font *f, const PointF &, RectF *out) const
		{
			*out = RectF(0, 0, (REAL)(rec::textWidth(s, f->size) + (0.33 * f->size)), f->size * 1.33F);	// GDI+ 는 앞뒤 여백을 더해 잼
			return Ok;
		}
		Status DrawString(const WCHAR *s, INT, const Font *f, const PointF &o, const StringFormat *fmt, const Brush *b);
		Status DrawString(const WCHAR *s, INT, const Font *f, const PointF &o, const Brush *b);
	private:
		Status fill(const Brush *b, REAL x, REAL y, REAL w, REAL h);
		Status line(const Pen *p, REAL x1, REAL y1, REAL x2, REAL y2);
		Status box(const Pen *p, REAL x, REAL y, REAL w, REAL h);
		Status ellipse(const Color *fillColor, const Pen *p, REAL x, REAL y, REAL w, REAL h);
		Status polygon(const Color *fillColor, const Pen *p, const PointF *pt, INT n);
		SmoothingMode mode = SmoothingModeAntiAlias;
	};
}
