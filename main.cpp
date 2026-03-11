#include "ncbind.hpp"
#include "LayerExDraw.hpp"
#include <vector>

/**
 * ログ出力用
 */
void
message_log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char msg[1024];
	_vsnprintf_s(msg, 1024, _TRUNCATE, format, args);
	TVPAddLog(ttstr(msg));
	va_end(args);
}

/**
 * エラーログ出力用
 */
void
error_log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char msg[1024];
	_vsnprintf_s(msg, 1024, _TRUNCATE, format, args);
	TVPAddImportantLog(ttstr(msg));
	va_end(args);
}

extern void initThorvg();
extern void deInitThorvg();

// ----------------------------------------------------------------
// 実体型の登録
// 数値パラメータ系は配列か辞書を使えるような特殊コンバータを構築
// ----------------------------------------------------------------

// 両方自前コンバータ
#define NCB_SET_CONVERTOR_BOTH(type, convertor)\
NCB_TYPECONV_SRCMAP_SET(type, convertor<type>, true);\
NCB_TYPECONV_DSTMAP_SET(type, convertor<type>, true)

// SRCだけ自前コンバータ
#define NCB_SET_CONVERTOR_SRC(type, convertor)\
NCB_TYPECONV_SRCMAP_SET(type, convertor<type>, true);\
NCB_TYPECONV_DSTMAP_SET(type, ncbNativeObjectBoxing::Unboxing, true)

// DSTだけ自前コンバータ
#define NCB_SET_CONVERTOR_DST(type, convertor)\
NCB_TYPECONV_SRCMAP_SET(type, ncbNativeObjectBoxing::Boxing,   true); \
NCB_TYPECONV_DSTMAP_SET(type, convertor<type>, true)

/**
 * 配列かどうかの判定
 * @param var VARIANT
 * @return 配列なら true
 */
bool IsArray(const tTJSVariant &var)
{
	if (var.Type() == tvtObject) {
		iTJSDispatch2 *obj = var.AsObjectNoAddRef();
		return obj->IsInstanceOf(0, NULL, NULL, TJS_W("Array"), obj) == TJS_S_TRUE;
	}
	return false;
}

// メンバ変数をプロパティとして登録
#define NCB_MEMBER_PROPERTY(name, type, membername) \
	struct AutoProp_ ## name { \
		static void ProxySet(Class *inst, type value) { inst->membername = value; } \
		static type ProxyGet(Class *inst) {      return inst->membername; } }; \
	NCB_PROPERTY_PROXY(name,AutoProp_ ## name::ProxyGet, AutoProp_ ## name::ProxySet)

// ポインタ引数型の getter を変換登録
#define NCB_ARG_PROPERTY_RO(name, type, methodname) \
	struct AutoProp_ ## name { \
		static type ProxyGet(Class *inst) { type var; inst->methodname(&var); return var; } }; \
	Property(TJS_W(# name), &AutoProp_ ## name::ProxyGet, (int)0, Proxy)

// ------------------------------------------------------
// 型コンバータ登録
// ------------------------------------------------------

NCB_TYPECONV_CAST_INTEGER(FontStyle);
NCB_TYPECONV_CAST_INTEGER(BrushType);
NCB_TYPECONV_CAST_INTEGER(LineCap);
NCB_TYPECONV_CAST_INTEGER(LineJoin);
NCB_TYPECONV_CAST_INTEGER(MatrixOrder);

// ------------------------------------------------------- PointF
template <class T>
struct PointFConvertor {
	typedef ncbInstanceAdaptor<T> AdaptorT;
	template <typename ANYT>
	void operator ()(ANYT &adst, const tTJSVariant &src) {
		if (src.Type() == tvtObject) {
			T *obj = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef());
			if (obj) {
				dst = *obj;
			} else {
				ncbPropAccessor info(src);
				if (IsArray(src)) {
					dst = PointF((REAL)info.getRealValue(0),
								 (REAL)info.getRealValue(1));
				} else {
					dst = PointF((REAL)info.getRealValue(TJS_W("x")),
								 (REAL)info.getRealValue(TJS_W("y")));
				}
			}
		} else {
			dst = T();
		}
		adst = ncbTypeConvertor::ToTarget<ANYT>::Get(&dst);
	}
private:
	T dst;
};

NCB_SET_CONVERTOR_DST(PointF, PointFConvertor);
NCB_REGISTER_SUBCLASS_DELAY(PointF) {
	NCB_CONSTRUCTOR((REAL,REAL));
	NCB_MEMBER_PROPERTY(x, REAL, X);
	NCB_MEMBER_PROPERTY(y, REAL, Y);
	NCB_METHOD(Equals);
};

PointF getPoint(const tTJSVariant &var)
{
	PointFConvertor<PointF> conv;
	PointF ret;
	conv(ret, var);
	return ret;
}

// ------------------------------------------------------- RectF
template <class T>
struct RectFConvertor {
	typedef ncbInstanceAdaptor<T> AdaptorT;
	template <typename ANYT>
	void operator ()(ANYT &adst, const tTJSVariant &src) {
		if (src.Type() == tvtObject) {
			T *obj = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef());
			if (obj) {
				dst = *obj;
			} else {
				ncbPropAccessor info(src);
				if (IsArray(src)) {
					dst = RectF((REAL)info.getRealValue(0),
								(REAL)info.getRealValue(1),
								(REAL)info.getRealValue(2),
								(REAL)info.getRealValue(3));
				} else {
					dst = RectF((REAL)info.getRealValue(TJS_W("x")),
								(REAL)info.getRealValue(TJS_W("y")),
								(REAL)info.getRealValue(TJS_W("width")),
								(REAL)info.getRealValue(TJS_W("height")));
				}
			}
		} else {
			dst = T();
		}
		adst = ncbTypeConvertor::ToTarget<ANYT>::Get(&dst);
	}
private:
	T dst;
};

