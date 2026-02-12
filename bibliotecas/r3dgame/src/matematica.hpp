#ifndef MATEMATICA_HPP
#define MATEMATICA_HPP

#include <cmath>

// Constante PI para cálculos de radianos
#ifndef PI
#define PI 3.1415926535f
#endif

// === ESTRUTURAS BÁSICAS ===

struct Vec3 { 
    float x, y, z; 
};

struct Mat4x4 { 
    float m[4][4]; 
};

// === FUNÇÕES DE MATRIZES (Static para evitar conflito de linkagem) ===

// 1. Matriz Identidade (1 na diagonal, 0 no resto)
static Mat4x4 mat_identity() {
    Mat4x4 res = {0};
    res.m[0][0] = 1; res.m[1][1] = 1; res.m[2][2] = 1; res.m[3][3] = 1;
    return res;
}

// 2. Multiplicação de Matrizes (A * B)
static Mat4x4 mat_mul(Mat4x4 a, Mat4x4 b) {
    Mat4x4 res = {0};
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            res.m[r][c] = a.m[r][0]*b.m[0][c] + 
                          a.m[r][1]*b.m[1][c] + 
                          a.m[r][2]*b.m[2][c] + 
                          a.m[r][3]*b.m[3][c];
        }
    }
    return res;
}

// 3. Transposta (Inverte linhas/colunas - Necessário para DirectX/HLSL)
static Mat4x4 mat_transpose(Mat4x4 in) {
    Mat4x4 out;
    for(int r=0; r<4; r++) {
        for(int c=0; c<4; c++) {
            out.m[r][c] = in.m[c][r];
        }
    }
    return out;
}

// 4. Matriz de Translação (Move x, y, z)
static Mat4x4 mat_translation(float x, float y, float z) {
    Mat4x4 res = mat_identity();
    res.m[3][0] = x; 
    res.m[3][1] = y; 
    res.m[3][2] = z;
    return res;
}

// 5. Matriz de Escala (Aumenta/Diminui)
static Mat4x4 mat_scale(float x, float y, float z) {
    Mat4x4 res = mat_identity();
    res.m[0][0] = x; 
    res.m[1][1] = y; 
    res.m[2][2] = z;
    return res;
}

// 6. Rotação Eixo X
static Mat4x4 mat_rotX(float angle) {
    Mat4x4 res = mat_identity();
    float c = cos(angle); 
    float s = sin(angle);
    res.m[1][1] = c; res.m[1][2] = s;
    res.m[2][1] = -s; res.m[2][2] = c;
    return res;
}

// 7. Rotação Eixo Y
static Mat4x4 mat_rotY(float angle) {
    Mat4x4 res = mat_identity();
    float c = cos(angle); 
    float s = sin(angle);
    res.m[0][0] = c; res.m[0][2] = -s;
    res.m[2][0] = s; res.m[2][2] = c;
    return res;
}

// 8. Rotação Eixo Z
static Mat4x4 mat_rotZ(float angle) {
    Mat4x4 res = mat_identity();
    float c = cos(angle); 
    float s = sin(angle);
    res.m[0][0] = c; res.m[0][1] = s;
    res.m[1][0] = -s; res.m[1][1] = c;
    return res;
}

// 9. Projeção Perspectiva (Transforma o mundo 3D em tela 2D)
static Mat4x4 mat_perspective(float fovRad, float aspect, float nearZ, float farZ) {
    Mat4x4 res = {0};
    float tanHalfFov = tan(fovRad / 2.0f);
    
    res.m[0][0] = 1.0f / (aspect * tanHalfFov);
    res.m[1][1] = 1.0f / tanHalfFov;
    res.m[2][2] = farZ / (farZ - nearZ);
    res.m[2][3] = 1.0f;
    res.m[3][2] = -(farZ * nearZ) / (farZ - nearZ);
    return res;
}

// 10. Câmera LookAt (Cria a matriz de Visão)
static Mat4x4 mat_lookat(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 zaxis;
    float len = sqrt(pow(target.x - eye.x, 2) + pow(target.y - eye.y, 2) + pow(target.z - eye.z, 2));
    if (len == 0) len = 1;
    zaxis.x = (target.x - eye.x)/len; 
    zaxis.y = (target.y - eye.y)/len; 
    zaxis.z = (target.z - eye.z)/len;

    Vec3 xaxis;
    // Cross product: up * zaxis
    xaxis.x = up.y*zaxis.z - up.z*zaxis.y;
    xaxis.y = up.z*zaxis.x - up.x*zaxis.z;
    xaxis.z = up.x*zaxis.y - up.y*zaxis.x;
    
    len = sqrt(xaxis.x*xaxis.x + xaxis.y*xaxis.y + xaxis.z*xaxis.z);
    if (len == 0) len = 1;
    xaxis.x/=len; xaxis.y/=len; xaxis.z/=len;

    Vec3 yaxis;
    // Cross product: zaxis * xaxis
    yaxis.x = zaxis.y*xaxis.z - zaxis.z*xaxis.y;
    yaxis.y = zaxis.z*xaxis.x - zaxis.x*xaxis.z;
    yaxis.z = zaxis.x*xaxis.y - zaxis.y*xaxis.x;

    Mat4x4 view = mat_identity();
    view.m[0][0] = xaxis.x; view.m[0][1] = yaxis.x; view.m[0][2] = zaxis.x;
    view.m[1][0] = xaxis.y; view.m[1][1] = yaxis.y; view.m[1][2] = zaxis.y;
    view.m[2][0] = xaxis.z; view.m[2][1] = yaxis.z; view.m[2][2] = zaxis.z;
    
    // Dot products para translação da câmera
    view.m[3][0] = -(xaxis.x*eye.x + xaxis.y*eye.y + xaxis.z*eye.z);
    view.m[3][1] = -(yaxis.x*eye.x + yaxis.y*eye.y + yaxis.z*eye.z);
    view.m[3][2] = -(zaxis.x*eye.x + zaxis.y*eye.y + zaxis.z*eye.z);
    
    return view;
}

#endif