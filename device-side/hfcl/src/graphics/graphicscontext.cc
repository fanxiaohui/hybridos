/*
** HFCL - HybridOS Foundation Class Library
**
** Copyright (C) 2018 Beijing FMSoft Technologies Co., Ltd.
**
** This file is part of HFCL.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "graphics/graphicscontext.h"

#include "graphics/font.h"
#include "graphics/color.h"
#include "graphics/image.h"
#include "graphics/textmode.h"

#include "view/view.h"

namespace hfcl {

GraphicsContext* GraphicsContext::screen_graphics_context = NULL;

VECTOR(HDC, DCVector);
VECTOR(POINT*, PointVector);

//#define _HFCL_DC_TRANSPARENT_ 1

#ifdef _HFCL_DC_TRANSPARENT_

#define _HFCL_CACHED_DC_    1

#ifdef _HFCL_CACHED_DC_
static HDC g_cached_dc = HDC_INVALID;

void delete_cached_dc(void)
{
    if (g_cached_dc != HDC_INVALID)
        DeleteCompatibleDC(g_cached_dc);
}
#endif

#endif

class GraphicsContextPrivate {
    public:
        GraphicsContextPrivate(HDC hdc)
        {
            m_winPos.x = 0;
            m_winPos.y = 0;
            m_dcAbsPos.x = 0;
            m_dcAbsPos.y = 0;
            m_dcRelPos.x = 0;
            m_dcRelPos.y = 0;
            initDC(hdc);
        }

        ~GraphicsContextPrivate()
        {
			// FIXME, 
			// delete memdc ....
        }

        void initDC(HDC hdc)
        {
            m_viewdc = m_context = hdc;

            if (m_context != HDC_INVALID) {
                m_dcSize.cx = GetGDCapability(m_context, GDCAP_MAXX);
                m_dcSize.cy = GetGDCapability(m_context, GDCAP_MAXY);
            } else {
                m_dcSize.cx = 0;
                m_dcSize.cy = 0;
            }
#ifdef _HFCL_DC_TRANSPARENT_
#ifdef _HFCL_CACHED_DC_
            if (g_cached_dc == HDC_INVALID) {
                g_cached_dc = CreateCompatibleDCEx(HDC_SCREEN, RECTW(g_rcScr), RECTH(g_rcScr));
                SetBrushColor(g_cached_dc, RGBA2Pixel(g_cached_dc, 0, 0, 0, 255));
            }
#endif
#endif
        }

        void beginTransparencyLayer(int alpha)
        {
#ifdef _HFCL_DC_TRANSPARENT_
            HDC hMemDC = HDC_INVALID;
            POINT *pPoint = HFCL_NEW_ARR(POINT, (2 * sizeof(POINT)));

            if (pPoint) {
                if (g_cached_dc != HDC_INVALID && m_layers.empty()) {
                    hMemDC = g_cached_dc;
                    SaveDC(g_cached_dc);
                } else {
                    hMemDC = CreateCompatibleDCEx(m_viewdc, m_dcSize.cx, m_dcSize.cy);
                }
            }

            m_layers.push_back(hMemDC);
            // memory not enough
            if (hMemDC == HDC_INVALID) {
                if (pPoint)
                    HFCL_DELETE_ARR(pPoint);
                pPoint = NULL;
                return;
            }

            if (hMemDC != g_cached_dc) {
                m_dcRelPos.x = m_winPos.x -m_dcAbsPos.x;
                m_dcRelPos.y = m_winPos.y -m_dcAbsPos.y;
                copyPoint(m_dcAbsPos, m_winPos);
            } else {
                m_dcRelPos.x = -m_dcAbsPos.x;
                m_dcRelPos.y = -m_dcAbsPos.y;

                m_dcAbsPos.x = 0;
                m_dcAbsPos.y = 0;
            }

            copyPoint(pPoint[0], m_dcAbsPos);
            copyPoint(pPoint[1], m_dcRelPos);
            m_points.push_back(pPoint);

            //set invalid clip region
            PCLIPRGN prgn = CreateClipRgn();
            if (prgn) {
                GetClipRegion(m_context, prgn);
                OffsetRegion(prgn, -m_dcRelPos.x, -m_dcRelPos.y);
                SelectClipRegion(hMemDC, prgn);
                DestroyClipRgn(prgn);
            }

            SetMemDCColorKey(hMemDC, MEMDC_FLAG_SRCCOLORKEY, COLOR_black);
            SetMemDCAlpha(hMemDC, MEMDC_FLAG_SRCALPHA, alpha);

            m_context = hMemDC;
#endif
        }

        void endTransparencyLayer()
        {
#ifdef _HFCL_DC_TRANSPARENT_
            HDC hMemDC = m_layers.back();
            m_layers.pop_back();

            if (hMemDC == HDC_INVALID)
                return;

            POINT* pPoint = m_points.back();
            m_points.pop_back();
            if (pPoint)
                HFCL_DELETE_ARR(pPoint);
            pPoint = NULL;

            if (!m_layers.empty()) {
                m_context = m_layers.back();
            } else {
                m_context = m_viewdc;
            }

            BitBlt(hMemDC, 0, 0, 0, 0, m_context, m_dcRelPos.x, m_dcRelPos.y, 0);

            if (!m_points.empty()) {
                pPoint = m_points.back();
                copyPoint(m_dcAbsPos, pPoint[0]);
                copyPoint(m_dcRelPos, pPoint[1]);
            } else {
                m_dcAbsPos.x = 0;
                m_dcAbsPos.y = 0;

                m_dcRelPos.x = 0;
                m_dcRelPos.y = 0;
            }

            if (hMemDC == g_cached_dc) {
                RestoreDC(g_cached_dc, -1);
                FillBox(g_cached_dc, 0, 0, RECTW(g_rcScr), RECTH(g_rcScr));
            } else {
                DeleteCompatibleDC(hMemDC);
            }
#endif
        }

        void setMapView(View* view)
        {
            if (view) {
                view->getSize(&m_dcSize.cx, &m_dcSize.cy);
                m_winPos.x = 0;
                m_winPos.y = 0;
                view->viewToWindow(&m_winPos.x, &m_winPos.y);
            }
        }

        void map(int x, int y, int &x2, int &y2) {
            x2 = x + m_winPos.x - m_dcAbsPos.x;
            y2 = y + m_winPos.y - m_dcAbsPos.y;
        }

        void mapRect(RECT &rc)
        {
            if (m_winPos.x != m_dcAbsPos.x || m_winPos.y != m_dcAbsPos.y) {
                OffsetRect(&rc, m_winPos.x - m_dcAbsPos.x, m_winPos.y - m_dcAbsPos.y);
				//rc.right = rc.right > m_dcSize.cx ? rc.left + m_dcSize.cx : rc.right;
				//rc.bottom = rc.bottom > m_dcSize.cy ? rc.top + m_dcSize.cy : rc.bottom;
            }
        }

        void translation(int offx, int offy)
        {
            m_winPos.x += offx;
            m_winPos.y += offy;
        }

        void getTranslation(int &offx, int &offy)
        {
            offx = m_winPos.x;
            offy = m_winPos.y;
        }

    private:
        void copyPoint(POINT &dstPt, POINT &srcPt)
        {
            dstPt.x = srcPt.x;
            dstPt.y = srcPt.y;
        }

    public:
        HDC   m_viewdc;
        HDC   m_context;
        POINT m_winPos;
        POINT m_dcAbsPos;
        SIZE  m_dcSize;

    private:
        POINT       m_dcRelPos;
        DCVector    m_layers;
        PointVector m_points;
};

GraphicsContext* GraphicsContext::screenGraphics() {
	if (NULL == screen_graphics_context) {
		screen_graphics_context = HFCL_NEW_EX(GraphicsContext, (HDC_SCREEN));
	}
	return screen_graphics_context;
}

void GraphicsContext::deleteScreenGraphics(void) {
	if (NULL != screen_graphics_context)
		HFCL_DELETE(screen_graphics_context);
	screen_graphics_context = NULL;
}

void GraphicsContext::setMapView(View* view)
{
    if (!view)
        return;

    m_data->setMapView(view);
}

void GraphicsContext::translation(int offx, int offy)
{
    m_data->translation(offx, offy);
}

void GraphicsContext::getTranslation(int &offx, int &offy)
{
    m_data->getTranslation(offx, offy);
}

Logfont* GraphicsContext::getLogFont(void)
{
	return GetCurFont(m_data->m_context);
}

void GraphicsContext::map(int x, int y, int &x2, int &y2)
{
    m_data->map(x, y, x2, y2);
}

void GraphicsContext::mapRect(RECT &rc)
{
    m_data->mapRect(rc);
}

HDC GraphicsContext::context()
{
    return m_data->m_context;
}

GraphicsContext::GraphicsContext(HDC hdc)
    : m_data(NULL)
{
    m_data = HFCL_NEW_EX(GraphicsContextPrivate, (hdc));
}

GraphicsContext::~GraphicsContext()
{
    if (m_data)
        HFCL_DELETE(m_data);
    m_data = NULL;
}

bool GraphicsContext::getBiDiFlag (void)
{
    return (bool)GetBIDIFlags (m_data->m_context);
}

void GraphicsContext::setBiDiFlag (bool bidi)
{
    SetBIDIFlags (m_data->m_context, bidi);
}

int GraphicsContext::getBiDiFirstChType (void)
{
    _DBG_PRINTF ("GraphicsContext::getBiDiFirstChType: NOT IMPLEMENTED\n");
    return BIDI_TYPE_LTR;
}

void GraphicsContext::setBiDiFirstChType (int type)
{
    _DBG_PRINTF ("GraphicsContext::setBiDiFirstChType: NOT IMPLEMENTED\n");
}

void GraphicsContext::textOutOmitted(int x, int y, const char * text, int len, int max_extent)
{
    if (!text)
        return;

    int outx, outy;
    HDC hdc = m_data->m_context;
    unsigned int oldBkMode = SetBkMode(hdc, BM_TRANSPARENT);

    map(x, y, outx, outy);
    TextOutOmitted(hdc, outx, outy, text, len, max_extent);
    
    SetBkMode(hdc, oldBkMode);
}

void GraphicsContext::textOutOmitted(int x, int y, const char * text, 
        int len, int max_extent, unsigned int color, Logfont * logfont)
{
    if (!text)
        return;

    int outx, outy;
    HDC hdc = m_data->m_context;
    map(x, y, outx, outy);

    SelectFont (m_data->m_context, logfont);
    SetBkMode (hdc, BM_TRANSPARENT);
    SetTextColor (hdc, DWORD2Pixel (hdc, color));
    TextOutOmitted (hdc, outx, outy, text, len, max_extent);
}

void GraphicsContext::textOut(int x, int y, const char * text, int len,
        unsigned int color, Logfont * logfont)
{
	if (!text)
        return;

    int outx, outy;
    HDC hdc = m_data->m_context;
    map(x, y, outx, outy);

    SelectFont (m_data->m_context, logfont);
    SetBkMode (hdc, BM_TRANSPARENT);
    SetTextColor (hdc, DWORD2Pixel (hdc, color));
    TextOutLen (hdc, outx, outy, text, len);
}

void GraphicsContext::getTextDrawSize (const string text, Logfont *f, int *w, int *h)
{
    getTextDrawSize (text.c_str(), f, w, h);
}

void GraphicsContext::getTextDrawSize (const char *text, Logfont *f, int *w, int *h)
{
    SIZE cz = {0, 0};
    int text_len = strlen (text);
    Glyph32* glyphs = NULL;
    GLYPHMAPINFO* glyphs_map = NULL;

    int nr_glyphs = BIDIGetTextLogicalGlyphs (f, text, text_len, &glyphs, &glyphs_map);

    if (nr_glyphs <= 0)
        goto END;

    if (getBiDiFlag ()) {
        BIDILogGlyphs2VisGlyphs (f, glyphs, nr_glyphs, glyphs_map);
    }

    SelectFont (m_data->m_context, f);
    GetGlyphsExtent (m_data->m_context, glyphs, nr_glyphs, &cz);

END:
    if (glyphs)
        free (glyphs);
    if (glyphs_map)
        free (glyphs_map);

    *w = cz.cx;
    *h = cz.cy;
}

int GraphicsContext::drawTextToCalcRect(const string& text, IntRect& rect, unsigned int format, Logfont *font)
{
    RECT rc = { rect.left(), rect.top(), rect.right(), rect.bottom() };

    PLOGFONT old_font = SelectFont (m_data->m_context, font);
    DrawText (m_data->m_context, text.c_str(), -1, &rc, format | DT_CALCRECT);
    SelectFont (m_data->m_context, old_font);
    
    rect.setRect( rc.left, rc.top, rc.right, rc.bottom);
    
    return RECTH(rc);
}
        
int GraphicsContext::calcTextRectOnDrawing(const char *text, 
        Logfont *f, unsigned int format, IntRect *rect, DTFIRSTLINE *firstline, Point *txtPos)
{
    if (rect == NULL)
        return -1;

    RECT rc = {rect->left(), rect->top(), rect->right(), rect->bottom()};

    DrawTextEx2(m_data->m_context, text, -1, &rc, 0, format | DT_CALCRECT, firstline);
 
    rect->setRect(rc.left, rc.top, rc.right, rc.bottom);
    
    return RECTH(rc);
}

int GraphicsContext::drawTextToGetLenght(const string& text)
{
    RECT rc = {0, 0, 100, 30};

    DrawText(m_data->m_context, text.c_str(), -1, &rc, DT_SINGLELINE | DT_CALCRECT);

    return rc.right - rc.left;
}

int GraphicsContext::getFontHeight(Logfont *f) 
{
    if (f) {
        return f->size;
    }

    Logfont font;
    GetLogFontInfo(m_data->m_context, &font);
    return font.size;
}

int GraphicsContext::getFontWidth(Logfont *f)
{
    FONTMETRICS m;
    if (f) {
        GetFontMetrics (f, &m);
    }
    else {
        Logfont font;
        GetLogFontInfo(m_data->m_context, &font);
        GetFontMetrics (&font, &m);
    }
    return m.ave_width;
}

void GraphicsContext::drawText(const string& text, const IntRect& rect)
{
    RECT rc = RECT(rect);
    HDC hdc = m_data->m_context;

    mapRect(rc);
    SetBkMode(hdc, BM_TRANSPARENT);
    DrawText(hdc, text.c_str(), -1, &rc, DT_SINGLELINE | DT_CENTER  | DT_VCENTER);
}

void GraphicsContext::drawText(const char * text, const IntRect& rect,
        unsigned int color, Logfont * logfont, unsigned int format)
{
    RECT rc = RECT(rect);
    Color clr(color);
    Logfont* oldFont = NULL;
    HDC hdc = m_data->m_context;

    mapRect(rc);

    SetBkMode(hdc, BM_TRANSPARENT);
    if (NULL != logfont)
        oldFont = SelectFont(hdc, logfont);
    SetTextColor(hdc, RGB2Pixel(hdc, clr.r(), clr.g(), clr.b()));


    if (format & TextMode::TextOutOmitted) {
        format &= ~(TextMode::WordBreak|TextMode::CharBreak);
        format &= format | TextMode::SingleLine;
        format &= ~(TextMode::TextOutOmitted);
        
        SIZE size;
        GetTextExtent(hdc, text, -1, &size);
        if (size.cx > RECTW(rc)) { //need text ommit
            //calc the size
            int y = rc.top;
            if (format & TextMode::ValignMiddle) {
                y = (rc.top + rc.bottom - size.cy) / 2;
            }
            else if (format & TextMode::ValignBottom) {
                y = rc.bottom - size.cy;
            }
            TextOutOmitted(hdc, rc.left, y, text, -1, RECTW(rc));
            goto END;
        }

    }
    
    if ((format & TextMode::ValignMiddle) ||
            (format & TextMode::ValignBottom)) {
        RECT rcc = rc;
        DrawText (hdc, text, -1, &rcc, format | DT_CALCRECT);
        int h = rcc.bottom - rc.top + 1;
        if (format & TextMode::ValignMiddle) {
            rc.top = (rc.top + rc.bottom - h) / 2;
        }
        else if (format & TextMode::ValignBottom) {
            rc.top = rc.bottom - h;
        }
    }
    
    DrawText(hdc, text, -1, &rc, format);

END:
    if (NULL != logfont)
        SelectFont(hdc, oldFont);
}

void GraphicsContext::clip(const IntRect& rect)
{
    RECT rc = RECT(rect);
    mapRect(rc);
    ClipRectIntersect(m_data->m_context, &rc);
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    RECT rc = RECT(rect);
    mapRect(rc);
    ExcludeClipRect(m_data->m_context, &rc);
}

void GraphicsContext::clipIn(const IntRect& rect)
{
    RECT rc = RECT(rect);
    mapRect(rc);
    IncludeClipRect(m_data->m_context, &rc);
}

bool GraphicsContext::rectVisible(IntRect &rect)
{
    RECT rc = RECT(rect);
    mapRect(rc);

    return RectVisible(m_data->m_context, &rc);
}

void GraphicsContext::save()
{
    SaveDC(m_data->m_context);
}

void GraphicsContext::restore()
{
    RestoreDC(m_data->m_context, -1);
}

GraphicsContext* GraphicsContext::createMemGc(int w, int h)
{
    HDC memdc = CreateCompatibleDCEx(m_data->m_context, w, h);

    if(memdc != HDC_INVALID)
        return HFCL_NEW_EX(GraphicsContext, (memdc));
    else
        return NULL;
}

void GraphicsContext::fillBox(int x, int y, int w, int h)
{
    FillBox(m_data->m_context, x, y, w, h);
}

void GraphicsContext::bitBlt(GraphicsContext* src, int sx, int sy, int sw, int sh, int dx, int dy, Uint32 op)
{
    BitBlt(src->context(), sx, sy, sw, sh, m_data->m_context, dx, dy, op);
}

void GraphicsContext::fillRect(const IntRect& rc, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (rc.width() <= 0 || rc.height() <= 0)
        return;

    RECT rect = RECT(rc);
    HDC hdc = m_data->m_context;

    mapRect(rect);

    if (a == HFCL_DEFAULT_OPACITY) {
        unsigned int oldColor = SetBrushColor(hdc, RGBA2Pixel(hdc, r, g, b, a));
        FillBox(hdc, rect.left, rect.top, RECTW(rect), RECTH(rect));
        SetBrushColor(hdc, oldColor);
    }
    else if (a == 0) {
        return;
    }
    else {
        int width = RECTW(rect);
        int height = RECTH(rect);
#if 0
#  ifdef __MMI_SAMSUNG_GT_FEATURE__
        HDC memdc = CreateCompatibleDCEx(hdc, width, height);
        if (memdc == HDC_INVALID)
            return;

        SetBrushColor(memdc, RGBA2Pixel(hdc, r, g, b, a));
        FillBox(memdc, 0, 0, width, height);
        SetMemDCAlpha(memdc, MEMDC_FLAG_SRCALPHA, a);
        BitBlt(memdc, 0, 0, width, height, hdc, rect.left, rect.top, 0);
        DeleteMemDC(memdc);
#  else
        /* VincentWei: use a 1x1 BITMAP object with alpha channel */
        gal_pixel pixel = RGBA2Pixel(hdc, r, g, b, 0xFF);
        BITMAP bmp_1by1 = {BMP_TYPE_ALPHACHANNEL, 
                    (Uint8)GetGDCapability (hdc, GDCAP_DEPTH), (Uint8)GetGDCapability (hdc, GDCAP_BPP),
                    a, 0, 1, 1, 1, (Uint8*)&pixel, NULL, 0};

        SetRasterOperation (hdc, ROP_SET);
        FillBoxWithBitmap (hdc, rect.left, rect.top, width, height, &bmp_1by1);
