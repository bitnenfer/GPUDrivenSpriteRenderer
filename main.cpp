
#include "gfx.h"
#include "utils.h"
#include "images.h"
#include "sprite_renderer.h"
#include <algorithm>

#include <Superluminal/PerformanceAPI.h>

struct Point {
    float x;
    float y;
    float width;
    float height;
    float rotation;
    float accelerationX;
    float accelerationY;
    float velocityX;
    float velocityY;
    float speed;
    uint32_t color;
    gfx::Image2D* image;

    void update(float dt, float tx, float ty) {
        float angle = atan2f(ty - y, tx - x);
        accelerationX = cosf(angle) * speed;
        accelerationY = sinf(angle) * speed;
        velocityX = std::clamp(velocityX + accelerationX, -10000.0f, 10000.0f);
        velocityY = std::clamp(velocityY + accelerationY, -10000.0f, 10000.0f);
        x += velocityX * dt; 
        y += velocityY * dt;
    }

};

//#define SPRITE_COUNT (MAX_DRAW_COMMANDS - 1)
#define SPRITE_COUNT 500000

int main() {

    ShowCursor(0);

	gfx::init(1920, 1080);
    SpriteRenderer* spriteRenderer = new SpriteRenderer();

    gfx::Image2D* images[4] = {};
    images[0] = gfx::createImage2D(L"image1", image_img1_width, image_img1_height, image_img1);
    images[1] = gfx::createImage2D(L"image2", image_img2_width, image_img2_height, image_img2);
    images[2] = gfx::createImage2D(L"image3", image_img3_width, image_img3_height, image_img3);
    images[3] = gfx::createImage2D(L"image4", image_img4_width, image_img4_height, image_img4);

    Point* points = new Point[SPRITE_COUNT];
    uint32_t sx = 0;
    uint32_t sy = 0;

    for (uint32_t index = 0; index < SPRITE_COUNT; ++index) {
        points[index].image = images[gfx::randomUint() % 4];
        points[index].x = 130.0f * sx + 20.0f;
        points[index].y = 130.0f * sy + 20.0f;
        points[index].width = points[index].image->width;
        points[index].height = points[index].image->height;
        points[index].rotation = gfx::randomFloat();
        points[index].color = NK_COLOR_UINT(0x000000ff| gfx::randomUint());
        points[index].velocityX = 0.0f;
        points[index].velocityY = 0.0f;
        points[index].speed = 50.8f;
        if (++sx > 500) {
            sx = 0;
            sy++;
        }
    }

    float viewPos[2] = { 500.0f * 130.0f / 2.0f, 500.0f * 130.0f / 2.0f };
    float viewVel[2] = { 0, 0 };
    float viewAcl[2] = { 0, 0 };
    float rotation = 0.0f;
    double deltaAccumulation = 0.0;
    double accumulationCounter = 0.0;
    float currentScale = 1.0f;
	while (!gfx::shouldQuit()) {
        double startTime = gfx::getSeconds();

        PerformanceAPI_BeginEvent("Frame", nullptr, PERFORMANCEAPI_MAKE_COLOR(0xFF, 0xFF, 0x00));

		gfx::pollEvents();

        if (gfx::mouseX() > gfx::getViewWidth() - 100 * currentScale) {
            viewAcl[0] = 1000.0f / currentScale;
        } else if (gfx::mouseX() < 100 * currentScale) {
            viewAcl[0] = -1000.0f / currentScale;
        } else {
            viewAcl[0] = 0;
            viewVel[0] *= 0.9f;
        }

        if (gfx::mouseY() > gfx::getViewHeight() - 100 * currentScale) {
            viewAcl[1] = 1000.0f / currentScale;
        } else if (gfx::mouseY() < 100 * currentScale) {
            viewAcl[1] = -1000.0f / currentScale;
        } else {
            viewAcl[1] = 0;
            viewVel[1] *= 0.9f;
        }

        if (gfx::keyDown(gfx::Q) && currentScale > 0.1f) {
            currentScale -= 0.01f;
        } else if (gfx::keyDown(gfx::E) && currentScale < 2.0f) {
            currentScale += 0.01f;
        }

        viewVel[0] = std::clamp(viewVel[0] + viewAcl[0], -10000.0f, 10000.0f);
        viewVel[1] = std::clamp(viewVel[1] + viewAcl[1], -10000.0f, 10000.0f);
        viewPos[0] += viewVel[0] * (1.0f / 60.0f);
        viewPos[1] += viewVel[1] * (1.0f / 60.0f);

        /* Render Sprites */
        {
            PerformanceAPI_BeginEvent("SpriteRendering", nullptr, PERFORMANCEAPI_MAKE_COLOR(0xFF, 0x00, 0xFF));
            spriteRenderer->reset();
            spriteRenderer->pushMatrix();
            spriteRenderer->translate(-viewPos[0], -viewPos[1]);
            spriteRenderer->scale(currentScale, currentScale);

            for (uint32_t index = 0; index < SPRITE_COUNT; ++index) {
                spriteRenderer->pushMatrix();
                spriteRenderer->translate(points[index].x, points[index].y);
                spriteRenderer->rotate(points[index].rotation);
                spriteRenderer->scale(0.25f, 0.25f);
                spriteRenderer->drawImage(-points[index].width * 0.5f, -points[index].height * 0.5f, points[index].width, points[index].height, NK_COLOR_UINT(0xffffffff), points[index].image);
                spriteRenderer->popMatrix();
                points[index].rotation -= 1.0f / 60.0f;
            }

            spriteRenderer->pushMatrix();
            spriteRenderer->translate((viewPos[0] + gfx::mouseX() / currentScale), (viewPos[1] + gfx::mouseY() / currentScale));
            spriteRenderer->rotate(-rotation);
            spriteRenderer->drawImage(-images[0]->width * 0.5f, -images[0]->height * 0.5f, images[0]->width, images[0]->height, NK_COLOR_UINT(0xff0000ff), images[0]);
            spriteRenderer->popMatrix();
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
		gfx::present(1);

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