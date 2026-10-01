#ifndef _layerExDraw_hpp_
#define _layerExDraw_hpp_

#include <cmath>
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>

#include "thorvg/inc/thorvg.h"

#include "layerExBase.hpp"

using namespace std;

// 型定義（GDIPlusとの互換性のため）
typedef float REAL;
typedef unsigned int ARGB;
typedef int INT;
typedef int BOOL;

// --------------------------------------------------------
// GDIPlus 互換 enum 定義
// --------------------------------------------------------

// ブラシタイプ
enum BrushType {
    BrushTypeSolidColor     = 0,
    BrushTypeTextureFill    = 2, // 画像タイル塗り (GDI+ 互換値)
    BrushTypePathGradient   = 3,
    BrushTypeLinearGradient = 4
};

// 線キャップ
enum LineCap {
    LineCapFlat          = 0,
    LineCapSquare        = 1,
    LineCapRound         = 2,
    LineCapTriangle      = 3
};

// 線結合
enum LineJoin {
    LineJoinMiter        = 0,
    LineJoinBevel        = 1,
    LineJoinRound        = 2,
    LineJoinMiterClipped = 3
};

// マトリックス演算順序
enum MatrixOrder {
    MatrixOrderPrepend = 0,
    MatrixOrderAppend  = 1
};

// 描画スムージング (GDI+ 互換値)。 ThorVG は AA を無効化できないため、
// None / HighSpeed 指定時は描画後にカバレッジ 50% しきい値でアルファを
// 二値化し、 非 AA (ジャギー) 描画と同等の見た目にする (applySmoothingPost)
enum SmoothingMode {
    SmoothingModeDefault     = 0,
    SmoothingModeHighSpeed   = 1,
    SmoothingModeHighQuality = 2,
    SmoothingModeNone        = 3,
    SmoothingModeAntiAlias   = 4,
    SmoothingModeInvalid     = -1
};

// 以下は layerExDraw (GDI+) との互換のための値。 GDI+ と同じ数値にしてある。
//   ⚠ 定数が無いと GdiPlus.X の参照が「メンバが見つかりません」で落ちるので、
//     再現していない機能の定数も登録する (どう扱うかは各定数のコメント)

// グラデーションの範囲外の扱い。 ThorVG の FillSpread へ対応付ける
//   Tile → Repeat / TileFlipX・Y・XY → Reflect (軸は区別しない) / Clamp → Pad
enum WrapMode {
    WrapModeTile        = 0,
    WrapModeTileFlipX   = 1,
    WrapModeTileFlipY   = 2,
    WrapModeTileFlipXY  = 3,
    WrapModeClamp       = 4
};

// 線形グラデーションの方向 (rect 指定時)
enum LinearGradientMode {
    LinearGradientModeHorizontal       = 0,
    LinearGradientModeVertical         = 1,
    LinearGradientModeForwardDiagonal  = 2,
    LinearGradientModeBackwardDiagonal = 3
};

// 破線の型 (ペンの dashStyle に整数で渡す)。 パターンはペン幅の倍数 (GDI+ と同じ)
enum DashStyle {
    DashStyleSolid      = 0,
    DashStyleDash       = 1,
    DashStyleDot        = 2,
    DashStyleDashDot    = 3,
    DashStyleDashDotDot = 4,
    DashStyleCustom     = 5
};

// 破線の端の形。 ThorVG に区別が無いので値は受けるだけ (端は startCap/endCap と同じ)
enum DashCap {
    DashCapFlat     = 0,
    DashCapRound    = 2,
    DashCapTriangle = 3
};

// 線端の矢印など (anchor)。 ThorVG に無いので受けるだけ (Flat 扱い)
enum LineCapAnchor {
    LineCapNoAnchor      = 0x10,
    LineCapSquareAnchor  = 0x11,
    LineCapRoundAnchor   = 0x12,
    LineCapDiamondAnchor = 0x13,
    LineCapArrowAnchor   = 0x14
};

// ペンの位置。 受けるだけ (常に Center)
enum PenAlignment {
    PenAlignmentCenter = 0,
    PenAlignmentInset  = 1
};

// フォントの書体 (Font の第 3 引数)
enum FontStyle {
    FontStyleRegular    = 0,
    FontStyleBold       = 1,
    FontStyleItalic     = 2,
    FontStyleBoldItalic = 3,
    FontStyleUnderline  = 4,
    FontStyleStrikeout  = 8
};

// 文字の描画品質。 受けるだけ
enum TextRenderingHint {
    TextRenderingHintSystemDefault            = 0,
    TextRenderingHintSingleBitPerPixelGridFit = 1,
    TextRenderingHintSingleBitPerPixel        = 2,
    TextRenderingHintAntiAliasGridFit         = 3,
    TextRenderingHintAntiAlias                = 4,
    TextRenderingHintClearTypeGridFit         = 5
};

