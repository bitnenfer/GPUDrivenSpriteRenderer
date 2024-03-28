#pragma once

#include <math.h>

#define MATRIX_STACK_DEPTH (1 << 10)

struct Matrix2D {

    inline void init() {
        identity();
    }
    inline void identity() {
        tx = 0.0f;
        ty = 0.0f;
        tscale = 1.0f;
        trotation = 0.0f;
    }
    inline void translate(float x, float y) {
        tx += x;
        ty += y;
    }
    inline void rotate(float rad) {
        trotation += rad;
    }
    inline void scale(float x, float y) {
        tscale = x;
    }

    float tx;
    float ty;
    float tscale;
    float trotation;
};

struct MatrixStack {
    MatrixStack() {
        current.init();
        depth = 0;
        memset(matrices, 0, sizeof(matrices));
    }
    ~MatrixStack() {
    }
    inline void pushMatrix() {
        if (depth < MATRIX_STACK_DEPTH) {
            matrices[depth++] = current;
        }
    }
    inline void popMatrix() {
        if (depth > 0) {
            current = matrices[--depth];
        }
    }
    inline void translate(float x, float y) { current.translate(x, y); }
    inline void rotate(float rad) { current.rotate(rad); }
    inline void scale(float x, float y) { current.scale(x, y); }
    inline void loadIdentity() { current.identity(); }
    inline Matrix2D& currentMatrix() { return current; }

    Matrix2D matrices[MATRIX_STACK_DEPTH];
    Matrix2D current;
    uint32_t depth;
};

