#include <stdint.h>

struct rect_t
{
    int x, y, w, h;

    void set(int _x, int _y, int _w, int _h);
    void adjust(int _x, int _y, int _w, int _h);
    void assign(const rect_t& dst);
};


int Tile_Baseline(int w, int h, int sw, int sh, rect_t* rgn, rect_t* fit, int m);
int Tile_Split(int w, int h, int sw, int sh, rect_t* rgn, rect_t* fit, int m);
int Tile_Combine(int w, int h, int sw, int sh, rect_t* rgn, rect_t* fit, int m);

int Tile_Blit(uint16_t* dp, int dw, int dh, int dx, int dy, uint16_t* sp, int sw, int sh, const rect_t& rgn);