#  endif
#else
        HDC memdc = CreateCompatibleDCEx(hdc, width, 1);
        if (memdc == HDC_INVALID)
            return;

        SetBrushColor(memdc, RGBA2Pixel(hdc, r, g, b, 0xFF));
        FillBox(memdc, 0, 0, width, 1);
        SetMemDCAlpha(memdc, MEMDC_FLAG_SRCALPHA, a);

        for (int i = 0; i < height; i++) {
            BitBlt(memdc, 0, 0, width, 1, hdc, rect.left, rect.top++, 0);
        }

        DeleteMemDC(memdc);
#endif
    }
}

void GraphicsContext::rectangle(int x0, int y0, int x1, int y1)
{
    int out_x0, out_y0, out_x1, out_y1;
    map(x0, y0, out_x0, out_y0);
    map(x1, y1, out_x1, out_y1);
    Rectangle(m_data->m_context, out_x0, out_y0, out_x1, out_y1);
}

void GraphicsContext::rectangle(int x0, int y0, int x1, int y1, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    drawLine(x0, y0, x1, y0, 1, r, g, b, a);
    drawLine(x1, y0, x1, y1, 1, r, g, b, a);
    drawLine(x1, y1, x0, y1, 1, r, g, b, a);
    drawLine(x0, y1, x0, y0, 1, r, g, b, a);
}

