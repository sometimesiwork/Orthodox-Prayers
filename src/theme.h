// Оформление приложения: цветовые темы, византийская орнаментика и векторные иконки.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <gdiplus.h>
#include <cmath>
#include <string>

using namespace Gdiplus;

namespace theme {

struct Palette {
    bool dark = true;
    int index = 0;
    COLORREF bg = 0;
    COLORREF sidebarTop = 0;
    COLORREF sidebarBottom = 0;
    COLORREF sidebarLine = 0;
    COLORREF card = 0;
    COLORREF cardTop = 0;
    COLORREF cardBorder = 0;
    COLORREF title = 0;
    COLORREF text = 0;
    COLORREF textDim = 0;
    COLORREF accent = 0;
    COLORREF accentDim = 0;
    COLORREF hover = 0;
    COLORREF selection = 0;
    COLORREF divider = 0;
    COLORREF rubric = 0;
    COLORREF dropCap = 0;
    COLORREF groupLabel = 0;
    COLORREF inputBg = 0;
    COLORREF inputText = 0;
    COLORREF shadow = 0;
};

inline Palette MakeByIndex(int themeIdx) {
    Palette p;
    p.index = themeIdx;
    switch (themeIdx) {
        case 1:  // «Древний пергамент» (светлая с уставной киноварью)
            p.dark = false;
            p.bg = RGB(246, 240, 226);
            p.sidebarTop = RGB(240, 231, 212);
            p.sidebarBottom = RGB(226, 214, 189);
            p.sidebarLine = RGB(205, 190, 160);
            p.card = RGB(254, 251, 243);
            p.cardTop = RGB(255, 254, 249);
            p.cardBorder = RGB(220, 205, 175);
            p.title = RGB(142, 48, 30);
            p.text = RGB(36, 28, 20);
            p.textDim = RGB(114, 98, 74);
            p.accent = RGB(156, 96, 32);
            p.accentDim = RGB(154, 126, 84);
            p.hover = RGB(232, 221, 198);
            p.selection = RGB(214, 196, 162);
            p.divider = RGB(216, 201, 172);
            p.rubric = RGB(156, 52, 36);
            p.dropCap = RGB(168, 42, 28);
            p.groupLabel = RGB(150, 112, 58);
            p.inputBg = RGB(255, 252, 245);
            p.inputText = RGB(36, 28, 20);
            p.shadow = RGB(110, 92, 62);
            break;

        case 2:  // «Монастырский камень» (тёплая тёмная византийская)
            p.dark = true;
            p.bg = RGB(18, 16, 15);
            p.sidebarTop = RGB(32, 27, 24);
            p.sidebarBottom = RGB(15, 13, 12);
            p.sidebarLine = RGB(56, 47, 40);
            p.card = RGB(27, 24, 22);
            p.cardTop = RGB(33, 29, 26);
            p.cardBorder = RGB(58, 49, 41);
            p.title = RGB(232, 188, 114);
            p.text = RGB(236, 230, 220);
            p.textDim = RGB(162, 148, 132);
            p.accent = RGB(230, 180, 102);
            p.accentDim = RGB(158, 128, 86);
            p.hover = RGB(44, 38, 33);
            p.selection = RGB(92, 68, 42);
            p.divider = RGB(64, 54, 44);
            p.rubric = RGB(212, 132, 112);
            p.dropCap = RGB(224, 108, 84);
            p.groupLabel = RGB(180, 146, 98);
            p.inputBg = RGB(34, 30, 27);
            p.inputText = RGB(234, 228, 218);
            p.shadow = RGB(0, 0, 0);
            break;

        case 3:  // «Светлая лазурь» (чистая слоновая кость и византийская лазурь)
            p.dark = false;
            p.bg = RGB(244, 246, 250);
            p.sidebarTop = RGB(234, 239, 247);
            p.sidebarBottom = RGB(220, 228, 240);
            p.sidebarLine = RGB(196, 207, 224);
            p.card = RGB(255, 255, 255);
            p.cardTop = RGB(255, 255, 255);
            p.cardBorder = RGB(210, 219, 233);
            p.title = RGB(34, 72, 128);
            p.text = RGB(28, 34, 46);
            p.textDim = RGB(100, 112, 132);
            p.accent = RGB(42, 88, 156);
            p.accentDim = RGB(112, 134, 168);
            p.hover = RGB(224, 232, 245);
            p.selection = RGB(196, 216, 244);
            p.divider = RGB(212, 220, 234);
            p.rubric = RGB(158, 52, 44);
            p.dropCap = RGB(168, 44, 36);
            p.groupLabel = RGB(76, 106, 152);
            p.inputBg = RGB(255, 255, 255);
            p.inputText = RGB(28, 34, 46);
            p.shadow = RGB(85, 100, 125);
            break;

        default:  // 0: «Ночная лампада» (классическая тёмно-синяя с золотом)
            p.dark = true;
            p.index = 0;
            p.bg = RGB(11, 16, 28);
            p.sidebarTop = RGB(23, 33, 57);
            p.sidebarBottom = RGB(9, 13, 24);
            p.sidebarLine = RGB(36, 51, 82);
            p.card = RGB(19, 26, 43);
            p.cardTop = RGB(23, 32, 52);
            p.cardBorder = RGB(38, 53, 84);
            p.title = RGB(232, 192, 122);
            p.text = RGB(235, 231, 222);
            p.textDim = RGB(144, 158, 184);
            p.accent = RGB(232, 192, 122);
            p.accentDim = RGB(154, 132, 95);
            p.hover = RGB(31, 43, 69);
            p.selection = RGB(48, 80, 132);
            p.divider = RGB(48, 64, 98);
            p.rubric = RGB(210, 146, 132);
            p.dropCap = RGB(226, 118, 96);
            p.groupLabel = RGB(172, 148, 106);
            p.inputBg = RGB(24, 33, 54);
            p.inputText = RGB(230, 228, 222);
            p.shadow = RGB(0, 0, 0);
            break;
    }
    return p;
}

inline Palette Make(bool dark) {
    return MakeByIndex(dark ? 0 : 1);
}

inline Color Gc(COLORREF c, int alpha = 255) {
    return Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
}

// Цвет богослужебного облачения дня:
// 0 — золотой, 1 — голубой, 2 — красный, 3 — зелёный, 4 — фиолетовый, 5 — белый
inline COLORREF VestmentColorRef(int vestmentCode, bool dark) {
    switch (vestmentCode) {
        case 1: return dark ? RGB(92, 172, 242) : RGB(36, 114, 188);   // голубой
        case 2: return dark ? RGB(232, 92, 82)  : RGB(184, 38, 32);    // красный
        case 3: return dark ? RGB(94, 196, 126) : RGB(38, 134, 68);    // зелёный
        case 4: return dark ? RGB(176, 124, 224): RGB(116, 58, 164);   // фиолетовый
        case 5: return dark ? RGB(238, 240, 246): RGB(130, 138, 154);  // белый
        default: return dark ? RGB(234, 190, 96) : RGB(176, 122, 24);  // золотой
    }
}

}  // namespace theme

