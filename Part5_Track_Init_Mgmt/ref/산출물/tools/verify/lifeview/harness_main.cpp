// TargetSimUI 트랙 수명 그림 (CTrackLifeView) 검증: 실제 소스 (extract.py 가 만든 life_tu.cpp) 를 data/tracks.csv 로 돌려
// 줄 · 칸 · 구간 · 휠 · 손실 스캔 · 머리 줄 배치를 검사하고, 그리기 호출을 JSON 으로 남김 (render.py 로 미리보기 PNG).
// usage: lifeview <data dir> <out dir>
#include "afx_stub.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#define private public
#define protected public
#include "UiCommon.h"
#include "TrajectoryPlot.h"

// ---------------------------------------------------------------- recorder
static std::vector<std::string> g_Ops;

namespace rec
{
	void op(const std::string &json) { g_Ops.push_back(json); }
	std::string utf8(const wchar_t *s)
	{
		std::string o;
		for (; *s; s++)
		{
			unsigned c = (unsigned)*s;
			if (c < 0x80) { if (c == '"' || c == '\\') o += '\\'; o += (char)c; }
			else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
			else { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
		}
		return o;
	}
}

namespace Gdiplus
{
	std::string col(const Color &c)
	{
		char b[32];
		snprintf(b, sizeof(b), "\"#%02x%02x%02x\",%d", c.R, c.G, c.B, c.A);
		return b;
	}
	// Malgun Gothic 에 가까운 글자 폭 (em): 한글 0.9, 공백 0.28, 그 밖 0.56
	REAL textWidth(const WCHAR *s, REAL size)
	{
		REAL w = 0;
		for (; *s; s++) w += ((*s >= 0x1100) ? 0.9F : ((*s == L' ') ? 0.28F : 0.56F)) * size;
		return w + 0.33F * size;                                            // GDI+ MeasureString 의 앞뒤 여백
	}
	Status Graphics::fill(const Brush *b, REAL x, REAL y, REAL w, REAL h)
	{
		char t[160]; snprintf(t, sizeof(t), "{\"op\":\"fill\",\"x\":%.2f,\"y\":%.2f,\"w\":%.2f,\"h\":%.2f,\"c\":[%s]}", x, y, w, h, col(b->c).c_str());
		rec::op(t); return Ok;
	}
	Status Graphics::line(const Pen *p, REAL x1, REAL y1, REAL x2, REAL y2)
	{
		char t[160]; snprintf(t, sizeof(t), "{\"op\":\"line\",\"x1\":%.2f,\"y1\":%.2f,\"x2\":%.2f,\"y2\":%.2f,\"c\":[%s]}", x1, y1, x2, y2, col(p->c).c_str());
		rec::op(t); return Ok;
	}
	Status Graphics::box(const Pen *p, REAL x, REAL y, REAL w, REAL h)
	{
		char t[160]; snprintf(t, sizeof(t), "{\"op\":\"box\",\"x\":%.2f,\"y\":%.2f,\"w\":%.2f,\"h\":%.2f,\"c\":[%s]}", x, y, w, h, col(p->c).c_str());
		rec::op(t); return Ok;
	}
	Status Graphics::DrawString(const WCHAR *s, INT, const Font *f, const PointF &o, const StringFormat *fmt, const Brush *b)
	{
		std::ostringstream t;
		t << "{\"op\":\"text\",\"s\":\"" << rec::utf8(s) << "\",\"x\":" << o.X << ",\"y\":" << o.Y << ",\"ax\":" << fmt->ax << ",\"ay\":" << fmt->ay
		  << ",\"size\":" << f->size << ",\"bold\":" << f->bold << ",\"w\":" << textWidth(s, f->size) << ",\"c\":[" << col(b->c) << "]}";
		rec::op(t.str()); return Ok;
	}
}

// ---------------------------------------------------------------- data
static std::vector<std::string> split(const std::string &l)
{
	std::vector<std::string> v; std::stringstream ss(l); std::string x;
	while (std::getline(ss, x, ',')) v.push_back(x);
	return v;
}

// tracks.csv / scanlog.csv -> CTrackResult (같은 자리에 f_AddScan 과 같은 순서로 쌓음). lost: 받지 못한 것으로 둘 스캔
static void load(CTrackResult *tr, const std::string &dir, int lostScan)
{
	std::ifstream fs(dir + "/scanlog.csv"), ft(dir + "/tracks.csv");
	std::string l;
	std::map<int, std::vector<std::vector<std::string>>> rows;
	std::getline(ft, l);
	while (std::getline(ft, l)) { auto v = split(l); rows[std::stoi(v[0])].push_back(v); }
	std::getline(fs, l);
	int nScan = 0;
	std::vector<std::vector<std::string>> logs;
	while (std::getline(fs, l)) logs.push_back(split(l));
	nScan = (int)logs.size();
	tr->nScanTotal = nScan;
	tr->st_ScanIndex.assign((size_t)nScan, -1);
	for (auto &g : logs)
	{
		int s = std::stoi(g[0]);
		if (s == lostScan) continue;
		ST_LnkScan sc = {};
		sc.ScanNum = (UINT32)s; sc.time = std::stod(g[1]); sc.nTrackNum = (int)rows[s].size(); sc.nPlotNum = std::stoi(g[2]);
		tr->st_ScanIndex[(size_t)s] = (int)tr->st_ScanBuf.size();
		tr->st_ScanBuf.push_back(sc);
		tr->st_TrackStart.push_back((int)tr->st_TrackBuf.size());
		for (auto &v : rows[s])
		{
			ST_TrackItem it = {};
			it.st_View.TrackId = (UINT32)std::stoul(v[2]); it.st_View.TrackState = std::stoi(v[3]); it.st_View.TrackScore = std::stoi(v[4]);
			it.st_View.MemoryScore = std::stoi(v[5]); it.st_View.Assigned_Flag = std::stoi(v[6]); it.st_View.nInitScan = std::stoi(v[7]);
			it.st_View.PlotId[0] = (UINT32)std::stoul(v[8]); it.st_View.PlotId[1] = (UINT32)std::stoul(v[9]);
			it.nSource = (v[10] == v[11]) ? std::stoi(v[11]) : TRK_SOURCE_MIXED;
			tr->st_TrackBuf.push_back(it);
		}
	}
}

static int g_Fail = 0;
#define CHECK(c, ...) do { if (c) { printf("  PASS: "); } else { printf("  FAIL: "); g_Fail++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void paint(CTrackLifeView *v, const std::string &file)
{
	g_Ops.clear();
	v->OnPaint();
	std::ofstream o(file);
	o << "{\"w\":" << v->clientW << ",\"h\":" << v->clientH << ",\"ops\":[\n";
	for (size_t i = 0; i < g_Ops.size(); i++) o << g_Ops[i] << (i + 1 < g_Ops.size() ? ",\n" : "\n");
	o << "]}\n";
}

static std::string title()
{
	for (auto &s : g_Ops) if (s.find("트랙 수명  ·") != std::string::npos) return s.substr(s.find("\"s\":\"") + 5, s.find("\",\"x\"") - s.find("\"s\":\"") - 5);
	return "";
}

int main(int argc, char **argv)
{
	if (argc < 3) { printf("usage: lifeview <data dir> <out dir>\n"); return 2; }
	const std::string dir = argv[1], out = argv[2];
	std::filesystem::create_directories(out);
	CTrackResult tr;
	CTrackLifeView v;
	load(&tr, dir, -1);
	v.clientW = 1592; v.clientH = 516;                                      // 1640 x 980 창의 그림 영역
	v.f_SetDpi(96);
	v.f_SetData(&tr);

	printf("[a] scan 106, span %d (처음 10 초)\n", v.nSpan);
	v.f_SetScan(106);
	paint(&v, out + "/a_s106.json");
	{
		std::vector<UINT32> ids; for (auto &r : v.st_RowBuf) ids.push_back(r.TrackId);
		CHECK(v.nFirstScan == 0 && v.nSpan == 110, "window scans %d ~ %d", v.nFirstScan, v.nFirstScan + v.nSpan - 1);
		CHECK(ids.size() == 22 && ids.front() == 100 && ids.back() == 121, "rows %zu: %u ~ %u", ids.size(), ids.front(), ids.back());
		bool sorted = std::is_sorted(v.st_RowBuf.begin(), v.st_RowBuf.end(), [](const ST_LifeRow &a, const ST_LifeRow &b) { return a.nInitScan < b.nInitScan || (a.nInitScan == b.nInitScan && a.TrackId < b.TrackId); });
		CHECK(sorted, "rows in initiation order (init scan, ID)");
		const ST_LifeRow &r0 = v.st_RowBuf[0];
		std::vector<std::pair<int, int>> c0; for (auto &c : v.st_CellBuf) if (c.nRow == 0) c0.push_back({c.nScan, c.nState});
		CHECK(r0.TrackId == 100 && r0.nSource == 2 && c0.size() == 3 && c0[0] == std::make_pair(10, 1) && c0[1] == std::make_pair(11, 1) && c0[2] == std::make_pair(12, 4),
			  "track 100 (target 2): cells scan 10 state 1, 11 state 1, 12 state 4");
		int bad = 0; for (size_t k = 0; k < v.st_RowBuf.size(); k++) { int n = 0; for (auto &c : v.st_CellBuf) if (c.nRow == (int)k) n++; if (n > 3) bad++; }
		CHECK(bad == 0, "no row longer than 3 cells (tentative, tentative, deleted)");
		int inTable = 0; for (auto &r : v.st_RowBuf) inTable += r.isInTable;
		CHECK(inTable == tr.f_GetTrackNum(tr.f_FindScan(106)), "bold IDs (%d) = tracks in scan 106 table", inTable);
		printf("  title: %s\n", title().c_str());
	}

	printf("[b] scan 701, span 110\n");
	v.f_SetScan(701);
	paint(&v, out + "/b_s701.json");
	{
		std::vector<UINT32> cur; for (auto &r : v.st_RowBuf) if (r.isInTable) cur.push_back(r.TrackId);
		CHECK(v.nFirstScan == 592 && cur.size() == 4 && cur[0] == 282 && cur[3] == 285, "window from 592, tracks in table 282 ~ 285 (UI table at scan 701)");
		int early = 0, bad = 0; for (auto &r : v.st_RowBuf) if (r.nInitScan < v.nFirstScan) { early++; if (r.nFirstScan != v.nFirstScan) bad++; }
		CHECK(early > 0 && bad == 0, "%d tracks started before the window begin at its first scan", early);
		printf("  title: %s\n", title().c_str());
	}

	printf("[c] scan 1159, whole run (span = %d)\n", tr.nScanTotal);
	v.nSpan = tr.nScanTotal;
	v.f_SetScan(1159);
	paint(&v, out + "/c_full.json");
	CHECK(v.st_RowBuf.size() == 355, "rows %zu = 355 tracks", v.st_RowBuf.size());
	{
		int cells = 0; for (auto &s : g_Ops) if (s.find("\"op\":\"fill\"") != std::string::npos && s.find("\"h\":1.00") != std::string::npos) cells++;
		printf("  title: %s\n", title().c_str());
		CHECK(title().find("위로 넘침") == std::string::npos && cells == (int)v.st_CellBuf.size(), "all 355 rows fit (1 px cells: %d of %zu)", cells, v.st_CellBuf.size());
	}

	printf("[c2] scan 1159, whole run, short window 1592 x 200 (overflow)\n");
	v.clientH = 200;
	paint(&v, out + "/c2_overflow.json");
	{
		int drawn = 0; for (auto &s : g_Ops) if (s.find("\"op\":\"fill\"") != std::string::npos && s.find("\"h\":1.00") != std::string::npos) drawn++;
		const int area = 200 - 26 - 42;
		printf("  title: %s\n", title().c_str());
		CHECK(title().find("앞 트랙 " + std::to_string(355 - area) + " 개는 위로 넘침") != std::string::npos, "overflow note: %d rows shown in %d px", area, area);
	}
	v.clientH = 516;

	printf("[d] scan 5 (no track yet)\n");
	v.nSpan = 110;
	v.f_SetScan(5);
	paint(&v, out + "/d_s5.json");
	{
		bool msg = false; for (auto &s : g_Ops) if (s.find("트랙이 없습니다") != std::string::npos) msg = true;
		CHECK(v.st_RowBuf.empty() && msg, "empty message");
	}

	printf("[e] wheel zoom and double click\n");
	v.f_SetScan(106);
	v.f_Zoom(120, CPoint(10, 10));
	CHECK(v.nSpan == 88, "wheel up 1 notch: 110 -> %d", v.nSpan);
	for (int k = 0; k < 20; k++) v.f_Zoom(120, CPoint(10, 10));
	CHECK(v.nSpan == 20, "clamped at LIFE_SPAN_MIN: %d", v.nSpan);
	for (int k = 0; k < 40; k++) v.f_Zoom(-120, CPoint(10, 10));
	CHECK(v.nSpan == tr.nScanTotal, "clamped at total scans: %d", v.nSpan);
	v.OnLButtonDblClk(0, CPoint(0, 0));
	CHECK(v.nSpan == 110, "double click resets span: %d", v.nSpan);
	v.f_Zoom(120, CPoint(5000, 10));
	CHECK(v.nSpan == 110, "wheel outside the view ignored");

	v.clientW = 1592; v.clientH = 516; v.nSpan = 110; v.f_SetScan(106);
	paint(&v, out + "/a_s106.json");

	printf("[f] lost scan 11 (UDP loss)\n");
	{
		CTrackResult tl; load(&tl, dir, 11);
		v.f_SetData(&tl); v.f_SetScan(106);
		paint(&v, out + "/f_lost.json");
		int n100 = 0; for (auto &c : v.st_CellBuf) if (v.st_RowBuf[(size_t)c.nRow].TrackId == 100) n100++;
		bool lostFill = false; for (auto &s : g_Ops) if (s.find("#f3f4f6") != std::string::npos) lostFill = true;
		CHECK(n100 == 2 && lostFill, "track 100 has 2 cells, lost column shaded");
	}

	// 머리 줄: 범례 글자가 제목 오른쪽 끝을 넘어오지 않음 (제목 = 첫 굵은 글자, 범례 = 같은 줄의 나머지 글자)
	auto headerOk = [](const char *name) {
		double titleRight = 0, legendLeft = 1e9, baseY = -1; int items = 0;
		for (auto &s : g_Ops)
		{
			if (s.find("\"op\":\"text\"") == std::string::npos) continue;
			auto num = [&](const char *k) { size_t p = s.find(std::string("\"") + k + "\":"); return std::stod(s.substr(p + strlen(k) + 3)); };
			if (s.find("트랙 수명  ·") != std::string::npos) { titleRight = num("x") + num("w"); baseY = num("y"); }
			else if (baseY >= 0 && std::fabs(num("y") - baseY) < 0.01) { legendLeft = std::min(legendLeft, num("x")); items++; }
		}
		printf("  %s: title right %.0f, legend left %.0f, legend texts %d\n", name, titleRight, legendLeft, items);
		return titleRight < legendLeft;
	};
	CHECK(headerOk("[a] wide"), "legend clear of the title (wide)");

	printf("[g] small window 700 x 300, scan 106\n");
	v.f_SetData(&tr); v.clientW = 700; v.clientH = 300; v.f_SetScan(106);
	paint(&v, out + "/g_small.json");
	CHECK(headerOk("[g] 700 px"), "legend clear of the title (700 px: state colors only)");
	printf("[h] very small window 420 x 240, scan 106\n");
	v.clientW = 420; v.clientH = 240;
	paint(&v, out + "/h_tiny.json");
	CHECK(headerOk("[h] 420 px"), "legend dropped when even the state colors do not fit");

	printf("FAILURES: %d\n", g_Fail);
	return g_Fail ? 1 : 0;
}
