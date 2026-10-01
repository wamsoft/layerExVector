#include "ncbind.hpp"
#include "LayerExDraw.hpp"
#include <vector>
#include <string>
#include <stdio.h>
#include <cmath>
#include <map>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// thorvg 初期化
#ifdef LAYEREXVECTOR_TVG_GW
#include "thorvg_gw_bridge.h"

static tvg::Matrix mulMatrix(const tvg::Matrix &a, const tvg::Matrix &b);
#endif

extern void RegisterLayerExVectorLicenses();   // LicensesGen.cpp (生成物)

void initThorvg()
{
    // 同梱コンポーネント (ThorVG 等) のライセンス文を本体へ登録する
    // (System.getLicenseList / getLicenseText で参照可能になる)
    RegisterLayerExVectorLicenses();
    // 利用するスレッド数を指定 (0 ならメインスレッドのみ、1 以上ならその数だけワーカースレッドを起動)
    tvg::Initializer::init(4);
#ifdef LAYEREXVECTOR_TVG_GW
    // gw テキストローダ使用時: 本体の glyphware ブリッジをこの DLL 内の
    // thorvg コピーへ注入する (thorvg を静的リンクしているため、exe 側で
    // 登録されたブリッジは DLL 側のグローバルには反映されない)
    tvgGwSetBridge(static_cast<const TvgGwBridge*>(TVPGetFontTvgBridge()));
#endif
}

// thorvg 終了
void deInitThorvg()
{
    tvg::Initializer::term();
}

// --------------------------------------------------------
// ユーティリティ関数
// --------------------------------------------------------

extern bool IsArray(const tTJSVariant &var);
extern PointF getPoint(const tTJSVariant &var);

void getPoints(const tTJSVariant &var, vector<PointF> &points)
{
    ncbPropAccessor info(var);
    int c = info.GetArrayCount();
    for (int i = 0; i < c; i++) {
        tTJSVariant p;
        if (info.checkVariant(i, p)) {
            points.push_back(getPoint(p));
        }
    }
}

extern RectF getRect(const tTJSVariant &var);

void getRects(const tTJSVariant &var, vector<RectF> &rects)
{
    ncbPropAccessor info(var);
    int c = info.GetArrayCount();
    for (int i = 0; i < c; i++) {
        tTJSVariant p;
        if (info.checkVariant(i, p)) {
            rects.push_back(getRect(p));
        }
    }
}

static void getReals(const tTJSVariant &var, vector<REAL> &points)
{
    ncbPropAccessor info(var);
    int c = info.GetArrayCount();
    for (int i = 0; i < c; i++) {
        points.push_back((REAL)info.getRealValue(i));
    }
}

// --------------------------------------------------------
// グラデーションの GDI+ 互換 (rect / mode / angle / wrapMode / blend 系)
// --------------------------------------------------------

static tvg::Fill::ColorStop makeStop(REAL offset, ARGB c)
{
    tvg::Fill::ColorStop s;
    s.offset = offset;
    s.a = (c >> 24) & 0xFF;
    s.r = (c >> 16) & 0xFF;
    s.g = (c >> 8) & 0xFF;
    s.b = c & 0xFF;
    return s;
}

static ARGB lerpARGB(ARGB c1, ARGB c2, REAL t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    ARGB r = 0;
    for (int sh = 0; sh <= 24; sh += 8) {
        int a = (c1 >> sh) & 0xFF, b = (c2 >> sh) & 0xFF;
        r |= (ARGB)((int)(a + (b - a) * t + 0.5f) & 0xFF) << sh;
    }
    return r;
}

// GDI+ の WrapMode → ThorVG の FillSpread
static tvg::FillSpread toSpread(int wrapMode)
{
    switch (wrapMode) {
    case WrapModeTileFlipX:
    case WrapModeTileFlipY:
    case WrapModeTileFlipXY: return tvg::FillSpread::Reflect;
    case WrapModeClamp:      return tvg::FillSpread::Pad;
    default:                 return tvg::FillSpread::Repeat; // WrapModeTile (GDI+ の既定)
    }
}

// LinearGradientBrush(rect, c1, c2, angle, isAngleScalable) と同じ始点・終点を求める。
//   角度方向の単位ベクトルへ矩形の 4 隅を射影し、最小 → 最大を色 1 → 色 2 にする。
//   isAngleScalable は角度を矩形の縦横比で補正する (tanθ' = tanθ * w / h)。
//   mode 指定は 横 0° / 縦 90° / 順斜め 45° / 逆斜め 135° (斜めは縦横比で補正)
static void linearPointsFromRect(const RectF &rect, REAL angle, bool scalable,
                                 REAL &x1, REAL &y1, REAL &x2, REAL &y2)
{
    double rad = angle * M_PI / 180.0;
    double dx = cos(rad), dy = sin(rad);
    if (scalable) {
        dx *= rect.Height;
        dy *= rect.Width;
    }
    double len = sqrt(dx * dx + dy * dy);
    if (len <= 0) { dx = 1; dy = 0; len = 1; }
    dx /= len; dy /= len;
    double cx[4] = { rect.X, rect.X + rect.Width, rect.X,               rect.X + rect.Width };
    double cy[4] = { rect.Y, rect.Y,              rect.Y + rect.Height, rect.Y + rect.Height };
    double tmin = 0, tmax = 0;
    for (int i = 0; i < 4; i++) {
        double t = cx[i] * dx + cy[i] * dy;
        if (i == 0 || t < tmin) tmin = t;
        if (i == 0 || t > tmax) tmax = t;
    }
    x1 = (REAL)(dx * tmin); y1 = (REAL)(dy * tmin);
    x2 = (REAL)(dx * tmax); y2 = (REAL)(dy * tmax);
}

static void readReals(ncbPropAccessor &acc, tjs_int index, vector<REAL> &out)
{
    tTJSVariant v;
    if (acc.checkVariant(index, v) && v.Type() == tvtObject) getReals(v, out);
}
static void readReals(ncbPropAccessor &acc, const tjs_char *key, vector<REAL> &out)
{
    tTJSVariant v;
    if (acc.checkVariant(key, v) && v.Type() == tvtObject) getReals(v, out);
}
static void readColors(const tTJSVariant &var, vector<ARGB> &out)
{
    ncbPropAccessor info(var);
    int c = info.GetArrayCount();
    for (int i = 0; i < c; i++) out.push_back((ARGB)info.getIntValue(i));
}

// GDI+ の色の配分指定 (commonBrushParameter) をカラーストップへ置き換える。
//   blend               … 位置ごとの係数 (0=色1 / 1=色2)
//   blendBellShape      … 釣鐘形 (正規分布) を 33 点で近似
//   blendTriangularShape… 三角形 (focus で色 2 に達し両端は色 1)
//   interpolationColors … 位置ごとの色 (多色)
//   ⚠ useGammaCorrection は再現しない (受けるだけ)
static void applyBlendParams(ncbPropAccessor &info, ARGB c1, ARGB c2, vector<tvg::Fill::ColorStop> &stops)
{
    tTJSVariant var;
    vector<tvg::Fill::ColorStop> r;
    if (info.checkVariant(TJS_W("interpolationColors"), var) && var.Type() == tvtObject) {
        vector<ARGB> colors;
        vector<REAL> pos;
        ncbPropAccessor b(var);
        if (IsArray(var)) {
            tTJSVariant cv;
            if (b.checkVariant((tjs_int)0, cv) && cv.Type() == tvtObject) readColors(cv, colors);
            readReals(b, (tjs_int)1, pos);
        } else {
            tTJSVariant cv;
            if (b.checkVariant(TJS_W("presetColors"), cv) && cv.Type() == tvtObject) readColors(cv, colors);
            readReals(b, TJS_W("blendPositions"), pos);
        }
        size_t n = colors.size() < pos.size() ? colors.size() : pos.size();
        for (size_t i = 0; i < n; i++) r.push_back(makeStop(pos[i], colors[i]));
    } else if (info.checkVariant(TJS_W("blend"), var) && var.Type() == tvtObject) {
        vector<REAL> factors, pos;
        ncbPropAccessor b(var);
        if (IsArray(var)) {
            readReals(b, (tjs_int)0, factors);
            readReals(b, (tjs_int)1, pos);
        } else {
            readReals(b, TJS_W("blendFactors"), factors);
            readReals(b, TJS_W("blendPositions"), pos);
        }
        size_t n = factors.size() < pos.size() ? factors.size() : pos.size();
        for (size_t i = 0; i < n; i++) r.push_back(makeStop(pos[i], lerpARGB(c1, c2, factors[i])));
    } else if (info.checkVariant(TJS_W("blendTriangularShape"), var)) {
        REAL focus, scale;
        if (var.Type() == tvtObject && IsArray(var)) {
            ncbPropAccessor s(var);
            focus = (REAL)s.getRealValue(0); scale = (REAL)s.getRealValue(1, 1.0);
        } else {
            focus = (REAL)info.getRealValue(TJS_W("focus"), 0.5); scale = (REAL)info.getRealValue(TJS_W("scale"), 1.0);
        }
        if (focus > 0) r.push_back(makeStop(0, c1));
        r.push_back(makeStop(focus, lerpARGB(c1, c2, scale)));
        if (focus < 1) r.push_back(makeStop(1, c1));
    } else if (info.checkVariant(TJS_W("blendBellShape"), var)) {
        REAL focus, scale;
        if (var.Type() == tvtObject && IsArray(var)) {
            ncbPropAccessor s(var);
            focus = (REAL)s.getRealValue(0); scale = (REAL)s.getRealValue(1, 1.0);
        } else {
            focus = (REAL)info.getRealValue(TJS_W("focus"), 0.5); scale = (REAL)info.getRealValue(TJS_W("scale"), 1.0);
        }
        // focus で scale に達する釣鐘。 両側をそれぞれ正規分布の累積で 0 → 1 に寄せる
        const int N = 33;
        for (int i = 0; i < N; i++) {
            REAL t = (REAL)i / (N - 1);
            double f;
            if (t <= focus) { double u = focus > 0 ? t / focus : 1; f = 0.5 * (1 + erf((u - 0.5) * 2 * 1.5)); }
            else            { double u = focus < 1 ? (1 - t) / (1 - focus) : 1; f = 0.5 * (1 + erf((u - 0.5) * 2 * 1.5)); }
            r.push_back(makeStop(t, lerpARGB(c1, c2, (REAL)(f * scale))));
        }
    }
    if (r.size() >= 2) stops = r;
}

// --------------------------------------------------------
// パスグラデーション (GDI+ PathGradientBrush 互換)
// --------------------------------------------------------

// blend 系の指定 (位置ごとの係数) を表にする。 interpolationColors は色の表。
//   パスグラデーションの位置は 0 = 外周 / 1 = 中心 (GDI+ と同じ)
static void readPathBlend(ncbPropAccessor &info, Appearance::DrawInfo &d)
{
    tTJSVariant var;
    if (info.checkVariant(TJS_W("interpolationColors"), var) && var.Type() == tvtObject) {
        vector<ARGB> colors; vector<REAL> pos;
        ncbPropAccessor b(var);
        tTJSVariant cv;
        if (IsArray(var)) {
            if (b.checkVariant((tjs_int)0, cv) && cv.Type() == tvtObject) readColors(cv, colors);
            readReals(b, (tjs_int)1, pos);
        } else {
            if (b.checkVariant(TJS_W("presetColors"), cv) && cv.Type() == tvtObject) readColors(cv, colors);
            readReals(b, TJS_W("blendPositions"), pos);
        }
        size_t n = colors.size() < pos.size() ? colors.size() : pos.size();
        for (size_t i = 0; i < n; i++) { d.pgPresetPos.push_back(pos[i]); d.pgPresetColors.push_back(colors[i]); }
    } else if (info.checkVariant(TJS_W("blend"), var) && var.Type() == tvtObject) {
        vector<REAL> fac, pos;
        ncbPropAccessor b(var);
        if (IsArray(var)) { readReals(b, (tjs_int)0, fac); readReals(b, (tjs_int)1, pos); }
        else { readReals(b, TJS_W("blendFactors"), fac); readReals(b, TJS_W("blendPositions"), pos); }
        size_t n = fac.size() < pos.size() ? fac.size() : pos.size();
        for (size_t i = 0; i < n; i++) { d.pgBlendPos.push_back(pos[i]); d.pgBlendFac.push_back(fac[i]); }
    } else if (info.checkVariant(TJS_W("blendTriangularShape"), var) || info.checkVariant(TJS_W("blendBellShape"), var)) {
        bool bell = !info.HasValue(TJS_W("blendTriangularShape"));
        REAL focus, scale;
        if (var.Type() == tvtObject && IsArray(var)) {
            ncbPropAccessor s(var);
            focus = (REAL)s.getRealValue(0); scale = (REAL)s.getRealValue(1, 1.0);
        } else {
            focus = (REAL)info.getRealValue(TJS_W("focus"), 0.5); scale = (REAL)info.getRealValue(TJS_W("scale"), 1.0);
        }
        const int N = bell ? 33 : 3;
        for (int i = 0; i < N; i++) {
            REAL t = bell ? (REAL)i / (N - 1) : (i == 0 ? 0 : i == 1 ? focus : 1);
            double f;
            if (!bell) f = (i == 1) ? 1 : 0;
            else if (t <= focus) { double u = focus > 0 ? t / focus : 1; f = 0.5 * (1 + erf((u - 0.5) * 2 * 1.5)); }
            else                 { double u = focus < 1 ? (1 - t) / (1 - focus) : 1; f = 0.5 * (1 + erf((u - 0.5) * 2 * 1.5)); }
            d.pgBlendPos.push_back(t); d.pgBlendFac.push_back((REAL)(f * scale));
        }
    }
}