NCB_SET_CONVERTOR_DST(RectF, RectFConvertor);
NCB_REGISTER_SUBCLASS_DELAY(RectF) {
	NCB_CONSTRUCTOR((REAL,REAL,REAL,REAL));
	NCB_MEMBER_PROPERTY(x, REAL, X);
	NCB_MEMBER_PROPERTY(y, REAL, Y);
	NCB_MEMBER_PROPERTY(width, REAL, Width);
	NCB_MEMBER_PROPERTY(height, REAL, Height);
	NCB_PROPERTY_RO(left, GetLeft);
	NCB_PROPERTY_RO(top, GetTop);
	NCB_PROPERTY_RO(right, GetRight);
	NCB_PROPERTY_RO(bottom, GetBottom);
	NCB_ARG_PROPERTY_RO(location, PointF, GetLocation);
	NCB_ARG_PROPERTY_RO(bounds, RectF, GetBounds);
	NCB_METHOD(Clone);
	NCB_METHOD(Equals);
	NCB_METHOD_DETAIL(Inflate, Class, void, Class::Inflate, (REAL,REAL));
	NCB_METHOD_DETAIL(InflatePoint, Class, void, Class::Inflate, (const PointF&));
	NCB_METHOD(IntersectsWith);
	NCB_METHOD(IsEmptyArea);
	NCB_METHOD_DETAIL(Offset, Class, void, Class::Offset, (REAL,REAL));
	NCB_METHOD(Union);
};

RectF getRect(const tTJSVariant &var)
{
	RectFConvertor<RectF> conv;
	RectF ret;
	conv(ret, var);
	return ret;
}

// ------------------------------------------------------- Matrix

/**
 * Matrixラッピング用テンプレートクラス
 */
class MatrixWrapper {
protected:
	Matrix *obj;
public:
	// デフォルトコンストラクタ
	MatrixWrapper() : obj(NULL) {
		obj = new Matrix();
	}

	// 関数の帰り値としてのオブジェクト生成時用。
	// そのまま渡されたポインタを使う
	MatrixWrapper(Matrix *matrix) : obj(matrix) {
	}

	// コピーコンストラクタ
	MatrixWrapper(const MatrixWrapper &orig) : obj(NULL) {
		if (orig.obj) {
			obj = new Matrix(*orig.obj);
		}
	}
	
	// デストラクタ
	~MatrixWrapper() {
		if (obj) {
			delete obj;
		}
	}
	
	Matrix *getMatrix() { return obj; }

	void setMatrix(Matrix *src) {
		if (obj) {
			delete obj;
		}
		obj = src;
	}

	struct BridgeFunctor {
		Matrix* operator()(MatrixWrapper *p) const {
			return p->getMatrix();
		}
	};
};

/**
 * Matrixコンバータ
 */
template <class T>
struct MatrixConvertor {
	typedef T *MatrixP;
	typedef MatrixWrapper WrapperT;
	typedef ncbInstanceAdaptor<WrapperT> AdaptorT;
protected:
	Matrix *result;
public:
	MatrixConvertor() : result(NULL) {}
	~MatrixConvertor() { delete result; }
	
