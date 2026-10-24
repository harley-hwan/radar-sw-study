// TargetSimUI 화면 검증 (리눅스): 실제 UI 소스 (대화상자, 그림, 결과 보관) 와 Core C 소스를 그대로 빌드해
// OnInitDialog -> UDP 송수신 -> 타이머 -> 배치 -> 그리기를 돌림. 배치 · 상태 · 조작을 검사하고 창 전체 그리기 호출을 JSON 으로 남김 (render.py 로 PNG)
// usage: uiview <TargetSimUI dir> <out dir> [<검증 harness runs.csv>]
#include "afx_stub.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <unistd.h>
#include <sys/stat.h>

#define private public
#define protected public
#include "Resource.h"
#include "TargetSimUIDlg.h"
#include "UiCommon.h"
#undef private
#undef protected

namespace rec
{
	extern std::vector<std::string>	g_Ops;
	extern int						nMessageBox;
}

// 컨트롤 종류 (검증 그림용)
enum { K_STATIC = 1, K_BUTTON, K_EDIT, K_COMBO, K_CHECK, K_SLIDER, K_LIST, K_VIEW };

static int g_Fail = 0;
static int g_Pass = 0;
#define CHECK(c, ...) do { if (c) { printf("  PASS: "); g_Pass++; } else { printf("  FAIL: "); g_Fail++; } printf(__VA_ARGS__); printf("\n"); } while (0)

struct RcControl { int id; int kind; std::wstring text; std::string name; };

static std::wstring widen(const std::string &s)
{
	std::vector<wchar_t> buf(s.size() + 1);
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), buf.data(), (int)buf.size());
	return std::wstring(buf.data(), (size_t)n);
}

// Resource.h 의 #define 이름 -> 값
static std::map<std::string, int> read_ids(const std::string &path)
{
	std::map<std::string, int> ids;
	std::ifstream f(path);
	std::string line;
	std::regex re("^#define\\s+(\\w+)\\s+(\\d+)");
	std::smatch m;
	while (std::getline(f, line)) if (std::regex_search(line, m, re)) ids[m[1]] = std::stoi(m[2]);
	return ids;
}

// TargetSimUI.rc 의 주 대화상자 컨트롤 (z 순서 = rc 순서)
static std::vector<RcControl> read_rc(const std::string &path, const std::map<std::string, int> &ids)
{
	std::vector<RcControl> out;
	std::ifstream f(path);
	std::string line;
	bool inDialog = false, inBody = false;
	std::regex reText("^\\s*(LTEXT|PUSHBUTTON|CONTROL)\\s+\"([^\"]*)\",(\\w+)(,\"(\\w+)\",([^,]*))?");
	std::regex reNoText("^\\s*(EDITTEXT|COMBOBOX)\\s+(\\w+),");
	std::smatch m;
	while (std::getline(f, line))
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.rfind("IDD_TARGETSIMUI_DIALOG DIALOGEX", 0) == 0) { inDialog = true; continue; }
		if (inDialog && line == "BEGIN") { inBody = true; continue; }
		if (inBody && line == "END") break;
		if (!inBody) continue;
		RcControl c{ 0, 0, L"", "" };
		if (std::regex_search(line, m, reText))
		{
			c.text = widen(m[2]); c.name = m[3];
			const std::string kw = m[1], cls = m[5], style = m[6];
			if (kw == "LTEXT") c.kind = K_STATIC;
			else if (kw == "PUSHBUTTON") c.kind = K_BUTTON;
			else if (cls == "Button") c.kind = K_CHECK;
			else if (cls == "msctls_trackbar32") c.kind = K_SLIDER;
			else if (cls == "SysListView32") c.kind = K_LIST;
			else c.kind = (style.find("SS_NOTIFY") != std::string::npos) ? K_VIEW : K_STATIC;
		}
		else if (std::regex_search(line, m, reNoText))
		{
			c.name = m[2]; c.kind = (m[1] == "EDITTEXT") ? K_EDIT : K_COMBO;
		}
		else continue;
		auto it = ids.find(c.name);
		if (it == ids.end()) { printf("  FAIL: rc control %s has no id in Resource.h\n", c.name.c_str()); g_Fail++; continue; }
		c.id = it->second;
		out.push_back(c);
	}
	return out;
}