// --------------------------------------------------------
// 例外・前方宣言
// --------------------------------------------------------
class NotImplementedException : public std::runtime_error {
public:
    NotImplementedException(const char* msg = "Not implemented") 
        : std::runtime_error(msg) {}
};

#define THROW_NOT_IMPLEMENTED() throw NotImplementedException(__FUNCTION__)


// ベースクラス
class GdiPlus {
public:
    GdiPlus(){}
    // フォント管理関数（グローバル）
    static bool loadFont(const tjs_char *path, const tjs_char *name);
    static bool unloadFont(const tjs_char *name);
};

// Forward declarations
class Image;
class RecordData;

/**
 * GDIPlus PointF 互換構造体
 */
struct PointF {
    REAL X;
    REAL Y;
    
    PointF() : X(0), Y(0) {}
    PointF(REAL x, REAL y) : X(x), Y(y) {}
    PointF(const PointF& other) : X(other.X), Y(other.Y) {}
    
    bool Equals(const PointF& other) const {
        return X == other.X && Y == other.Y;
    }
    
    PointF operator+(const PointF& other) const {
        return PointF(X + other.X, Y + other.Y);
    }
    
    PointF operator-(const PointF& other) const {
        return PointF(X - other.X, Y - other.Y);
    }
};

/**
 * GDIPlus RectF 互換構造体
 */
struct RectF {
    REAL X;
    REAL Y;
    REAL Width;
    REAL Height;
    
    RectF() : X(0), Y(0), Width(0), Height(0) {}
    RectF(REAL x, REAL y, REAL width, REAL height) 
        : X(x), Y(y), Width(width), Height(height) {}
    RectF(const RectF& other) 
        : X(other.X), Y(other.Y), Width(other.Width), Height(other.Height) {}
    
    REAL GetLeft() const { return X; }
    REAL GetTop() const { return Y; }
    REAL GetRight() const { return X + Width; }
    REAL GetBottom() const { return Y + Height; }
    
    void GetLocation(PointF* point) const {
        if (point) {
            point->X = X;
            point->Y = Y;
        }
    }
    
    void GetBounds(RectF* rect) const {
        if (rect) {
            *rect = *this;
        }
    }
    
    RectF Clone() const { return *this; }
    
    bool Equals(const RectF& other) const {
        return X == other.X && Y == other.Y && 
               Width == other.Width && Height == other.Height;
    }
    
    void Inflate(REAL dx, REAL dy) {
        X -= dx;
        Y -= dy;
        Width += 2 * dx;
        Height += 2 * dy;
    }
    
    void Inflate(const PointF& point) {
        Inflate(point.X, point.Y);
    }
    
    bool IntersectsWith(const RectF& other) const {
        return (GetLeft() < other.GetRight() && GetRight() > other.GetLeft() &&
                GetTop() < other.GetBottom() && GetBottom() > other.GetTop());
    }
    
    bool IsEmptyArea() const {
        return Width <= 0 || Height <= 0;
    }
    
    void Offset(REAL dx, REAL dy) {
        X += dx;
        Y += dy;
    }
    
    static void Union(RectF& result, const RectF& a, const RectF& b) {
        REAL left = (a.X < b.X) ? a.X : b.X;
        REAL top = (a.Y < b.Y) ? a.Y : b.Y;
        REAL right = (a.GetRight() > b.GetRight()) ? a.GetRight() : b.GetRight();
        REAL bottom = (a.GetBottom() > b.GetBottom()) ? a.GetBottom() : b.GetBottom();
        result.X = left;
        result.Y = top;
        result.Width = right - left;
        result.Height = bottom - top;
    }

    bool Contains(REAL x, REAL y) const {
        return x >= X && x < X + Width && y >= Y && y < Y + Height;
    }
};

/**
 * Matrix クラス（thorvg::Matrix ベース）
 */
class Matrix {
public:
    tvg::Matrix m;
    
    Matrix() {
        Reset();
    }
    
    Matrix(REAL m11, REAL m12, REAL m21, REAL m22, REAL dx, REAL dy) {
        m.e11 = m11; m.e12 = m21; m.e13 = dx;
        m.e21 = m12; m.e22 = m22; m.e23 = dy;
        m.e31 = 0;   m.e32 = 0;   m.e33 = 1;
    }
    
    Matrix(const Matrix& other) {
        m = other.m;
    }
    
    Matrix* Clone() const {
        return new Matrix(*this);
    }
    
    void Reset() {
        m.e11 = 1; m.e12 = 0; m.e13 = 0;
        m.e21 = 0; m.e22 = 1; m.e23 = 0;
        m.e31 = 0; m.e32 = 0; m.e33 = 1;
    }
    