	void operator ()(MatrixP &dst, const tTJSVariant &src) {
		WrapperT *obj;
		if (src.Type() == tvtObject) {
			if ((obj = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef()))) {
				dst = obj->getMatrix();
			} else {
				ncbPropAccessor info(src);
				if (IsArray(src)) {
					result = new Matrix((REAL)info.getRealValue(0),
										(REAL)info.getRealValue(1),
										(REAL)info.getRealValue(2),
										(REAL)info.getRealValue(3),
										(REAL)info.getRealValue(4),
										(REAL)info.getRealValue(5));
				} else {
					result = new Matrix((REAL)info.getRealValue(TJS_W("m11")),
										(REAL)info.getRealValue(TJS_W("m12")),
										(REAL)info.getRealValue(TJS_W("m21")),
										(REAL)info.getRealValue(TJS_W("m22")),
										(REAL)info.getRealValue(TJS_W("dx")),
										(REAL)info.getRealValue(TJS_W("dy")));
				}
				dst = result;
			}
		} else {
			dst = NULL;
		}
	}

	void operator ()(tTJSVariant &dst, const MatrixP &src) {
		if (src != NULL) {
			Matrix *clone = new Matrix(*src);
			iTJSDispatch2 *adpobj = AdaptorT::CreateAdaptor(new WrapperT(clone));
			if (adpobj) {
				dst = tTJSVariant(adpobj, adpobj);
				adpobj->Release();			
			} else {
				delete clone;
				dst = NULL;
			}
		} else {
			dst.Clear();
		}
	}
};

NCB_SET_CONVERTOR(Matrix*, MatrixConvertor<Matrix>);
NCB_SET_CONVERTOR(const Matrix*, MatrixConvertor<const Matrix>);

static tjs_error
MatrixFactory(MatrixWrapper **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	Matrix *matrix = NULL;
	if (numparams == 0) {
		matrix = new Matrix();
	} else if (numparams == 6) {
		matrix = new Matrix((REAL)params[0]->AsReal(),
							(REAL)params[1]->AsReal(),
							(REAL)params[2]->AsReal(),
							(REAL)params[3]->AsReal(),
							(REAL)params[4]->AsReal(),
							(REAL)params[5]->AsReal());
	} else {
		return TJS_E_INVALIDPARAM;
	}
	*result = new MatrixWrapper(matrix);
	return TJS_S_OK;
}

#define NCB_MATRIX_METHOD(name)  Method(TJS_W(# name), &Matrix::name, Bridge<MatrixWrapper::BridgeFunctor>())

NCB_REGISTER_SUBCLASS(MatrixWrapper)
{
	Factory(MatrixFactory);
	NCB_MATRIX_METHOD(OffsetX);
	NCB_MATRIX_METHOD(OffsetY);
	NCB_MATRIX_METHOD(Equals);
	NCB_MATRIX_METHOD(SetElements);
	NCB_MATRIX_METHOD(Invert);
	NCB_MATRIX_METHOD(IsIdentity);
	NCB_MATRIX_METHOD(IsInvertible);
	NCB_MATRIX_METHOD(Multiply);
	NCB_MATRIX_METHOD(Reset);
	NCB_MATRIX_METHOD(Rotate);
	NCB_MATRIX_METHOD(Scale);
	NCB_MATRIX_METHOD(Translate);
};

// ------------------------------------------------------
// 自前記述クラス登録
// ------------------------------------------------------

NCB_REGISTER_SUBCLASS(FontInfo) {
	NCB_CONSTRUCTOR((const tjs_char *, REAL, INT));
	NCB_PROPERTY(familyName, getFamilyName, setFamilyName);
	NCB_PROPERTY(emSize, getEmSize, setEmSize);
	NCB_PROPERTY(style, getStyle, setStyle);
	NCB_PROPERTY_RO(ascent, getAscent);
	NCB_PROPERTY_RO(descent, getDescent);
	NCB_PROPERTY_RO(ascentLeading, getAscentLeading);
	NCB_PROPERTY_RO(descentLeading, getDescentLeading);
	NCB_PROPERTY_RO(lineSpacing, getLineSpacing);
};

NCB_REGISTER_SUBCLASS(Appearance) {
	NCB_CONSTRUCTOR(());
	NCB_METHOD(clear);
	NCB_METHOD(addBrush);
	NCB_METHOD(addPen);
};

