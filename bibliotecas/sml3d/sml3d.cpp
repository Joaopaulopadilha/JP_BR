// sml3d.cpp
// Biblioteca engine 3D SML3D para JPLang 3.0 - Wrapper OpenGL 4.6 via GLFW + GLAD
//
// COMPILAÇÃO:
//   Windows: g++ -shared -o bibliotecas\sml3d\sml3d.jpd bibliotecas\sml3d\sml3d.cpp bibliotecas\sml3d\src\src\glad.c bibliotecas\sml3d\src\ufbx-master\ufbx.c -Ibibliotecas\sml3d\src\include -Ibibliotecas\sml3d -Ibibliotecas\sml3d\src\ufbx-master -Lbibliotecas\sml3d\src\lib -lglfw3 -lopengl32 -lgdi32 -O2 -static
//   Linux:   g++ -shared -fPIC -o bibliotecas/sml3d/libsml3d.jpd bibliotecas/sml3d/sml3d.cpp bibliotecas/sml3d/src/src/glad.c bibliotecas/sml3d/src/ufbx-master/ufbx.c -Ibibliotecas/sml3d/src/include -Ibibliotecas/sml3d -Ibibliotecas/sml3d/src/ufbx-master -lglfw -lGL -ldl -O2 -std=c++17

#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>

// =============================================================================
// PLATAFORMA E EXPORT
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define JP_WINDOWS 1
    #define JP_EXPORT extern "C" __declspec(dllexport)
#else
    #define JP_WINDOWS 0
    #define JP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "src/stb_image.h"

#include "contexto.hpp"

// =============================================================================
// HELPER: CONVERTER INT64 (BITS DE DOUBLE) → DOUBLE
// JPLang passa decimais como double empacotado nos bits de um int64
// =============================================================================
static double toDouble(int64_t v) {
    double d;
    memcpy(&d, &v, sizeof(double));
    return d;
}

static float toFloat(int64_t v) {
    return (float)toDouble(v);
}

// =============================================================================
// BUFFER ROTATIVO PARA RETORNO DE STRINGS
// =============================================================================
static char str_bufs[8][4096];
static int str_buf_idx = 0;

static int64_t retorna_str(const char* s) {
    char* buf = str_bufs[str_buf_idx++ & 7];
    strncpy(buf, s, 4095);
    buf[4095] = '\0';
    return (int64_t)buf;
}

static int64_t retorna_str(const std::string& s) {
    return retorna_str(s.c_str());
}

// =============================================================================
// JANELA
// =============================================================================

// sm_window(titulo, largura, altura) -> inteiro (id da janela, 0=falha)
JP_EXPORT int64_t sm_window(int64_t titulo, int64_t largura, int64_t altura) {
    return (int64_t)criarJanela(std::string((const char*)titulo), (int)largura, (int)altura);
}

