# layerExVector plugin

**Author:** わたなべごう

## これはなに？

Layer クラスにベクタ描画機能拡張を行うプラグインです。
Windows専用の LayerExDraw プラグインのサブセットとして実装したものになります。
※このためクラス名が GdiPlus のままになってます

ベクタ描画には [ThorVG](https://www.thorvg.org/) を使用しています

### 特徴

- **アウトラインベースの描画が可能**

  複数の PATH/FILL を組み合わせて様々なプリミティブを描画することができます

- **テキスト描画機構は従来の LayerExDraw のそれとは別物になります**

  1. `GdiPlus.loadFont()` でフォントを登録
  2. 描画時には登録名を指定して `GdiPlus.Font` を作成。イタリックや変形などの指定がこれで可能
  3. フォントを指定して `drawString()` で描画

## 使い方

[manual.tjs](manual.tjs) 参照

## 拡張想定

- 内部の Canvas を表にだして連続描画したうえでのSVG出力も対応させる
- Canvas を使って OGL描画用のインターフェース対応

## ライセンス

このプラグインのライセンスは吉里吉里本体に準拠してください。

### ThorVG License

```
MIT License

Copyright (c) 2020 - 2026 ThorVG Project

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