NCB_REGISTER_SUBCLASS(Path) {
	NCB_CONSTRUCTOR(());
	NCB_METHOD(startFigure);
	NCB_METHOD(closeFigure);
	NCB_METHOD(drawArc);
	NCB_METHOD(drawPie);
	NCB_METHOD(drawBezier);
	NCB_METHOD(drawBeziers);
	NCB_METHOD(drawClosedCurve);
	NCB_METHOD(drawClosedCurve2);
	NCB_METHOD(drawCurve);
	NCB_METHOD(drawCurve2);
	NCB_METHOD(drawCurve3);
	NCB_METHOD(drawEllipse);
	NCB_METHOD(drawLine);
	NCB_METHOD(drawLines);
	NCB_METHOD(drawPolygon);
	NCB_METHOD(drawRectangle);
	NCB_METHOD(drawRectangles);
};

// ------------------------------------------------------- Image

/**
 * Imageコンバータ
 * 文字列からも変更可能
 */
template <class T>
struct ImageConvertor {
	typedef Image* ImageP;
	typedef ncbInstanceAdaptor<Image> AdaptorT;
protected:
	Image* result;
public:
	ImageConvertor() : result(NULL) {}
	~ImageConvertor() { delete result; }
	
	void operator ()(ImageP &dst, const tTJSVariant &src) {
		if (src.Type() == tvtObject) {
			Image *obj = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef());
			if (obj) {
				dst = obj;
			} else {
				// LayerExDrawからの変換は未サポート
				dst = NULL;
			}
		} else if (src.Type() == tvtString) {
			// 文字列から生成
			dst = result = loadImage(src.GetString());
		} else {
			dst = NULL;
		}
	}

	void operator ()(tTJSVariant &dst, const ImageP &src) {
		if (src != NULL) {
			Image *clone = src->Clone();
			iTJSDispatch2 *adpobj = AdaptorT::CreateAdaptor(clone);
			if (adpobj) {
				dst = tTJSVariant(adpobj, adpobj);
				adpobj->Release();
			} else {
				delete clone;
				dst = NULL;
			}
		} else {
			dst.Clear();
		}
	}
};

NCB_SET_CONVERTOR(Image*, ImageConvertor<Image>);
NCB_SET_CONVERTOR(const Image*, ImageConvertor<const Image>);

static tjs_error
ImageFactory(Image **result, tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis)
{
	if (numparams == 0) {
		*result = new Image();
		return TJS_S_OK;
	} else if (numparams > 0 && params[0]->Type() == tvtString) {
		Image *image = loadImage(params[0]->GetString());
		if (image) {
			*result = image;
			return TJS_S_OK;
		} else {
			TVPThrowExceptionMessage(TJS_W("cannot open:%1"), *params[0]);
		}
	}
	return TJS_E_INVALIDPARAM;
}

static void ImageLoad(Image *obj, const tjs_char *filename)
{
	if (!obj->load(filename)) {
		TVPThrowExceptionMessage(TJS_W("cannot open:%1"), ttstr(filename));
	}
}

static tTJSVariant ImageClone(Image *obj)
{
	typedef ncbInstanceAdaptor<Image> AdaptorT;
	tTJSVariant ret;
	if (obj && obj->IsLoaded()) {
		Image *newimage = obj->Clone();
		iTJSDispatch2 *adpobj = AdaptorT::CreateAdaptor(newimage);
		if (adpobj) {
			ret = tTJSVariant(adpobj, adpobj);
			adpobj->Release();
		} else {
			delete newimage;
		}
	}
	return ret;
}

static tTJSVariant ImageGetBounds(Image *obj)
{
	typedef ncbInstanceAdaptor<RectF> AdaptorT;
	tTJSVariant ret;
	if (obj) {
		RectF *bounds = new RectF(obj->GetBounds());
		iTJSDispatch2 *adpobj = AdaptorT::CreateAdaptor(bounds);
		if (adpobj) {
			ret = tTJSVariant(adpobj, adpobj);
			adpobj->Release();
		} else {
			delete bounds;
		}
	}
	return ret;
}

NCB_REGISTER_SUBCLASS(Image) {
	Factory(ImageFactory);
	NCB_METHOD_PROXY(load, ImageLoad);
	NCB_METHOD_PROXY(Clone, ImageClone);
	NCB_METHOD_PROXY(GetBounds, ImageGetBounds);
	NCB_PROPERTY_RO(width, GetWidth);
	NCB_PROPERTY_RO(height, GetHeight);
	NCB_PROPERTY_RO(isLoaded, IsLoaded);
};