namespace ui {

inline void Text(Graphics& g, const wchar_t* str, int count, const Font* font, const RectF& box,
                 StringFormat* fmt, Color color) {
    SolidBrush brush(color);
    g.DrawString(str, count, font, box, fmt, &brush);
}

inline void Text(Graphics& g, const std::wstring& str, const Font* font, const RectF& box,
                 StringFormat* fmt, Color color) {
    Text(g, str.c_str(), -1, font, box, fmt, color);
}

inline void AddRoundRect(GraphicsPath& path, const RectF& r, float rad) {
    const float d = std::min({rad * 2.0f, r.Width, r.Height});
    if (d <= 0.5f) {
        path.AddRectangle(r);
        return;
    }
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    path.CloseFigure();
}

// Византийский орнаментальный разделитель под заголовком карточки (─── ❖ ───)
inline void DrawOrnamentDivider(Graphics& g, float x, float y, float width, float scale,
                                Color lineCol, Color accentCol) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    const float len = std::min(width * 0.42f, 168.0f * scale);
    const float mid = x + 36.0f * scale;
    Pen pen(lineCol, 1.0f);
    g.DrawLine(&pen, x, y, mid - 8.0f * scale, y);
    g.DrawLine(&pen, mid + 8.0f * scale, y, x + len, y);

