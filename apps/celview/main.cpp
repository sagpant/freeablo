#include "../components/settings/settings.h"
#include <chrono>
#include <faio/fafileobject.h>
#include <fmt/format.h>
#include <input/inputmanager.h>
#include <misc/stringops.h>
#include <nfd.h>
#include <nuklearmisc/inputfwd.h>
#include <nuklearmisc/standaloneguispritehandler.h>
#include <nuklearmisc/widgets.h>
#include <render/render.h>

int main(int argc, char** argv)
{
    Misc::saveArgv0(argv[0]);

    if (argc > 2)
        message_and_abort_fmt("Usage: %s [filename]", argv[0]);

    Render::RenderSettings renderSettings = {};
    renderSettings.windowWidth = 800;
    renderSettings.windowHeight = 600;
    renderSettings.fullscreen = false;

    NuklearMisc::StandaloneGuiHandler guiHandler("Cel Viewer", renderSettings);

    nk_context* ctx = guiHandler.getNuklearContext();

    Settings::Settings settings;
    settings.loadFromFile(Misc::getResourcesPath().str() + "/celview.ini");

    bool faioInitDone = false;
    auto listFile = settings.get<std::string>("celview", "listFile", "Diablo I.txt");
    auto mpqFile = settings.get<std::string>("celview", "mpqFile", "DIABDAT.MPQ");

    std::vector<std::string> celFiles;

    std::string selectedImage;
    std::unique_ptr<NuklearMisc::GuiSprite> image;

    std::unique_ptr<NuklearMisc::GuiSprite> nextImage;

    int animate = false;
    int32_t frame = 0;

    float rowHeight = 30;
    auto lastFrame = std::chrono::high_resolution_clock::now();

    bool quit = false;
    while (!quit)
    {
        auto now = std::chrono::high_resolution_clock::now();

        if (nextImage)
            image = std::unique_ptr<NuklearMisc::GuiSprite>(nextImage.release());

        renderSettings = Render::getWindowSize();

        if (nk_begin(ctx, "main_window", nk_rect(0, 0, renderSettings.windowWidth, renderSettings.windowHeight), NK_WINDOW_NO_SCROLLBAR))
        {
            struct nk_rect bounds = nk_window_get_content_region(ctx);

            nk_layout_row_dynamic(ctx, bounds.h, 2);

            if (nk_group_begin(ctx, "image", 0))
            {
                nk_layout_row_dynamic(ctx, rowHeight, 1);

                std::string label = selectedImage;

                if (selectedImage.empty())
                    label = "No image selected";

                nk_label(ctx, label.c_str(), NK_TEXT_CENTERED);

                nk_checkbox_label(ctx, "Animate", &animate);

                if (image)
                {
                    nk_label(ctx, fmt::format("Number of Frames: {}", image->getSprite()->size()).c_str(), NK_TEXT_LEFT);
                    nk_label(ctx, fmt::format("Width: {}", image->getSprite()->getWidth()).c_str(), NK_TEXT_LEFT);
                    nk_label(ctx, fmt::format("Height: {}", image->getSprite()->getHeight()).c_str(), NK_TEXT_LEFT);
                    frame = nk_propertyi(ctx, "Frame", 0, frame, image->getSprite()->size(), 1, 0.2f);

                    if (nk_button_label(ctx, "save as png"))
                    {
                        nfdchar_t* outPath = nullptr;
                        nfdresult_t result = NFD_SaveDialog("png", nullptr, &outPath);
                        if (result == NFD_OKAY)
                        {
                            Render::SpriteGroup::toPng(selectedImage, outPath);
                            free(outPath);
                        }
                    }

                    if (nk_button_label(ctx, "save as gif"))
                    {
                        nfdchar_t* outPath = nullptr;
                        nfdresult_t result = NFD_SaveDialog("gif", nullptr, &outPath);
                        if (result == NFD_OKAY)
                        {
                            Render::SpriteGroup::toGif(selectedImage, outPath);
                            free(outPath);
                        }
                    }

                    auto msSinceLastFrame = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrame).count();
                    if (animate && msSinceLastFrame > 100)
                    {
                        lastFrame = now;
                        frame++;
                    }

                    if (frame >= (int32_t)image->getSprite()->size())
                        frame = 0;

                    Render::Sprite sprite = image->getSprite()->operator[](frame);

                    int32_t w, h;
                    Render::spriteSize(sprite, w, h);

                    nk_layout_space_begin(ctx, NK_STATIC, h, 1);
                    {
                        nk_layout_space_push(ctx, nk_rect(0, 0, w, h));

                        auto canvas = nk_window_get_canvas(ctx);

                        struct nk_rect imageRect = {};
                        nk_widget(&imageRect, ctx);
                        nk_fill_rect(canvas, imageRect, 0.0, nk_rgb(0, 255, 0));

                        auto img = image->getNkImage(frame);
                        nk_draw_image(canvas, imageRect, &img, nk_rgb(255, 255, 255));
                    }
                    nk_layout_space_end(ctx);
                }

                nk_group_end(ctx);
            }

            if (nk_group_begin(ctx, "file list", 0))
            {
                if (!faioInitDone)
                {
                    nk_layout_row_dynamic(ctx, rowHeight * 2, 1);

                    NuklearMisc::nk_file_pick(ctx, "DIABDAT.MPQ", mpqFile, "mpq,MPQ", rowHeight);
                    NuklearMisc::nk_file_pick(ctx, "Diablo listfile", listFile, "txt", rowHeight);

                    if (nk_button_label(ctx, "load"))
                    {
                        FAIO::init(mpqFile, listFile);
                        celFiles = FAIO::listMpqFiles("*.cel");
                        auto tmp = FAIO::listMpqFiles("*.cl2");
                        celFiles.insert(celFiles.end(), tmp.begin(), tmp.end());

                        std::sort(celFiles.begin(), celFiles.end(), [](const std::string& l, const std::string& r) {
                            return Misc::StringUtils::toLower(l) < Misc::StringUtils::toLower(r);
                        });

                        settings.set<std::string>("celview", "listFile", listFile);
                        settings.set<std::string>("celview", "mpqFile", mpqFile);
                        settings.save();

                        faioInitDone = true;

                        if (argc > 1)
                        {
                            selectedImage = argv[1];
                            frame = 0;
                            nextImage = std::make_unique<NuklearMisc::GuiSprite>(new Render::SpriteGroup(selectedImage, false));
                        }
                    }
                }

                nk_layout_row_dynamic(ctx, rowHeight, 1);

                for (auto& celFile : celFiles)
                {
                    auto buttonStyle = ctx->style.button;

                    if (selectedImage == celFile)
                        buttonStyle.normal = buttonStyle.hover;

                    if (nk_button_label_styled(ctx, &buttonStyle, celFile.c_str()))
                    {
                        selectedImage = celFile;
                        frame = 0;
                        nextImage = std::make_unique<NuklearMisc::GuiSprite>(new Render::SpriteGroup(selectedImage, false));
                    }
                }

                nk_group_end(ctx);
            }
        }
        nk_end(ctx);

        quit = guiHandler.update();
    }

    FAIO::quit();

    return 0;
}