void GraphicsContext::drawLine(int x0, int y0, int x1, int y1,
                int width, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    int out_x0, out_y0, out_x1, out_y1;
    HDC hdc = m_data->m_context;

    int oldW = SetPenWidth(hdc, width);
    gal_pixel oldC = SetPenColor(hdc, RGBA2Pixel(hdc, r, g, b, a));
    
    map(x0, y0, out_x0, out_y0);
    map(x1, y1, out_x1, out_y1);

    LineEx(hdc, out_x0, out_y0, out_x1, out_y1);

    SetPenColor(hdc, oldC);
    SetPenWidth(hdc, oldW);
}

void GraphicsContext::drawHVDotLine(int x, int y, int wh, bool isHorz,
				Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    int out_x, out_y;
    HDC hdc = m_data->m_context;
    
    gal_pixel oldC = SetPenColor(hdc, RGBA2Pixel(hdc, r, g, b, a));
    
    map(x, y, out_x, out_y);
    DrawHVDotLine(hdc, out_x, out_y, wh, isHorz ? TRUE : FALSE);

    SetPenColor(hdc, oldC);
}

void GraphicsContext::drawPolygonLine(Point *pts, int nums, int width, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	HDC hdc = m_data->m_context;
	Point* p = HFCL_NEW_ARR(Point, (sizeof(Point) * nums));
    if (NULL == p) {
        _ERR_PRINTF("HFCL_NEW_ARR for fillPolygon fail..\n");
        return;
    }

	SetPenWidth(hdc, width);
	SetPenJoinStyle(hdc, PT_JOIN_ROUND);
	SetPenCapStyle(hdc, PT_CAP_ROUND);
    SetPenColor(hdc, RGBA2Pixel(hdc, r, g, b, a));

    for (int c = 0; c < nums; ++c) {
        map(pts[c].x, pts[c].y, p[c].x, p[c].y);
    }

	PolyLineEx(hdc, p, nums);

	if (NULL != p) {
        HFCL_DELETE_ARR(p);
        p = NULL;
    }
}