// GDI+ の PathGradientBrush と同じ指定を受ける。
//   points (必須) / surroundColors (既定 白、足りなければ最後を繰り返す) /
//   centerColor (既定 黒) / centerPoint (既定 頂点の平均) / focusScales /
//   wrapMode (既定 Tile) / blend 系 / interpolationColors
static void parsePathGradient(ncbPropAccessor &info, const tTJSVariant &pointsVar, Appearance::DrawInfo &d)
{
    ncbPropAccessor pts(pointsVar);
    int n = pts.GetArrayCount();
    for (int i = 0; i < n; i++) {
        tTJSVariant pv;
        if (!pts.checkVariant((tjs_int)i, pv)) continue;
        PointF p = getPoint(pv);
        d.pgX.push_back(p.X); d.pgY.push_back(p.Y);
    }
    if (d.pgX.size() < 2) return;
    d.usePathGradient = true;

    tTJSVariant var;
    if (info.checkVariant(TJS_W("surroundColors"), var) && var.Type() == tvtObject) readColors(var, d.pgColors);
    if (d.pgColors.empty()) d.pgColors.push_back(0xFFFFFFFF);
    while (d.pgColors.size() < d.pgX.size()) d.pgColors.push_back(d.pgColors.back());

    d.pgCenterColor = (ARGB)info.getIntValue(TJS_W("centerColor"), (tjs_int)0xFF000000);
    if (info.checkVariant(TJS_W("centerPoint"), var)) {
        PointF c = getPoint(var);
        d.pgCx = c.X; d.pgCy = c.Y;
    } else {
        double sx = 0, sy = 0;
        for (size_t i = 0; i < d.pgX.size(); i++) { sx += d.pgX[i]; sy += d.pgY[i]; }
        d.pgCx = (REAL)(sx / d.pgX.size()); d.pgCy = (REAL)(sy / d.pgY.size());
    }
    if (info.checkVariant(TJS_W("focusScales"), var)) {
        if (var.Type() == tvtObject && IsArray(var)) {
            ncbPropAccessor s(var);
            d.pgFocusX = (REAL)s.getRealValue(0); d.pgFocusY = (REAL)s.getRealValue(1);
        }
    } else if (info.HasValue(TJS_W("xScale")) || info.HasValue(TJS_W("yScale"))) {
        d.pgFocusX = (REAL)info.getRealValue(TJS_W("xScale")); d.pgFocusY = (REAL)info.getRealValue(TJS_W("yScale"));
    }
    d.pgWrap = (int)info.getIntValue(TJS_W("wrapMode"), WrapModeTile);
    readPathBlend(info, d);
}

// 表の位置 p (0..1) での値を線形補間で引く
static REAL lookupTable(const vector<REAL> &pos, const vector<REAL> &val, REAL p)
{
    size_t n = pos.size();
    if (n == 0) return p;
    if (p <= pos[0]) return val[0];
    for (size_t i = 1; i < n; i++) {
        if (p <= pos[i]) {
            REAL span = pos[i] - pos[i-1];
            REAL t = span > 0 ? (p - pos[i-1]) / span : 0;
            return val[i-1] + (val[i] - val[i-1]) * t;
        }
    }
    return val[n-1];
}
static ARGB lookupColorTable(const vector<REAL> &pos, const vector<ARGB> &col, REAL p)
{
    size_t n = pos.size();
    if (n == 0) return 0;
    if (p <= pos[0]) return col[0];
    for (size_t i = 1; i < n; i++) {
        if (p <= pos[i]) {
            REAL span = pos[i] - pos[i-1];
            return lerpARGB(col[i-1], col[i], span > 0 ? (p - pos[i-1]) / span : 0);
        }
    }
    return col[n-1];
}

// 範囲外の座標を外接矩形の中へ畳む (Tile は繰り返し、TileFlip* は折り返し)
static REAL wrapCoord(REAL v, REAL lo, REAL w, bool flip)
{
    if (w <= 0) return v;
    double r = (v - lo) / w;
    double k = floor(r);
    double f = (r - k) * w;
    if (flip && ((long long)k & 1)) f = w - f;
    return (REAL)(lo + f);
}

// 点 (qx, qy) の色。 中心と各辺 (Pi, Pi+1) の三角形で重心座標を取り、
//   t = 中心 0 → 外周 1、s = 辺の上の位置。 色 = lerp(中心色, lerp(Ci, Ci+1, s), t)
//   (= GDI+ の PathGradient)。 多角形の外は透明 (0)。 hint は前回当たった辺
static ARGB pathGradientColor(const Appearance::DrawInfo &d, REAL qx, REAL qy, int &hint)
{
    const int n = (int)d.pgX.size();
    const double cx = d.pgCx, cy = d.pgCy;
    const double px = qx - cx, py = qy - cy;
    for (int k = 0; k < n; k++) {
        // 前回の辺から外側へ順に試す (隣の画素はたいてい同じ三角形)
        int e = (k & 1) ? hint - (k + 1) / 2 : hint + k / 2;
        e = ((e % n) + n) % n;
        int e2 = (e + 1) % n;
        double ax = d.pgX[e]  - cx, ay = d.pgY[e]  - cy;
        double bx = d.pgX[e2] - cx, by = d.pgY[e2] - cy;
        double det = ax * by - ay * bx;
        if (fabs(det) < 1e-12) continue;
        double a = (px * by - py * bx) / det;
        double b = (ax * py - ay * px) / det;
        const double eps = 1e-9;
        if (a < -eps || b < -eps || a + b > 1 + eps) continue;
        hint = e;
        double t = a + b;
        double s = t > 0 ? b / t : 0;
        // focusScales: 中心側の縮小した多角形の中は中心色
        if (d.pgFocusX > 0 || d.pgFocusY > 0) {
            double dx = fabs(ax + (bx - ax) * s), dy = fabs(ay + (by - ay) * s);
            double f = (dx + dy) > 0 ? (d.pgFocusX * dx + d.pgFocusY * dy) / (dx + dy) : 0;
            if (f >= 1) t = 0;
            else t = (t <= f) ? 0 : (t - f) / (1 - f);
        }
        REAL p = (REAL)(1 - t); // 位置: 0 = 外周 / 1 = 中心
        if (!d.pgPresetPos.empty()) return lookupColorTable(d.pgPresetPos, d.pgPresetColors, p);
        ARGB edge = lerpARGB(d.pgColors[e], d.pgColors[e2], (REAL)s);
        REAL fac = d.pgBlendPos.empty() ? p : lookupTable(d.pgBlendPos, d.pgBlendFac, p);
        return lerpARGB(edge, d.pgCenterColor, fac);
    }
    return 0;
}

// --------------------------------------------------------
// アピアランス情報
// --------------------------------------------------------

Appearance::Appearance() {}

Appearance::~Appearance()
{
    clear();
}

void Appearance::clear()
{
    drawInfos.clear();
}

// tvg::Picture を w×h の ARGB8888 バッファへネイティブ解像度でラスタライズする。
// (テクスチャブラシのソースタイルのピクセルを取り出すために使用)
static bool rasterizePicture(tvg::Picture* src, uint32_t* out, int w, int h)
{
    if (!src || !out || w <= 0 || h <= 0) return false;
    memset(out, 0, (size_t)w * h * sizeof(uint32_t));
    tvg::SwCanvas* c = tvg::SwCanvas::gen();
    if (!c) return false;
    bool ok = false;
    if (c->target(out, w, w, h, tvg::ColorSpace::ARGB8888) == tvg::Result::Success) {
        tvg::Picture* dup = (tvg::Picture*)src->duplicate();
        if (dup) {
            dup->size((float)w, (float)h);
            if (c->add(dup) == tvg::Result::Success) {
                c->draw(true);
                c->sync();
                ok = true;
            }
        }
    }
    delete c; // 追加した Picture は Canvas 破棄時に解放される
    return ok;
}

void Appearance::addBrush(tTJSVariant colorOrBrush, REAL ox, REAL oy)
{
    DrawInfo info;
    info.type = 1; // フィル
    info.ox = ox;
    info.oy = oy;

    if (colorOrBrush.Type() != tvtObject) {
        // ARGB色
        ARGB color = (ARGB)(tjs_int)colorOrBrush;
        info.fillA = (color >> 24) & 0xFF;
        info.fillR = (color >> 16) & 0xFF;
        info.fillG = (color >> 8) & 0xFF;
        info.fillB = color & 0xFF;
    } else {
        // ブラシ情報（辞書）
        ncbPropAccessor propInfo(colorOrBrush);
        int type = propInfo.getIntValue(TJS_W("type"), BrushTypeSolidColor);
        tTJSVariant pathPoints;

        if (type == BrushTypeLinearGradient) {
            info.useLinearGradient = true;

            // 始点・終点。 GDI+ と同じく point1/point2、無ければ rect + (angle | mode)
            tTJSVariant var;
            if (propInfo.checkVariant(TJS_W("point1"), var)) {
                PointF p1 = getPoint(var);
                info.gradX1 = p1.X;
                info.gradY1 = p1.Y;
                if (propInfo.checkVariant(TJS_W("point2"), var)) {
                    PointF p2 = getPoint(var);
                    info.gradX2 = p2.X;
                    info.gradY2 = p2.Y;
                }
            } else if (propInfo.checkVariant(TJS_W("rect"), var)) {
                RectF rect = getRect(var);
                REAL angle = 0;
                bool scalable = false;
                if (propInfo.HasValue(TJS_W("angle"))) {
                    angle    = (REAL)propInfo.getRealValue(TJS_W("angle"), 0);
                    scalable = propInfo.getIntValue(TJS_W("isAngleScalable"), 0) != 0;
                } else {
                    switch (propInfo.getIntValue(TJS_W("mode"), LinearGradientModeHorizontal)) {
                    case LinearGradientModeVertical:         angle =  90; break;
                    case LinearGradientModeForwardDiagonal:  angle =  45; scalable = true; break;
                    case LinearGradientModeBackwardDiagonal: angle = 135; scalable = true; break;
                    default:                                 angle =   0; break;
                    }
                }
                linearPointsFromRect(rect, angle, scalable, info.gradX1, info.gradY1, info.gradX2, info.gradY2);
            }
            // 範囲外の扱い。 GDI+ の線形グラデーションの既定は Tile (繰り返し)
            info.gradSpread = toSpread(propInfo.getIntValue(TJS_W("wrapMode"), WrapModeTile));

            ARGB color1 = (ARGB)propInfo.getIntValue(TJS_W("color1"), 0);
            ARGB color2 = (ARGB)propInfo.getIntValue(TJS_W("color2"), 0);

            tvg::Fill::ColorStop stop1, stop2;
            stop1.offset = 0.0f;
            stop1.a = (color1 >> 24) & 0xFF;
            stop1.r = (color1 >> 16) & 0xFF;
            stop1.g = (color1 >> 8) & 0xFF;
            stop1.b = color1 & 0xFF;

            stop2.offset = 1.0f;
            stop2.a = (color2 >> 24) & 0xFF;
            stop2.r = (color2 >> 16) & 0xFF;
            stop2.g = (color2 >> 8) & 0xFF;
            stop2.b = color2 & 0xFF;

            info.colorStops.push_back(stop1);
            info.colorStops.push_back(stop2);
            applyBlendParams(propInfo, color1, color2, info.colorStops);
        } else if (type == BrushTypePathGradient && propInfo.checkVariant(TJS_W("points"), pathPoints) && pathPoints.Type() == tvtObject) {
            // GDI+ の PathGradientBrush と同じ計算 (描画時に色の画像を作る)
            parsePathGradient(propInfo, pathPoints, info);
        } else if (type == BrushTypePathGradient) { // points が無い: 放射グラデーションで近似 (旧来の radius 指定)
            info.useRadialGradient = true;

            tTJSVariant var;
            if (propInfo.checkVariant(TJS_W("centerPoint"), var)) {
                PointF cp = getPoint(var);
                info.gradCx = cp.X;
                info.gradCy = cp.Y;
            }
            info.gradR = (REAL)propInfo.getRealValue(TJS_W("radius"), 100);

            ARGB centerColor = (ARGB)propInfo.getIntValue(TJS_W("centerColor"), 0xFFFFFFFF);

            tvg::Fill::ColorStop stop1, stop2;
            stop1.offset = 0.0f;
            stop1.a = (centerColor >> 24) & 0xFF;
            stop1.r = (centerColor >> 16) & 0xFF;
            stop1.g = (centerColor >> 8) & 0xFF;
            stop1.b = centerColor & 0xFF;

            stop2.offset = 1.0f;
            stop2.a = 0;
            stop2.r = 0;
            stop2.g = 0;
            stop2.b = 0;

            info.colorStops.push_back(stop1);
            info.colorStops.push_back(stop2);
        } else if (type == BrushTypeTextureFill) {
            // 画像タイル塗り (GDI+ TextureBrush 相当)。image はストレージパス文字列。
            ttstr imgPath = propInfo.getStrValue(TJS_W("image"));
            if (imgPath.length() > 0) {
                ::Image img;
                if (img.load(imgPath.c_str()) && img.getPicture()) {
                    int tw = (int)(img.GetWidth()  + 0.5f);
                    int th = (int)(img.GetHeight() + 0.5f);
                    // dstRect でソース領域を指定 (省略時は画像全体)
                    int sx = 0, sy = 0, sw = tw, sh = th;
                    tTJSVariant rectVar;
                    if (propInfo.checkVariant(TJS_W("dstRect"), rectVar)) {
                        ncbPropAccessor ra(rectVar);
                        if (ra.GetArrayCount() >= 4) {
                            sx = (int)ra.getRealValue(0);
                            sy = (int)ra.getRealValue(1);
                            sw = (int)ra.getRealValue(2);
                            sh = (int)ra.getRealValue(3);
                        }
                    }
                    if (sw <= 0) sw = tw;
                    if (sh <= 0) sh = th;
                    // ソース画像をラスタライズして dstRect 範囲をタイル素材に切り出す
                    if (tw > 0 && th > 0) {
                        std::vector<uint32_t> full((size_t)tw * th, 0);
                        if (rasterizePicture(img.getPicture(), full.data(), tw, th)) {
                            info.texW = sw;
                            info.texH = sh;
                            info.texPixels.assign((size_t)sw * sh, 0);
                            for (int y = 0; y < sh; y++) {
                                for (int x = 0; x < sw; x++) {
                                    int fx = sx + x, fy = sy + y;
                                    if (fx >= 0 && fx < tw && fy >= 0 && fy < th)
                                        info.texPixels[(size_t)y*sw + x] = full[(size_t)fy*tw + fx];
                                }
                            }
                            info.useTextureFill = true;
                        }
                    }
                }
            }
        } else {
            // SolidColor
            ARGB color = (ARGB)propInfo.getIntValue(TJS_W("color"), 0xFFFFFFFF);
            info.fillA = (color >> 24) & 0xFF;
            info.fillR = (color >> 16) & 0xFF;
            info.fillG = (color >> 8) & 0xFF;
            info.fillB = color & 0xFF;
        }
    }

    drawInfos.push_back(info);
}