    REAL OffsetX() const { return m.e13; }
    REAL OffsetY() const { return m.e23; }
    
    bool Equals(const Matrix* other) const {
        if (!other) return false;
        return m.e11 == other->m.e11 && m.e12 == other->m.e12 && m.e13 == other->m.e13 &&
               m.e21 == other->m.e21 && m.e22 == other->m.e22 && m.e23 == other->m.e23 &&
               m.e31 == other->m.e31 && m.e32 == other->m.e32 && m.e33 == other->m.e33;
    }
    
    void SetElements(REAL m11, REAL m12, REAL m21, REAL m22, REAL dx, REAL dy) {
        m.e11 = m11; m.e12 = m21; m.e13 = dx;
        m.e21 = m12; m.e22 = m22; m.e23 = dy;
        m.e31 = 0;   m.e32 = 0;   m.e33 = 1;
    }
    
    void Multiply(const Matrix* other, int order = 0) {
        if (!other) return;
        tvg::Matrix result;
        if (order == 0) { // Prepend
            result.e11 = other->m.e11 * m.e11 + other->m.e12 * m.e21;
            result.e12 = other->m.e11 * m.e12 + other->m.e12 * m.e22;
            result.e13 = other->m.e11 * m.e13 + other->m.e12 * m.e23 + other->m.e13;
            result.e21 = other->m.e21 * m.e11 + other->m.e22 * m.e21;
            result.e22 = other->m.e21 * m.e12 + other->m.e22 * m.e22;
            result.e23 = other->m.e21 * m.e13 + other->m.e22 * m.e23 + other->m.e23;
            result.e31 = 0;
            result.e32 = 0;
            result.e33 = 1;
        } else { // Append
            result.e11 = m.e11 * other->m.e11 + m.e12 * other->m.e21;
            result.e12 = m.e11 * other->m.e12 + m.e12 * other->m.e22;
            result.e13 = m.e11 * other->m.e13 + m.e12 * other->m.e23 + m.e13;
            result.e21 = m.e21 * other->m.e11 + m.e22 * other->m.e21;
            result.e22 = m.e21 * other->m.e12 + m.e22 * other->m.e22;
            result.e23 = m.e21 * other->m.e13 + m.e22 * other->m.e23 + m.e23;
            result.e31 = 0;
            result.e32 = 0;
            result.e33 = 1;
        }
        m = result;
    }
    
    void Rotate(REAL angle, int order = 0) {
        REAL rad = angle * 3.14159265358979323846f / 180.0f;
        REAL cos_val = cosf(rad);
        REAL sin_val = sinf(rad);
        Matrix rot(cos_val, sin_val, -sin_val, cos_val, 0, 0);
        Multiply(&rot, order);
    }
    
    void RotateAt(REAL angle, const PointF& center, int order = 0) {
        Translate(center.X, center.Y, order);
        Rotate(angle, order);
        Translate(-center.X, -center.Y, order);
    }
    
    void Scale(REAL sx, REAL sy, int order = 0) {
        Matrix scale(sx, 0, 0, sy, 0, 0);
        Multiply(&scale, order);
    }
    
    void Shear(REAL shx, REAL shy, int order = 0) {
        Matrix shear(1, shy, shx, 1, 0, 0);
        Multiply(&shear, order);
    }
    
    void Translate(REAL dx, REAL dy, int order = 0) {
        Matrix trans(1, 0, 0, 1, dx, dy);
        Multiply(&trans, order);
    }
    
    void Invert() {
        REAL det = m.e11 * m.e22 - m.e12 * m.e21;
        if (det == 0) return;
        
        tvg::Matrix result;
        result.e11 = m.e22 / det;
        result.e12 = -m.e12 / det;
        result.e13 = (m.e12 * m.e23 - m.e22 * m.e13) / det;
        result.e21 = -m.e21 / det;
        result.e22 = m.e11 / det;
        result.e23 = (m.e21 * m.e13 - m.e11 * m.e23) / det;
        result.e31 = 0;
        result.e32 = 0;
        result.e33 = 1;
        m = result;
    }
    
    bool IsIdentity() const {
        return m.e11 == 1 && m.e12 == 0 && m.e13 == 0 &&
               m.e21 == 0 && m.e22 == 1 && m.e23 == 0;
    }
    
    bool IsInvertible() const {
        return (m.e11 * m.e22 - m.e12 * m.e21) != 0;
    }
    
    void TransformPoints(PointF* points, int count) const {
        for (int i = 0; i < count; i++) {
            REAL x = points[i].X;
            REAL y = points[i].Y;
            points[i].X = m.e11 * x + m.e12 * y + m.e13;
            points[i].Y = m.e21 * x + m.e22 * y + m.e23;
        }
    }
    