void GraphicsContext::fillPolygon(const Point* pts, int vertices, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    HDC hdc = m_data->m_context;
    int c = 0;
    Point* p = HFCL_NEW_ARR(Point, (sizeof(Point) * vertices));
    if (NULL == p) {
        _ERR_PRINTF("HFCL_NEW_ARR for fillPolygon fail..\n");
        return;
    }

    for (c = 0; c < vertices; ++c) {
        map(pts[c].x, pts[c].y, p[c].x, p[c].y);
    }

    SetBrushColor(hdc, RGBA2Pixel(hdc, r, g, b, a));
    FillPolygon(hdc, p, vertices);

    if (NULL != p) {
        HFCL_DELETE_ARR(p);
        p = NULL;
    }
}

bool GraphicsContext::fillBoxWithBitmap(int x, int y, int w, int h, const Bitmap* pBitmap)
{
    int outx, outy;
    HDC hdc = m_data->m_context;

    map(x, y, outx, outy);

    return FillBoxWithBitmap(hdc, outx, outy, w, h, pBitmap);
}

extern "C" Uint32 __mg_newgal_replace_color;
bool GraphicsContext::setReplaceColor(const DWORD color)
{
    __mg_newgal_replace_color = (Uint32)color;
    return true;
}

bool GraphicsContext::drawRotateBitmap(const Bitmap* pBitmap,int lx, int ty, int angle)
{
    HDC hdc = m_data->m_context;

    RotateBitmap(hdc,pBitmap, lx, ty, angle);
    return true;
}