#define ENUM(n) Variant(#n, (int)n)
#define NCB_SUBCLASS_NAME(name) NCB_SUBCLASS(name, name)




NCB_REGISTER_CLASS(GdiPlus)
{
	// FontStyle
	ENUM(FontStyleBold);
	ENUM(FontStyleItalic);
	ENUM(FontStyleUnderline);
	ENUM(FontStyleStrikeout);

	// BrushType
	ENUM(BrushTypeSolidColor);
	ENUM(BrushTypePathGradient);
	ENUM(BrushTypeLinearGradient);

	// LineCap
	ENUM(LineCapFlat);
	ENUM(LineCapSquare);
	ENUM(LineCapRound);
	ENUM(LineCapTriangle);

	// LineJoin
	ENUM(LineJoinMiter);
	ENUM(LineJoinBevel);
	ENUM(LineJoinRound);
	ENUM(LineJoinMiterClipped);

	// MatrixOrder
	ENUM(MatrixOrderPrepend);
	ENUM(MatrixOrderAppend);

// classes
	NCB_SUBCLASS_NAME(PointF);
	NCB_SUBCLASS_NAME(RectF);
	NCB_SUBCLASS(Matrix, MatrixWrapper);
	NCB_SUBCLASS_NAME(Image);
	
	NCB_SUBCLASS(Font,FontInfo);
	NCB_SUBCLASS(Appearance,Appearance);
	NCB_SUBCLASS(Path,Path);
}

NCB_GET_INSTANCE_HOOK(LayerExDraw)
{
	// インスタンスゲッタ
	NCB_INSTANCE_GETTER(objthis) { // objthis を iTJSDispatch2* 型の引数とする
		ClassT* obj = GetNativeInstance(objthis);	// ネイティブインスタンスポインタ取得
		if (!obj) {
			obj = new ClassT(objthis);				// ない場合は生成する
			SetNativeInstance(objthis, obj);		// objthis に obj をネイティブインスタンスとして登録する
		}
		obj->reset();
		return obj;
	}
	// デストラクタ（実際のメソッドが呼ばれた後に呼ばれる）
	~NCB_GET_INSTANCE_HOOK_CLASS () {
	}
};

// フックつきアタッチ
NCB_ATTACH_CLASS_WITH_HOOK(LayerExDraw, Layer) {
	NCB_PROPERTY(updateWhenDraw, getUpdateWhenDraw, setUpdateWhenDraw);
	NCB_PROPERTY(smoothingMode, getSmoothingMode, setSmoothingMode);
	
	NCB_METHOD(setViewTransform);
	NCB_METHOD(resetViewTransform);
	NCB_METHOD(rotateViewTransform);
	NCB_METHOD(scaleViewTransform);
	NCB_METHOD(translateViewTransform);

	NCB_METHOD(setTransform);
	NCB_METHOD(resetTransform);
	NCB_METHOD(rotateTransform);
	NCB_METHOD(scaleTransform);
	NCB_METHOD(translateTransform);

	NCB_METHOD(clear);
	NCB_METHOD(drawPath);
	NCB_METHOD(drawArc);
	NCB_METHOD(drawPie);
	NCB_METHOD(drawBezier);
	NCB_METHOD(drawBeziers);
	NCB_METHOD(drawClosedCurve);
	NCB_METHOD(drawClosedCurve2);
	NCB_METHOD(drawCurve);
	NCB_METHOD(drawCurve2);
	NCB_METHOD(drawCurve3);
	NCB_METHOD(drawEllipse);
	NCB_METHOD(drawLine);
	NCB_METHOD(drawLines);
	NCB_METHOD(drawPolygon);
	NCB_METHOD(drawRectangle);
	NCB_METHOD(drawRectangles);
	NCB_METHOD(drawPathString);
	NCB_METHOD(drawString);
	NCB_METHOD(measureString);
	NCB_METHOD(measureStringInternal);

	NCB_METHOD(drawImage);
	NCB_METHOD(drawImageRect);
	NCB_METHOD(drawImageStretch);
	NCB_METHOD(drawImageAffine);	
}

// ----------------------------------- 起動・開放処理

NCB_PRE_REGIST_CALLBACK(initThorvg);
NCB_POST_UNREGIST_CALLBACK(deInitThorvg);