    int GetLastStatus() const { return 0; }

    const tvg::Matrix& getTvgMatrix() const { return m; }
};

// 前方宣言
class Path;

/**
 * フォント情報（thorvg::Text 用）
 */
class FontInfo {
    friend class LayerExDraw;

protected:
    ttstr fontFamily;           // フォントファミリー名
    REAL fontSize;              // フォントサイズ
    REAL italic;                // イタリック傾斜 (0=なし, 0.18=標準)
    REAL letterSpacing;         // 文字間隔スケール (1.0=標準)
    REAL lineSpacing;           // 行間隔スケール (1.0=標準)

public:
    FontInfo();
    
    /**
     * コンストラクタ
     * @param fontFamily フォントファミリー名（loadFontで登録した名前）
     * @param fontSize フォントサイズ
     */
    FontInfo(const tjs_char *fontFamily, REAL fontSize);
    FontInfo(const FontInfo &orig);
    virtual ~FontInfo();

    void setFontFamily(const tjs_char *name);
    const tjs_char *getFontFamily() { return fontFamily.c_str(); }
    
    void setFontSize(REAL size) { this->fontSize = size; }
    REAL getFontSize() { return fontSize; }
    
    void setItalic(REAL shear) { this->italic = shear; }
    REAL getItalic() { return italic; }
    
    void setLetterSpacing(REAL spacing) { this->letterSpacing = spacing; }
    REAL getLetterSpacing() { return letterSpacing; }
    
    void setLineSpacing(REAL spacing) { this->lineSpacing = spacing; }
    REAL getLineSpacing() { return lineSpacing; }
};

/**
 * 描画外観情報
 */
class Appearance {
    friend class LayerExDraw;
public:
    // 描画情報
    struct DrawInfo {
        int type;   // 0:ストローク 1:フィル
        REAL ox;    // 表示オフセット
        REAL oy;    // 表示オフセット
        
        // ストローク情報
        REAL strokeWidth;
        uint8_t strokeR, strokeG, strokeB, strokeA;
        tvg::StrokeCap strokeCap;
        tvg::StrokeJoin strokeJoin;
        REAL miterLimit;
        vector<REAL> dashPattern;
        REAL dashOffset;
        int dashCap; // 破線の各区切りの端 (DashCap。既定は GDI+ と同じ Flat)
        
        // フィル情報
        uint8_t fillR, fillG, fillB, fillA;
        
        // グラデーション情報（簡易）
        bool useLinearGradient;
        bool useRadialGradient;
        REAL gradX1, gradY1, gradX2, gradY2;
        REAL gradCx, gradCy, gradR;
        vector<tvg::Fill::ColorStop> colorStops;
        tvg::FillSpread gradSpread; // 範囲外の扱い (wrapMode)

        // パスグラデーション (GDI+ PathGradientBrush 相当。 points 指定時)。
        //   ThorVG に無いので、描画時に色の画像を作って図形をアルファマスクにして貼る
        bool usePathGradient;
        vector<REAL> pgX, pgY;       // 多角形の頂点
        vector<ARGB> pgColors;       // 頂点ごとの色 (surroundColors。足りなければ最後を繰り返す)
        REAL pgCx, pgCy;             // 中心点 (既定は頂点の平均)
        ARGB pgCenterColor;          // 中心色 (既定は黒)
        REAL pgFocusX, pgFocusY;     // focusScales (0 なら無し)
        int  pgWrap;                 // WrapMode (既定 Tile)
        vector<REAL> pgBlendPos, pgBlendFac;     // blend 系 (位置: 0=外周 1=中心)
        vector<REAL> pgPresetPos;                // interpolationColors
        vector<ARGB> pgPresetColors;

        // テクスチャ(タイル)フィル情報 (GDI+ TextureBrush 相当)
        bool useTextureFill;
        vector<uint32_t> texPixels; // ソースタイル ARGB8888 (texW*texH)
        int texW, texH;

        DrawInfo() : type(0), ox(0), oy(0), strokeWidth(1),
            strokeR(0), strokeG(0), strokeB(0), strokeA(255),
            strokeCap(tvg::StrokeCap::Square), strokeJoin(tvg::StrokeJoin::Bevel),
            miterLimit(4), dashOffset(0), dashCap(DashCapFlat),
            fillR(0), fillG(0), fillB(0), fillA(255),
            useLinearGradient(false), useRadialGradient(false),
            gradX1(0), gradY1(0), gradX2(0), gradY2(0),
            gradCx(0), gradCy(0), gradR(0), gradSpread(tvg::FillSpread::Pad),
            usePathGradient(false), pgCx(0), pgCy(0), pgCenterColor(0xFF000000),
            pgFocusX(0), pgFocusY(0), pgWrap(WrapModeTile),
            useTextureFill(false), texW(0), texH(0) {}
    };
    vector<DrawInfo> drawInfos;

public:
    Appearance();
    virtual ~Appearance();