bool GraphicsContext::fillBoxWithBitmapPart(int x, int y, int w, int h, const Bitmap* pBitmap, int xo, int yo)
{
    int outx, outy;
    HDC hdc = m_data->m_context;

    map(x, y, outx, outy);

    return FillBoxWithBitmapPart(hdc, outx, outy, w, h, 0, 0, pBitmap, xo, yo);
}

int GraphicsContext::loadBitmap(Bitmap* bmp, const char * file_name)
{
    int ret = LoadBitmapFromFile(m_data->m_viewdc, bmp, file_name);
    if (ret != 0) {
        _ERR_PRINTF ("Load Bitmap [%s] error.\n", file_name);
    }

    return ret;
}
int GraphicsContext::loadBitmap(Bitmap **bmp, const void* data, int size, const char * extern_name)
{
#ifdef _HFCL_INCORE_BMPDATA
    return LoadBitmapFromMem(m_data->m_viewdc, *bmp, data, size, extern_name);
#else
// incore res is BITMAP *
    BitmapFrameArray *b = (BitmapFrameArray *)data;
    // resouce error : 0 frames
    if (0 == b->nr_frames || NULL == b->frames)
    {
        return ERR_BMP_LOAD;
    }

    PBITMAP pBmp = (BITMAP*)((b->frames->bmp));
    if (bmp)
        *bmp = pBmp;

    if (!pBmp->bmBits) {
        return ERR_BMP_LOAD;
    }

    return ERR_BMP_OK;
#endif
}