// ---------------------------------------------------------------- 창 전체 그림 (흉내낸 표준 컨트롤 + 실제 그림 코드)
static void op_text(double x, double y, const std::wstring &s, int ax, int ay, double size, int bold, COLORREF c)
{
	std::ostringstream t;
	t << "{\"op\":\"text\",\"s\":\"" << rec::utf8(s.c_str()) << "\",\"x\":" << x << ",\"y\":" << y << ",\"ax\":" << ax << ",\"ay\":" << ay << ",\"size\":" << size
	  << ",\"bold\":" << bold << ",\"w\":" << rec::textWidth(s.c_str(), size) << ",\"c\":" << rec::col(c, 255) << "}";
	rec::op(t.str());
}
static void op_fill(double x, double y, double w, double h, COLORREF c)
{
	char t[200]; snprintf(t, sizeof(t), "{\"op\":\"fill\",\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f,\"c\":%s}", x, y, w, h, rec::col(c, 255).c_str()); rec::op(t);
}
static void op_box(double x, double y, double w, double h, COLORREF c)
{
	char t[200]; snprintf(t, sizeof(t), "{\"op\":\"box\",\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f,\"c\":%s,\"lw\":1}", x, y, w, h, rec::col(c, 255).c_str()); rec::op(t);
}
static void op_clip(const CRect &r)
{
	char t[160]; snprintf(t, sizeof(t), "{\"op\":\"clip\",\"x\":%ld,\"y\":%ld,\"w\":%d,\"h\":%d}", (long)r.left, (long)r.top, r.Width(), r.Height()); rec::op(t);
}