    /**
     * 情報のクリア
     */
    void clear();
    
    /**
     * ブラシの追加
     * @param colorOrBrush ARGB色指定またはブラシ情報（辞書）
     * @param ox 表示オフセットX
     * @param oy 表示オフセットY
     */
    void addBrush(tTJSVariant colorOrBrush, REAL ox=0, REAL oy=0);
    
    /**
     * ペンの追加
     * @param colorOrBrush ARGB色指定またはブラシ情報（辞書）
     * @param widthOrOption ペン幅またはペン情報（辞書）
     * @param ox 表示オフセットX
     * @param oy 表示オフセットY
     */
    void addPen(tTJSVariant colorOrBrush, tTJSVariant widthOrOption, REAL ox=0, REAL oy=0);
};

/**
 * パス描画情報（thorvg::Shapeベース）
 */
class Path {
    friend class LayerExDraw;
public:
    Path();
    virtual ~Path();
    
    void startFigure();
    void closeFigure();
    void drawArc(REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle);
    void drawBezier(REAL x1, REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4);
    void drawBeziers(tTJSVariant points);
    void drawClosedCurve(tTJSVariant points);
    void drawClosedCurve2(tTJSVariant points, REAL tension);
    void drawCurve(tTJSVariant points);
    void drawCurve2(tTJSVariant points, REAL tension);
    void drawCurve3(tTJSVariant points, int offset, int numberOfSegments, REAL tension);
    void drawPie(REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle);
    void drawEllipse(REAL x, REAL y, REAL width, REAL height);
    void drawLine(REAL x1, REAL y1, REAL x2, REAL y2);
    void drawLines(tTJSVariant points);
    void drawPolygon(tTJSVariant points);
    void drawRectangle(REAL x, REAL y, REAL width, REAL height);
    void drawRectangles(tTJSVariant rects);
    
    // パスデータを取得
    void getPathData(vector<tvg::PathCommand>& cmds, vector<tvg::Point>& pts) const;
    
    // バウンディングボックスを取得
    RectF getBounds() const;
    
    // パスのクリア
    void clear();
    
    // 開始点に移動
    void moveTo(REAL x, REAL y);
    
    // 線分を追加
    void lineTo(REAL x, REAL y);
    
    // 3次ベジェ曲線を追加
    void cubicTo(REAL cx1, REAL cy1, REAL cx2, REAL cy2, REAL x, REAL y);
    
    // 他のパスを追加（オフセット付き）
    void addPath(const Path& path, REAL offsetX = 0, REAL offsetY = 0);

protected:
    // パスコマンドと点を保持
    vector<tvg::PathCommand> commands;
    vector<tvg::Point> points;
    
    // 現在の位置
    tvg::Point currentPos;
    tvg::Point figureStartPos;
    bool figureStarted;
    
    void ensureFigureStarted();
    void addArcPoints(REAL cx, REAL cy, REAL rx, REAL ry, REAL startAngle, REAL sweepAngle);
    void computeCardinalSpline(const vector<PointF>& pts, REAL tension, bool closed, vector<tvg::Point>& outPts);
};

/*
 * thorvg ベースのベクタ描画メソッドの実装
 */
class LayerExDraw : public layerExBase
{
protected:
    // 情報保持用
    GeometryT width, height;
    BufferT buffer;
    PitchT pitch;
    GeometryT clipLeft, clipTop, clipWidth, clipHeight;
    
    // thorvg キャンバス
    tvg::SwCanvas* canvas;

    // ビットマップ上下反転フラグ（pitch が負の場合 true）
    bool flipped;

    // Transform 指定
    Matrix transform;
    Matrix viewTransform;
    Matrix calcTransform;

protected:
    // 描画スムージング指定。 SmoothingModeNone / HighSpeed のときは
    // flushToLayer の合成前にスクラッチを二値化して AA を打ち消す
    int smoothingMode;

