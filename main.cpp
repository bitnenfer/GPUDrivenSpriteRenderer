
#include "ni.h"
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
    ni::Texture* image;

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

#define SPRITE_COUNT (MAX_DRAW_COMMANDS - 1)

int main() {

    //ShowCursor(0);

	ni::init(1920, 1080);
    SpriteRenderer* spriteRenderer = new SpriteRenderer();

    ni::Texture* images[4] = {};
    images[0] = ni::createTexture(L"image1", image_img1_width, image_img1_height, 1, image_img1);
    images[1] = ni::createTexture(L"image2", image_img2_width, image_img2_height, 1, image_img2);
    images[2] = ni::createTexture(L"image3", image_img3_width, image_img3_height, 1, image_img3);
    images[3] = ni::createTexture(L"image4", image_img4_width, image_img4_height, 1, image_img4);

    Point* points = new Point[SPRITE_COUNT];
    uint32_t sx = 0;
    uint32_t sy = 0;

    for (uint32_t index = 0; index < SPRITE_COUNT; ++index) {
        points[index].image = images[ni::randomUint() % 4];
        points[index].x = 30.0f * sx + 20.0f;
        points[index].y = 30.0f * sy + 20.0f;
        points[index].width = (float)points[index].image->width;
        points[index].height = (float)points[index].image->height;
        points[index].rotation = ni::randomFloat();
        points[index].color = NI_COLOR_UINT(0x000000ff| ni::randomUint());
        points[index].velocityX = 0.0f;
        points[index].velocityY = 0.0f;
        points[index].speed = 50.8f;
        if (++sx > 999) {
            sx = 0;
            sy++;
        }
    }

    float viewPos[2] = { 0, 0 };
    float viewVel[2] = { 0, 0 };
    float viewAcl[2] = { 0, 0 };
    float rotation = 0.0f;
    double deltaAccumulation = 0.0;
    double accumulationCounter = 0.0;
	while (!ni::shouldQuit()) {
        double startTime = ni::getSeconds();

        PerformanceAPI_BeginEvent("Frame", nullptr, PERFORMANCEAPI_MAKE_COLOR(0xFF, 0xFF, 0x00));

		ni::pollEvents();

        bool btnDown = ni::mouseDown(ni::MOUSE_BUTTON_LEFT);

        if (btnDown && ni::mouseX() > ni::getViewWidth() - 300) {
            viewAcl[0] = 100.0f;
        } else if (btnDown && ni::mouseX() < 300) {
            viewAcl[0] = -100.0f;
        } else {
            viewAcl[0] = 0;
            viewVel[0] *= 0.9f;
        }

        if (btnDown && ni::mouseY() > ni::getViewHeight() - 300) {
            viewAcl[1] = 100.0f;
        } else if (btnDown && ni::mouseY() < 300) {
            viewAcl[1] = -100.0f;
        } else {
            viewAcl[1] = 0;
            viewVel[1] *= 0.9f;
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

            for (uint32_t index = 0; index < SPRITE_COUNT; ++index) {
                spriteRenderer->pushMatrix();
                spriteRenderer->translate(points[index].x, points[index].y);
                spriteRenderer->rotate(points[index].rotation);
                spriteRenderer->scale(0.25f, 0.25f);
                spriteRenderer->drawImage(-(float)points[index].width * 0.5f, -(float)points[index].height * 0.5f, (float)points[index].width, (float)points[index].height, NI_COLOR_UINT(0xffffffff), points[index].image);
                spriteRenderer->popMatrix();
                points[index].rotation -= 1.0f / 60.0f;
            }

            spriteRenderer->pushMatrix();
            spriteRenderer->translate((viewPos[0] + ni::mouseX()), (viewPos[1] + ni::mouseY()));
            spriteRenderer->rotate(-rotation);
            spriteRenderer->drawImage(-(float)images[0]->width * 0.5f, -(float)images[0]->height * 0.5f, (float)images[0]->width, (float)images[0]->height, NI_COLOR_UINT(0xff0000ff), images[0]);
            spriteRenderer->popMatrix();
            spriteRenderer->popMatrix();
            PerformanceAPI_EndEvent();
        }
        
        /* Submit to the GPU */
        {
            PerformanceAPI_BeginEvent("GPU Submit", nullptr, PERFORMANCEAPI_MAKE_COLOR(0x00, 0xFF, 0xFF));
            ni::FrameData& frame = ni::beginFrame();
            spriteRenderer->flushCommands(frame);
            ni::endFrame();
            PerformanceAPI_EndEvent();
        }
		ni::present(0);
        ni::waitForCurrentFrame();

        rotation += 1.0f / 60.0f;

        PerformanceAPI_EndEvent();

        double endTime = ni::getSeconds();
        deltaAccumulation += (endTime - startTime) * 1000.0;
        accumulationCounter += 1.0f;
        if (accumulationCounter == 20) {
            printf("%.4lf ms\n", deltaAccumulation / accumulationCounter);
            accumulationCounter = 0.0f;
            deltaAccumulation = 0.0f;
        }
    }

    ni::waitForAllFrames();
    delete spriteRenderer;
    delete[] points;
	ni::destroy();
    return 0;
}