void GraphicsContext::unloadBitmap(Bitmap* pBitmap)
{
    UnloadBitmap(pBitmap);
}

GraphicsContext* GraphicsContext::createGraphicsFromBitmap(Bitmap *pbmp)
{
    m_data->initDC(CreateMemDCFromBitmap (m_data->m_context, pbmp));
    return this;
}

bool GraphicsContext::initBitmap(int w, int h, Bitmap* pbmp)
{
    return InitBitmap(context(), w, h, 0, NULL, pbmp);
}

bool GraphicsContext::captureScreen2Bitmap(Bitmap* pbmp)
{
    return GetBitmapFromDC (m_data->m_context, 0, 0,
            GetGDCapability (m_data->m_context, GDCAP_HPIXEL),
            GetGDCapability (m_data->m_context, GDCAP_VPIXEL),
            pbmp);
}

bool GraphicsContext::getBitmapSize(Bitmap* pBitmap, int* w, int* h)
{
    if (!pBitmap)
        return false;

    if (w)
        *w = pBitmap->bmWidth;
    if (h)
        *h = pBitmap->bmHeight;

    return true;
}

Logfont* GraphicsContext::getCurFont(void) 
{ 
	return GetCurFont(m_data->m_context); 
}

Logfont* GraphicsContext::createLogFontByName(const char * fontname)
{
    return CreateLogFontByName(fontname);
}