static void paint_window(CTargetSimUIDlg &dlg, const std::vector<RcControl> &rc, const std::string &path)
{
	CRect client;
	CPaintDC dc(&dlg);
	rec::g_Ops.clear();
	rec::offsetX = rec::offsetY = 0.0;
	dlg.GetClientRect(&client);
	dlg.OnEraseBkgnd(&dc);

	for (const RcControl &c : rc)
	{
		CWnd *w = dlg.GetDlgItem(c.id);
		const CRect &r = w->rect;
		if (!w->visible || r.Width() <= 0 || r.Height() <= 0) continue;
		const int bold = (w->font != nullptr && w->font->log.lfWeight == FW_BOLD) ? 1 : 0;
		const double size = bold ? 13.0 : 12.0;
		const COLORREF dim = w->enabled ? UI_COLOR_TEXT : UI_COLOR_TEXT_OFF;
		switch (c.kind)
		{
		case K_VIEW:
			rec::offsetX = (double)r.left; rec::offsetY = (double)r.top;
			if (w == &dlg.st_View3D) dlg.st_View3D.OnPaint();
			else if (w == &dlg.st_ProfileLat) dlg.st_ProfileLat.OnPaint();
			else if (w == &dlg.st_ProfileLon) dlg.st_ProfileLon.OnPaint();
			else if (w == &dlg.st_Plot) dlg.st_Plot.OnPaint();
			else if (w == &dlg.st_FilterPlot) dlg.st_FilterPlot.OnPaint();
			rec::offsetX = rec::offsetY = 0.0;
			rec::op("{\"op\":\"noclip\"}");
			break;
		case K_STATIC:
		{
			CDC sdc;
			dlg.OnCtlColor(&sdc, w, CTLCOLOR_STATIC);
			op_clip(r);
			std::wstring all = w->text.w, line;
			std::wstringstream ss(all);
			double y = (all.find(L'\n') == std::wstring::npos) ? (r.top + 0.5 * r.Height()) : (double)r.top;
			const int ay = (all.find(L'\n') == std::wstring::npos) ? 1 : 0;
			while (std::getline(ss, line, L'\n'))
			{
				if (!line.empty() && line.back() == L'\r') line.pop_back();
				op_text(r.left, y, line, 0, ay, size, bold, sdc.textColor);
				y += 16.0;
			}
			rec::op("{\"op\":\"noclip\"}");
			break;
		}
		case K_BUTTON:
			op_fill(r.left, r.top, r.Width(), r.Height(), RGB(248, 249, 250));
			op_box(r.left, r.top, r.Width() - 1, r.Height() - 1, RGB(173, 181, 189));
			op_text(r.left + 0.5 * r.Width(), r.top + 0.5 * r.Height(), w->text.w, 1, 1, 12.0, 0, dim);
			break;
		case K_EDIT:
			op_fill(r.left, r.top, r.Width(), r.Height(), RGB(255, 255, 255));
			op_box(r.left, r.top, r.Width() - 1, r.Height() - 1, RGB(122, 122, 122));
			op_text(r.left + 4, r.top + 0.5 * r.Height(), w->text.w, 0, 1, 12.0, 0, UI_COLOR_TEXT);
			break;
		case K_COMBO:
		{
			const double h = 24.0;
			op_fill(r.left, r.top, r.Width(), h, RGB(255, 255, 255));
			op_box(r.left, r.top, r.Width() - 1, h - 1, RGB(122, 122, 122));
			op_text(r.left + 5, r.top + 0.5 * h, w->text.w, 0, 1, 12.0, 0, dim);
			op_text(r.right - 14, r.top + 0.5 * h, L"v", 1, 1, 11.0, 0, UI_COLOR_TEXT_SUB);
			break;
		}
		case K_CHECK:
		{
			const bool on = (w->kind & 0x100) != 0;
			const double y = r.top + 0.5 * r.Height();
			op_fill(r.left + 2, y - 6, 13, 13, on ? (w->enabled ? RGB(0, 95, 184) : UI_COLOR_TEXT_OFF) : RGB(255, 255, 255));
			op_box(r.left + 2, y - 6, 12, 12, RGB(80, 80, 80));
			op_text(r.left + 20, y, w->text.w, 0, 1, 12.0, 0, dim);
			break;
		}
		case K_SLIDER:
		{
			const CSliderCtrl *s = &dlg.st_ScanSlider;
			const double y = r.top + 0.5 * r.Height();
			const double f = (s->maxPos > s->minPos) ? (double)(s->pos - s->minPos) / (s->maxPos - s->minPos) : 0.0;
			op_fill(r.left + 8, y - 2, r.Width() - 16, 4, RGB(214, 214, 214));
			op_fill(r.left + 8 + f * (r.Width() - 16) - 5, y - 10, 10, 20, RGB(0, 120, 215));
			break;
		}
		case K_LIST:
		{
			CListCtrl &list = dlg.st_TrackList;
			double x = r.left;
			op_fill(r.left, r.top, r.Width(), r.Height(), RGB(255, 255, 255));
			op_box(r.left, r.top, r.Width() - 1, r.Height() - 1, UI_COLOR_BORDER);
			op_clip(r);
			for (size_t col = 0; col < list.columns.size(); col++)
			{
				const auto &cd = list.columns[col];
				op_fill(x, r.top, cd.width, 24, RGB(250, 250, 250));
				op_box(x, r.top, cd.width, 24, RGB(229, 229, 229));
				op_text(x + 6, r.top + 12, cd.name.w, 0, 1, 12.0, 0, UI_COLOR_TEXT);
				for (int row = 0; row < list.itemCount && r.top + 24 + (row + 1) * 20 <= r.bottom; row++)
				{
					NMLVDISPINFO info;
					wchar_t cell[128] = L"";
					LRESULT lr = 0;
					memset(&info, 0, sizeof(info));
					info.item.mask = LVIF_TEXT; info.item.iItem = row; info.item.iSubItem = (int)col; info.item.pszText = cell; info.item.cchTextMax = 128;
					dlg.f_OnTrackGetDispInfo(&info.hdr, &lr);
					const double ty = r.top + 24 + row * 20 + 10;
					if (cd.fmt == LVCFMT_RIGHT) op_text(x + cd.width - 6, ty, cell, 2, 1, 12.0, 0, UI_COLOR_TEXT);
					else op_text(x + 6, ty, cell, 0, 1, 12.0, 0, UI_COLOR_TEXT);
					op_box(x, r.top + 24 + row * 20, cd.width, 20, RGB(240, 240, 240));
				}
				x += cd.width;
			}
			rec::op("{\"op\":\"noclip\"}");
			break;
		}
		default:
			break;
		}
	}

	// 보이는 컨트롤과 카드의 위치 (발표자료가 그림 부분을 잘라 쓸 때. Windows 캡처 도우미도 같은 이름으로 남김)
	std::ostringstream rects;
	auto put_rect = [&rects](const std::string &name, const CRect &r) {
		rects << (rects.tellp() > 0 ? "," : "") << "\"" << name << "\":[" << r.left << "," << r.top << "," << r.Width() << "," << r.Height() << "]";
	};
	for (const RcControl &c : rc)
	{
		const CWnd *w = dlg.GetDlgItem(c.id);
		if (w->visible && w->rect.Width() > 0 && w->rect.Height() > 0) put_rect(c.name, w->rect);
	}
	put_rect("CARD_PLOT", dlg.st_CardPlot);
	put_rect("CARD_TRK", dlg.st_CardTrk);
	put_rect("CARD_LIST", dlg.st_CardList);

	std::ofstream f(path);
	f << "{\"w\":" << client.Width() << ",\"h\":" << client.Height() << ",\"rects\":{" << rects.str() << "},\"ops\":[\n";
	for (size_t i = 0; i < rec::g_Ops.size(); i++) f << rec::g_Ops[i] << (i + 1 < rec::g_Ops.size() ? ",\n" : "\n");
	f << "]}\n";
	printf("    wrote %s (%zu ops)\n", path.c_str(), rec::g_Ops.size());
}