    // ThorVG は保持シーンの差分再描画時に「シーン外の既存画素」を damage
    // 領域ごとクリアするため、 レイヤバッファへ直接 target すると krkrz 側で
    // 描いた下地 (fillRect / ディザ結果など) が消える (線を引くたびにその
    // バウンディング矩形が黒く抜ける)。 描画は常にこのスクラッチ
    // (premultiplied ARGB、 トップダウン) へ行い、 draw のたびに
    // flushToLayer でレイヤバッファ (straight ARGB) へ α 合成する。
    std::vector<uint32_t> scratch;
    void flushToLayer(const RectF &rect);

public:
    int getSmoothingMode() {
        return smoothingMode;
    }
    void setSmoothingMode(int mode) {
        smoothingMode = mode;
    }

protected:
    bool updateWhenDraw;
    void updateRect(RectF &rect);
    
public:
    void setUpdateWhenDraw(int updateWhenDraw) {
        this->updateWhenDraw = updateWhenDraw != 0;
    }
    int getUpdateWhenDraw() { return updateWhenDraw ? 1 : 0; }

public:    
    LayerExDraw(DispatchT obj);
    ~LayerExDraw();
    virtual void reset();

    // ------------------------------------------------------------------
    // 描画パラメータ指定
    // ------------------------------------------------------------------

protected:
    void updateTransform();
    
public:
    /**
     * 表示トランスフォームの指定
     */
    void setViewTransform(const Matrix *transform);
    void resetViewTransform();
    void rotateViewTransform(REAL angle);
    void scaleViewTransform(REAL sx, REAL sy);
    void translateViewTransform(REAL dx, REAL dy);
    
    /**
     * トランスフォームの指定
     * @param matrix トランスフォームマトリックス
     */
    void setTransform(const Matrix *transform);
    void resetTransform();
    void rotateTransform(REAL angle);
    void scaleTransform(REAL sx, REAL sy);
    void translateTransform(REAL dx, REAL dy);

    // ------------------------------------------------------------------
    // 描画メソッド群
    // ------------------------------------------------------------------

protected:
    /**
     * Shapeを作成し、アピアランスを適用して描画
     */
    RectF drawShapeWithAppearance(const Appearance *app, tvg::Shape* shape);

    /**
     * パスグラデーションの塗り (塗り / 線とも)。 色の画像を作り、mask (図形) の
     * アルファで切り抜いて canvas に置く。 mask の所有権は受け取る
     * @return 置いた範囲 (デバイス座標)。 置けなかったら幅 0
     */
    RectF addPathGradientPaint(const Appearance::DrawInfo &info, tvg::Shape *mask, const tvg::Matrix &tm);

    /**
     * drawShapeWithAppearance の本体。 base はユーザー座標 → デバイス座標の行列
     * (通常は calcTransform)。 userOffset が真なら Appearance のずらし (ox/oy) を
     * base の前 (ユーザー座標) に掛ける (記録の描き直しで拡大縮小に追従させる)
     */
    RectF drawShapeWithAppearanceM(const Appearance *app, tvg::Shape* shape, const tvg::Matrix &base, bool userOffset);

    /**
     * 記録の画像を描く。 map は記録座標 → 描き先 (ユーザー座標)。
     * 元の範囲 (sleft, stop, swidth, sheight) を写した描き先の範囲で切り抜く
     */
    RectF drawRecordImage(const RecordData &rec, const tvg::Matrix &map, REAL sleft, REAL stop, REAL swidth, REAL sheight);

    std::shared_ptr<RecordData> recording; // record 中の記録

public:
    // 描画内容の記録 (GDI+ の Metafile 相当)
    bool getRecord() const { return (bool)recording; }
    void setRecord(bool rec);
    // 記録内容の写しを持つ Image を返す (呼び出し側が所有)。 record 中でなければ null
    ::Image* getRecordImage();
    // TJS 向け (GdiPlus.Image で包む。本体は main.cpp)
    tTJSVariant getRecordImageVariant();

protected:

public:
    /**
     * 画面の消去
     * @param argb 消去色
     */
    void clear(ARGB argb);

	/**
	 * パスの描画
	 * @param app アピアランス
	 * @param path パス
	 */
	RectF drawPath(const Appearance *app, const Path *path);
	
	/**
	 * 円弧の描画
	 * @param app アピアランス
	 * @param x 左上座標
	 * @param y 左上座標
	 * @param width 横幅
	 * @param height 縦幅
	 * @param startAngle 時計方向円弧開始位置
	 * @param sweepAngle 描画角度
	 * @return 更新領域情報
	 */
	RectF drawArc(const Appearance *app, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle);

	/**
	 * 円錐の描画
	 * @param app アピアランス
	 * @param x 左上座標
	 * @param y 左上座標
	 * @param width 横幅
	 * @param height 縦幅
	 * @param startAngle 時計方向円弧開始位置
	 * @param sweepAngle 描画角度
	 * @return 更新領域情報
	 */
	RectF drawPie(const Appearance *app, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle);
	
	/**
	 * ベジェ曲線の描画
	 * @param app アピアランス
	 * @param x1
	 * @param y1
	 * @param x2
	 * @param y2
	 * @param x3
	 * @param y3
	 * @param x4
	 * @param y4
	 * @return 更新領域情報
	 */
	RectF drawBezier(const Appearance *app, REAL x1, REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4);