void Appearance::addPen(tTJSVariant colorOrBrush, tTJSVariant widthOrOption, REAL ox, REAL oy)
{
    DrawInfo info;
    info.type = 0; // ストローク
    info.ox = ox;
    info.oy = oy;

    // 色設定
    if (colorOrBrush.Type() != tvtObject) {
        ARGB color = (ARGB)(tjs_int)colorOrBrush;
        info.strokeA = (color >> 24) & 0xFF;
        info.strokeR = (color >> 16) & 0xFF;
        info.strokeG = (color >> 8) & 0xFF;
        info.strokeB = color & 0xFF;
    } else {
        ncbPropAccessor propInfo(colorOrBrush);
        int btype = propInfo.getIntValue(TJS_W("type"), BrushTypeSolidColor);
        if (btype == BrushTypeLinearGradient || btype == BrushTypePathGradient) {
            // GDI+ の Pen(brush): 線をブラシで塗る。 ブラシの解釈は addBrush と同じ
            Appearance tmp;
            tmp.addBrush(colorOrBrush, 0, 0);
            const DrawInfo &b = tmp.drawInfos.back();
            info.useLinearGradient = b.useLinearGradient;
            info.useRadialGradient = b.useRadialGradient;
            info.gradX1 = b.gradX1; info.gradY1 = b.gradY1; info.gradX2 = b.gradX2; info.gradY2 = b.gradY2;
            info.gradCx = b.gradCx; info.gradCy = b.gradCy; info.gradR = b.gradR;
            info.colorStops = b.colorStops; info.gradSpread = b.gradSpread;
            info.usePathGradient = b.usePathGradient;
            info.pgX = b.pgX; info.pgY = b.pgY; info.pgColors = b.pgColors;
            info.pgCx = b.pgCx; info.pgCy = b.pgCy; info.pgCenterColor = b.pgCenterColor;
            info.pgFocusX = b.pgFocusX; info.pgFocusY = b.pgFocusY; info.pgWrap = b.pgWrap;
            info.pgBlendPos = b.pgBlendPos; info.pgBlendFac = b.pgBlendFac;
            info.pgPresetPos = b.pgPresetPos; info.pgPresetColors = b.pgPresetColors;
        } else {
            ARGB color = (ARGB)propInfo.getIntValue(TJS_W("color"), 0xFFFFFFFF);
            info.strokeA = (color >> 24) & 0xFF;
            info.strokeR = (color >> 16) & 0xFF;
            info.strokeG = (color >> 8) & 0xFF;
            info.strokeB = color & 0xFF;
        }
    }

    // 幅とオプション設定
    if (widthOrOption.Type() != tvtObject) {
        info.strokeWidth = (REAL)(tjs_real)widthOrOption;
    } else {
        ncbPropAccessor propInfo(widthOrOption);

        tTJSVariant var;
        if (propInfo.checkVariant(TJS_W("width"), var)) {
            info.strokeWidth = (REAL)(tjs_real)var;
        }

        // LineCap
        if (propInfo.checkVariant(TJS_W("startCap"), var) || propInfo.checkVariant(TJS_W("endCap"), var)) {
            int cap = (int)(tjs_int)var;
            switch (cap) {
            case LineCapFlat:
                info.strokeCap = tvg::StrokeCap::Butt;
                break;
            case LineCapSquare:
                info.strokeCap = tvg::StrokeCap::Square;
                break;
            case LineCapRound:
                info.strokeCap = tvg::StrokeCap::Round;
                break;
            case LineCapTriangle:
                // ThorVG doesn't have Triangle cap, use Square as fallback
                info.strokeCap = tvg::StrokeCap::Square;
                break;
            default:
                info.strokeCap = tvg::StrokeCap::Square;
                break;
            }
        }

        // LineJoin
        if (propInfo.checkVariant(TJS_W("lineJoin"), var)) {
            int join = (int)(tjs_int)var;
            switch (join) {
            case LineJoinMiter:
                info.strokeJoin = tvg::StrokeJoin::Miter;
                break;
            case LineJoinBevel:
                info.strokeJoin = tvg::StrokeJoin::Bevel;
                break;
            case LineJoinRound:
                info.strokeJoin = tvg::StrokeJoin::Round;
                break;
            case LineJoinMiterClipped:
                // ThorVG doesn't have MiterClipped, use Miter as fallback
                info.strokeJoin = tvg::StrokeJoin::Miter;
                break;
            default:
                info.strokeJoin = tvg::StrokeJoin::Bevel;
                break;
            }
        }

        // MiterLimit
        if (propInfo.checkVariant(TJS_W("miterLimit"), var)) {
            info.miterLimit = (REAL)(tjs_real)var;
        }

        // DashStyle
        if (propInfo.checkVariant(TJS_W("dashStyle"), var)) {
            if (IsArray(var)) {
                getReals(var, info.dashPattern);
            } else if (var.Type() == tvtInteger) {
                // GDI+ の DashStyle。 パターンはペン幅の倍数
                static const REAL dash[]       = { 3, 1 };
                static const REAL dot[]        = { 1, 1 };
                static const REAL dashdot[]    = { 3, 1, 1, 1 };
                static const REAL dashdotdot[] = { 3, 1, 1, 1, 1, 1 };
                const REAL *pat = nullptr; int n = 0;
                switch ((tjs_int)var) {
                case DashStyleDash:       pat = dash;       n = 2; break;
                case DashStyleDot:        pat = dot;        n = 2; break;
                case DashStyleDashDot:    pat = dashdot;    n = 4; break;
                case DashStyleDashDotDot: pat = dashdotdot; n = 6; break;
                default: break; // Solid / Custom は実線
                }
                REAL w = info.strokeWidth > 0 ? info.strokeWidth : 1;
                for (int i = 0; i < n; i++) info.dashPattern.push_back(pat[i] * w);
            }
        }

        // DashCap (破線の各区切りの端)
        if (propInfo.checkVariant(TJS_W("dashCap"), var)) {
            info.dashCap = (int)(tjs_int)var;
        }

        // DashOffset
        if (propInfo.checkVariant(TJS_W("dashOffset"), var)) {
            info.dashOffset = (REAL)(tjs_real)var;
        }
    }

    drawInfos.push_back(info);
}

// --------------------------------------------------------
// FontInfo クラス
// --------------------------------------------------------

FontInfo::FontInfo()
    : fontSize(12.0f), italic(0), letterSpacing(1.0f), lineSpacing(1.0f)
{
}

FontInfo::FontInfo(const tjs_char *family, REAL size)
    : fontSize(size), italic(0), letterSpacing(1.0f), lineSpacing(1.0f)
{
    if (family) {
        fontFamily = family;
    }
}

FontInfo::FontInfo(const FontInfo &orig)
    : fontFamily(orig.fontFamily),
      fontSize(orig.fontSize),
      italic(orig.italic),
      letterSpacing(orig.letterSpacing),
      lineSpacing(orig.lineSpacing)
{
}

FontInfo::~FontInfo()
{
}

void FontInfo::setFontFamily(const tjs_char *name)
{
    if (name) {
        fontFamily = name;
    } else {
        fontFamily = TJS_W("");
    }
}

// --------------------------------------------------------
// Path クラス
// --------------------------------------------------------

Path::Path() : figureStarted(false)
{
    currentPos.x = 0;
    currentPos.y = 0;
    figureStartPos.x = 0;
    figureStartPos.y = 0;
}

Path::~Path()
{
}

void Path::ensureFigureStarted()
{
    if (!figureStarted) {
        commands.push_back(tvg::PathCommand::MoveTo);
        points.push_back(currentPos);
        figureStartPos = currentPos;
        figureStarted = true;
    }
}

void Path::startFigure()
{
    figureStarted = false;
}

void Path::closeFigure()
{
    if (figureStarted) {
        commands.push_back(tvg::PathCommand::Close);
        currentPos = figureStartPos;
        figureStarted = false;
    }
}

void Path::addArcPoints(REAL cx, REAL cy, REAL rx, REAL ry, REAL startAngle, REAL sweepAngle)
{
    // 楕円弧をベジェ曲線で近似
    const int numSegments = (int)(fabs(sweepAngle) / 90.0f) + 1;
    const REAL segmentAngle = sweepAngle / numSegments;

    REAL angle = startAngle * (REAL)M_PI / 180.0f;
    const REAL deltaAngle = segmentAngle * (REAL)M_PI / 180.0f;

    // 開始点
    REAL startX = cx + rx * cosf(angle);
    REAL startY = cy + ry * sinf(angle);

    if (!figureStarted) {
        currentPos.x = startX;
        currentPos.y = startY;
        ensureFigureStarted();
    } else {
        commands.push_back(tvg::PathCommand::LineTo);
        tvg::Point p = {startX, startY};
        points.push_back(p);
    }

    for (int i = 0; i < numSegments; i++) {
        REAL endAngle = angle + deltaAngle;

        // ベジェ制御点の計算
        REAL kappa = 4.0f / 3.0f * tanf(deltaAngle / 4.0f);

        REAL x1 = cx + rx * cosf(angle);
        REAL y1 = cy + ry * sinf(angle);
        REAL x4 = cx + rx * cosf(endAngle);
        REAL y4 = cy + ry * sinf(endAngle);

        REAL x2 = x1 - kappa * rx * sinf(angle);
        REAL y2 = y1 + kappa * ry * cosf(angle);
        REAL x3 = x4 + kappa * rx * sinf(endAngle);
        REAL y3 = y4 - kappa * ry * cosf(endAngle);

        commands.push_back(tvg::PathCommand::CubicTo);
        tvg::Point cp1 = {x2, y2};
        tvg::Point cp2 = {x3, y3};
        tvg::Point ep = {x4, y4};
        points.push_back(cp1);
        points.push_back(cp2);
        points.push_back(ep);

        angle = endAngle;
    }

    currentPos.x = cx + rx * cosf(angle);
    currentPos.y = cy + ry * sinf(angle);
}

void Path::drawArc(REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle)
{
    REAL cx = x + width / 2;
    REAL cy = y + height / 2;
    REAL rx = width / 2;
    REAL ry = height / 2;

    addArcPoints(cx, cy, rx, ry, startAngle, sweepAngle);
}

void Path::drawBezier(REAL x1, REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4)
{
    currentPos.x = x1;
    currentPos.y = y1;
    ensureFigureStarted();

    commands.push_back(tvg::PathCommand::CubicTo);
    tvg::Point cp1 = {x2, y2};
    tvg::Point cp2 = {x3, y3};
    tvg::Point ep = {x4, y4};
    points.push_back(cp1);
    points.push_back(cp2);
    points.push_back(ep);

    currentPos = ep;
}