// sm_background(janela_id, r, g, b) -> inteiro (1=ok, 0=falha)
JP_EXPORT int64_t sm_background(int64_t id, int64_t r, int64_t g, int64_t b) {
    return janelaFundo((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// sm_display(janela_id) -> inteiro (1=aberta, 0=fechada)
JP_EXPORT int64_t sm_display(int64_t id) {
    return (int64_t)exibirJanela((int)id);
}

// sm_active(janela_id) -> inteiro (1=ativa, 0=inativa)
JP_EXPORT int64_t sm_active(int64_t id) {
    return janelaAtiva((int)id) ? 1 : 0;
}

// sm_width(janela_id) -> inteiro
JP_EXPORT int64_t sm_width(int64_t id) {
    return (int64_t)janelaLargura((int)id);
}

// sm_height(janela_id) -> inteiro
JP_EXPORT int64_t sm_height(int64_t id) {
    return (int64_t)janelaAltura((int)id);
}

// =============================================================================
// PRIMITIVAS
// =============================================================================

// sm_cube(janela_id) -> inteiro (id da mesh)
JP_EXPORT int64_t sm_cube(int64_t janela_id) {
    return (int64_t)criarCubo((int)janela_id);
}

// sm_plane(janela_id) -> inteiro (id da mesh)
JP_EXPORT int64_t sm_plane(int64_t janela_id) {
    return (int64_t)criarPlano((int)janela_id);
}

// sm_sphere(janela_id) -> inteiro (id da mesh)
JP_EXPORT int64_t sm_sphere(int64_t janela_id) {
    return (int64_t)criarEsfera((int)janela_id);
}

// =============================================================================
// TRANSFORMAÇÕES
// =============================================================================

// sm_position(mesh_id, x, y, z) -> inteiro (1=ok, 0=falha)
JP_EXPORT int64_t sm_position(int64_t id, int64_t x, int64_t y, int64_t z) {
    return meshPosicao((int)id, toFloat(x), toFloat(y), toFloat(z)) ? 1 : 0;
}

// sm_rotation(mesh_id, x, y, z) -> inteiro (1=ok, 0=falha) — ângulos em graus
JP_EXPORT int64_t sm_rotation(int64_t id, int64_t x, int64_t y, int64_t z) {
    float rad = (float)M_PI / 180.0f;
    return meshRotacao((int)id, toFloat(x) * rad, toFloat(y) * rad, toFloat(z) * rad) ? 1 : 0;
}

// sm_scale(mesh_id, x, y, z) -> inteiro (1=ok, 0=falha)
JP_EXPORT int64_t sm_scale(int64_t id, int64_t x, int64_t y, int64_t z) {
    return meshEscala((int)id, toFloat(x), toFloat(y), toFloat(z)) ? 1 : 0;
}

// =============================================================================
// COR
// =============================================================================

// sm_color(mesh_id, r, g, b) -> inteiro (1=ok, 0=falha)
JP_EXPORT int64_t sm_color(int64_t id, int64_t r, int64_t g, int64_t b) {
    return meshCor((int)id, (int)r, (int)g, (int)b) ? 1 : 0;
}

// =============================================================================
// VISIBILIDADE
// =============================================================================

// sm_visible(mesh_id, visivel) -> inteiro (1=ok, 0=falha)
JP_EXPORT int64_t sm_visible(int64_t id, int64_t visivel) {
    return meshVisivel((int)id, visivel != 0) ? 1 : 0;
}

// =============================================================================
// DESTRUIR MESH
// =============================================================================

// sm_destroy(mesh_id) -> inteiro (1=ok)
JP_EXPORT int64_t sm_destroy(int64_t id) {
    destruirMesh((int)id);
    return 1;
}

// =============================================================================
// CÂMERA
// =============================================================================

// sm_camera(janela_id, px, py, pz, alvoX, alvoY, alvoZ) -> inteiro (id da câmera)
JP_EXPORT int64_t sm_camera(int64_t janela_id, int64_t px, int64_t py, int64_t pz,
                            int64_t ax, int64_t ay, int64_t az) {
    return (int64_t)criarCamera((int)janela_id, toFloat(px), toFloat(py), toFloat(pz),
                                toFloat(ax), toFloat(ay), toFloat(az));
}

// sm_camera_pos(cam_id, x, y, z) -> inteiro
JP_EXPORT int64_t sm_camera_pos(int64_t id, int64_t x, int64_t y, int64_t z) {
    return cameraPosicao((int)id, toFloat(x), toFloat(y), toFloat(z)) ? 1 : 0;
}

// sm_camera_look(cam_id, x, y, z) -> inteiro
JP_EXPORT int64_t sm_camera_look(int64_t id, int64_t x, int64_t y, int64_t z) {
    return cameraAlvo((int)id, toFloat(x), toFloat(y), toFloat(z)) ? 1 : 0;
}

// sm_camera_mode(cam_id, modo) -> inteiro (0=livre, 1=orbital)
JP_EXPORT int64_t sm_camera_mode(int64_t id, int64_t modo) {
    return cameraModo((int)id, (int)modo) ? 1 : 0;
}

// sm_camera_fov(cam_id, fov) -> inteiro
JP_EXPORT int64_t sm_camera_fov(int64_t id, int64_t fov) {
    return cameraFov((int)id, toFloat(fov)) ? 1 : 0;
}

// sm_camera_rotate(cam_id, yaw, pitch) -> inteiro — graus
JP_EXPORT int64_t sm_camera_rotate(int64_t id, int64_t yaw, int64_t pitch) {
    return cameraYawPitch((int)id, toFloat(yaw), toFloat(pitch)) ? 1 : 0;
}

// sm_camera_orbital_dist(cam_id, dist) -> inteiro
JP_EXPORT int64_t sm_camera_orbital_dist(int64_t id, int64_t dist) {
    return cameraOrbitalDist((int)id, toFloat(dist)) ? 1 : 0;
}

// sm_camera_orbital_angles(cam_id, yaw, pitch) -> inteiro — graus
JP_EXPORT int64_t sm_camera_orbital_angles(int64_t id, int64_t yaw, int64_t pitch) {
    return cameraOrbitalAngulos((int)id, toFloat(yaw), toFloat(pitch)) ? 1 : 0;
}

// sm_camera_forward(cam_id, delta) -> inteiro
JP_EXPORT int64_t sm_camera_forward(int64_t id, int64_t delta) {
    return cameraMoverFrente((int)id, toFloat(delta)) ? 1 : 0;
}

// sm_camera_strafe(cam_id, delta) -> inteiro
JP_EXPORT int64_t sm_camera_strafe(int64_t id, int64_t delta) {
    return cameraMoverLado((int)id, toFloat(delta)) ? 1 : 0;
}

// sm_camera_up(cam_id, delta) -> inteiro
JP_EXPORT int64_t sm_camera_up(int64_t id, int64_t delta) {
    return cameraMoverCima((int)id, toFloat(delta)) ? 1 : 0;
}

// sm_camera_activate(cam_id) -> inteiro
JP_EXPORT int64_t sm_camera_activate(int64_t id) {
    return cameraAtivar((int)id) ? 1 : 0;
}

// =============================================================================
// CÂMERA — FOLLOW E MOVIMENTO RELATIVO
// =============================================================================

// sm_camera_follow(cam_id, mesh_id) -> inteiro (1=ok) — 0 pra desvincular
JP_EXPORT int64_t sm_camera_follow(int64_t camId, int64_t meshId) {
    return cameraFollow((int)camId, (int)meshId) ? 1 : 0;
}

// sm_move_relative(mesh_id, cam_id, frente, lado) -> inteiro (1=ok)
// Move a mesh relativo à direção da câmera
JP_EXPORT int64_t sm_move_relative(int64_t meshId, int64_t camId,
                                   int64_t frente, int64_t lado) {
    float posX, posZ;
    if (!moverRelativoCamera((int)camId, (int)meshId,
                             toFloat(frente), toFloat(lado), posX, posZ)) {
        return 0;
    }

    auto itMesh = g_meshes.find((int)meshId);
    if (itMesh == g_meshes.end()) return 0;

    // Aplicar posição (mantém Y da física)
    itMesh->second.posicao.x = posX;
    itMesh->second.posicao.z = posZ;

    // Atualizar follow da câmera
    atualizarFollow((int)camId);

    return 1;
}

// sm_camera_follow_update(cam_id) -> inteiro (1=ok)
// Atualizar posição do alvo da câmera e rotação do mesh seguido
JP_EXPORT int64_t sm_camera_follow_update(int64_t camId) {
    atualizarFollow((int)camId);
    atualizarRotacaoFollow((int)camId);

    // Aplicar lerp na rotação do mesh seguido
    auto itFollow = g_camera_follow.find((int)camId);
    if (itFollow != g_camera_follow.end()) {
        atualizarRotacaoSuave(itFollow->second, 0.15f);
    }

    return 1;
}

// =============================================================================
// INPUT — TECLADO
// =============================================================================

// sm_key_pressed(tecla) -> inteiro (1=pressionada, 0=não)
JP_EXPORT int64_t sm_key_pressed(int64_t tecla) {
    return inputTeclaPressed((const char*)tecla) ? 1 : 0;
}

// sm_key_down(tecla) -> inteiro (1=acabou de pressionar neste frame)
JP_EXPORT int64_t sm_key_down(int64_t tecla) {
    return inputTeclaDown((const char*)tecla) ? 1 : 0;
}

// sm_key_up(tecla) -> inteiro (1=acabou de soltar neste frame)
JP_EXPORT int64_t sm_key_up(int64_t tecla) {
    return inputTeclaUp((const char*)tecla) ? 1 : 0;
}

// =============================================================================
// INPUT — MOUSE
// =============================================================================

// sm_mouse_x() -> inteiro (posição X do cursor)
JP_EXPORT int64_t sm_mouse_x() {
    return (int64_t)inputMouseX();
}

// sm_mouse_y() -> inteiro (posição Y do cursor)
JP_EXPORT int64_t sm_mouse_y() {
    return (int64_t)inputMouseY();
}

// sm_mouse_dx() -> inteiro (delta X do mouse no frame)
JP_EXPORT int64_t sm_mouse_dx() {
    return (int64_t)inputMouseDX();
}

// sm_mouse_dy() -> inteiro (delta Y do mouse no frame)
JP_EXPORT int64_t sm_mouse_dy() {
    return (int64_t)inputMouseDY();
}

// sm_mouse_button(botao) -> inteiro (1=pressionado) — 0=esq, 1=dir, 2=meio
JP_EXPORT int64_t sm_mouse_button(int64_t botao) {
    return inputMouseBotao((int)botao) ? 1 : 0;
}

// sm_mouse_button_down(botao) -> inteiro (1=clicou neste frame)
JP_EXPORT int64_t sm_mouse_button_down(int64_t botao) {
    return inputMouseBotaoDown((int)botao) ? 1 : 0;
}

// sm_mouse_button_up(botao) -> inteiro (1=soltou neste frame)
JP_EXPORT int64_t sm_mouse_button_up(int64_t botao) {
    return inputMouseBotaoUp((int)botao) ? 1 : 0;
}

// sm_mouse_scroll() -> inteiro (delta scroll vertical)
JP_EXPORT int64_t sm_mouse_scroll() {
    return (int64_t)inputMouseScrollY();
}

// sm_mouse_lock(janela_id, lock) -> inteiro (1=ok)
JP_EXPORT int64_t sm_mouse_lock(int64_t janela_id, int64_t lock) {
    (void)janela_id;
    inputMouseLock(lock != 0);
    return 1;
}

// =============================================================================
// MOUSE GRAB — captura do mouse pra jogos
// =============================================================================

// sm_mouse_grab(janela_id, grab) -> inteiro (1=ok)
JP_EXPORT int64_t sm_mouse_grab(int64_t janela_id, int64_t grab) {
    (void)janela_id;
    inputMouseGrab(grab != 0);
    return 1;
}

// sm_mouse_grab_toggle(janela_id, tecla) -> inteiro (1=ok)
// Define tecla pra alternar grab (ex: "escape")
JP_EXPORT int64_t sm_mouse_grab_toggle(int64_t janela_id, int64_t tecla) {
    (void)janela_id;
    inputMouseGrabToggleKey((const char*)tecla);
    return 1;
}

// sm_mouse_grabbed() -> inteiro (1=preso, 0=livre)
JP_EXPORT int64_t sm_mouse_grabbed() {
    return inputMouseGrabbed() ? 1 : 0;
}

// =============================================================================
// COLISÃO
// =============================================================================

// sm_collision(id1, id2) -> inteiro (1=colidindo, 0=não)
JP_EXPORT int64_t sm_collision(int64_t id1, int64_t id2) {
    return colisaoMeshMesh((int)id1, (int)id2) ? 1 : 0;
}

// sm_distance(id1, id2) -> decimal (distância entre centros)
JP_EXPORT int64_t sm_distance(int64_t id1, int64_t id2) {
    double d = (double)distanciaMeshMesh((int)id1, (int)id2);
    int64_t r;
    memcpy(&r, &d, sizeof(int64_t));
    return r;
}

// sm_raycast(cam_id, mesh_id) -> inteiro (1=raio acerta, 0=não)
JP_EXPORT int64_t sm_raycast(int64_t camId, int64_t meshId) {
    return raycastCameraMesh((int)camId, (int)meshId) ? 1 : 0;
}

// sm_bounds_min_x(mesh_id) -> decimal
// sm_bounds_min_y(mesh_id) -> decimal
// sm_bounds_min_z(mesh_id) -> decimal
// sm_bounds_max_x(mesh_id) -> decimal
// sm_bounds_max_y(mesh_id) -> decimal
// sm_bounds_max_z(mesh_id) -> decimal

static int64_t retorna_double(double val) {
    int64_t r;
    memcpy(&r, &val, sizeof(int64_t));
    return r;
}

// sm_position_x(mesh_id) -> decimal
JP_EXPORT int64_t sm_position_x(int64_t id) {
    return retorna_double((double)meshObterPosX((int)id));
}

// sm_position_y(mesh_id) -> decimal
JP_EXPORT int64_t sm_position_y(int64_t id) {
    return retorna_double((double)meshObterPosY((int)id));
}

// sm_position_z(mesh_id) -> decimal
JP_EXPORT int64_t sm_position_z(int64_t id) {
    return retorna_double((double)meshObterPosZ((int)id));
}

// sm_rotation_y(mesh_id) -> decimal (rotação Y em radianos)
JP_EXPORT int64_t sm_rotation_y(int64_t id) {
    return retorna_double((double)meshObterRotY((int)id));
}

JP_EXPORT int64_t sm_bounds_min_x(int64_t id) {
    SML3DAABB box;
    if (!obterBounds((int)id, box)) return 0;
    return retorna_double((double)box.min_x);
}

JP_EXPORT int64_t sm_bounds_min_y(int64_t id) {
    SML3DAABB box;
    if (!obterBounds((int)id, box)) return 0;
    return retorna_double((double)box.min_y);
}

JP_EXPORT int64_t sm_bounds_min_z(int64_t id) {
    SML3DAABB box;
    if (!obterBounds((int)id, box)) return 0;
    return retorna_double((double)box.min_z);
}

JP_EXPORT int64_t sm_bounds_max_x(int64_t id) {
    SML3DAABB box;
    if (!obterBounds((int)id, box)) return 0;
    return retorna_double((double)box.max_x);
}

JP_EXPORT int64_t sm_bounds_max_y(int64_t id) {
    SML3DAABB box;
    if (!obterBounds((int)id, box)) return 0;
    return retorna_double((double)box.max_y);
}

JP_EXPORT int64_t sm_bounds_max_z(int64_t id) {
    SML3DAABB box;
    if (!obterBounds((int)id, box)) return 0;
    return retorna_double((double)box.max_z);
}

// =============================================================================
// FÍSICA / GRAVIDADE
// =============================================================================

// sm_gravity(mesh_id, ativar) -> inteiro (1=ok)
JP_EXPORT int64_t sm_gravity(int64_t id, int64_t ativar) {
    return ativarGravidade((int)id, ativar != 0) ? 1 : 0;
}

// sm_gravity_force(forca) -> inteiro (1=ok)
JP_EXPORT int64_t sm_gravity_force(int64_t forca) {
    definirGravidade(toFloat(forca));
    return 1;
}

// sm_velocity(mesh_id, vx, vy, vz) -> inteiro (1=ok)
JP_EXPORT int64_t sm_velocity(int64_t id, int64_t vx, int64_t vy, int64_t vz) {
    return definirVelocidade((int)id, toFloat(vx), toFloat(vy), toFloat(vz)) ? 1 : 0;
}

// sm_ground(mesh_id, y) -> inteiro (1=ok)
JP_EXPORT int64_t sm_ground(int64_t id, int64_t y) {
    return definirGround((int)id, toFloat(y)) ? 1 : 0;
}

// sm_solid(mesh_id, ehChao) -> inteiro (1=ok)
// Marca uma mesh como sólida (outros objetos com gravidade param nela)
JP_EXPORT int64_t sm_solid(int64_t id, int64_t ehChao) {
    return definirMeshChao((int)id, ehChao != 0) ? 1 : 0;
}

// sm_pushable(mesh_id, pushable) -> inteiro (1=ok)
// Marca uma mesh como empurrável por outros objetos
JP_EXPORT int64_t sm_pushable(int64_t id, int64_t pushable) {
    return definirPushable((int)id, pushable != 0) ? 1 : 0;
}

// sm_bounce(mesh_id, fator) -> inteiro (1=ok)
// Define fator de quique (0.0 = sem bounce, 1.0 = bounce total)
JP_EXPORT int64_t sm_bounce(int64_t id, int64_t fator) {
    return definirBounce((int)id, toFloat(fator)) ? 1 : 0;
}

// sm_on_ground(mesh_id) -> inteiro (1=no chão, 0=no ar)
JP_EXPORT int64_t sm_on_ground(int64_t id) {
    return estaNoChao((int)id) ? 1 : 0;
}

// sm_physics_update(janela_id) -> inteiro (1=ok)
JP_EXPORT int64_t sm_physics_update(int64_t janela_id) {
    atualizarFisica((int)janela_id);
    return 1;
}

// sm_delta_time() -> decimal (tempo do frame em segundos)
JP_EXPORT int64_t sm_delta_time() {
    return retorna_double((double)obterDeltaTime());
}

// =============================================================================
// TEMPO / TIMER
// =============================================================================

// sm_fps() -> decimal (FPS atual)
JP_EXPORT int64_t sm_fps() {
    return retorna_double((double)obterFPS());
}

// sm_fps_limit(limite) -> inteiro (1=ok) — 0 pra desativar limite
JP_EXPORT int64_t sm_fps_limit(int64_t limite) {
    definirFPSLimite((int)limite);
    return 1;
}

// sm_timer() -> decimal (tempo total desde o início em segundos)
JP_EXPORT int64_t sm_timer() {
    return retorna_double(obterTempoTotal());
}

// sm_timer_create() -> inteiro (id do cronômetro)
JP_EXPORT int64_t sm_timer_create() {
    return (int64_t)criarCronometro();
}

// sm_timer_elapsed(id) -> decimal (tempo decorrido em segundos)
JP_EXPORT int64_t sm_timer_elapsed(int64_t id) {
    return retorna_double(cronometroElapsed((int)id));
}

// sm_timer_reset(id) -> inteiro (1=ok)
JP_EXPORT int64_t sm_timer_reset(int64_t id) {
    return cronometroReset((int)id) ? 1 : 0;
}

// sm_timer_pause(id) -> inteiro (1=ok)
JP_EXPORT int64_t sm_timer_pause(int64_t id) {
    return cronometroPausar((int)id) ? 1 : 0;
}

// sm_timer_resume(id) -> inteiro (1=ok)
JP_EXPORT int64_t sm_timer_resume(int64_t id) {
    return cronometroRetomar((int)id) ? 1 : 0;
}

// sm_timer_destroy(id) -> inteiro (1=ok)
JP_EXPORT int64_t sm_timer_destroy(int64_t id) {
    destruirCronometro((int)id);
    return 1;
}

// sm_wait(ms) -> inteiro (1=ok)
JP_EXPORT int64_t sm_wait(int64_t ms) {
    esperarMS((int)ms);
    return 1;
}

// =============================================================================
// TEXTURAS
// =============================================================================

// sm_texture(mesh_id, caminho) -> inteiro (1=ok, 0=falha)
// Carrega a textura (ou pega do cache) e aplica na mesh
JP_EXPORT int64_t sm_texture(int64_t meshId, int64_t caminho) {
    return texturizarMesh((int)meshId, (const char*)caminho) ? 1 : 0;
}

// sm_texture_remove(mesh_id) -> inteiro (1=ok, 0=falha)
// Remove a textura da mesh (volta pra cor sólida)
JP_EXPORT int64_t sm_texture_remove(int64_t meshId) {
    return removerTexturaMesh((int)meshId) ? 1 : 0;
}

// sm_texture_tile(mesh_id, repeat_x, repeat_y) -> inteiro (1=ok, 0=falha)
// Define quantas vezes a textura repete em X e Y
JP_EXPORT int64_t sm_texture_tile(int64_t meshId, int64_t rx, int64_t ry) {
    return meshTexTile((int)meshId, toFloat(rx), toFloat(ry)) ? 1 : 0;
}

// =============================================================================
// MODELOS 3D
// =============================================================================

// sm_model(janela_id, caminho) -> inteiro (id da mesh, 0=falha)
// Carrega um modelo 3D (.obj ou .fbx) e retorna o ID da mesh
JP_EXPORT int64_t sm_model(int64_t janelaId, int64_t caminho) {
    return (int64_t)carregarModelo((int)janelaId, (const char*)caminho);
}

// =============================================================================
// ANIMAÇÃO
// =============================================================================

// sm_anim_load(mesh_id, caminho) -> inteiro (índice do clip, -1=falha)
// Carrega animação de outro FBX e adiciona como clip no modelo
JP_EXPORT int64_t sm_anim_load(int64_t meshId, int64_t caminho) {
    return (int64_t)animCarregarClip((int)meshId, (const char*)caminho);
}

// sm_anim_play(mesh_id) -> inteiro (1=ok, 0=falha)
// Inicia/retoma a animação da mesh
JP_EXPORT int64_t sm_anim_play(int64_t meshId) {
    return animPlay((int)meshId) ? 1 : 0;
}

// sm_anim_pause(mesh_id) -> inteiro (1=ok, 0=falha)
// Pausa a animação da mesh
JP_EXPORT int64_t sm_anim_pause(int64_t meshId) {
    return animPause((int)meshId) ? 1 : 0;
}

// sm_anim_clip(mesh_id, clip_index) -> inteiro (1=ok, 0=falha)
// Troca o clip de animação ativo (0, 1, 2...)
JP_EXPORT int64_t sm_anim_clip(int64_t meshId, int64_t clipIndex) {
    return animSetClip((int)meshId, (int)clipIndex) ? 1 : 0;
}

// sm_anim_speed(mesh_id, velocidade) -> inteiro (1=ok, 0=falha)
// Define velocidade da animação (1.0=normal, 2.0=dobro, 0.5=metade)
JP_EXPORT int64_t sm_anim_speed(int64_t meshId, int64_t vel) {
    return animVelocidade((int)meshId, toFloat(vel)) ? 1 : 0;
}

// sm_anim_loop(mesh_id, loop) -> inteiro (1=ok, 0=falha)
// Ativa/desativa loop da animação
JP_EXPORT int64_t sm_anim_loop(int64_t meshId, int64_t loop) {
    return animLoop((int)meshId, loop != 0) ? 1 : 0;
}

// sm_anim_reset(mesh_id) -> inteiro (1=ok, 0=falha)
// Reinicia a animação do começo
JP_EXPORT int64_t sm_anim_reset(int64_t meshId) {
    return animReset((int)meshId) ? 1 : 0;
}

// sm_anim_blend(mesh_id, clip_b, fator) -> inteiro (1=ok, 0=falha)
// Mistura clip atual com clip_b. fator: 0.0=100% atual, 1.0=100% clip_b
// clip_b=-1 desliga o blend
JP_EXPORT int64_t sm_anim_blend(int64_t meshId, int64_t clipB, int64_t fator) {
    return animBlend((int)meshId, (int)clipB, toFloat(fator)) ? 1 : 0;
}

// sm_anim_clips(mesh_id) -> inteiro (número de clips disponíveis)
JP_EXPORT int64_t sm_anim_clips(int64_t meshId) {
    return (int64_t)animNumClips((int)meshId);
}

// sm_anim_duration(mesh_id) -> decimal (duração do clip atual em segundos)
JP_EXPORT int64_t sm_anim_duration(int64_t meshId) {
    return retorna_double((double)animDuracao((int)meshId));
}

// sm_anim_time(mesh_id) -> decimal (tempo atual da animação em segundos)
JP_EXPORT int64_t sm_anim_time(int64_t meshId) {
    return retorna_double((double)animTempo((int)meshId));
}

// sm_anim_root_motion(mesh_id, remover) -> inteiro (1=ok, 0=falha)
// 1=remove root motion (anda no lugar), 0=deixa animação mover o modelo
JP_EXPORT int64_t sm_anim_root_motion(int64_t meshId, int64_t remover) {
    return animRootMotion((int)meshId, remover != 0) ? 1 : 0;
}