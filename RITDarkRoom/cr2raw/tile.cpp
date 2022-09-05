#include "stdafx.h"
#include "tile.h"
#include <memory.h>

void rect_t::set(int _x, int _y, int _w, int _h)
{
    x = _x;
    y = _y;
    w = _w;
    h = _h;
}

void rect_t::adjust(int _x, int _y, int _w, int _h)
{
    x += _x;
    y += _y;
    w += _w;
    h += _h;
}

void rect_t::assign(const rect_t& dst)
{
    x = dst.x;
    y = dst.y;
    w = dst.w;
    h = dst.h;
}


int Tile_Baseline(int w, int h, int sw, int sh, rect_t* rgn, rect_t* fit, int m)
{
    const int ww = (w - sw) / 2;
    const int hh = (h - sh) / 2;
    const int mm = m * 2;

    if ((ww + mm) * 4 > sw) return 0;
    if ((hh + mm) * 2 + h / 2 > sh) return 0;

    rgn[0].set(ww, hh, sw, sh);
    fit[0].set(0, 0, sw, sh);

    rgn[1].set(ww, 0, sw, hh);
    fit[1].set(0, 0, sw, hh);

    rgn[2].set(ww, hh + sh, sw, hh);
    fit[2].set(0, hh + mm, sw, hh);

    rgn[3].set(0, 0, ww, h / 2);
    fit[3].set(0, (hh + mm) * 2, ww, h / 2);

    rgn[4].set(0, h / 2, ww, h / 2);
    fit[4].set(ww + mm, (hh + mm) * 2, ww, h / 2);

    rgn[5].set(ww + sw, 0, ww, h / 2);
    fit[5].set((ww + mm) * 2, (hh + mm) * 2, ww, h / 2);

    rgn[6].set(ww + sw, h / 2, ww, h / 2);
    fit[6].set((ww + mm) * 3, (hh + mm) * 2, ww, h / 2);

    return 7;
}

int Tile_Split(int w, int h, int sw, int sh, rect_t* rgn, rect_t* fit, int m)
{
    const int mm = m * 2;
    const int c = Tile_Baseline(w, h, sw - mm, sh - mm, rgn, fit, m);

    if (c < 1)
        return 0;

    rgn[0].adjust(-m, -m, mm, mm);
    fit[0].adjust(0, 0, mm, mm);

    rgn[1].adjust(-m, 0, mm, m);
    fit[1].adjust(0, 0, mm, m);

    rgn[2].adjust(-m, -m, mm, m);
    fit[2].adjust(0, 0, mm, m);

    rgn[3].adjust(0, 0, m, m);
    fit[3].adjust(0, 0, m, m);

    rgn[4].adjust(0, -m, m, m);
    fit[4].adjust(0, 0, m, m);

    rgn[5].adjust(-m, 0, m, m);
    fit[5].adjust(0, 0, m, m);

    rgn[6].adjust(-m, -m, m, m);
    fit[6].adjust(0, 0, m, m);

    return c;
}

int Tile_Combine(int w, int h, int sw, int sh, rect_t* rgn, rect_t* fit, int m)
{
    const int mm = m * 2;
    const int c = Tile_Baseline(w, h, sw - mm, sh - mm, rgn, fit, m);

    if (c < 1)
        return 0;
        
    fit[0].adjust(m, m, 0, 0);
    fit[1].adjust(m, 0, 0, 0);
    fit[2].adjust(m, m, 0, 0);
    fit[3].adjust(0, 0, 0, 0);
    fit[4].adjust(0, m, 0, 0);
    fit[5].adjust(m, 0, 0, 0);
    fit[6].adjust(m, m, 0, 0);

    return c;
}

int Tile_Blit(uint16_t* dp, int dw, int dh, int dx, int dy, uint16_t* sp, int sw, int sh, const rect_t& rgn)
{
    const int rlen = rgn.w * sizeof (uint16_t);
    int i = 0;

    for (; i < rgn.h; i ++)
    {
        const int drow = dy + i;
        const int srow = rgn.y + i;

        if (drow >= dh || srow >= sh)
            break;

        memcpy(dp + dw * drow + dx, sp + sw * srow + rgn.x, rlen);
    }

    return i;
}