// ---------------------------------------------------------------- 실행 보조
static int wait_run(CTargetSimUIDlg &dlg)
{
	int n;
	for (n = 0; n < 6000 && !dlg.timers.empty(); n++) { usleep(5000); dlg.OnTimer(1); }
	return dlg.timers.empty() ? 1 : 0;
}

static void set_edit(CTargetSimUIDlg &dlg, int id, const wchar_t *s) { dlg.GetDlgItem(id)->text = s; }

static bool inside(const CRect &a, const CRect &b) { return a.left >= b.left && a.top >= b.top && a.right <= b.right && a.bottom <= b.bottom; }
static bool overlap(const CRect &a, const CRect &b) { return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom; }

static std::map<std::string, std::map<std::string, std::string>> read_runs(const std::string &path)
{
	std::map<std::string, std::map<std::string, std::string>> out;
	std::ifstream f(path);
	std::string line, cell;
	std::vector<std::string> head;
	if (!std::getline(f, line)) return out;
	std::stringstream hs(line);
	while (std::getline(hs, cell, ',')) head.push_back(cell);
	while (std::getline(f, line))
	{
		std::stringstream ls(line);
		std::map<std::string, std::string> row;
		for (size_t i = 0; std::getline(ls, cell, ',') && i < head.size(); i++) row[head[i]] = cell;
		out[row["run"]] = row;
	}
	return out;
}

