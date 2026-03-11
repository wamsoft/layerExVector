Title: layerExVector plugin
Author: わたなべごう

●これはなに？

Layer クラスにベクタ描画機能拡張を行うプラグインです。
Windows専用の LayerExDraw プラグインのサブセットを実装したものになります。

ベクタ描画には thorvg を使用しています（現状ではCPU描画のみの対応になります）
https://www.thorvg.org/

 == 特徴 ==

・アウトラインベースの描画が可能

　複数の PATH/FILL を組み合わせて様々なプリミティブを描画することができます

・テキスト描画にプライベートフォントセットが利用できる

　アーカイブ中にあるフォントファイルを
  直接指定してフォント生成させることができます

●使い方

manual.tjs 参照

●ライセンス

このプラグインのライセンスは吉里吉里本体に準拠してください。

ThorVG のライセンスは以下です

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