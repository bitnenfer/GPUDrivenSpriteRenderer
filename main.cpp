
#include "gfx.h"
#include "utils.h"
#include "images.h"
#include "sprite_renderer.h"

#include <Superluminal/PerformanceAPI.h>

struct Point {
    float x;
    float y;
    float width;
    float height;
    float rotation;
    uint32_t color;
    gfx::Image2D* image;
};

#define SPRITE_COUNT (MAX_DRAW_COMMANDS - 1)
//#define SPRITE_COUNT 0

int main() {
	gfx::init(1920, 1080);
    SpriteRenderer* spriteRenderer = new SpriteRenderer();

    gfx::Image2D* images[4] = {};
    images[0] = gfx::createImage2D(L"image1", image_img1_width, image_img1_height, image_img1);
    images[1] = gfx::createImage2D(L"image2", image_img2_width, image_img2_height, image_img2);
    images[2] = gfx::createImage2D(L"image3", image_img3_width, image_img3_height, image_img3);
    images[3] = gfx::createImage2D(L"image4", image_img4_width, image_img4_height, image_img4);

    Point* points = new Point[SPRITE_COUNT];
    for (uint32_t index = 0; index < SPRITE_COUNT; ++index) {
        points[index].x = gfx::randomFloat() * gfx::getViewWidth();
        points[index].y = gfx::randomFloat() * gfx::getViewHeight();
        points[index].image = images[gfx::randomUint() % 4];
        points[index].width = points[index].image->width;
        points[index].height = points[index].image->height;
        points[index].rotation = gfx::randomFloat();
        points[index].color = NK_COLOR_UINT(0xffffffff);// | gfx::randomUint());
    }

    float rotation = 0.0f;
    double deltaAccumulation = 0.0;
    double accumulationCounter = 0.0;
	while (!gfx::shouldQuit()) {
        double startTime = gfx::getSeconds();

        PerformanceAPI_BeginEvent("Frame", nullptr, PERFORMANCEAPI_MAKE_COLOR(0xFF, 0xFF, 0x00));

		gfx::pollEvents();

        /* Render Sprites */
        {
            PerformanceAPI_BeginEvent("SpriteRendering", nullptr, PERFORMANCEAPI_MAKE_COLOR(0xFF, 0x00, 0xFF));
            spriteRenderer->reset();

            for (uint32_t index = 0; index < SPRITE_COUNT; ++index) {
                spriteRenderer->pushMatrix();
                spriteRenderer->translate(points[index].x, points[index].y);
                spriteRenderer->rotate(points[index].rotation);
                spriteRenderer->scale(0.25f, 0.25f);
                spriteRenderer->drawImage(-points[index].width * 0.5f, -points[index].height * 0.5f, points[index].width, points[index].height, points[index].color, points[index].image);
                spriteRenderer->popMatrix();
                points[index].rotation -= 1.0f / 60.0f;
            }

            spriteRenderer->pushMatrix();
            spriteRenderer->translate(gfx::mouseX(), gfx::mouseY());
            spriteRenderer->rotate(-rotation);
            spriteRenderer->drawImage(-images[0]->width * 0.5f, -images[0]->height * 0.5f, images[0]->width, images[0]->height, NK_COLOR_UINT(0xffffffff), images[0]);
            spriteRenderer->popMatrix();
            PerformanceAPI_EndEvent();
        }
        
        /* Submit to the GPU */
        {
            PerformanceAPI_BeginEvent("GPU Submit", nullptr, PERFORMANCEAPI_MAKE_COLOR(0x00, 0xFF, 0xFF));
            gfx::RenderFrame& frame = gfx::beginFrame();
            spriteRenderer->flushCommands(frame);
            gfx::endFrame();
            PerformanceAPI_EndEvent();
        }
		gfx::present(false);

        rotation += 1.0f / 60.0f;

        PerformanceAPI_EndEvent();

        double endTime = gfx::getSeconds();
        deltaAccumulation += (endTime - startTime) * 1000.0;
        accumulationCounter += 1.0f;
        if (accumulationCounter == 20) {
            printf("%.4lf ms\n", deltaAccumulation / accumulationCounter);
            accumulationCounter = 0.0f;
            deltaAccumulation = 0.0f;
        }
    }

    delete[] points;
	gfx::destroy();
    delete spriteRenderer;
    return 0;
}