int main(int argc, char **argv)
{
	const std::string ui = (argc > 1) ? argv[1] : ".";
	const std::string out = (argc > 2) ? argv[2] : "out";
	const auto runs = read_runs((argc > 3) ? argv[3] : "");
	const auto ids = read_ids(ui + "/Resource.h");
	const auto rc = read_rc(ui + "/TargetSimUI.rc", ids);
	static CTargetSimUIDlg dlg;

	mkdir(out.c_str(), 0755);
	printf("[rc] %zu controls in IDD_TARGETSIMUI_DIALOG\n", rc.size());
	for (const RcControl &c : rc) dlg.StubAddControl(c.id, c.kind, c.text.c_str());

	// ---------------------------------------------------------------- 시작: 대함, 과제 값
	printf("[start] OnInitDialog (대함 표적, 과제 값)\n");
	dlg.OnInitDialog();
	CHECK(wait_run(dlg), "UDP 송수신이 끝나 타이머가 멈춤");
	CHECK(dlg.st_TrackResult.f_GetScanNum() == 1160 && dlg.st_TrackResult.st_Status.nLostNum == 0, "1160 스캔 받음 (손실 %d)", dlg.st_TrackResult.st_Status.nLostNum);
	CHECK(dlg.nCurScan == 1159 && dlg.st_ScanSlider.pos == 1159, "따라가기: 마지막 스캔 1159 를 보임");
	{
		ST_TrkStats s; dlg.st_TrackResult.f_GetStats(1159, &s);
		printf("    stats: ids %d created %d deleted %d diverged %d rms %.3f max %.3f nis %.4f gate-out %.3f %%\n", s.nTrackIdNum, s.nCreatedNum, s.nDeletedNum,
			s.nDivergedNum, s.rmsError, s.maxError, s.meanNis, s.gateOutPct);
		CHECK(s.nTrackIdNum == 1 && s.nDeletedNum == 0 && s.nDivergedNum == 0, "대함 과제 값: 트랙 하나 유지, 발산 판정 없음");
		if (runs.count("ship_given"))
		{
			const auto &r = runs.at("ship_given");
			CHECK(fabs(s.rmsError - std::stod(r.at("rms"))) < 1e-3 && fabs(s.meanNis - std::stod(r.at("mean_nis"))) < 1e-3,
				"화면 누적 값 = 검증 harness 값 (RMS %.3f / %s m, NIS %.4f / %s)", s.rmsError, r.at("rms").c_str(), s.meanNis, r.at("mean_nis").c_str());
		}
	}
	CHECK(dlg.GetDlgItem(IDC_PRM_SIGAZ)->text == CString(L"0.3") && dlg.GetDlgItem(IDC_PRM_Q)->text == CString(L"0.5") && dlg.st_TrackResult.st_Param.singerTau == 10.0,
		"입력 칸 = 과제 값 (σaz 0.3, q 0.5, τ 10)");

	// ---------------------------------------------------------------- 배치
	printf("[layout] 1680 x 1000 창\n");
	{
		CRect client; dlg.GetClientRect(&client);
		const std::vector<int> card = { IDC_TRK_TITLE, IDC_TRK_SCORE, IDC_PRM_R_LABEL, IDC_PRM_SIGR_LABEL, IDC_PRM_SIGR, IDC_PRM_SIGAZ_LABEL, IDC_PRM_SIGAZ, IDC_PRM_SIGEL_LABEL,
			IDC_PRM_SIGEL, IDC_PRM_SINGER_LABEL, IDC_PRM_Q_LABEL, IDC_PRM_Q, IDC_PRM_TAU_LABEL, IDC_PRM_TAU, IDC_PRM_GIVEN, IDC_PRM_TUNED, IDC_TRK_GAP_LABEL,
			IDC_TRK_RESEND, IDC_TRK_SUMMARY_LABEL, IDC_TRK_SUMMARY };
		bool okIn = true, okOver = true, okFit = true;
		for (size_t i = 0; i < card.size(); i++)
		{
			const CRect &a = dlg.GetDlgItem(card[i])->rect;
			if (!inside(a, dlg.st_CardTrk)) { okIn = false; printf("    outside card: id %d (%ld %ld %ld %ld)\n", card[i], a.left, a.top, a.right, a.bottom); }
			for (size_t j = i + 1; j < card.size(); j++)
				if (overlap(a, dlg.GetDlgItem(card[j])->rect)) { okOver = false; printf("    overlap: id %d and %d\n", card[i], card[j]); }
		}
		CHECK(okIn, "필터 카드 컨트롤이 모두 카드 안");
		CHECK(okOver, "필터 카드 컨트롤끼리 겹치지 않음");
		{
			// 콤보 창 높이에는 펼친 목록이 들어 있어 닫힌 칸 (한 줄) 만 봄
			CRect combo = dlg.GetDlgItem(IDC_TRK_GAP)->rect;
			combo.bottom = combo.top + 24;
			CHECK(inside(combo, dlg.st_CardTrk) && !overlap(combo, dlg.GetDlgItem(IDC_TRK_RESEND)->rect) && !overlap(combo, dlg.GetDlgItem(IDC_TRK_GAP_LABEL)->rect),
				"송신 간격 선택 (닫힌 칸) 이 카드 안, 이름표 · 적용 단추와 겹치지 않음");
		}
		for (const RcControl &c : rc)
		{
			CWnd *w = dlg.GetDlgItem(c.id);
			if ((c.kind == K_STATIC || c.kind == K_BUTTON || c.kind == K_CHECK) && w->text.GetLength() > 0 && w->text.w.find(L'\n') == std::wstring::npos && c.id != IDC_SCN_NAME
				&& c.id != IDC_TRK_LIST_TITLE)
			{
				const double need = rec::textWidth(w->text.GetString(), (w->font && w->font->log.lfWeight == FW_BOLD) ? 13.0 : 12.0) + ((c.kind == K_CHECK) ? 20.0 : 0.0);
				if (need > w->rect.Width() + 0.5) { okFit = false; printf("    text does not fit: %s \"%s\" need %.0f px, have %d\n", c.name.c_str(), rec::utf8(w->text.GetString()).c_str(), need, w->rect.Width()); }
			}
		}
		CHECK(okFit, "이름표 · 단추 · 체크 상자 글자가 칸에 들어감 (글자 폭 근사)");
		{
			int nLine = 0; std::wstring t = dlg.GetDlgItem(IDC_TRK_SUMMARY)->text.w; for (wchar_t ch : t) nLine += (ch == L'\n');
			CHECK(nLine + 1 == 9 && (nLine + 1) * 16 <= dlg.GetDlgItem(IDC_TRK_SUMMARY)->rect.Height(), "요약 9 줄이 높이 안 (%d px)", dlg.GetDlgItem(IDC_TRK_SUMMARY)->rect.Height());
		}
		const CRect &v3 = dlg.st_View3D.rect, &la = dlg.st_ProfileLat.rect, &lo = dlg.st_ProfileLon.rect;
		CHECK(inside(v3, dlg.st_CardPlot) && inside(la, dlg.st_CardPlot) && inside(lo, dlg.st_CardPlot) && !overlap(v3, la) && !overlap(v3, lo) && !overlap(la, lo),
			"3D + 2D: 3D (%dx%d), 위도-고도 (%dx%d), 경도-고도 (%dx%d) 가 그림 카드 안에 겹치지 않음", v3.Width(), v3.Height(), la.Width(), la.Height(), lo.Width(), lo.Height());
		CHECK(dlg.st_View3D.visible && dlg.st_ProfileLat.visible && dlg.st_ProfileLon.visible && !dlg.st_Plot.visible && !dlg.st_FilterPlot.visible, "3D + 2D 보기에서 보이는 그림");
		CHECK(inside(dlg.st_CardTrk, client) && inside(dlg.st_CardList, client) && !overlap(dlg.st_CardTrk, dlg.st_CardList), "아래 두 카드가 창 안, 겹치지 않음");
	}
	paint_window(dlg, rc, out + "/ui_ship_given_all.json");
	dlg.st_ViewMode.SetCurSel(UI_VIEW_PERF);
	dlg.f_OnViewModeChanged();
	paint_window(dlg, rc, out + "/ui_ship_given_perf.json");
	dlg.st_ViewMode.SetCurSel(UI_VIEW_ALL);
	dlg.f_OnViewModeChanged();

	// ---------------------------------------------------------------- 조작
	printf("[interaction]\n");
	{
		CTrackView3D &v = dlg.st_View3D;
		const double yaw0 = v.viewYaw, pitch0 = v.viewPitch;
		const CPoint p0(v.rect.left + 200, v.rect.top + 200), p1(v.rect.left + 260, v.rect.top + 180);
		dlg.OnMouseWheel(0, 240, CPoint(v.rect.left + 100, v.rect.top + 100));
		CHECK(fabs(v.zoom - 1.5625) < 1e-9, "3D 휠 두 칸: 1.25^2 배 (%.4f)", v.zoom);
		v.OnLButtonDown(0, CPoint(200, 200)); v.OnMouseMove(0, CPoint(260, 180)); v.OnLButtonUp(0, CPoint(260, 180));
		(void)p0; (void)p1;
		CHECK(fabs(v.viewYaw - (yaw0 - f_Deg_To_Rad(0.4 * 60))) < 1e-9 && fabs(v.viewPitch - (pitch0 - f_Deg_To_Rad(0.4 * 20))) < 1e-9, "3D 끌기: 방위 -24°, 내려다보는 각 -8°");
		v.OnLButtonDblClk(0, CPoint(0, 0));
		CHECK(v.zoom == 1.0 && v.viewYaw == yaw0 && v.viewPitch == pitch0, "3D 두 번 클릭: 처음 보기");
	}
	{
		CProfilePlot &v = dlg.st_ProfileLat;
		v.OnPaint();
		const CRect a = v.st_Area;
		const double px = (double)(a.left + (3 * a.Width()) / 10), py = (double)(a.top + (7 * a.Height()) / 10);	// 정수 픽셀 (휠 커서와 같은 자리)
		const double ac = v.centerAcross + ((px - (a.left + 0.5 * a.Width())) / a.Width()) * v.spanAcross;
		const double uc = v.centerUp - ((py - (a.top + 0.5 * a.Height())) / a.Height()) * v.spanUp;
		dlg.OnMouseWheel(0, 360, CPoint(v.rect.left + (LONG)px, v.rect.top + (LONG)py));
		v.OnPaint();
		ST_PlotPoint q = { 0.0, ac, uc };
		double x = 0, y = 0;
		v.f_ToPixel(&q, &x, &y);
		CHECK(fabs(v.zoom - 1.953125) < 1e-9 && fabs(x - px) < 1e-6 && fabs(y - py) < 1e-6, "위도-고도 휠: 커서 아래 지점이 제자리 (%.2f, %.2f) -> (%.6f, %.6f)", px, py, x, y);
		v.OnLButtonDown(0, CPoint(50, 50)); v.OnMouseMove(0, CPoint(80, 50)); v.OnLButtonUp(0, CPoint(80, 50));
		CHECK(v.panAcross < 0.0, "위도-고도 끌기: 오른쪽으로 끌면 보이는 범위가 남쪽으로");
		v.OnLButtonDblClk(0, CPoint(0, 0));
		CHECK(v.zoom == 1.0 && v.panAcross == 0.0 && v.panUp == 0.0, "위도-고도 두 번 클릭: 처음 보기");
	}
	{
		const int nBox = rec::nMessageBox;
		const int nScan = dlg.st_TrackResult.f_GetScanNum();
		set_edit(dlg, IDC_PRM_Q, L"abc");
		dlg.f_OnApplyClicked();
		CHECK(rec::nMessageBox == nBox + 1 && dlg.st_TrackResult.f_GetScanNum() == nScan && CWnd::s_Focus == dlg.GetDlgItem(IDC_PRM_Q), "q = abc: 알림, 다시 실행하지 않음, q 칸으로");
		set_edit(dlg, IDC_PRM_Q, L"-1");
		dlg.f_OnApplyClicked();
		set_edit(dlg, IDC_PRM_TAU, L"0");
		set_edit(dlg, IDC_PRM_Q, L"0.5");
		dlg.f_OnApplyClicked();
		set_edit(dlg, IDC_PRM_TAU, L"10");
		set_edit(dlg, IDC_PRM_SIGAZ, L"31");
		dlg.f_OnApplyClicked();
		set_edit(dlg, IDC_PRM_SIGAZ, L" 0.3 ");
		CHECK(rec::nMessageBox == nBox + 4, "q -1, τ 0, σaz 31 도 알림 (모두 4 번)");
		CWnd::s_Focus = dlg.GetDlgItem(IDC_SCAN_SLIDER);
		dlg.OnOK();
		CHECK(dlg.timers.empty(), "슬라이더에서 Enter: 다시 실행하지 않음");
		set_edit(dlg, IDC_PRM_Q, L"0");
		CWnd::s_Focus = dlg.GetDlgItem(IDC_PRM_Q);
		dlg.OnOK();
		CHECK(!dlg.timers.empty() && dlg.st_TrackResult.st_Param.singerQ == 0.0 && dlg.st_TrackResult.st_Param.sigmaAzimuth == 0.3, "q 칸에서 Enter: q = 0 (Q = 0) 허용, 앞뒤 공백 무시하고 적용");
		CHECK(wait_run(dlg), "q = 0 실행 끝");
		{ ST_TrkStats s; dlg.st_TrackResult.f_GetStats(1159, &s); printf("    q = 0: rms %.1f m, nis %.2f, diverged %d\n", s.rmsError, s.meanNis, s.nDivergedNum); }
	}

	// ---------------------------------------------------------------- 대공 표적: 과제 값, 조정 값
	printf("[air] 시나리오 바꾸기, 과제 값 / 조정 값\n");
	dlg.st_ScnCombo.SetCurSel(SCN_AIR);
	dlg.f_OnScenarioChanged();
	dlg.f_OnGivenClicked();
	CHECK(wait_run(dlg), "대공 과제 값 실행 끝");
	{
		ST_TrkStats s; dlg.st_TrackResult.f_GetStats(1159, &s);
		printf("    stats: ids %d diverged %d rms %.3f max %.3f nis %.4f gate-out %.3f %%\n", s.nTrackIdNum, s.nDivergedNum, s.rmsError, s.maxError, s.meanNis, s.gateOutPct);
		CHECK(dlg.st_Result.f_GetPoint(0, 1)->up == 2000.0 && s.nDivergedNum > 0, "대공 (고도 2000 m) 과제 값: 발산 판정 %d 번", s.nDivergedNum);
		if (runs.count("air_given"))
		{
			const auto &r = runs.at("air_given");
			CHECK(fabs(s.rmsError - std::stod(r.at("rms"))) < 1e-3 && s.nDivergedNum == std::stoi(r.at("divergence")), "화면 누적 값 = 검증 harness 값 (RMS %.3f / %s m, 발산 %d / %s)",
				s.rmsError, r.at("rms").c_str(), s.nDivergedNum, r.at("divergence").c_str());
		}
	}
	dlg.f_ShowScan(300);
	paint_window(dlg, rc, out + "/ui_air_given_all_s300.json");
	dlg.f_ShowScan(1159);
	paint_window(dlg, rc, out + "/ui_air_given_all.json");
	dlg.st_ViewMode.SetCurSel(UI_VIEW_PERF);
	dlg.f_OnViewModeChanged();
	CHECK(dlg.st_FilterPlot.visible && !dlg.st_View3D.visible && !dlg.GetDlgItem(IDC_SHOW_TRUE)->enabled, "추적 성능 보기: 표시 체크 상자 끔");
	paint_window(dlg, rc, out + "/ui_air_given_perf.json");
	dlg.st_ViewMode.SetCurSel(UI_VIEW_MAP);
	dlg.f_OnViewModeChanged();
	paint_window(dlg, rc, out + "/ui_air_given_map.json");
	dlg.st_ViewMode.SetCurSel(UI_VIEW_2D);
	dlg.f_OnViewModeChanged();
	CHECK(dlg.st_ProfileLat.visible && dlg.st_ProfileLon.visible && dlg.st_ProfileLat.isShowLegend == 1 && dlg.st_ProfileLon.isShowLegend == 0, "2D 보기: 두 단면, 범례는 위도-고도에");
	paint_window(dlg, rc, out + "/ui_air_given_2d.json");
	dlg.st_ViewMode.SetCurSel(UI_VIEW_3D);
	dlg.f_OnViewModeChanged();
	paint_window(dlg, rc, out + "/ui_air_given_3d.json");

	dlg.f_OnTunedClicked();
	CHECK(dlg.GetDlgItem(IDC_PRM_Q)->text == CString(L"20") && dlg.GetDlgItem(IDC_PRM_SIGEL)->text == CString(L"0.5"), "조정 값 단추: 대공 q 20, σ 0.5");
	CHECK(wait_run(dlg), "대공 조정 값 실행 끝");
	{
		ST_TrkStats s; dlg.st_TrackResult.f_GetStats(1159, &s);
		printf("    stats: ids %d diverged %d rms %.3f max %.3f nis %.4f gate-out %.3f %%\n", s.nTrackIdNum, s.nDivergedNum, s.rmsError, s.maxError, s.meanNis, s.gateOutPct);
		CHECK(s.nTrackIdNum == 1 && s.nDivergedNum == 0, "대공 조정 값: 트랙 하나 유지, 발산 판정 없음");
	}
	dlg.st_ViewMode.SetCurSel(UI_VIEW_ALL);
	dlg.f_OnViewModeChanged();
	dlg.f_ShowScan(300);
	paint_window(dlg, rc, out + "/ui_air_tuned_all_s300.json");
	dlg.f_ShowScan(1159);
	paint_window(dlg, rc, out + "/ui_air_tuned_all.json");
	dlg.st_ViewMode.SetCurSel(UI_VIEW_PERF);
	dlg.f_OnViewModeChanged();
	paint_window(dlg, rc, out + "/ui_air_tuned_perf.json");

	// 대함 조정 값, CSV
	dlg.st_ScnCombo.SetCurSel(SCN_SHIP);
	dlg.f_OnScenarioChanged();
	dlg.f_OnTunedClicked();
	CHECK(wait_run(dlg), "대함 조정 값 실행 끝");
	dlg.st_ViewMode.SetCurSel(UI_VIEW_ALL);
	dlg.f_OnViewModeChanged();
	paint_window(dlg, rc, out + "/ui_ship_tuned_all.json");
	dlg.st_ViewMode.SetCurSel(UI_VIEW_PERF);
	dlg.f_OnViewModeChanged();
	paint_window(dlg, rc, out + "/ui_ship_tuned_perf.json");
	{
		const std::string path = out + "/ui_ship_tuned_track.csv";
		CHECK(dlg.st_TrackResult.f_WriteCsv(CString(widen(path).c_str())) == 1, "CSV 저장");
		std::ifstream f(path); std::string line; int n = 0; bool hdr = false;
		while (std::getline(f, line)) { if (n == 1) hdr = line.rfind("scan,time_s,track_id", 0) == 0; n++; }
		CHECK(hdr && n >= 2 + 1150, "CSV: 파라미터 줄 + 머리 줄 + 트랙 %d 줄", n - 2);
	}

	printf("PASS %d, FAIL %d\n", g_Pass, g_Fail);
	return g_Fail ? 2 : 0;
}