void Path::drawBeziers(tTJSVariant pts)
{
    vector<PointF> ps;
    getPoints(pts, ps);

    if (ps.size() < 4) return;

    currentPos.x = ps[0].X;
    currentPos.y = ps[0].Y;
    ensureFigureStarted();

    for (size_t i = 1; i + 2 < ps.size(); i += 3) {
        commands.push_back(tvg::PathCommand::CubicTo);
        tvg::Point cp1 = {ps[i].X, ps[i].Y};
        tvg::Point cp2 = {ps[i+1].X, ps[i+1].Y};
        tvg::Point ep = {ps[i+2].X, ps[i+2].Y};
        points.push_back(cp1);
        points.push_back(cp2);
        points.push_back(ep);
        currentPos = ep;
    }
}

void Path::computeCardinalSpline(const vector<PointF>& pts, REAL tension, bool closed, vector<tvg::Point>& outPts)
{
    if (pts.size() < 2) return;

    // Cardinal spline をベジェ曲線に変換
    REAL t = (1.0f - tension) / 2.0f;

    size_t n = pts.size();
    for (size_t i = 0; i < n - 1; i++) {
        PointF p0 = (i == 0) ? (closed ? pts[n-1] : pts[0]) : pts[i-1];
        PointF p1 = pts[i];
        PointF p2 = pts[i+1];
        PointF p3 = (i == n - 2) ? (closed ? pts[0] : pts[n-1]) : pts[i+2];

        REAL cp1x = p1.X + t * (p2.X - p0.X) / 3.0f;
        REAL cp1y = p1.Y + t * (p2.Y - p0.Y) / 3.0f;
        REAL cp2x = p2.X - t * (p3.X - p1.X) / 3.0f;
        REAL cp2y = p2.Y - t * (p3.Y - p1.Y) / 3.0f;

        if (i == 0) {
            tvg::Point sp = {p1.X, p1.Y};
            outPts.push_back(sp);
        }

        tvg::Point c1 = {cp1x, cp1y};
        tvg::Point c2 = {cp2x, cp2y};
        tvg::Point ep = {p2.X, p2.Y};
        outPts.push_back(c1);
        outPts.push_back(c2);
        outPts.push_back(ep);
    }

    if (closed && n > 2) {
        // 閉じた曲線の最後のセグメント
        PointF p0 = pts[n-2];
        PointF p1 = pts[n-1];
        PointF p2 = pts[0];
        PointF p3 = pts[1];

        REAL cp1x = p1.X + t * (p2.X - p0.X) / 3.0f;
        REAL cp1y = p1.Y + t * (p2.Y - p0.Y) / 3.0f;
        REAL cp2x = p2.X - t * (p3.X - p1.X) / 3.0f;
        REAL cp2y = p2.Y - t * (p3.Y - p1.Y) / 3.0f;

        tvg::Point c1 = {cp1x, cp1y};
        tvg::Point c2 = {cp2x, cp2y};
        tvg::Point ep = {p2.X, p2.Y};
        outPts.push_back(c1);
        outPts.push_back(c2);
        outPts.push_back(ep);
    }
}

void Path::drawClosedCurve(tTJSVariant pts)
{
    drawClosedCurve2(pts, 0.5f);
}

void Path::drawClosedCurve2(tTJSVariant pts, REAL tension)
{
    vector<PointF> ps;
    getPoints(pts, ps);

    if (ps.size() < 3) return;

    vector<tvg::Point> splinePts;
    computeCardinalSpline(ps, tension, true, splinePts);

    if (splinePts.empty()) return;

    currentPos = splinePts[0];
    ensureFigureStarted();

    for (size_t i = 1; i + 2 < splinePts.size(); i += 3) {
        commands.push_back(tvg::PathCommand::CubicTo);
        points.push_back(splinePts[i]);
        points.push_back(splinePts[i+1]);
        points.push_back(splinePts[i+2]);
        currentPos = splinePts[i+2];
    }

    closeFigure();
}

void Path::drawCurve(tTJSVariant pts)
{
    drawCurve2(pts, 0.5f);
}

void Path::drawCurve2(tTJSVariant pts, REAL tension)
{
    vector<PointF> ps;
    getPoints(pts, ps);

    if (ps.size() < 2) return;

    vector<tvg::Point> splinePts;
    computeCardinalSpline(ps, tension, false, splinePts);

    if (splinePts.empty()) return;

    currentPos = splinePts[0];
    ensureFigureStarted();

    for (size_t i = 1; i + 2 < splinePts.size(); i += 3) {
        commands.push_back(tvg::PathCommand::CubicTo);
        points.push_back(splinePts[i]);
        points.push_back(splinePts[i+1]);
        points.push_back(splinePts[i+2]);
        currentPos = splinePts[i+2];
    }
}

void Path::drawCurve3(tTJSVariant pts, int offset, int numberOfSegments, REAL tension)
{
    vector<PointF> ps;
    getPoints(pts, ps);

    if ((size_t)(offset + numberOfSegments + 1) > ps.size()) return;

    vector<PointF> subPts(ps.begin() + offset, ps.begin() + offset + numberOfSegments + 1);

    vector<tvg::Point> splinePts;
    computeCardinalSpline(subPts, tension, false, splinePts);

    if (splinePts.empty()) return;

    currentPos = splinePts[0];
    ensureFigureStarted();

    for (size_t i = 1; i + 2 < splinePts.size(); i += 3) {
        commands.push_back(tvg::PathCommand::CubicTo);
        points.push_back(splinePts[i]);
        points.push_back(splinePts[i+1]);
        points.push_back(splinePts[i+2]);
        currentPos = splinePts[i+2];
    }
}

void Path::drawPie(REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle)
{
    REAL cx = x + width / 2;
    REAL cy = y + height / 2;
    REAL rx = width / 2;
    REAL ry = height / 2;

    // 中心から開始
    currentPos.x = cx;
    currentPos.y = cy;
    ensureFigureStarted();

    // 円弧の開始点へ
    REAL startRad = startAngle * (REAL)M_PI / 180.0f;
    REAL startX = cx + rx * cosf(startRad);
    REAL startY = cy + ry * sinf(startRad);

    commands.push_back(tvg::PathCommand::LineTo);
    tvg::Point sp = {startX, startY};
    points.push_back(sp);
    currentPos = sp;

    // 円弧を描画
    addArcPoints(cx, cy, rx, ry, startAngle, sweepAngle);

    // 中心に戻って閉じる
    closeFigure();
}

void Path::drawEllipse(REAL x, REAL y, REAL width, REAL height)
{
    REAL cx = x + width / 2;
    REAL cy = y + height / 2;
    REAL rx = width / 2;
    REAL ry = height / 2;

    currentPos.x = cx + rx;
    currentPos.y = cy;
    ensureFigureStarted();

    addArcPoints(cx, cy, rx, ry, 0, 360);
    closeFigure();
}

void Path::drawLine(REAL x1, REAL y1, REAL x2, REAL y2)
{
    currentPos.x = x1;
    currentPos.y = y1;
    ensureFigureStarted();

    commands.push_back(tvg::PathCommand::LineTo);
    tvg::Point ep = {x2, y2};
    points.push_back(ep);
    currentPos = ep;
}

void Path::drawLines(tTJSVariant pts)
{
    vector<PointF> ps;
    getPoints(pts, ps);

    if (ps.size() < 2) return;

    currentPos.x = ps[0].X;
    currentPos.y = ps[0].Y;
    ensureFigureStarted();

    for (size_t i = 1; i < ps.size(); i++) {
        commands.push_back(tvg::PathCommand::LineTo);
        tvg::Point p = {ps[i].X, ps[i].Y};
        points.push_back(p);
        currentPos = p;
    }
}

void Path::drawPolygon(tTJSVariant pts)
{
    vector<PointF> ps;
    getPoints(pts, ps);

    if (ps.size() < 3) return;

    currentPos.x = ps[0].X;
    currentPos.y = ps[0].Y;
    ensureFigureStarted();

    for (size_t i = 1; i < ps.size(); i++) {
        commands.push_back(tvg::PathCommand::LineTo);
        tvg::Point p = {ps[i].X, ps[i].Y};
        points.push_back(p);
        currentPos = p;
    }

    closeFigure();
}

void Path::drawRectangle(REAL x, REAL y, REAL width, REAL height)
{
    currentPos.x = x;
    currentPos.y = y;
    ensureFigureStarted();

    tvg::Point p1 = {x + width, y};
    tvg::Point p2 = {x + width, y + height};
    tvg::Point p3 = {x, y + height};

    commands.push_back(tvg::PathCommand::LineTo);
    points.push_back(p1);
    commands.push_back(tvg::PathCommand::LineTo);
    points.push_back(p2);
    commands.push_back(tvg::PathCommand::LineTo);
    points.push_back(p3);

    closeFigure();
}

void Path::drawRectangles(tTJSVariant rects)
{
    vector<RectF> rs;
    getRects(rects, rs);

    for (size_t i = 0; i < rs.size(); i++) {
        drawRectangle(rs[i].X, rs[i].Y, rs[i].Width, rs[i].Height);
        startFigure();
    }
}

void Path::getPathData(vector<tvg::PathCommand>& cmds, vector<tvg::Point>& pts) const
{
    cmds = commands;
    pts = points;
}

RectF Path::getBounds() const
{
    if (points.empty()) {
        return RectF(0, 0, 0, 0);
    }

    REAL minX = points[0].x;
    REAL minY = points[0].y;
    REAL maxX = points[0].x;
    REAL maxY = points[0].y;

    for (size_t i = 1; i < points.size(); i++) {
        if (points[i].x < minX) minX = points[i].x;
        if (points[i].y < minY) minY = points[i].y;
        if (points[i].x > maxX) maxX = points[i].x;
        if (points[i].y > maxY) maxY = points[i].y;
    }

    return RectF(minX, minY, maxX - minX, maxY - minY);
}

void Path::clear()
{
    commands.clear();
    points.clear();
    currentPos.x = 0;
    currentPos.y = 0;
    figureStartPos.x = 0;
    figureStartPos.y = 0;
    figureStarted = false;
}

void Path::moveTo(REAL x, REAL y)
{
    currentPos.x = x;
    currentPos.y = y;
    commands.push_back(tvg::PathCommand::MoveTo);
    points.push_back(currentPos);
    figureStartPos = currentPos;
    figureStarted = true;
}

void Path::lineTo(REAL x, REAL y)
{
    ensureFigureStarted();
    commands.push_back(tvg::PathCommand::LineTo);
    tvg::Point p = {x, y};
    points.push_back(p);
    currentPos = p;
}

void Path::cubicTo(REAL cx1, REAL cy1, REAL cx2, REAL cy2, REAL x, REAL y)
{
    ensureFigureStarted();
    commands.push_back(tvg::PathCommand::CubicTo);
    tvg::Point cp1 = {cx1, cy1};
    tvg::Point cp2 = {cx2, cy2};
    tvg::Point ep = {x, y};
    points.push_back(cp1);
    points.push_back(cp2);
    points.push_back(ep);
    currentPos = ep;
}

void Path::addPath(const Path& other, REAL offsetX, REAL offsetY)
{
    // 他のパスのコマンドと点を追加（オフセット付き）
    size_t ptIdx = 0;
    for (size_t i = 0; i < other.commands.size(); i++) {
        commands.push_back(other.commands[i]);

        switch (other.commands[i]) {
            case tvg::PathCommand::MoveTo:
            case tvg::PathCommand::LineTo: {
                tvg::Point p = {other.points[ptIdx].x + offsetX, other.points[ptIdx].y + offsetY};
                points.push_back(p);
                currentPos = p;
                if (other.commands[i] == tvg::PathCommand::MoveTo) {
                    figureStartPos = p;
                    figureStarted = true;
                }
                ptIdx++;
                break;
            }
            case tvg::PathCommand::CubicTo: {
                tvg::Point cp1 = {other.points[ptIdx].x + offsetX, other.points[ptIdx].y + offsetY};
                tvg::Point cp2 = {other.points[ptIdx + 1].x + offsetX, other.points[ptIdx + 1].y + offsetY};
                tvg::Point ep = {other.points[ptIdx + 2].x + offsetX, other.points[ptIdx + 2].y + offsetY};
                points.push_back(cp1);
                points.push_back(cp2);
                points.push_back(ep);
                currentPos = ep;
                ptIdx += 3;
                break;
            }
            case tvg::PathCommand::Close: {
                currentPos = figureStartPos;
                figureStarted = false;
                break;
            }
        }
    }
}

// --------------------------------------------------------
// LayerExDraw クラス
// --------------------------------------------------------

LayerExDraw::LayerExDraw(DispatchT obj)
    : layerExBase(obj), width(-1), height(-1), pitch(0), buffer(NULL),
      canvas(NULL), flipped(false),
      clipLeft(-1), clipTop(-1), clipWidth(-1), clipHeight(-1),
      smoothingMode(4), // SmoothingModeAntiAlias
      updateWhenDraw(true)
{
}

LayerExDraw::~LayerExDraw()
{
    if (canvas) {
        delete canvas;
        canvas = NULL;
    }
}