void GraphicsContext::deleteLogFont(Logfont* logfont)
{
    DestroyLogFont(logfont);
}

void GraphicsContext::rotateBitmap(const Bitmap *pBitmap, int lx, int ty, int angle)
{
    int outx, outy;
    map(lx, ty, outx, outy);
    RotateBitmap(context(), pBitmap, outx, outy, angle);
}

void GraphicsContext::beginTransparencyLayer(int alpha)
{
    m_data->beginTransparencyLayer(alpha);
}

void GraphicsContext::endTransparencyLayer(void)
{
    m_data->endTransparencyLayer();
}

int GraphicsContext::getLayers(void)
{
/* VincentWei: disable multiple-layer support */
#if __MG_MULTILAYER_SUPPORT
	return GetDCLayers(context());
#else
    return 1;
#endif
}

void GraphicsContext::setLayer(int layer)
{
/* VincentWei: disable multiple-layer support */
#if __MG_MULTILAYER_SUPPORT
	SetDCLayer(context(), layer);
#else
    return;
#endif
}

void GraphicsContext::setLayerEnable(int layer, BOOL enable)
{
/* VincentWei: disable multiple-layer support */
#if __MG_MULTILAYER_SUPPORT
	SetDCLayerEnable (context(), layer, enable);
#else
    return;
#endif
}