	/**
	 * 連続ベジェ曲線の描画
	 * @param app アピアランス
	 * @param points 点の配列
	 * @return 更新領域情報
	 */
	RectF drawBeziers(const Appearance *app, tTJSVariant points);

	/**
	 * Closed cardinal spline の描画
	 * @param app アピアランス
	 * @param points 点の配列
	 * @return 更新領域情報
	 */
	RectF drawClosedCurve(const Appearance *app, tTJSVariant points);

	/**
	 * Closed cardinal spline の描画
	 * @param app アピアランス
	 * @param points 点の配列
	 * @pram tension tension
	 * @return 更新領域情報
	 */
	RectF drawClosedCurve2(const Appearance *app, tTJSVariant points, REAL tension);

	/**
	 * cardinal spline の描画
	 * @param app アピアランス
	 * @param points 点の配列
	 * @return 更新領域情報
	 */
	RectF drawCurve(const Appearance *app, tTJSVariant points);

	/**
	 * cardinal spline の描画
	 * @param app アピアランス
	 * @param points 点の配列
	 * @parma tension tension
	 * @return 更新領域情報
	 */
	RectF drawCurve2(const Appearance *app, tTJSVariant points, REAL tension);

	/**
	 * cardinal spline の描画
	 * @param app アピアランス
	 * @param points 点の配列
	 * @param offset
	 * @param numberOfSegment
	 * @param tension tension
	 * @return 更新領域情報
	 */
	RectF drawCurve3(const Appearance *app, tTJSVariant points, int offset, int numberOfSegments, REAL tension);
	
	/**
	 * 楕円の描画
	 * @param app アピアランス
	 * @param x
	 * @param y
	 * @param width
	 * @param height
	 * @return 更新領域情報
	 */
	RectF drawEllipse(const Appearance *app, REAL x, REAL y, REAL width, REAL height);

	/**
	 * 線分の描画
	 * @param app アピアランス
	 * @param x1 始点X座標
	 * @param y1 始点Y座標
	 * @param x2 終点X座標
	 * @param y2 終点Y座標
	 * @return 更新領域情報
	 */
	RectF drawLine(const Appearance *app, REAL x1, REAL y1, REAL x2, REAL y2);

	/**
	 * 連続線分の描画
	 * @param app アピアランス
	 * @param points 点の配列
	 * @return 更新領域情報
	 */
	RectF drawLines(const Appearance *app, tTJSVariant points);

	/**
	 * 多角形の描画
	 * @param app アピアランス
	 * @param points 点の配列
	 * @return 更新領域情報
	 */
	RectF drawPolygon(const Appearance *app, tTJSVariant points);
	
	/**
	 * 矩形の描画
	 * @param app アピアランス
	 * @param x
	 * @param y
	 * @param width
	 * @param height
	 * @return 更新領域情報
	 */
	RectF drawRectangle(const Appearance *app, REAL x, REAL y, REAL width, REAL height);

	/**
	 * 複数矩形の描画
	 * @param app アピアランス
	 * @param rects 矩形情報の配列
	 * @return 更新領域情報
	 */
	RectF drawRectangles(const Appearance *app, tTJSVariant rects);

	// ------------------------------------------------------------------
	// 文字列描画
	// ------------------------------------------------------------------

	/**
	 * 文字列の描画
	 * @param font フォント情報
	 * @param app アピアランス
	 * @param x 描画位置X
	 * @param y 描画位置Y
	 * @param text 描画テキスト
	 * @return 更新領域情報
	 */
	RectF drawString(const FontInfo *font, const Appearance *app, REAL x, REAL y, const tjs_char *text);

	/**
	 * 矩形領域内への文字列の描画
	 * @param font フォント情報
	 * @param app アピアランス
	 * @param x 矩形左端
	 * @param y 矩形上端
	 * @param w 矩形幅
	 * @param h 矩形高さ
	 * @param alignX 水平アラインメント (0=左, 0.5=中央, 1=右)
	 * @param alignY 垂直アラインメント (0=上, 0.5=中央, 1=下)
	 * @param wrap ワードラップモード (0=None, 1=Char, 2=Word, 3=MixedWrap, 4=Ellipsis)
	 * @param text 描画テキスト
	 * @return 更新領域情報
	 */
	RectF drawStringArea(const FontInfo *font, const Appearance *app, REAL x, REAL y, REAL w, REAL h, REAL alignX, REAL alignY, int wrap, const tjs_char *text);

	// -----------------------------------------------------------------------------
	
	/**
	 * 画像の描画。コピー先は元画像の Bounds を配慮した位置、サイズは Pixel 指定になります。
	 * @param x コピー先原点X
	 * @param y コピー先原点Y
	 * @param image コピー元画像
	 * @return 更新領域情報
	 */
	RectF drawImage(REAL x, REAL y, Image *src);