void LayerExDraw::reset()
{
    layerExBase::reset();

    // 変更されている場合は作り直し
    if (!(canvas &&
          width == _width &&
          height == _height &&
          pitch == _pitch &&
          buffer == _buffer)) {

        if (canvas) {
            delete canvas;
            canvas = NULL;
        }

        width = _width;
        height = _height;
        pitch = _pitch;
        buffer = _buffer;
        // スクラッチは常にトップダウン。 レイヤの上下反転 (負 pitch) は
        // flushToLayer の行アドレス計算 (buffer + y*pitch) が吸収するので、
        // 描画側の反転補正は不要になった
        flipped = false;

        scratch.assign((size_t)width * height, 0);
        canvas = tvg::SwCanvas::gen();
        if (canvas) {
            canvas->target(scratch.data(), width, width, height,
                           tvg::ColorSpace::ARGB8888);
        }

        clipWidth = clipHeight = -1;
    }

    // クリッピング領域変更の場合は設定し直し
    if (_clipLeft != clipLeft ||
        _clipTop != clipTop ||
        _clipWidth != clipWidth ||
        _clipHeight != clipHeight) {
        clipLeft = _clipLeft;
        clipTop = _clipTop;
        clipWidth = _clipWidth;
        clipHeight = _clipHeight;

        if (canvas) {
            if (flipped) {
                // 上下反転時は viewport の Y 座標も反転
                canvas->viewport(clipLeft, height - clipTop - clipHeight, clipWidth, clipHeight);
            } else {
                canvas->viewport(clipLeft, clipTop, clipWidth, clipHeight);
            }
        }
    }

    updateTransform();
}

// スクラッチ (premultiplied ARGB、 トップダウン) の rect 部分をレイヤ
// バッファ (straight ARGB、 krkrz 規約) へ α 合成する。 合成した範囲の
// スクラッチはクリアし、 canvas の保持シェイプも捨てる (保持シーンの
// 差分再描画で krkrz 側の下地が消えるのを防ぐため、 シーンは draw ごとに
// 使い捨てる)。 SmoothingModeNone / HighSpeed のときは合成時にカバレッジ
// 50% のしきい値で二値化し、 非 AA (ジャギー) 描画と同等の見た目にする。
void LayerExDraw::flushToLayer(const RectF &rect)
{
    if (!buffer || width <= 0 || height <= 0 || scratch.empty()) {
        if (canvas) canvas->remove();
        return;
    }
    int x0 = (int)rect.X - 2, y0 = (int)rect.Y - 2;
    int x1 = (int)(rect.X + rect.Width) + 3;
    int y1 = (int)(rect.Y + rect.Height) + 3;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > width)  x1 = width;
    if (y1 > height) y1 = height;
    if (x0 >= x1 || y0 >= y1) {
        if (canvas) canvas->remove();
        return;
    }

    const bool binarize = (smoothingMode == SmoothingModeNone ||
                           smoothingMode == SmoothingModeHighSpeed);
    for (int y = y0; y < y1; y++) {
        uint32_t *srow = scratch.data() + (size_t)y * width;
        uint32_t *drow = (uint32_t*)(buffer + (ptrdiff_t)y * pitch);
        for (int x = x0; x < x1; x++) {
            uint32_t s = srow[x];
            uint32_t sa = s >> 24;
            if (!sa) continue;
            uint32_t sr = (s >> 16) & 0xFF, sg = (s >> 8) & 0xFF, sb = s & 0xFF;
            if (sa < 255) {
                // premultiplied -> straight
                sr = (sr * 255 + sa / 2) / sa; if (sr > 255) sr = 255;
                sg = (sg * 255 + sa / 2) / sa; if (sg > 255) sg = 255;
                sb = (sb * 255 + sa / 2) / sa; if (sb > 255) sb = 255;
            }
            if (binarize) {
                if (sa < 128) continue;
                sa = 255;
            }
            if (sa == 255) {
                drow[x] = 0xFF000000u | (sr << 16) | (sg << 8) | sb;
            } else {
                uint32_t d = drow[x];
                uint32_t da = d >> 24;
                uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
                uint32_t oa = sa + da * (255 - sa) / 255;
                uint32_t orr = 0, og = 0, ob = 0;
                if (oa) {
                    orr = (sr * sa + dr * da * (255 - sa) / 255) / oa;
                    og  = (sg * sa + dg * da * (255 - sa) / 255) / oa;
                    ob  = (sb * sa + db * da * (255 - sa) / 255) / oa;
                    if (orr > 255) orr = 255;
                    if (og  > 255) og  = 255;
                    if (ob  > 255) ob  = 255;
                }
                drow[x] = (oa << 24) | (orr << 16) | (og << 8) | ob;
            }
        }
        memset(srow + x0, 0, (size_t)(x1 - x0) * sizeof(uint32_t));
    }
    if (canvas) canvas->remove();
}

void LayerExDraw::updateRect(RectF &rect)
{
    flushToLayer(rect);
    if (updateWhenDraw) {
        // 上下反転時は更新矩形の Y 座標を反転
        REAL y = flipped ? (REAL)(height - rect.Y - rect.Height) : rect.Y;
        tTJSVariant vars[4] = { rect.X, y, rect.Width, rect.Height };
        tTJSVariant *varsp[4] = { vars, vars+1, vars+2, vars+3 };
        _pUpdate(4, varsp);
    }
}

void LayerExDraw::updateTransform()
{
    calcTransform.Reset();
    if (flipped) {
        // 上下反転: Y軸を反転して height 分オフセット
        calcTransform.Scale(1, -1, MatrixOrderAppend);
        calcTransform.Translate(0, -(REAL)height, MatrixOrderAppend);
    }
    calcTransform.Multiply(&transform, MatrixOrderAppend);
    calcTransform.Multiply(&viewTransform, MatrixOrderAppend);
}

void LayerExDraw::setViewTransform(const ::Matrix *trans)
{
    if (!viewTransform.Equals(trans)) {
        viewTransform.Reset();
        viewTransform.Multiply(trans);
        updateTransform();
    }
}

void LayerExDraw::resetViewTransform()
{
    viewTransform.Reset();
    updateTransform();
}

void LayerExDraw::rotateViewTransform(REAL angle)
{
    viewTransform.Rotate(angle, MatrixOrderAppend);
    updateTransform();
}

void LayerExDraw::scaleViewTransform(REAL sx, REAL sy)
{
    viewTransform.Scale(sx, sy, MatrixOrderAppend);
    updateTransform();
}

void LayerExDraw::translateViewTransform(REAL dx, REAL dy)
{
    viewTransform.Translate(dx, dy, MatrixOrderAppend);
    updateTransform();
}

void LayerExDraw::setTransform(const ::Matrix *trans)
{
    if (!transform.Equals(trans)) {
        transform.Reset();
        transform.Multiply(trans);
        updateTransform();
    }
}

void LayerExDraw::resetTransform()
{
    transform.Reset();
    updateTransform();
}

void LayerExDraw::rotateTransform(REAL angle)
{
    transform.Rotate(angle, MatrixOrderAppend);
    updateTransform();
}

void LayerExDraw::scaleTransform(REAL sx, REAL sy)
{
    transform.Scale(sx, sy, MatrixOrderAppend);
    updateTransform();
}

void LayerExDraw::translateTransform(REAL dx, REAL dy)
{
    transform.Translate(dx, dy, MatrixOrderAppend);
    updateTransform();
}

void LayerExDraw::clear(ARGB argb)
{
    if (canvas) canvas->remove();
    if (!buffer || width <= 0 || height <= 0) return;

    // レイヤバッファ (straight ARGB) を直接塗りつぶす。 スクラッチ合成方式
    // では背景をシーンとして保持しないため、 ThorVG を介す必要がない
    uint32_t col = (uint32_t)argb;
    for (int y = 0; y < height; y++) {
        uint32_t *drow = (uint32_t*)(buffer + (ptrdiff_t)y * pitch);
        for (int x = 0; x < width; x++) drow[x] = col;
    }
    if (!scratch.empty())
        memset(scratch.data(), 0, scratch.size() * sizeof(uint32_t));

    _pUpdate(0, NULL);
}

RectF LayerExDraw::addPathGradientPaint(const Appearance::DrawInfo &d, tvg::Shape *mask, const tvg::Matrix &tm)
{
    // 描く範囲 (デバイス座標)。 Clamp は多角形の外を塗らないので多角形の外接矩形で足りる。
    // Tile / TileFlip* は外接矩形の外も繰り返すのでクリップ範囲全体
    double x0, y0, x1, y1;
    int cl = clipWidth  > 0 ? clipLeft : 0, ct = clipHeight > 0 ? clipTop : 0;
    int cw = clipWidth  > 0 ? clipWidth  : width, ch = clipHeight > 0 ? clipHeight : height;
    if (d.pgWrap == WrapModeClamp) {
        x0 = y0 = 1e30; x1 = y1 = -1e30;
        for (size_t i = 0; i < d.pgX.size(); i++) {
            double dx = tm.e11 * d.pgX[i] + tm.e12 * d.pgY[i] + tm.e13;
            double dy = tm.e21 * d.pgX[i] + tm.e22 * d.pgY[i] + tm.e23;
            if (dx < x0) x0 = dx; if (dx > x1) x1 = dx;
            if (dy < y0) y0 = dy; if (dy > y1) y1 = dy;
        }
        x0 = floor(x0) - 1; y0 = floor(y0) - 1; x1 = ceil(x1) + 1; y1 = ceil(y1) + 1;
        if (x0 < cl) x0 = cl; if (y0 < ct) y0 = ct;
        if (x1 > cl + cw) x1 = cl + cw; if (y1 > ct + ch) y1 = ct + ch;
    } else {
        x0 = cl; y0 = ct; x1 = cl + cw; y1 = ct + ch;
    }
    int ox = (int)x0, oy = (int)y0, W = (int)(x1 - x0), H = (int)(y1 - y0);
    double det = tm.e11 * tm.e22 - tm.e12 * tm.e21;
    if (W <= 0 || H <= 0 || fabs(det) < 1e-12) { tvg::Paint::rel(mask); return RectF(); }

    // 多角形の外接矩形 (Tile / TileFlip* の畳み込み用。 ユーザー座標)
    REAL bx0 = d.pgX[0], by0 = d.pgY[0], bx1 = bx0, by1 = by0;
    for (size_t i = 1; i < d.pgX.size(); i++) {
        if (d.pgX[i] < bx0) bx0 = d.pgX[i]; if (d.pgX[i] > bx1) bx1 = d.pgX[i];
        if (d.pgY[i] < by0) by0 = d.pgY[i]; if (d.pgY[i] > by1) by1 = d.pgY[i];
    }
    bool wrap  = d.pgWrap != WrapModeClamp;
    bool flipX = d.pgWrap == WrapModeTileFlipX || d.pgWrap == WrapModeTileFlipXY;
    bool flipY = d.pgWrap == WrapModeTileFlipY || d.pgWrap == WrapModeTileFlipXY;

    // 画素の中心をユーザー座標へ戻して色を決める (ARGB、 乗算済みでない)
    std::vector<uint32_t> buf((size_t)W * H, 0);
    int hint = 0;
    for (int y = 0; y < H; y++) {
        double dy = oy + y + 0.5 - tm.e23;
        for (int x = 0; x < W; x++) {
            double dx = ox + x + 0.5 - tm.e13;
            REAL ux = (REAL)(( tm.e22 * dx - tm.e12 * dy) / det);
            REAL uy = (REAL)((-tm.e21 * dx + tm.e11 * dy) / det);
            if (wrap && (ux < bx0 || ux > bx1 || uy < by0 || uy > by1)) {
                ux = wrapCoord(ux, bx0, bx1 - bx0, flipX);
                uy = wrapCoord(uy, by0, by1 - by0, flipY);
            }
            buf[(size_t)y * W + x] = pathGradientColor(d, ux, uy, hint);
        }
    }

    tvg::Picture* pic = tvg::Picture::gen();
    if (!pic || pic->load(buf.data(), W, H, tvg::ColorSpace::ARGB8888S, true) != tvg::Result::Success) {
        if (pic) tvg::Paint::rel(pic);
        tvg::Paint::rel(mask);
        return RectF();
    }
    pic->translate((float)ox, (float)oy);
    pic->mask(mask, tvg::MaskMethod::Alpha); // 図形のアルファで切り抜く (所有権は pic へ)
    canvas->add(pic);
    return RectF((REAL)ox, (REAL)oy, (REAL)W, (REAL)H);
}


void LayerExDraw::setRecord(bool rec)
{
    if (rec) { if (!recording) recording = std::make_shared<RecordData>(); }
    else recording.reset();
}

::Image* LayerExDraw::getRecordImage()
{
    if (!recording) return nullptr;
    // 写しを渡す (この後の記録は画像に混ざらない。 GDI+ も記録を閉じて新しく始め直す)
    ::Image* img = new ::Image();
    img->setRecord(std::make_shared<RecordData>(*recording));
    return img;
}