void GraphicsContext::setLayerOpacity(int layer, BOOL enable,unsigned char opacity_value)
{
/* VincentWei: disable multiple-layer support */
#if __MG_MULTILAYER_SUPPORT
	SetDCLayerOpacity (context(), layer, enable, opacity_value);
#else
    return;
#endif
}

void GraphicsContext::setLayerColorKey(int layer, BOOL enable, 
        unsigned char r, unsigned char g, unsigned char b )
{
/* VincentWei: disable multiple-layer support */
#if __MG_MULTILAYER_SUPPORT
	SetDCLayerColorKey (context(),  layer,  enable, r, g, b);
#else
#endif
}

/////////////////////////////////////////////////////////

GraphicsContext* CreateMemGC(int w, int h)
{
	GraphicsContext* gc = NULL;
	gc = GraphicsContext::screenGraphics();
	if(NULL != gc)
		return gc->createMemGc(w, h);

	return NULL;
}

void DeleteMemGC(GraphicsContext *memGc)
{
    if (NULL != memGc) {
        DeleteMemDC(memGc->context());
        HFCL_DELETE(memGc);
    }
}

bool SaveScreenToFile(const char * bmpFile)
{
#if defined (_MGMISC_SAVESCREEN)
    RECT rc = {0, 0, 
            (int)GetGDCapability (HDC_SCREEN, GDCAP_HPIXEL),
            (int)GetGDCapability (HDC_SCREEN, GDCAP_VPIXEL)};
    return SaveScreenRectContent(&rc, bmpFile);
#endif
    return false;
}

} // namespace hfcl

