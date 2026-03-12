#include "ncbind.hpp"
#include "LayerExDraw.hpp"
#include <vector>
#include <stdio.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// thorvg 初期化
void initThorvg()
{
    // 利用するスレッド数を指定 (0 ならメインスレッドのみ、1 以上ならその数だけワーカースレッドを起動)
    tvg::Initializer::init(4);
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
// フォント情報
// --------------------------------------------------------

FontInfo::FontInfo() : emSize(12), style(0), propertyModified(true),
    ascent(0), descent(0), lineSpacing(0), ascentLeading(0), descentLeading(0)
{
}

FontInfo::FontInfo(const tjs_char *familyName, REAL emSize, INT style)
    : propertyModified(true), ascent(0), descent(0), lineSpacing(0),
      ascentLeading(0), descentLeading(0)
{
    setFamilyName(familyName);
    setEmSize(emSize);
    setStyle(style);
}

FontInfo::FontInfo(const FontInfo &orig)
{
    familyName = orig.familyName;
    emSize = orig.emSize;
    style = orig.style;
    propertyModified = orig.propertyModified;
    ascent = orig.ascent;
    descent = orig.descent;
    lineSpacing = orig.lineSpacing;
    ascentLeading = orig.ascentLeading;
    descentLeading = orig.descentLeading;
}

FontInfo::~FontInfo()
{
    clear();
}

void FontInfo::clear()
{
    familyName = L"";
    propertyModified = true;
}

void FontInfo::setFamilyName(const tjs_char *name)
{
    propertyModified = true;
    if (name) {
        familyName = name;
    } else {
        familyName = L"";
    }
}

void FontInfo::updateSizeParams() const
{
    if (!propertyModified)
        return;

    propertyModified = false;
    
    // Windows GDI を使用してフォントメトリクスを取得
    HDC dc = ::CreateCompatibleDC(NULL);
    if (dc == NULL) {
        // デフォルト値を設定
        ascent = emSize * 0.8f;
        descent = emSize * 0.2f;
        lineSpacing = emSize;
        ascentLeading = 0;
        descentLeading = 0;
        return;
    }

    LOGFONTW font;
    memset(&font, 0, sizeof(font));
    font.lfHeight = (LONG)(-emSize);
    font.lfWeight = (style & FontStyleBold) ? FW_BOLD : FW_REGULAR;
    font.lfItalic = (style & FontStyleItalic) ? TRUE : FALSE;
    font.lfUnderline = (style & FontStyleUnderline) ? TRUE : FALSE;
    font.lfStrikeOut = (style & FontStyleStrikeout) ? TRUE : FALSE;
    font.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(font.lfFaceName, familyName.c_str(), LF_FACESIZE - 1);

    HFONT hFont = CreateFontIndirectW(&font);
    if (hFont == NULL) {
        DeleteDC(dc);
        ascent = emSize * 0.8f;
        descent = emSize * 0.2f;
        lineSpacing = emSize;
        ascentLeading = 0;
        descentLeading = 0;
        return;
    }

    HGDIOBJ hOldFont = SelectObject(dc, hFont);
    
    TEXTMETRICW tm;
    if (GetTextMetricsW(dc, &tm)) {
        ascent = (REAL)tm.tmAscent;
        descent = (REAL)tm.tmDescent;
        lineSpacing = (REAL)tm.tmHeight;
        ascentLeading = (REAL)tm.tmInternalLeading;
        descentLeading = 0;
    } else {
        ascent = emSize * 0.8f;
        descent = emSize * 0.2f;
        lineSpacing = emSize;
        ascentLeading = 0;
        descentLeading = 0;
    }

    SelectObject(dc, hOldFont);
    DeleteObject(hFont);
    DeleteDC(dc);
}

REAL FontInfo::getAscent() const
{
    updateSizeParams();
    return ascent;
}

REAL FontInfo::getDescent() const
{
    updateSizeParams();
    return descent;
}

REAL FontInfo::getAscentLeading() const
{
    updateSizeParams();
    return ascentLeading;
}

REAL FontInfo::getDescentLeading() const
{
    updateSizeParams();
    return descentLeading;
}

REAL FontInfo::getLineSpacing() const
{
    updateSizeParams();
    return lineSpacing;
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
        
        if (type == BrushTypeLinearGradient) {
            info.useLinearGradient = true;
            
            tTJSVariant var;
            if (propInfo.checkVariant(TJS_W("point1"), var)) {
                PointF p1 = getPoint(var);
                info.gradX1 = p1.X;
                info.gradY1 = p1.Y;
            }
            if (propInfo.checkVariant(TJS_W("point2"), var)) {
                PointF p2 = getPoint(var);
                info.gradX2 = p2.X;
                info.gradY2 = p2.Y;
            }
            
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
        } else if (type == BrushTypePathGradient) { // PathGradient (RadialGradient として近似)
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
        ARGB color = (ARGB)propInfo.getIntValue(TJS_W("color"), 0xFFFFFFFF);
        info.strokeA = (color >> 24) & 0xFF;
        info.strokeR = (color >> 16) & 0xFF;
        info.strokeG = (color >> 8) & 0xFF;
        info.strokeB = color & 0xFF;
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
            }
        }
        
        // DashOffset
        if (propInfo.checkVariant(TJS_W("dashOffset"), var)) {
            info.dashOffset = (REAL)(tjs_real)var;
        }
    }
    
    drawInfos.push_back(info);
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
        flipped = (pitch < 0);
        
        canvas = tvg::SwCanvas::gen();
        if (canvas) {
            if (flipped) {
                // pitch が負の場合、メモリが上下反転しているので順方向に補正
                uint32_t absPitch = (uint32_t)(-pitch);
                uint32_t stridePixels = absPitch / (uint32_t)sizeof(uint32_t);
                uint8_t* canvasBuffer = buffer + (height - 1) * pitch;
                canvas->target((uint32_t*)canvasBuffer, stridePixels, width, height, tvg::ColorSpace::ARGB8888);
            } else {
                canvas->target((uint32_t*)buffer, width, width, height, tvg::ColorSpace::ARGB8888);
            }
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

void LayerExDraw::updateRect(RectF &rect)
{
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

void LayerExDraw::setViewTransform(const Matrix *trans)
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

void LayerExDraw::setTransform(const Matrix *trans)
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
    if (!canvas) return;
    
    // キャンバスをクリア
    canvas->remove();
    
    // 背景色で塗りつぶし
    tvg::Shape* bg = tvg::Shape::gen();
    bg->appendRect(0, 0, (float)width, (float)height);
    
    uint8_t a = (argb >> 24) & 0xFF;
    uint8_t r = (argb >> 16) & 0xFF;
    uint8_t g = (argb >> 8) & 0xFF;
    uint8_t b = argb & 0xFF;
    bg->fill(r, g, b, a);
    
    canvas->add(bg);
    canvas->draw();
    canvas->sync();
    
    _pUpdate(0, NULL);
}

RectF LayerExDraw::drawShapeWithAppearance(const Appearance *app, tvg::Shape* baseShape)
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
        tm.e11 = calcTransform.m.e11;
        tm.e12 = calcTransform.m.e12;
        tm.e13 = calcTransform.m.e13 + info.ox;
        tm.e21 = calcTransform.m.e21;
        tm.e22 = calcTransform.m.e22;
        tm.e23 = calcTransform.m.e23 + info.oy;
        tm.e31 = 0;
        tm.e32 = 0;
        tm.e33 = 1;
        shape->transform(tm);
        
        if (info.type == 0) { // ストローク
            shape->strokeWidth(info.strokeWidth);
            shape->strokeFill(info.strokeR, info.strokeG, info.strokeB, info.strokeA);
            shape->strokeCap(info.strokeCap);
            shape->strokeJoin(info.strokeJoin);
            shape->strokeMiterlimit(info.miterLimit);
            
            if (!info.dashPattern.empty()) {
                shape->strokeDash(info.dashPattern.data(), (uint32_t)info.dashPattern.size(), info.dashOffset);
            }
            
            // フィル色を透明に
            shape->fill(0, 0, 0, 0);
        } else { // フィル
            if (info.useLinearGradient) {
                tvg::LinearGradient* grad = tvg::LinearGradient::gen();
                grad->linear(info.gradX1, info.gradY1, info.gradX2, info.gradY2);
                grad->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                shape->fill(grad);
            } else if (info.useRadialGradient) {
                tvg::RadialGradient* grad = tvg::RadialGradient::gen();
                grad->radial(info.gradCx, info.gradCy, info.gradR, info.gradCx, info.gradCy, 0);
                grad->colorStops(info.colorStops.data(), (uint32_t)info.colorStops.size());
                shape->fill(grad);
            } else {
                shape->fill(info.fillR, info.fillG, info.fillB, info.fillA);
            }
            
            // ストロークなし
            shape->strokeWidth(0);
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

RectF LayerExDraw::drawPath(const Appearance *app, const Path *path)
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
    Path path;
    path.drawArc(x, y, width, height, startAngle, sweepAngle);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawPie(const Appearance *app, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle)
{
    Path path;
    path.drawPie(x, y, width, height, startAngle, sweepAngle);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawBezier(const Appearance *app, REAL x1, REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4)
{
    Path path;
    path.drawBezier(x1, y1, x2, y2, x3, y3, x4, y4);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawBeziers(const Appearance *app, tTJSVariant points)
{
    Path path;
    path.drawBeziers(points);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawClosedCurve(const Appearance *app, tTJSVariant points)
{
    Path path;
    path.drawClosedCurve(points);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawClosedCurve2(const Appearance *app, tTJSVariant points, REAL tension)
{
    Path path;
    path.drawClosedCurve2(points, tension);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawCurve(const Appearance *app, tTJSVariant points)
{
    Path path;
    path.drawCurve(points);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawCurve2(const Appearance *app, tTJSVariant points, REAL tension)
{
    Path path;
    path.drawCurve2(points, tension);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawCurve3(const Appearance *app, tTJSVariant points, int offset, int numberOfSegments, REAL tension)
{
    Path path;
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
    Path path;
    path.drawLine(x1, y1, x2, y2);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawLines(const Appearance *app, tTJSVariant points)
{
    Path path;
    path.drawLines(points);
    return drawPath(app, &path);
}

RectF LayerExDraw::drawPolygon(const Appearance *app, tTJSVariant points)
{
    Path path;
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

RectF LayerExDraw::drawPathString(const FontInfo *font, const Appearance *app, REAL x, REAL y, const tjs_char *text)
{
    // thorvg の Text を使用してテキストを描画
    // 注: thorvg はフォントサポートが限定的なため、基本的な実装
    if (!canvas || !font || !text) return RectF();
    
    tvg::Text* tvgText = tvg::Text::gen();
    if (!tvgText) {
        THROW_NOT_IMPLEMENTED();
        return RectF();
    }
    
    // UTF-16 から UTF-8 に変換
    std::string utf8Text;
    const tjs_char* p = text;
    while (*p) {
        tjs_char ch = *p++;
        if (ch < 0x80) {
            utf8Text += (char)ch;
        } else if (ch < 0x800) {
            utf8Text += (char)(0xC0 | (ch >> 6));
            utf8Text += (char)(0x80 | (ch & 0x3F));
        } else {
            utf8Text += (char)(0xE0 | (ch >> 12));
            utf8Text += (char)(0x80 | ((ch >> 6) & 0x3F));
            utf8Text += (char)(0x80 | (ch & 0x3F));
        }
    }
    
    // フォント名を UTF-8 に変換
    std::string fontName;
    const tjs_char* fn = font->familyName.c_str();
    while (*fn) {
        tjs_char ch = *fn++;
        if (ch < 0x80) {
            fontName += (char)ch;
        } else if (ch < 0x800) {
            fontName += (char)(0xC0 | (ch >> 6));
            fontName += (char)(0x80 | (ch & 0x3F));
        } else {
            fontName += (char)(0xE0 | (ch >> 12));
            fontName += (char)(0x80 | ((ch >> 6) & 0x3F));
            fontName += (char)(0x80 | (ch & 0x3F));
        }
    }
    
    tvgText->font(fontName.empty() ? nullptr : fontName.c_str());
    tvgText->size(font->emSize);
    tvgText->text(utf8Text.c_str());
    
    // 最初のブラシ/ペンの色を使用
    if (!app->drawInfos.empty()) {
        const Appearance::DrawInfo& info = app->drawInfos[0];
        if (info.type == 1) { // フィル
            tvgText->fill(info.fillR, info.fillG, info.fillB);
        } else { // ストローク
            tvgText->fill(info.strokeR, info.strokeG, info.strokeB);
        }
    }
    
    // 位置を設定
    tvgText->translate(x, y);
    
    // トランスフォームを適用
    tvgText->transform(calcTransform.getTvgMatrix());
    
    canvas->add(tvgText);
    canvas->draw();
    canvas->sync();
    
    // 簡易的なバウンディングボックス計算
    RectF rect(x, y, font->emSize * utf8Text.length() * 0.6f, font->getLineSpacing());
    updateRect(rect);
    return rect;
}

RectF LayerExDraw::drawString(const FontInfo *font, const Appearance *app, REAL x, REAL y, const tjs_char *text)
{
    return drawPathString(font, app, x, y, text);
}

RectF LayerExDraw::measureString(const FontInfo *font, const tjs_char *text)
{
    if (!font || !text) return RectF();
    
    // 簡易的な文字列サイズ計算
    size_t len = wcslen(text);
    REAL width = font->emSize * len * 0.6f;
    REAL height = font->getLineSpacing();
    
    return RectF(0, 0, width, height);
}

RectF LayerExDraw::measureStringInternal(const FontInfo *font, const tjs_char *text)
{
    return measureString(font, text);
}

// --------------------------------------------------------
// Image クラス
// --------------------------------------------------------

Image::Image() : picture(nullptr), imgWidth(0), imgHeight(0), loaded(false)
{
}

Image::Image(const Image& orig) : picture(nullptr), imgWidth(orig.imgWidth), imgHeight(orig.imgHeight), loaded(false)
{
    if (orig.picture && orig.loaded) {
        // ThorVG Picture を複製
        picture = (tvg::Picture*)orig.picture->duplicate();
        if (picture) {
            loaded = true;
        }
    }
}

Image::~Image()
{
    if (picture) {
        tvg::Paint::rel(picture);
        picture = nullptr;
    }
}

// UTF-16 から UTF-8 への変換ヘルパー
static std::string wcharToUtf8(const tjs_char* wstr)
{
    if (!wstr) return "";
    
    std::string utf8;
    while (*wstr) {
        tjs_char ch = *wstr++;
        if (ch < 0x80) {
            utf8 += (char)ch;
        } else if (ch < 0x800) {
            utf8 += (char)(0xC0 | (ch >> 6));
            utf8 += (char)(0x80 | (ch & 0x3F));
        } else {
            utf8 += (char)(0xE0 | (ch >> 12));
            utf8 += (char)(0x80 | ((ch >> 6) & 0x3F));
            utf8 += (char)(0x80 | (ch & 0x3F));
        }
    }
    return utf8;
}

bool Image::load(const char* filename)
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

bool Image::load(const tjs_char* filename)
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
    IStream* stream = TVPCreateIStream(resolved, TJS_BS_READ);
    if (!stream) return false;
    
    STATSTG stat;
    stream->Stat(&stat, STATFLAG_NONAME);
    uint32_t size = (uint32_t)stat.cbSize.QuadPart;
    
    std::vector <uint8_t> dataBuffer;    
    
    dataBuffer.resize(size);
    ULONG bytesRead;
    HRESULT hr = stream->Read(dataBuffer.data(), size, &bytesRead);
    stream->Release();
    
    if (FAILED(hr) || bytesRead != size) {
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

bool Image::load(const void* data, uint32_t size, const char* mimeType)
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

bool Image::loadRaw(const uint32_t* data, uint32_t w, uint32_t h, bool copy)
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

Image* Image::Clone() const
{
    return new Image(*this);
}

void Image::SetSize(float w, float h)
{
    if (picture && loaded) {
        picture->size(w, h);
        imgWidth = w;
        imgHeight = h;
    }
}

// グローバルヘルパー関数
Image* loadImage(const tjs_char* name)
{
    Image* image = new Image();
    if (image->load(name)) {
        return image;
    }
    delete image;
    return nullptr;
}

RectF* getBounds(Image* image)
{
    if (!image) return new RectF(0, 0, 0, 0);
    return new RectF(image->GetBounds());
}

// --------------------------------------------------------
// LayerExDraw - Image 描画メソッド
// --------------------------------------------------------

RectF LayerExDraw::drawImage(REAL x, REAL y, Image* src)
{
    RectF rect;
    if (!src || !src->IsLoaded()) return rect;
    
    RectF bounds = src->GetBounds();
    rect = drawImageRect(x + bounds.X, y + bounds.Y, src, 0, 0, bounds.Width, bounds.Height);
    updateRect(rect);
    return rect;
}

RectF LayerExDraw::drawImageRect(REAL dleft, REAL dtop, Image* src, REAL sleft, REAL stop, REAL swidth, REAL sheight)
{
    return drawImageAffine(src, sleft, stop, swidth, sheight, true, 1, 0, 0, 1, dleft, dtop);
}

RectF LayerExDraw::drawImageStretch(REAL dleft, REAL dtop, REAL dwidth, REAL dheight, Image* src, REAL sleft, REAL stop, REAL swidth, REAL sheight)
{
    if (swidth == 0 || sheight == 0) return RectF();
    return drawImageAffine(src, sleft, stop, swidth, sheight, true, dwidth/swidth, 0, 0, dheight/sheight, dleft, dtop);
}

RectF LayerExDraw::drawImageAffine(Image* src, REAL sleft, REAL stop, REAL swidth, REAL sheight, bool affine, REAL A, REAL B, REAL C, REAL D, REAL E, REAL F)
{
    RectF rect;
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