RectF LayerExDraw::drawRecordImage(const RecordData &rec, const tvg::Matrix &map, REAL sleft, REAL stop, REAL swidth, REAL sheight)
{
    // 元の範囲を描き先 (デバイス座標) へ写した外接矩形で切り抜く
    tvg::Matrix toDev = mulMatrix(calcTransform.m, map);
    REAL sx[4] = { sleft, sleft + swidth, sleft, sleft + swidth };
    REAL sy[4] = { stop,  stop,           stop + sheight, stop + sheight };
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (int i = 0; i < 4; i++) {
        double x = toDev.e11 * sx[i] + toDev.e12 * sy[i] + toDev.e13;
        double y = toDev.e21 * sx[i] + toDev.e22 * sy[i] + toDev.e23;
        if (i == 0 || x < x0) x0 = x; if (i == 0 || x > x1) x1 = x;
        if (i == 0 || y < y0) y0 = y; if (i == 0 || y > y1) y1 = y;
    }
    int cl = clipWidth  > 0 ? clipLeft : 0, ct = clipHeight > 0 ? clipTop : 0;
    int cw = clipWidth  > 0 ? clipWidth  : width, ch = clipHeight > 0 ? clipHeight : height;
    int vx0 = (int)floor(x0 + 0.5), vy0 = (int)floor(y0 + 0.5), vx1 = (int)floor(x1 + 0.5), vy1 = (int)floor(y1 + 0.5);
    if (vx0 < cl) vx0 = cl; if (vy0 < ct) vy0 = ct;
    if (vx1 > cl + cw) vx1 = cl + cw; if (vy1 > ct + ch) vy1 = ct + ch;
    if (vx1 <= vx0 || vy1 <= vy0) return RectF();

    canvas->viewport(vx0, vy0, vx1 - vx0, vy1 - vy0);
    for (size_t i = 0; i < rec.entries.size(); i++) {
        const RecordData::Entry &e = rec.entries[i];
        if (!e.shape) continue;
        tvg::Matrix m = mulMatrix(toDev, e.transform);
        drawShapeWithAppearanceM(&e.app, (tvg::Shape*)e.shape->duplicate(), m, true);
    }
    canvas->viewport(cl, ct, cw, ch);

    RectF r((REAL)vx0, (REAL)vy0, (REAL)(vx1 - vx0), (REAL)(vy1 - vy0));
    updateRect(r);
    return r;
}

RectF LayerExDraw::drawShapeWithAppearance(const Appearance *app, tvg::Shape* baseShape)
{
    // record 中は描画命令を記録する (GDI+ と同じく transform まで。 viewTransform は含めない)
    if (recording && app && baseShape) recording->add(app, baseShape, transform.m);
    return drawShapeWithAppearanceM(app, baseShape, calcTransform.m, false);
}

RectF LayerExDraw::drawShapeWithAppearanceM(const Appearance *app, tvg::Shape* baseShape, const tvg::Matrix &base, bool userOffset)
{
    if (!canvas || !app || !baseShape) {
        tvg::Paint::rel(baseShape);
        return RectF();
    }

    RectF totalBounds;
    bool first = true;

    // 描画情報を使って次々描画
    for (size_t i = 0; i < app->drawInfos.size(); i++) {
        const Appearance::DrawInfo& info = app->drawInfos[i];

        // シェイプを複製
        tvg::Shape* shape = (tvg::Shape*)baseShape->duplicate();
        if (!shape) continue;

        // オフセットとトランスフォームを適用
        tvg::Matrix tm;
        tm.e11 = base.e11;
        tm.e12 = base.e12;
        tm.e21 = base.e21;
        tm.e22 = base.e22;
        if (userOffset) {
            // ずらしをユーザー座標で掛ける (記録の描き直しで拡大縮小に追従)
            tm.e13 = base.e11 * info.ox + base.e12 * info.oy + base.e13;
            tm.e23 = base.e21 * info.ox + base.e22 * info.oy + base.e23;
        } else {
            tm.e13 = base.e13 + info.ox;
            tm.e23 = base.e23 + info.oy;
        }
        tm.e31 = 0;
        tm.e32 = 0;
        tm.e33 = 1;
        shape->transform(tm);

        // GDI+ の既定 FillMode は Alternate (even-odd)。ThorVG 既定の NonZero の
        // ままだと複数サブパスの「穴」まで塗り潰されるので塗り時は even-odd に揃える
        if (info.type != 0) shape->fillRule(tvg::FillRule::EvenOdd);

        if (info.type == 0) { // ストローク
            shape->strokeWidth(info.strokeWidth);
            shape->strokeFill(info.strokeR, info.strokeG, info.strokeB, info.strokeA);
            shape->strokeCap(info.strokeCap);
            shape->strokeJoin(info.strokeJoin);
            shape->strokeMiterlimit(info.miterLimit);

            if (!info.dashPattern.empty()) {
                shape->strokeDash(info.dashPattern.data(), (uint32_t)info.dashPattern.size(), info.dashOffset);
                // ThorVG は破線の区切りごとに線端を付ける。 GDI+ は区切りの端を DashCap
                // (既定 Flat) で描くので合わせる (Square のままだと隙間が埋まる)
                shape->strokeCap(info.dashCap == DashCapRound ? tvg::StrokeCap::Round : tvg::StrokeCap::Butt);
            }

            // 線をブラシで塗る (Pen(brush))。 パスグラデーションは下で別に扱う
            if (info.useLinearGradient) {
                tvg::LinearGradient* g = tvg::LinearGradient::gen();
                g->linear(info.gradX1, info.gradY1, info.gradX2, info.gradY2);
                g->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                g->spread(info.gradSpread);
                shape->strokeFill(g);
            } else if (info.useRadialGradient && !info.usePathGradient) {
                tvg::RadialGradient* g = tvg::RadialGradient::gen();
                g->radial(info.gradCx, info.gradCy, info.gradR, info.gradCx, info.gradCy, 0);
                g->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                g->spread(info.gradSpread);
                shape->strokeFill(g);
            }

            // フィル色を透明に
            shape->fill(0, 0, 0, 0);
        } else if (info.type == 1 && info.useTextureFill && !info.texPixels.empty()
                   && info.texW > 0 && info.texH > 0) {
            // テクスチャ(タイル)フィル: shape 範囲にタイル展開した Picture を
            // 生成し、shape をクリッパとしてクリップ配置する。
            float bx, by, bw, bh;
            bool done = false;
            if (shape->bounds(&bx, &by, &bw, &bh) == tvg::Result::Success && bw > 0 && bh > 0) {
                int ox = (int)floorf(bx);
                int oy = (int)floorf(by);
                int W  = (int)ceilf(bx + bw) - ox;
                int H  = (int)ceilf(by + bh) - oy;
                if (W > 0 && H > 0) {
                    int tw = info.texW, th = info.texH;
                    std::vector<uint32_t> tiled((size_t)W * H);
                    for (int y = 0; y < H; y++) {
                        // タイル位相は視覚(トップダウン)座標の(0,0)基準。flipped 時は
                        // キャンバス行がボトムアップなので視覚行に直してから位相を取る
                        int cy = oy + y;
                        int vy = flipped ? (height - 1 - cy) : cy;
                        int sy = (vy % th + th) % th;
                        for (int x = 0; x < W; x++) {
                            int sx = ((ox + x) % tw + tw) % tw;
                            tiled[(size_t)y*W + x] = info.texPixels[(size_t)sy*tw + sx];
                        }
                    }
                    tvg::Picture* pic = tvg::Picture::gen();
                    if (pic && pic->load(tiled.data(), W, H, tvg::ColorSpace::ARGB8888, true) == tvg::Result::Success) {
                        pic->translate((float)ox, (float)oy);
                        shape->fill(255, 255, 255, 255); // クリッパのカバレッジ確保
                        shape->strokeWidth(0);
                        pic->clip(shape);       // shape をクリッパに (所有権は pic へ移る)
                        canvas->add(pic);
                        RectF bounds(bx + info.ox, by + info.oy, bw, bh);
                        if (first) { totalBounds = bounds; first = false; }
                        else RectF::Union(totalBounds, totalBounds, bounds);
                        done = true;
                    } else if (pic) {
                        tvg::Paint::rel(pic);
                    }
                }
            }
            if (done) continue;         // 通常の shape 追加をスキップ
            // タイル失敗時は透明フィルで素通し
            shape->fill(0, 0, 0, 0);
            shape->strokeWidth(0);
        } else { // フィル
            if (info.useLinearGradient) {
                tvg::LinearGradient* grad = tvg::LinearGradient::gen();
                grad->linear(info.gradX1, info.gradY1, info.gradX2, info.gradY2);
                grad->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                grad->spread(info.gradSpread);
                shape->fill(grad);
            } else if (info.useRadialGradient) {
                tvg::RadialGradient* grad = tvg::RadialGradient::gen();
                grad->radial(info.gradCx, info.gradCy, info.gradR, info.gradCx, info.gradCy, 0);
                grad->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                grad->spread(info.gradSpread);
                shape->fill(grad);
            } else {
                shape->fill(info.fillR, info.fillG, info.fillB, info.fillA);
            }

            // ストロークなし
            shape->strokeWidth(0);
        }

        if (info.usePathGradient) {
            // 図形 (塗りなら塗り、線なら線) を不透明の白にしてアルファマスクにする
            if (info.type == 0) {
                shape->strokeFill(255, 255, 255, 255);
                shape->fill(0, 0, 0, 0);
            } else {
                shape->fill(255, 255, 255, 255);
                shape->strokeWidth(0);
            }
            RectF r = addPathGradientPaint(info, shape, tm); // shape の所有権は移る
            if (r.Width > 0 && r.Height > 0) {
                if (first) { totalBounds = r; first = false; }
                else RectF::Union(totalBounds, totalBounds, r);
            }
            continue;
        }

        canvas->add(shape);

        // バウンディングボックスを計算
        // （簡易的にパスから計算）
        float bx, by, bw, bh;
        if (shape->bounds(&bx, &by, &bw, &bh) == tvg::Result::Success) {
            RectF bounds(bx + info.ox, by + info.oy, bw, bh);
            if (first) {
                totalBounds = bounds;
                first = false;
            } else {
                RectF::Union(totalBounds, totalBounds, bounds);
            }
        }
    }

    // 描画を実行
    canvas->draw();
    canvas->sync();

    // 元のシェイプを解放
    tvg::Paint::rel(baseShape);

    updateRect(totalBounds);
    return totalBounds;
}

RectF LayerExDraw::drawPath(const Appearance *app, const ::Path *path)
{
    if (!path) return RectF();

    tvg::Shape* shape = tvg::Shape::gen();

    vector<tvg::PathCommand> cmds;
    vector<tvg::Point> pts;
    path->getPathData(cmds, pts);

    if (!cmds.empty()) {
        shape->appendPath(cmds.data(), (uint32_t)cmds.size(), pts.data(), (uint32_t)pts.size());
    }

    return drawShapeWithAppearance(app, shape);
}