	/**
	 * 画像の矩形コピー
	 * @param dleft コピー先左端
	 * @param dtop  コピー先上端
	 * @param src コピー元画像
	 * @param sleft 元矩形の左端
	 * @param stop  元矩形の上端
	 * @param swidth 元矩形の横幅
	 * @param sheight  元矩形の縦幅
	 * @return 更新領域情報
	 */
	RectF drawImageRect(REAL dleft, REAL dtop, Image *src, REAL sleft, REAL stop, REAL swidth, REAL sheight);

	/**
	 * 画像の拡大縮小コピー
	 * @param dleft コピー先左端
	 * @param dtop  コピー先上端
	 * @param dwidth コピー先の横幅
	 * @param dheight  コピー先の縦幅
	 * @param src コピー元画像
	 * @param sleft 元矩形の左端
	 * @param stop  元矩形の上端
	 * @param swidth 元矩形の横幅
	 * @param sheight  元矩形の縦幅
	 * @return 更新領域情報
	 */
	RectF drawImageStretch(REAL dleft, REAL dtop, REAL dwidth, REAL dheight, Image *src, REAL sleft, REAL stop, REAL swidth, REAL sheight);

	/**
	 * 画像のアフィン変換コピー
	 * @param src コピー元画像
	 * @param sleft 元矩形の左端
	 * @param stop  元矩形の上端
	 * @param swidth 元矩形の横幅
	 * @param sheight  元矩形の縦幅
	 * @param affine アフィンパラメータの種類(true:変換行列, false:座標指定), 
	 * @return 更新領域情報
	 */
	RectF drawImageAffine(Image *src, REAL sleft, REAL stop, REAL swidth, REAL sheight, bool affine, REAL A, REAL B, REAL C, REAL D, REAL E, REAL F);

};

class Image {

protected:
    tvg::Picture* picture;
    float imgWidth;
    float imgHeight;
    bool loaded;
    std::shared_ptr<RecordData> record; // Layer.getRecordImage の記録 (picture の代わり)

public:
    Image();
    Image(const Image& orig);
    ~Image();

    // ファイルから読み込み
    bool load(const char* filename);
    bool load(const tjs_char* filename);
    
    // メモリから読み込み
    bool load(const void* data, uint32_t size, const char* mimeType = nullptr);
    
    // RAWピクセルデータ読み込み
    bool loadRaw(const uint32_t* data, uint32_t w, uint32_t h, bool copy = true);

    // 描画用Pictureを取得
    tvg::Picture* getPicture() const { return picture; }

    // クローン
    Image* Clone() const;

    // プロパティ
    float GetWidth() const { return imgWidth; }
    float GetHeight() const { return imgHeight; }
    RectF GetBounds() const; // 記録の画像は記録内容の外接矩形 (記録座標)
    bool IsLoaded() const { return loaded; }

    // 記録の画像 (Layer.getRecordImage)
    bool IsRecord() const { return (bool)record; }
    RecordData* getRecord() const { return record.get(); }
    void setRecord(std::shared_ptr<RecordData> r);

    // サイズ変更（描画時のサイズに影響）
    void SetSize(float w, float h);
};

// --------------------------------------------------------
// Layer.record / getRecordImage の記録 (GDI+ の Metafile 相当)。
//   描画命令 (図形のパス + Appearance の写し + 記録時の transform) の一覧で持ち、
//   drawImage* で「元の範囲 → 描き先」の写像を掛けて描き直す (ベクタのまま拡大縮小できる)。
//   ⚠ 記録するのは図形の描画 (drawShapeWithAppearance を通るもの) だけ。文字と画像は記録しない
// --------------------------------------------------------
class RecordData {
public:
    struct Entry {
        Appearance app;          // 見た目の写し
        tvg::Shape* shape;       // 変換前のパス (RecordData が所有)
        tvg::Matrix transform;   // 記録時の Layer の transform (viewTransform は含めない。GDI+ と同じ)
    };
    vector<Entry> entries;
    RectF bounds;    // 記録内容の外接矩形 (記録座標。線の太さ込み)
    bool hasBounds;

    RecordData() : hasBounds(false) {}
    RecordData(const RecordData& o);   // 図形も複製する
    ~RecordData();
    void add(const Appearance* app, const tvg::Shape* base, const tvg::Matrix& tr);
private:
    RecordData& operator=(const RecordData&);
};

// 画像読み込みヘルパー関数
Image* loadImage(const tjs_char* name);
RectF* getBounds(Image* image);

// 初期化・終了処理
void initThorvg();
void deInitThorvg();

#endif