    // Ромб с точками по бокам
    const float r = 3.4f * scale;
    PointF diamond[4] = {
        PointF(mid, y - r),
        PointF(mid + r, y),
        PointF(mid, y + r),
        PointF(mid - r, y),
    };
    SolidBrush brush(accentCol);
    g.FillPolygon(&brush, diamond, 4);
    g.FillEllipse(&brush, mid - 6.6f * scale - 1.2f * scale, y - 1.2f * scale,
                  2.4f * scale, 2.4f * scale);
    g.FillEllipse(&brush, mid + 6.6f * scale - 1.2f * scale, y - 1.2f * scale,
                  2.4f * scale, 2.4f * scale);
}

}  // namespace ui

namespace icons {

constexpr double kPi = 3.14159265358979323846;

inline void Style(Pen& pen) {
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    pen.SetLineJoin(LineJoinRound);
}

inline void Sun(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.5f;
    const float r0 = r.Width * 0.20f;
    GraphicsPath path;
    path.AddEllipse(cx - r0, cy - r0, r0 * 2, r0 * 2);
    g.DrawPath(&pen, &path);
    for (int i = 0; i < 8; ++i) {
        const double a = i * kPi / 4.0 + kPi / 8.0;
        const float r1 = r.Width * 0.30f;
        const float r2 = r.Width * 0.46f;
        g.DrawLine(&pen, static_cast<float>(cx + std::cos(a) * r1),
                   static_cast<float>(cy + std::sin(a) * r1),
                   static_cast<float>(cx + std::cos(a) * r2),
                   static_cast<float>(cy + std::sin(a) * r2));
    }
}

inline void Moon(Graphics& g, const RectF& r, Color c, float w) {
    (void)w;
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.5f;
    const float rad = r.Width * 0.42f;
    GraphicsPath outer;
    outer.AddEllipse(cx - rad, cy - rad, rad * 2, rad * 2);
    GraphicsPath cut;
    cut.AddEllipse(cx - rad * 0.62f, cy - rad * 1.12f, rad * 1.85f, rad * 1.85f);
    Region region(&outer);
    Region hole(&cut);
    region.Exclude(&hole);
    SolidBrush brush(c);
    g.FillRegion(&brush, &region);
}

inline void Cross(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.5f;
    const float h = r.Height * 0.86f;
    const float v = r.Width * 0.24f;
    const float hh = r.Height * 0.26f;
    const float top = cy - h * 0.5f;
    g.DrawLine(&pen, cx, top, cx, cy + h * 0.5f);
    // верхняя малая перекладина
    g.DrawLine(&pen, cx - v * 0.55f, top + h * 0.14f, cx + v * 0.55f, top + h * 0.14f);
    // главная перекладина
    g.DrawLine(&pen, cx - v, top + hh, cx + v, top + hh);
    // косая нижняя перекладина восьмиконечного креста
    g.DrawLine(&pen, cx - v * 0.55f, top + h * 0.68f, cx + v * 0.55f, top + h * 0.78f);
}

inline void Book(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float top = r.Y + r.Height * 0.18f;
    const float bottom = r.Y + r.Height * 0.84f;
    const float half = r.Width * 0.42f;
    GraphicsPath path;
    path.StartFigure();
    path.AddBezier(cx, top + r.Height * 0.08f, cx - half * 0.6f, top - r.Height * 0.04f,
                   cx - half, top, cx - half, top);
    path.AddLine(cx - half, top, cx - half, bottom - r.Height * 0.06f);
    path.AddBezier(cx - half, bottom - r.Height * 0.06f, cx - half * 0.6f, bottom - r.Height * 0.02f,
                   cx, bottom - r.Height * 0.02f, cx, bottom - r.Height * 0.02f);
    path.AddBezier(cx, bottom - r.Height * 0.02f, cx + half * 0.6f, bottom - r.Height * 0.02f,
                   cx + half, bottom - r.Height * 0.06f, cx + half, bottom - r.Height * 0.06f);
    path.AddLine(cx + half, bottom - r.Height * 0.06f, cx + half, top);
    path.AddBezier(cx + half, top, cx + half * 0.6f, top, cx, top + r.Height * 0.08f,
                   cx, top + r.Height * 0.08f);
    g.DrawPath(&pen, &path);
    g.DrawLine(&pen, cx, top + r.Height * 0.08f, cx, bottom - r.Height * 0.04f);
}

inline void GospelBook(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float x = r.X + r.Width * 0.20f;
    const float y = r.Y + r.Height * 0.12f;
    const float bw = r.Width * 0.60f;
    const float bh = r.Height * 0.76f;
    GraphicsPath cover;
    ui::AddRoundRect(cover, RectF(x, y, bw, bh), r.Width * 0.07f);
    g.DrawPath(&pen, &cover);
    // корешок
    g.DrawLine(&pen, x + bw * 0.18f, y, x + bw * 0.18f, y + bh);
    // крест на окладе
    const float cx = x + bw * 0.58f;
    const float cy = y + bh * 0.48f;
    g.DrawLine(&pen, cx, cy - bh * 0.22f, cx, cy + bh * 0.22f);
    g.DrawLine(&pen, cx - bw * 0.18f, cy - bh * 0.05f, cx + bw * 0.18f, cy - bh * 0.05f);
}

inline void Scroll(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float left = r.X + r.Width * 0.26f;
    const float right = r.X + r.Width * 0.74f;
    const float top = r.Y + r.Height * 0.14f;
    const float bottom = r.Y + r.Height * 0.86f;
    g.DrawLine(&pen, left, top, left, bottom);
    g.DrawLine(&pen, right, top, right, bottom);
    for (int i = 0; i < 3; ++i) {
        const float y = top + r.Height * 0.18f + i * r.Height * 0.22f;
        g.DrawLine(&pen, left, y, right - r.Width * 0.06f, y);
    }
    GraphicsPath cap;
    cap.AddEllipse(left - r.Width * 0.12f, top - r.Height * 0.06f, r.Width * 0.24f, r.Height * 0.12f);
    cap.AddEllipse(left - r.Width * 0.12f, bottom - r.Height * 0.06f, r.Width * 0.24f,
                   r.Height * 0.12f);
    g.DrawPath(&pen, &cap);
}

inline void Flower(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.5f;
    const float rad = r.Width * 0.40f;
    for (int i = 0; i < 5; ++i) {
        GraphicsState state = g.Save();
        Matrix m;
        m.Translate(cx, cy);
        m.Rotate(static_cast<REAL>(i * 72.0f));
        m.Translate(0.0f, -rad * 0.52f);
        g.SetTransform(&m);
        GraphicsPath petal;
        petal.AddEllipse(-rad * 0.20f, -rad * 0.30f, rad * 0.40f, rad * 0.60f);
        g.DrawPath(&pen, &petal);
        g.Restore(state);
    }
    GraphicsPath center;
    center.AddEllipse(cx - rad * 0.16f, cy - rad * 0.16f, rad * 0.32f, rad * 0.32f);
    g.DrawPath(&pen, &center);
}

inline void Leaf(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.5f;
    const float rad = r.Width * 0.42f;
    GraphicsPath path;
    path.AddBezier(cx - rad * 0.55f, cy + rad * 0.75f, cx - rad * 0.95f, cy - rad * 0.25f,
                   cx - rad * 0.15f, cy - rad * 0.85f, cx + rad * 0.30f, cy - rad * 0.70f);
    path.AddBezier(cx + rad * 0.95f, cy - rad * 0.25f, cx + rad * 0.80f, cy + rad * 0.55f,
                   cx - rad * 0.55f, cy + rad * 0.75f, cx - rad * 0.55f, cy + rad * 0.75f);
    g.DrawPath(&pen, &path);
    g.DrawLine(&pen, cx - rad * 0.45f, cy + rad * 0.68f, cx + rad * 0.25f, cy - rad * 0.58f);
}

inline void Bell(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float top = r.Y + r.Height * 0.22f;
    const float bottom = r.Y + r.Height * 0.72f;
    const float half = r.Width * 0.34f;
    GraphicsPath path;
    path.StartFigure();
    path.AddBezier(cx - half, bottom, cx - half * 0.95f, bottom - r.Height * 0.24f,
                   cx - half * 0.62f, top + r.Height * 0.12f, cx, top);
    path.AddBezier(cx + half * 0.62f, top + r.Height * 0.12f, cx + half * 0.95f,
                   bottom - r.Height * 0.24f, cx + half, bottom, cx + half, bottom);
    path.AddLine(cx + half, bottom, cx - half, bottom);
    g.DrawPath(&pen, &path);
    g.DrawLine(&pen, cx - half * 1.25f, bottom, cx + half * 1.25f, bottom);
    g.DrawLine(&pen, cx, top - r.Height * 0.10f, cx, top);
    g.DrawEllipse(&pen, cx - half * 0.16f, bottom + r.Height * 0.03f, half * 0.32f, half * 0.32f);
}

inline void Star(Graphics& g, const RectF& r, Color c, float w, bool filled) {
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.5f;
    const float R = r.Width * 0.46f;
    const float r2 = R * 0.42f;
    PointF pts[10];
    for (int i = 0; i < 10; ++i) {
        const double a = -kPi / 2.0 + i * kPi / 5.0;
        const float rad = (i % 2 == 0) ? R : r2;
        pts[i].X = static_cast<float>(cx + std::cos(a) * rad);
        pts[i].Y = static_cast<float>(cy + std::sin(a) * rad);
    }
    GraphicsPath path;
    path.AddPolygon(pts, 10);
    if (filled) {
        SolidBrush brush(c);
        g.FillPath(&brush, &path);
    } else {
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        Pen pen(c, w);
        Style(pen);
        g.DrawPath(&pen, &path);
    }
}

inline void Heart(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.55f;
    const float rad = r.Width * 0.42f;
    GraphicsPath path;
    path.StartFigure();
    path.AddBezier(cx - rad, cy - rad * 0.30f, cx - rad * 0.55f, cy - rad * 1.25f, cx,
                   cy - rad * 0.42f, cx, cy - rad * 0.42f);
    path.AddBezier(cx, cy - rad * 0.42f, cx + rad * 0.55f, cy - rad * 1.25f, cx + rad,
                   cy - rad * 0.30f, cx + rad, cy - rad * 0.30f);
    path.AddBezier(cx + rad, cy - rad * 0.30f, cx + rad, cy + rad * 0.55f, cx, cy + rad * 0.85f,
                   cx, cy + rad * 0.85f);
    path.AddBezier(cx, cy + rad * 0.85f, cx - rad, cy + rad * 0.55f, cx - rad, cy - rad * 0.30f,
                   cx - rad, cy - rad * 0.30f);
    g.DrawPath(&pen, &path);
}

// Чаша Святого Причастия (Потир).
inline void Chalice(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float top = r.Y + r.Height * 0.24f;
    const float mid = r.Y + r.Height * 0.56f;
    const float bot = r.Y + r.Height * 0.88f;
    const float hw = r.Width * 0.32f;
    // малый крестик над чашей
    g.DrawLine(&pen, cx, r.Y + r.Height * 0.06f, cx, r.Y + r.Height * 0.19f);
    g.DrawLine(&pen, cx - r.Width * 0.08f, r.Y + r.Height * 0.12f,
               cx + r.Width * 0.08f, r.Y + r.Height * 0.12f);
    // чаша
    GraphicsPath cup;
    cup.AddBezier(cx - hw, top, cx - hw, mid, cx, mid, cx, mid);
    cup.AddBezier(cx, mid, cx + hw, mid, cx + hw, top, cx + hw, top);
    cup.AddLine(cx + hw, top, cx - hw, top);
    g.DrawPath(&pen, &cup);
    // ножка и подставка
    g.DrawLine(&pen, cx, mid, cx, bot);
    g.DrawLine(&pen, cx - r.Width * 0.12f, (mid + bot) * 0.5f,
               cx + r.Width * 0.12f, (mid + bot) * 0.5f);
    g.DrawLine(&pen, cx - hw * 0.85f, bot, cx + hw * 0.85f, bot);
}

// Праздничный венец / корона (для Тропарей праздников).
inline void Crown(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float left = r.X + r.Width * 0.16f;
    const float right = r.X + r.Width * 0.84f;
    const float cx = r.X + r.Width * 0.5f;
    const float top = r.Y + r.Height * 0.24f;
    const float bot = r.Y + r.Height * 0.78f;
    PointF pts[5] = {
        PointF(left, bot),
        PointF(left, top + r.Height * 0.10f),
        PointF(cx - r.Width * 0.16f, top + r.Height * 0.30f),
        PointF(cx, top),
        PointF(cx + r.Width * 0.16f, top + r.Height * 0.30f),
    };
    GraphicsPath p;
    p.AddLines(pts, 5);
    p.AddLine(cx + r.Width * 0.16f, top + r.Height * 0.30f, right, top + r.Height * 0.10f);
    p.AddLine(right, top + r.Height * 0.10f, right, bot);
    p.AddLine(right, bot, left, bot);
    g.DrawPath(&pen, &p);
}

// Крылья Ангела / щит (для Канона Ангелу-Хранителю).
inline void Wings(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.55f;
    GraphicsPath leftW;
    leftW.AddBezier(cx - r.Width * 0.05f, cy + r.Height * 0.22f,
                    cx - r.Width * 0.44f, cy + r.Height * 0.05f,
                    cx - r.Width * 0.44f, cy - r.Height * 0.36f,
                    cx - r.Width * 0.10f, cy - r.Height * 0.16f);
    GraphicsPath rightW;
    rightW.AddBezier(cx + r.Width * 0.05f, cy + r.Height * 0.22f,
                     cx + r.Width * 0.44f, cy + r.Height * 0.05f,
                     cx + r.Width * 0.44f, cy - r.Height * 0.36f,
                     cx + r.Width * 0.10f, cy - r.Height * 0.16f);
    g.DrawPath(&pen, &leftW);
    g.DrawPath(&pen, &rightW);
    g.DrawEllipse(&pen, cx - r.Width * 0.12f, r.Y + r.Height * 0.14f,
                  r.Width * 0.24f, r.Height * 0.24f);
}

// Пасхальный крест в лучах (для Пасхального канона).
inline void SunriseCross(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.52f;
    g.DrawLine(&pen, cx, r.Y + r.Height * 0.12f, cx, r.Y + r.Height * 0.88f);
    g.DrawLine(&pen, cx - r.Width * 0.26f, cy - r.Height * 0.10f,
               cx + r.Width * 0.26f, cy - r.Height * 0.10f);
    // диагональные лучи славы
    const float d1 = r.Width * 0.16f;
    const float d2 = r.Width * 0.34f;
    g.DrawLine(&pen, cx - d2, cy - r.Height * 0.10f - d2, cx - d1, cy - r.Height * 0.10f - d1);
    g.DrawLine(&pen, cx + d2, cy - r.Height * 0.10f - d2, cx + d1, cy - r.Height * 0.10f - d1);
    g.DrawLine(&pen, cx - d2, cy - r.Height * 0.10f + d2, cx - d1, cy - r.Height * 0.10f + d1);
    g.DrawLine(&pen, cx + d2, cy - r.Height * 0.10f + d2, cx + d1, cy - r.Height * 0.10f + d1);
}

// Давидова арфа (для Чина 12 псалмов).
inline void Harp(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float x0 = r.X + r.Width * 0.22f;
    const float x1 = r.X + r.Width * 0.78f;
    const float y0 = r.Y + r.Height * 0.16f;
    const float y1 = r.Y + r.Height * 0.84f;
    g.DrawLine(&pen, x0, y0, x0, y1);
    g.DrawLine(&pen, x0, y1, x1 * 0.92f, y1);
    GraphicsPath curve;
    curve.AddBezier(x0, y0, x0 + (x1 - x0) * 0.5f, y0 - r.Height * 0.06f,
                    x1, y0 + r.Height * 0.16f, x1 * 0.92f, y1);
    g.DrawPath(&pen, &curve);
    for (int i = 1; i <= 3; ++i) {
        const float sx = x0 + (x1 - x0) * (0.20f * static_cast<float>(i));
        g.DrawLine(&pen, sx, y0 + r.Height * 0.08f * static_cast<float>(i), sx, y1);
    }
}

// Церковная лампада с огоньком (для Молитв на разные случаи).
inline void Censer(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float top = r.Y + r.Height * 0.14f;
    const float bowlY = r.Y + r.Height * 0.52f;
    const float bot = r.Y + r.Height * 0.84f;
    const float hw = r.Width * 0.32f;
    // подвесы
    g.DrawLine(&pen, cx, top, cx - hw * 0.8f, bowlY);
    g.DrawLine(&pen, cx, top, cx + hw * 0.8f, bowlY);
    // чаша лампады
    GraphicsPath bowl;
    bowl.AddBezier(cx - hw, bowlY, cx - hw, bot, cx, bot, cx, bot);
    bowl.AddBezier(cx, bot, cx + hw, bot, cx + hw, bowlY, cx + hw, bowlY);
    bowl.AddLine(cx + hw, bowlY, cx - hw, bowlY);
    g.DrawPath(&pen, &bowl);
    // язычок пламени
    SolidBrush br(c);
    g.FillEllipse(&br, cx - r.Width * 0.07f, bowlY - r.Height * 0.18f,
                  r.Width * 0.14f, r.Height * 0.18f);
}

// Чётки с крестом (для раздела «Иисусова молитва и чётки»).
inline void Rosary(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.42f;
    const float rad = r.Width * 0.28f;
    SolidBrush br(c);
    for (int i = 0; i < 10; ++i) {
        const double a = i * (2.0 * kPi / 10.0) - kPi * 0.5;
        const float bx = static_cast<float>(cx + std::cos(a) * rad);
        const float by = static_cast<float>(cy + std::sin(a) * rad);
        const float brd = r.Width * 0.055f;
        g.FillEllipse(&br, bx - brd, by - brd, brd * 2.0f, brd * 2.0f);
    }
    // крестик внизу чёток
    const float top = cy + rad + r.Height * 0.04f;
    const float bot = r.Y + r.Height * 0.92f;
    g.DrawLine(&pen, cx, top, cx, bot);
    g.DrawLine(&pen, cx - r.Width * 0.11f, (top + bot) * 0.5f,
               cx + r.Width * 0.11f, (top + bot) * 0.5f);
}

// Портал / Глобус с крестом (для раздела «Азбука веры и порталы»).
inline void Portal(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.56f;
    const float rad = r.Width * 0.34f;
    g.DrawEllipse(&pen, cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
    g.DrawEllipse(&pen, cx - rad * 0.48f, cy - rad, rad * 0.96f, rad * 2.0f);
    g.DrawLine(&pen, cx - rad, cy, cx + rad, cy);
    // крест над державой
    g.DrawLine(&pen, cx, r.Y + r.Height * 0.06f, cx, cy - rad);
    g.DrawLine(&pen, cx - r.Width * 0.10f, r.Y + r.Height * 0.13f,
               cx + r.Width * 0.10f, r.Y + r.Height * 0.13f);
}

// Восьмиконечный крест на Голгофе (для Поминальника).
inline void Memorial(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    const float top = r.Y + r.Height * 0.10f;
    const float bot = r.Y + r.Height * 0.76f;
    g.DrawLine(&pen, cx, top, cx, bot);
    g.DrawLine(&pen, cx - r.Width * 0.22f, top + r.Height * 0.20f,
               cx + r.Width * 0.22f, top + r.Height * 0.20f);
    g.DrawLine(&pen, cx - r.Width * 0.12f, top + r.Height * 0.44f,
               cx + r.Width * 0.12f, top + r.Height * 0.50f);
    // ступени Голгофы
    g.DrawArc(&pen, cx - r.Width * 0.28f, bot, r.Width * 0.56f, r.Height * 0.28f, 180, 180);
}

// Свеча с живым огнём.
inline void Candle(Graphics& g, const RectF& r, Color body, Color flame, double flicker) {
    const float cx = r.X + r.Width * 0.5f;
    const float base = r.Y + r.Height * 0.96f;
    const float w = r.Width * 0.30f;
    const float h = r.Height * 0.46f;
    GraphicsPath bodyPath;
    bodyPath.AddRectangle(RectF(cx - w * 0.5f, base - h, w, h));
    SolidBrush bodyBrush(body);
    g.FillPath(&bodyBrush, &bodyPath);

    const float flameBase = base - h;
    const float sway = static_cast<float>(std::sin(flicker) * r.Width * 0.03f);
    const float fh = r.Height * (0.34f + 0.03f * static_cast<float>(std::sin(flicker * 2.3)));
    GraphicsPath flamePath;
    flamePath.StartFigure();
    flamePath.AddBezier(cx + sway - w * 0.30f, flameBase, cx + sway - w * 0.34f,
                        flameBase - fh * 0.55f, cx + sway - w * 0.12f, flameBase - fh * 0.80f,
                        cx + sway, flameBase - fh);
    flamePath.AddBezier(cx + sway + w * 0.12f, flameBase - fh * 0.80f, cx + sway + w * 0.34f,
                        flameBase - fh * 0.55f, cx + sway + w * 0.30f, flameBase,
                        cx + sway + w * 0.30f, flameBase);
    flamePath.CloseFigure();
    SolidBrush flameBrush(flame);
    g.FillPath(&flameBrush, &flamePath);

    for (int i = 0; i < 3; ++i) {
        const float gr = w * (1.1f + 0.55f * static_cast<float>(i));
        const int alpha = 26 - i * 8;
        SolidBrush glowBrush(Color(alpha, flame.GetR(), flame.GetG(), flame.GetB()));
        g.FillEllipse(&glowBrush, cx - gr, flameBase - fh - gr * 0.6f, gr * 2, gr * 2);
    }
}

inline void Magnifier(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.42f;
    const float cy = r.Y + r.Height * 0.42f;
    const float rad = r.Width * 0.28f;
    GraphicsPath ring;
    ring.AddEllipse(cx - rad, cy - rad, rad * 2, rad * 2);
    g.DrawPath(&pen, &ring);
    g.DrawLine(&pen, cx + rad * 0.72f, cy + rad * 0.72f, r.X + r.Width * 0.92f,
               r.Y + r.Height * 0.92f);
}

// Листок календаря.
inline void CalendarIcon(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float x0 = r.X + r.Width * 0.16f;
    const float y0 = r.Y + r.Height * 0.22f;
    const float qw = r.Width * 0.68f;
    const float qh = r.Height * 0.62f;
    g.DrawRectangle(&pen, RectF(x0, y0, qw, qh));
    g.DrawLine(&pen, x0 + qw * 0.30f, y0 - r.Height * 0.06f, x0 + qw * 0.30f,
               y0 + qh * 0.14f);
    g.DrawLine(&pen, x0 + qw * 0.70f, y0 - r.Height * 0.06f, x0 + qw * 0.70f,
               y0 + qh * 0.14f);
    g.DrawLine(&pen, x0, y0 + qh * 0.20f, x0 + qw, y0 + qh * 0.20f);
    for (int i = 0; i < 6; ++i) {
        const float px = x0 + qw * (0.18f + (i % 3) * 0.30f);
        const float py = y0 + qh * (0.40f + (i / 3) * 0.30f);
        SolidBrush br(c);
        g.FillEllipse(&br, px - qw * 0.05f, py - qh * 0.05f, qw * 0.10f, qh * 0.10f);
    }
}

// Купол с крестом (для раздела богослужений).
inline void Church(Graphics& g, const RectF& r, Color c, float w) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(c, w);
    Style(pen);
    const float cx = r.X + r.Width * 0.5f;
    g.DrawLine(&pen, cx, r.Y + r.Height * 0.06f, cx, r.Y + r.Height * 0.22f);
    g.DrawLine(&pen, cx - r.Width * 0.08f, r.Y + r.Height * 0.12f,
               cx + r.Width * 0.08f, r.Y + r.Height * 0.12f);
    GraphicsPath dome;
    dome.AddBezier(cx - r.Width * 0.26f, r.Y + r.Height * 0.56f,
                   cx - r.Width * 0.26f, r.Y + r.Height * 0.26f,
                   cx + r.Width * 0.26f, r.Y + r.Height * 0.26f,
                   cx + r.Width * 0.26f, r.Y + r.Height * 0.56f);
    g.DrawPath(&pen, &dome);
    g.DrawLine(&pen, cx - r.Width * 0.30f, r.Y + r.Height * 0.56f,
               cx + r.Width * 0.30f, r.Y + r.Height * 0.56f);
    g.DrawLine(&pen, cx - r.Width * 0.24f, r.Y + r.Height * 0.56f,
               cx - r.Width * 0.24f, r.Y + r.Height * 0.92f);
    g.DrawLine(&pen, cx + r.Width * 0.24f, r.Y + r.Height * 0.56f,
               cx + r.Width * 0.24f, r.Y + r.Height * 0.92f);
    g.DrawLine(&pen, cx - r.Width * 0.24f, r.Y + r.Height * 0.92f,
               cx + r.Width * 0.24f, r.Y + r.Height * 0.92f);
    g.DrawLine(&pen, cx, r.Y + r.Height * 0.66f, cx, r.Y + r.Height * 0.92f);
}

}  // namespace icons