RectF LayerExDraw::drawArc(const Appearance *app, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle)
{
    ::Path path;
    path.drawArc(x, y, width, height, startAngle, sweepAngle);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawPie(const Appearance *app, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle)
{
    ::Path path;
    path.drawPie(x, y, width, height, startAngle, sweepAngle);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawBezier(const Appearance *app, REAL x1, REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4)
{
    ::Path path;
    path.drawBezier(x1, y1, x2, y2, x3, y3, x4, y4);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawBeziers(const Appearance *app, tTJSVariant points)
{
    ::Path path;
    path.drawBeziers(points);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawClosedCurve(const Appearance *app, tTJSVariant points)
{
    ::Path path;
    path.drawClosedCurve(points);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawClosedCurve2(const Appearance *app, tTJSVariant points, REAL tension)
{
    ::Path path;
    path.drawClosedCurve2(points, tension);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawCurve(const Appearance *app, tTJSVariant points)
{
    ::Path path;
    path.drawCurve(points);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawCurve2(const Appearance *app, tTJSVariant points, REAL tension)
{
    ::Path path;
    path.drawCurve2(points, tension);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawCurve3(const Appearance *app, tTJSVariant points, int offset, int numberOfSegments, REAL tension)
{
    ::Path path;
    path.drawCurve3(points, offset, numberOfSegments, tension);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawEllipse(const Appearance *app, REAL x, REAL y, REAL width, REAL height)
{
    tvg::Shape* shape = tvg::Shape::gen();
    shape->appendCircle(x + width/2, y + height/2, width/2, height/2);
    return drawShapeWithAppearance(app, shape);
}

RectF LayerExDraw::drawLine(const Appearance *app, REAL x1, REAL y1, REAL x2, REAL y2)
{
    ::Path path;
    path.drawLine(x1, y1, x2, y2);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawLines(const Appearance *app, tTJSVariant points)
{
    ::Path path;
    path.drawLines(points);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawPolygon(const Appearance *app, tTJSVariant points)
{
    ::Path path;
    path.drawPolygon(points);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawRectangle(const Appearance *app, REAL x, REAL y, REAL width, REAL height)
{
    tvg::Shape* shape = tvg::Shape::gen();
    shape->appendRect(x, y, width, height);
    return drawShapeWithAppearance(app, shape);
}

RectF LayerExDraw::drawRectangles(const Appearance *app, tTJSVariant rects)
{
    vector<RectF> rs;
    getRects(rects, rs);

    tvg::Shape* shape = tvg::Shape::gen();
    for (size_t i = 0; i < rs.size(); i++) {
        shape->appendRect(rs[i].X, rs[i].Y, rs[i].Width, rs[i].Height);
    }

    return drawShapeWithAppearance(app, shape);
}

// --------------------------------------------------------
// フォント管理
// --------------------------------------------------------

// UTF-16 を UTF-8 に変換するヘルパー関数
static std::string wcharToUtf8(const tjs_char* wstr)
{
    if (!wstr) return "";

    std::string result;
    while (*wstr) {
        uint32_t ch = *wstr++;

        // サロゲートペアの処理
        if (ch >= 0xD800 && ch <= 0xDBFF && *wstr >= 0xDC00 && *wstr <= 0xDFFF) {
            uint32_t high = ch;
            uint32_t low = *wstr++;
            ch = 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00);
        }

        if (ch < 0x80) {
            result += (char)ch;
        } else if (ch < 0x800) {
            result += (char)(0xC0 | (ch >> 6));
            result += (char)(0x80 | (ch & 0x3F));
        } else if (ch < 0x10000) {
            result += (char)(0xE0 | (ch >> 12));
            result += (char)(0x80 | ((ch >> 6) & 0x3F));
            result += (char)(0x80 | (ch & 0x3F));
        } else {
            result += (char)(0xF0 | (ch >> 18));
            result += (char)(0x80 | ((ch >> 12) & 0x3F));
            result += (char)(0x80 | ((ch >> 6) & 0x3F));
            result += (char)(0x80 | (ch & 0x3F));
        }
    }
    return result;
}

// フォントデータを保持するマップ（アンロード時に解放するため）
static std::map<std::string, uint8_t*> loadedFontData;

bool GdiPlus::loadFont(const tjs_char *path, const tjs_char *name)
{
    if (!path) return false;

    // 吉里吉里のパス解決を使用
    ttstr resolved = TVPGetPlacedPath(ttstr(path));
    if (resolved.length() == 0) {
        resolved = path;
    }

    // ストリームを開く
    iTJSBinaryStream* stream = TVPCreateStream(resolved, TJS_BS_READ);
    if (!stream) {
        return false;
    }

    // ファイルサイズ取得
    uint32_t dataSize = (uint32_t)stream->GetSize();

    // データ読み込み
    uint8_t* fontData = new uint8_t[dataSize];
    tjs_uint bytesRead = stream->Read(fontData, dataSize);
    stream->Destruct();

    if (bytesRead != dataSize) {
        delete[] fontData;
        return false;
    }

    // 登録名を決定
    std::string fontName;
    if (name && *name) {
        fontName = wcharToUtf8(name);
    } else {
        // パスからファイル名を抽出して使用
        fontName = wcharToUtf8(path);
        size_t lastSlash = fontName.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            fontName = fontName.substr(lastSlash + 1);
        }
        // 拡張子を除去
        size_t lastDot = fontName.find_last_of('.');
        if (lastDot != std::string::npos) {
            fontName = fontName.substr(0, lastDot);
        }
    }

    // 同名の既存登録があれば先に解除してから旧バイトを解放する
    // (copy=false ではローダが旧バイトを借用参照しているため、解放前の解除が必須)
    {
        auto it = loadedFontData.find(fontName);
        if (it != loadedFontData.end()) {
            tvg::Text::load(fontName.c_str(), nullptr, 0, "ttf", false);
            delete[] it->second;
            loadedFontData.erase(it);
        }
    }

    // ThorVG にフォントを登録。バイトは loadedFontData がアンロードまで保持する
    // ので copy=false で渡す (gw/ft どちらのローダも借用参照で開ける。従来の
    // copy=true はローダ側にもう 1 コピー作っていた)
    tvg::Result result = tvg::Text::load(fontName.c_str(), (const char*)fontData, dataSize, "ttf", false);

    if (result == tvg::Result::Success) {
        loadedFontData[fontName] = fontData;
        return true;
    }
    delete[] fontData;
    return false;
}

bool GdiPlus::unloadFont(const tjs_char *name)
{
    if (!name) return false;

    std::string fontName = wcharToUtf8(name);

    // ThorVGからアンロード（データをnullptrでloadするとアンロードされる）
    tvg::Result result = tvg::Text::load(fontName.c_str(), nullptr, 0, "ttf", false);

    // 保持しているデータを解放
    auto it = loadedFontData.find(fontName);
    if (it != loadedFontData.end()) {
        delete[] it->second;
        loadedFontData.erase(it);
    }

    return result == tvg::Result::Success;
}

// --------------------------------------------------------
// 文字列描画
// --------------------------------------------------------

RectF LayerExDraw::drawString(const FontInfo *font, const Appearance *app, REAL x, REAL y, const tjs_char *text)
{
    if (!canvas || !font || !text || !app) return RectF();

    std::string fontName = wcharToUtf8(font->fontFamily.c_str());
    std::string textUtf8 = wcharToUtf8(text);

    RectF totalBounds;
    bool first = true;

    // Appearance の各描画情報に対して Text オブジェクトを生成して描画
    for (size_t i = 0; i < app->drawInfos.size(); i++) {
        const Appearance::DrawInfo& info = app->drawInfos[i];

        // Text オブジェクトを生成
        tvg::Text* textObj = tvg::Text::gen();
        if (!textObj) continue;

        // フォントとサイズを設定
        textObj->font(fontName.c_str());
        textObj->size(font->fontSize);

        // テキストを設定
        textObj->text(textUtf8.c_str());

        // テキストパラメータを設定
        if (font->italic > 0) {
            textObj->italic(font->italic);
        }
        textObj->spacing(font->letterSpacing, font->lineSpacing);

        // ストロークまたはフィルを設定
        if (info.type == 0) {
            // ストローク
            textObj->outline(info.strokeWidth, info.strokeR, info.strokeG, info.strokeB);
            textObj->fill(0, 0, 0); // 塗りつぶしなし
        } else {
            // フィル
            if (info.useLinearGradient || info.useRadialGradient) {
                // グラデーションフィル
                tvg::Fill* grad = nullptr;
                if (info.useLinearGradient) {
                    tvg::LinearGradient* lg = tvg::LinearGradient::gen();
                    lg->linear(info.gradX1, info.gradY1, info.gradX2, info.gradY2);
                    lg->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                    lg->spread(info.gradSpread);
                    grad = lg;
                } else {
                    tvg::RadialGradient* rg = tvg::RadialGradient::gen();
                    rg->radial(info.gradCx, info.gradCy, info.gradR, info.gradCx, info.gradCy, 0);
                    rg->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                    rg->spread(info.gradSpread);
                    grad = rg;
                }
                textObj->fill(grad);
            } else {
                textObj->fill(info.fillR, info.fillG, info.fillB);
            }
        }

        // トランスフォームを適用
        Matrix transform;
        transform.Translate(x + info.ox, y + info.oy);
        transform.Multiply(&calcTransform, MatrixOrderPrepend);

        tvg::Matrix tm;
        tm.e11 = transform.m.e11;
        tm.e12 = transform.m.e12;
        tm.e13 = transform.m.e13;
        tm.e21 = transform.m.e21;
        tm.e22 = transform.m.e22;
        tm.e23 = transform.m.e23;
        tm.e31 = 0;
        tm.e32 = 0;
        tm.e33 = 1;
        textObj->transform(tm);

        // キャンバスに追加
        canvas->add(textObj);

        // バウンディングボックスを計算
        float bx, by, bw, bh;
        if (textObj->bounds(&bx, &by, &bw, &bh) == tvg::Result::Success) {
            RectF bounds(bx + info.ox, by + info.oy, bw, bh);
            if (first) {
                totalBounds = bounds;
                first = false;
            } else {
                RectF::Union(totalBounds, totalBounds, bounds);
            }
        }
    }

    // 描画を実行
    canvas->draw();
    canvas->sync();

    updateRect(totalBounds);
    return totalBounds;
}

RectF LayerExDraw::drawStringArea(const FontInfo *font, const Appearance *app, REAL x, REAL y, REAL w, REAL h, REAL alignX, REAL alignY, int wrap, const tjs_char *text)
{
    if (!canvas || !font || !text || !app) return RectF();

    std::string fontName = wcharToUtf8(font->fontFamily.c_str());
    std::string textUtf8 = wcharToUtf8(text);

    RectF totalBounds;
    bool first = true;

    // Appearance の各描画情報に対して Text オブジェクトを生成して描画
    for (size_t i = 0; i < app->drawInfos.size(); i++) {
        const Appearance::DrawInfo& info = app->drawInfos[i];

        // Text オブジェクトを生成
        tvg::Text* textObj = tvg::Text::gen();
        if (!textObj) continue;

        // フォントとサイズを設定
        textObj->font(fontName.c_str());
        textObj->size(font->fontSize);

        // テキストを設定
        textObj->text(textUtf8.c_str());

        // 矩形領域用のレイアウトを設定
        textObj->layout(w, h);
        textObj->align(alignX, alignY);
        if (wrap != 0) {
            textObj->wrap((tvg::TextWrap)wrap);
        }
        if (font->italic > 0) {
            textObj->italic(font->italic);
        }
        textObj->spacing(font->letterSpacing, font->lineSpacing);

        // ストロークまたはフィルを設定
        if (info.type == 0) {
            // ストローク
            textObj->outline(info.strokeWidth, info.strokeR, info.strokeG, info.strokeB);
            textObj->fill(0, 0, 0);
        } else {
            // フィル
            if (info.useLinearGradient || info.useRadialGradient) {
                tvg::Fill* grad = nullptr;
                if (info.useLinearGradient) {
                    tvg::LinearGradient* lg = tvg::LinearGradient::gen();
                    lg->linear(info.gradX1, info.gradY1, info.gradX2, info.gradY2);
                    lg->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                    lg->spread(info.gradSpread);
                    grad = lg;
                } else {
                    tvg::RadialGradient* rg = tvg::RadialGradient::gen();
                    rg->radial(info.gradCx, info.gradCy, info.gradR, info.gradCx, info.gradCy, 0);
                    rg->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                    rg->spread(info.gradSpread);
                    grad = rg;
                }
                textObj->fill(grad);
            } else {
                textObj->fill(info.fillR, info.fillG, info.fillB);
            }
        }

        // トランスフォームを適用
        Matrix transform;
        transform.Translate(x + info.ox, y + info.oy);
        transform.Multiply(&calcTransform, MatrixOrderPrepend);

        tvg::Matrix tm;
        tm.e11 = transform.m.e11;
        tm.e12 = transform.m.e12;
        tm.e13 = transform.m.e13;
        tm.e21 = transform.m.e21;
        tm.e22 = transform.m.e22;
        tm.e23 = transform.m.e23;
        tm.e31 = 0;
        tm.e32 = 0;
        tm.e33 = 1;
        textObj->transform(tm);

        // キャンバスに追加
        canvas->add(textObj);

        // バウンディングボックスを計算
        float bx, by, bw, bh;
        if (textObj->bounds(&bx, &by, &bw, &bh) == tvg::Result::Success) {
            RectF bounds(bx + info.ox, by + info.oy, bw, bh);
            if (first) {
                totalBounds = bounds;
                first = false;
            } else {
                RectF::Union(totalBounds, totalBounds, bounds);
            }
        }
    }

    // 描画を実行
    canvas->draw();
    canvas->sync();

    updateRect(totalBounds);
    return totalBounds;
}

// --------------------------------------------------------
// Image クラス
// --------------------------------------------------------

::Image::Image() : picture(nullptr), imgWidth(0), imgHeight(0), loaded(false)
{
}

::Image::Image(const ::Image& orig) : picture(nullptr), imgWidth(orig.imgWidth), imgHeight(orig.imgHeight), loaded(false), record(orig.record)
{
    if (record) loaded = true; // 記録の画像は記録を共有する (中身は不変)
    if (orig.picture && orig.loaded) {
        // ThorVG Picture を複製
        picture = (tvg::Picture*)orig.picture->duplicate();
        if (picture) {
            loaded = true;
        }
    }
}

::Image::~Image()
{
    if (picture) {
        tvg::Paint::rel(picture);
        picture = nullptr;
    }
}

bool ::Image::load(const char* filename)
{
    if (picture) {
        tvg::Paint::rel(picture);
        picture = nullptr;
    }
    loaded = false;

    picture = tvg::Picture::gen();
    if (!picture) return false;

    if (picture->load(filename) != tvg::Result::Success) {
        tvg::Paint::rel(picture);
        picture = nullptr;
        return false;
    }

    // サイズを取得
    float w, h;
    if (picture->size(&w, &h) == tvg::Result::Success) {
        imgWidth = w;
        imgHeight = h;
    }

    loaded = true;
    return true;
}

bool ::Image::load(const tjs_char* filename)
{
    // 吉里吉里のパス解決を使用
    ttstr resolved = TVPGetPlacedPath(filename);
    if (resolved.length() == 0) {
        return false;
    }

    // ローカルアクセス可能なパスを取得
    ttstr localname(TVPGetLocallyAccessibleName(resolved));
    if (localname.length()) {
        // 実ファイルが存在する場合
        std::string utf8path = wcharToUtf8(localname.c_str());
        return load(utf8path.c_str());
    }

    // ストリームから読み込む
    iTJSBinaryStream* stream = TVPCreateStream(resolved, TJS_BS_READ);
    if (!stream) return false;

    uint32_t size = (uint32_t)stream->GetSize();

    std::vector <uint8_t> dataBuffer;

    dataBuffer.resize(size);
    tjs_uint bytesRead = stream->Read(dataBuffer.data(), size);
    stream->Destruct();

    if (bytesRead != size) {
        dataBuffer.clear();
        return false;
    }

    // MIMEタイプを拡張子から推測
    const char* mimeType = nullptr;
    ttstr ext;
    tjs_int dotPos = resolved.GetLen() - 1;
    while (dotPos >= 0 && resolved[dotPos] != TJS_W('.')) dotPos--;
    if (dotPos >= 0) {
        ext = resolved.c_str() + dotPos + 1;
        ext.ToLowerCase();
        if (ext == TJS_W("png")) mimeType = "png";
        else if (ext == TJS_W("jpg") || ext == TJS_W("jpeg")) mimeType = "jpg";
        else if (ext == TJS_W("svg")) mimeType = "svg";
        else if (ext == TJS_W("webp")) mimeType = "webp";
    }

    return load(dataBuffer.data(), (uint32_t)dataBuffer.size(), mimeType);
}

bool ::Image::load(const void* data, uint32_t size, const char* mimeType)
{
    if (picture) {
        tvg::Paint::rel(picture);
        picture = nullptr;
    }
    loaded = false;

    picture = tvg::Picture::gen();
    if (!picture) return false;

    if (picture->load((const char*)data, size, mimeType ? mimeType : "", nullptr, true) != tvg::Result::Success) {
        tvg::Paint::rel(picture);
        picture = nullptr;
        return false;
    }

    // サイズを取得
    float w, h;
    if (picture->size(&w, &h) == tvg::Result::Success) {
        imgWidth = w;
        imgHeight = h;
    }

    loaded = true;
    return true;
}

bool ::Image::loadRaw(const uint32_t* data, uint32_t w, uint32_t h, bool copy)
{
    if (picture) {
        tvg::Paint::rel(picture);
        picture = nullptr;
    }
    loaded = false;

    picture = tvg::Picture::gen();
    if (!picture) return false;

    if (picture->load(data, w, h, tvg::ColorSpace::ARGB8888, copy) != tvg::Result::Success) {
        tvg::Paint::rel(picture);
        picture = nullptr;
        return false;
    }

    imgWidth = (float)w;
    imgHeight = (float)h;
    loaded = true;
    return true;
}

::Image* ::Image::Clone() const
{
    return new Image(*this);
}

void ::Image::SetSize(float w, float h)
{
    if (picture && loaded) {
        picture->size(w, h);
        imgWidth = w;
        imgHeight = h;
    }
}


// --------------------------------------------------------
// 記録 (Layer.record / getRecordImage)
// --------------------------------------------------------

// a × b (列ベクトル。 b を先に適用する)
static tvg::Matrix mulMatrix(const tvg::Matrix &a, const tvg::Matrix &b)
{
    tvg::Matrix r;
    r.e11 = a.e11 * b.e11 + a.e12 * b.e21;
    r.e12 = a.e11 * b.e12 + a.e12 * b.e22;
    r.e13 = a.e11 * b.e13 + a.e12 * b.e23 + a.e13;
    r.e21 = a.e21 * b.e11 + a.e22 * b.e21;
    r.e22 = a.e21 * b.e12 + a.e22 * b.e22;
    r.e23 = a.e21 * b.e13 + a.e22 * b.e23 + a.e23;
    r.e31 = 0; r.e32 = 0; r.e33 = 1;
    return r;
}

RecordData::RecordData(const RecordData& o) : bounds(o.bounds), hasBounds(o.hasBounds)
{
    for (size_t i = 0; i < o.entries.size(); i++) {
        Entry e = o.entries[i];
        e.shape = o.entries[i].shape ? (tvg::Shape*)o.entries[i].shape->duplicate() : nullptr;
        entries.push_back(e);
    }
}

RecordData::~RecordData()
{
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].shape) tvg::Paint::rel(entries[i].shape);
    }
}

void RecordData::add(const Appearance* app, const tvg::Shape* base, const tvg::Matrix& tr)
{
    Entry e;
    e.app = *app;
    e.shape = (tvg::Shape*)base->duplicate();
    e.transform = tr;
    if (!e.shape) return;
    entries.push_back(e);

    // 外接矩形 (記録座標)。 線は太さの半分だけ広げる
    const tvg::PathCommand* cmds = nullptr; uint32_t nc = 0;
    const tvg::Point* pts = nullptr; uint32_t np = 0;
    if (base->path(&cmds, &nc, &pts, &np) != tvg::Result::Success || np == 0) return;
    REAL x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (uint32_t i = 0; i < np; i++) {
        REAL x = tr.e11 * pts[i].x + tr.e12 * pts[i].y + tr.e13;
        REAL y = tr.e21 * pts[i].x + tr.e22 * pts[i].y + tr.e23;
        if (i == 0 || x < x0) x0 = x; if (i == 0 || x > x1) x1 = x;
        if (i == 0 || y < y0) y0 = y; if (i == 0 || y > y1) y1 = y;
    }
    REAL margin = 0;
    REAL scale = (REAL)sqrt(fabs(tr.e11 * tr.e22 - tr.e12 * tr.e21));
    REAL dx0 = 0, dy0 = 0, dx1 = 0, dy1 = 0; bool firstOfs = true;
    for (size_t i = 0; i < app->drawInfos.size(); i++) {
        const Appearance::DrawInfo &d = app->drawInfos[i];
        if (d.type == 0 && d.strokeWidth * scale / 2 > margin) margin = d.strokeWidth * scale / 2;
        if (firstOfs || d.ox < dx0) dx0 = d.ox; if (firstOfs || d.ox > dx1) dx1 = d.ox;
        if (firstOfs || d.oy < dy0) dy0 = d.oy; if (firstOfs || d.oy > dy1) dy1 = d.oy;
        firstOfs = false;
    }
    RectF r(x0 + dx0 - margin, y0 + dy0 - margin, (x1 - x0) + (dx1 - dx0) + margin * 2, (y1 - y0) + (dy1 - dy0) + margin * 2);
    if (!hasBounds) { bounds = r; hasBounds = true; }
    else RectF::Union(bounds, bounds, r);
}

void ::Image::setRecord(std::shared_ptr<RecordData> r)
{
    record = r;
    loaded = (bool)r;
    if (r && r->hasBounds) { imgWidth = r->bounds.Width; imgHeight = r->bounds.Height; }
}

RectF (::Image::GetBounds)() const
{
    if (record) return record->hasBounds ? record->bounds : RectF();
    return RectF(0, 0, imgWidth, imgHeight);
}

// グローバルヘルパー関数
::Image* loadImage(const tjs_char* name)
{
    ::Image* image = new ::Image();
    if (image->load(name)) {
        return image;
    }
    delete image;
    return nullptr;
}

RectF* getBounds(::Image* image)
{
    if (!image) return new RectF(0, 0, 0, 0);
    return new RectF(image->GetBounds());
}

// --------------------------------------------------------
// LayerExDraw - Image 描画メソッド
// --------------------------------------------------------

RectF LayerExDraw::drawImage(REAL x, REAL y, ::Image* src)
{
    RectF rect;
    if (!src || !src->IsLoaded()) return rect;

    RectF bounds = src->GetBounds();
    // 記録の画像も同じ式 (GDI+ と同じく、記録座標の (0,0) が描き先の (x + bounds.X, y + bounds.Y) に来る)
    rect = drawImageRect(x + bounds.X, y + bounds.Y, src, 0, 0, bounds.Width, bounds.Height);
    updateRect(rect);
    return rect;
}

RectF LayerExDraw::drawImageRect(REAL dleft, REAL dtop, ::Image* src, REAL sleft, REAL stop, REAL swidth, REAL sheight)
{
    return drawImageAffine(src, sleft, stop, swidth, sheight, true, 1, 0, 0, 1, dleft, dtop);
}

RectF LayerExDraw::drawImageStretch(REAL dleft, REAL dtop, REAL dwidth, REAL dheight, ::Image* src, REAL sleft, REAL stop, REAL swidth, REAL sheight)
{
    if (swidth == 0 || sheight == 0) return RectF();
    return drawImageAffine(src, sleft, stop, swidth, sheight, true, dwidth/swidth, 0, 0, dheight/sheight, dleft, dtop);
}

RectF LayerExDraw::drawImageAffine(::Image* src, REAL sleft, REAL stop, REAL swidth, REAL sheight, bool affine, REAL A, REAL B, REAL C, REAL D, REAL E, REAL F)
{
    RectF rect;
    if (canvas && src && src->IsRecord() && src->getRecord()) {
        // 記録 (getRecordImage) の画像: 記録座標の元の範囲 → 描き先 の写像を掛けて描き直す。
        //   GDI+ と同じく元の範囲は記録したときの座標そのもの (外接矩形の左上が原点ではない)
        if (swidth == 0 || sheight == 0) return rect;
        tvg::Matrix map;
        if (affine) { // x' = A x + C y + E (x は元の範囲の左上からの位置)
            map.e11 = A; map.e12 = C; map.e13 = E - (A * sleft + C * stop);
            map.e21 = B; map.e22 = D; map.e23 = F - (B * sleft + D * stop);
        } else {      // 3 点指定 (A,B)=左上 (C,D)=右上 (E,F)=左下
            REAL ux = (C - A) / swidth, uy = (D - B) / swidth;
            REAL vx = (E - A) / sheight, vy = (F - B) / sheight;
            map.e11 = ux; map.e12 = vx; map.e13 = A - (ux * sleft + vx * stop);
            map.e21 = uy; map.e22 = vy; map.e23 = B - (uy * sleft + vy * stop);
        }
        map.e31 = 0; map.e32 = 0; map.e33 = 1;
        return drawRecordImage(*src->getRecord(), map, sleft, stop, swidth, sheight);
    }
    if (!canvas || !src || !src->IsLoaded() || !src->getPicture()) return rect;

    // 元画像を複製して使用
    tvg::Picture* pic = (tvg::Picture*)src->getPicture()->duplicate();
    if (!pic) return rect;

    // ソース領域のクリッピング（ThorVGはソース領域の切り出しを直接サポートしていないため、
    // 変換行列で対応する）

    // アフィン変換行列を計算
    PointF points[4];
    if (affine) {
#define AFFINEX(x,y) A*(x)+C*(y)+E
#define AFFINEY(x,y) B*(x)+D*(y)+F
        points[0].X = AFFINEX(0, 0);
        points[0].Y = AFFINEY(0, 0);
        points[1].X = AFFINEX(swidth, 0);
        points[1].Y = AFFINEY(swidth, 0);
        points[2].X = AFFINEX(0, sheight);
        points[2].Y = AFFINEY(0, sheight);
        points[3].X = AFFINEX(swidth, sheight);
        points[3].Y = AFFINEY(swidth, sheight);
#undef AFFINEX
#undef AFFINEY
    } else {
        points[0].X = A;
        points[0].Y = B;
        points[1].X = C;
        points[1].Y = D;
        points[2].X = E;
        points[2].Y = F;
        points[3].X = C - A + E;
        points[3].Y = D - B + F;
    }

    // サイズを設定
    pic->size(swidth, sheight);

    // 変換行列を作成
    // 画像をソース領域からデスティネーションへ変換
    tvg::Matrix tm;
    tm.e11 = A * calcTransform.m.e11 + B * calcTransform.m.e21;
    tm.e12 = A * calcTransform.m.e12 + B * calcTransform.m.e22;
    tm.e13 = E * calcTransform.m.e11 + F * calcTransform.m.e21 + calcTransform.m.e13 - sleft * tm.e11 - stop * tm.e12;
    tm.e21 = C * calcTransform.m.e11 + D * calcTransform.m.e21;
    tm.e22 = C * calcTransform.m.e12 + D * calcTransform.m.e22;
    tm.e23 = E * calcTransform.m.e12 + F * calcTransform.m.e22 + calcTransform.m.e23 - sleft * tm.e21 - stop * tm.e22;
    tm.e31 = 0;
    tm.e32 = 0;
    tm.e33 = 1;

    pic->transform(tm);

    canvas->add(pic);
    canvas->draw();
    canvas->sync();

    // 描画領域を計算
    calcTransform.TransformPoints(points, 4);
    REAL minx = points[0].X;
    REAL maxx = points[0].X;
    REAL miny = points[0].Y;
    REAL maxy = points[0].Y;
    for (int i = 1; i < 4; i++) {
        if (points[i].X < minx) minx = points[i].X;
        if (points[i].X > maxx) maxx = points[i].X;
        if (points[i].Y < miny) miny = points[i].Y;
        if (points[i].Y > maxy) maxy = points[i].Y;
    }

    rect.X = minx;
    rect.Y = miny;
    rect.Width = maxx - minx;
    rect.Height = maxy - miny;

    updateRect(rect);
    return rect;
}
