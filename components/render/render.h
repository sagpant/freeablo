#pragma once
#include "cel/pal.h"
#include "misc.h"
#include <cel/celframe.h>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct SDL_Surface;
class SpriteGroup;

namespace Render
{
    typedef void* Sprite;
    typedef void* FACursor;
    typedef SDL_Surface* FASurface;
}

#include "levelobjects.h"
#include "nuklear_sdl_gl3.h"

namespace Level
{
    class Level;
}

namespace Render
{
    extern int32_t WIDTH;
    extern int32_t HEIGHT;

    enum class TileHalf
    {
        left,
        right,
    };

    // Tile mesasured in indexes on tile grid
    struct Tile
    {
        Misc::Point pos;
        TileHalf half;
        Tile(int32_t xArg, int32_t yArg, TileHalf halfArg = TileHalf::left) : pos(xArg, yArg), half(halfArg) {}
        Tile(Misc::Point pos, TileHalf halfArg = TileHalf::left) : pos(pos), half(halfArg) {}
    };
    /**
     * @brief Render settings for initialization.
     */
    struct RenderSettings
    {
        int32_t windowWidth;
        int32_t windowHeight;
        bool fullscreen;
    };

    struct NuklearGraphicsContext
    {
        nk_gl_device dev = {};
        nk_font_atlas atlas = {};
    };

    void init(const std::string& title, const RenderSettings& settings, NuklearGraphicsContext& nuklearGraphics, nk_context* nk_ctx);

    void setWindowSize(const RenderSettings& settings);

    const std::string& getWindowTitle();
    void setWindowTitle(const std::string& title);

    void destroyNuklearGraphicsContext(NuklearGraphicsContext& nuklearGraphics);
    void quit();

    void resize(size_t w, size_t h);
    RenderSettings getWindowSize();
    void drawGui(NuklearFrameDump& dump);

    FACursor createCursor(const Cel::CelFrame& celFrame, int32_t hot_x = 0, int32_t hot_y = 0);
    void freeCursor(FACursor cursor);
    void drawCursor(FACursor cursor);
    Image loadNonCelImageTrans(const std::string& path, bool hasTrans, size_t transR, size_t transG, size_t transB);

    void draw();

    void handleEvents();

    void drawSprite(const Sprite& sprite, int32_t x, int32_t y, std::optional<Cel::Colour> highlightColor = std::nullopt);

    void spriteSize(const Sprite& sprite, int32_t& w, int32_t& h);

    SpriteGroup* loadTilesetSprite(const std::string& celPath, const std::string& minPath, bool top, bool trim);
    void drawLevel(const Level::Level& level,
                   SpriteGroup* minTops,
                   SpriteGroup* minBottoms,
                   SpriteGroup* specialSprites,
                   const std::map<int32_t, int32_t>& specialSpritesMap,
                   LevelObjects& objs,
                   LevelObjects& items,
                   const Vec2Fix& fractionalPos);

    Tile getTileByScreenPos(size_t x, size_t y, const Vec2Fix& fractionalPos);

    void clear(int r = 0, int g = 0, int b = 255);

    extern RenderInstance* mainRenderInstance;
    extern std::unique_ptr<AtlasTexture> atlasTexture;
}
