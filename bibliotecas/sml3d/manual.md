# SML3D — Manual de Referência

Biblioteca 3D para JPLang. Usa OpenGL 4.6 via GLFW + GLAD.

---

## Janela

### sm_window(titulo, largura, altura)
Cria uma janela com contexto OpenGL.
- `titulo` (texto): título da janela
- `largura` (inteiro): largura em pixels
- `altura` (inteiro): altura em pixels
- **Retorna**: ID da janela (inteiro), 0 se falhar

### sm_background(janela_id, r, g, b)
Define a cor de fundo da janela.
- `janela_id` (inteiro): ID da janela
- `r, g, b` (inteiro): cor RGB (0-255)
- **Retorna**: 1=ok, 0=falha

### sm_display(janela_id)
Renderiza o frame atual (limpa tela, desenha meshes, swap buffers, poll events).
- `janela_id` (inteiro): ID da janela
- **Retorna**: 1=janela aberta, 0=janela fechada

### sm_active(janela_id)
Verifica se a janela está ativa.
- `janela_id` (inteiro): ID da janela
- **Retorna**: 1=ativa, 0=inativa

### sm_width(janela_id)
Retorna a largura atual da janela em pixels.
- `janela_id` (inteiro): ID da janela
- **Retorna**: largura (inteiro)

### sm_height(janela_id)
Retorna a altura atual da janela em pixels.
- `janela_id` (inteiro): ID da janela
- **Retorna**: altura (inteiro)

---

## Primitivas

### sm_cube(janela_id)
Cria um cubo unitário (-0.5 a 0.5) com normais e UVs.
- `janela_id` (inteiro): ID da janela
- **Retorna**: ID da mesh (inteiro)

### sm_plane(janela_id)
Cria um plano unitário 1x1 no eixo XZ.
- `janela_id` (inteiro): ID da janela
- **Retorna**: ID da mesh (inteiro)

### sm_sphere(janela_id)
Cria uma esfera unitária (raio 0.5) com 32 setores e 16 pilhas.
- `janela_id` (inteiro): ID da janela
- **Retorna**: ID da mesh (inteiro)

---

## Transformações

### sm_position(mesh_id, x, y, z)
Define a posição da mesh no mundo (ou local se tem pai).
- `mesh_id` (inteiro): ID da mesh
- `x, y, z` (decimal): coordenadas
- **Retorna**: 1=ok, 0=falha

### sm_rotation(mesh_id, x, y, z)
Define a rotação da mesh em graus.
- `mesh_id` (inteiro): ID da mesh
- `x, y, z` (decimal): ângulos em graus (X=pitch, Y=yaw, Z=roll)
- **Retorna**: 1=ok, 0=falha

### sm_scale(mesh_id, x, y, z)
Define a escala da mesh.
- `mesh_id` (inteiro): ID da mesh
- `x, y, z` (decimal): fator de escala por eixo
- **Retorna**: 1=ok, 0=falha

### sm_offset_position(mesh_id, x, y, z)
Offset de posição local do modelo. Aplicado antes de todas as transformações. Útil para corrigir pivot de modelos importados.
- `mesh_id` (inteiro): ID da mesh
- `x, y, z` (decimal): offset local
- **Retorna**: 1=ok, 0=falha

### sm_offset_rotation(mesh_id, x, y, z)
Offset de rotação local do modelo em graus. Aplicado antes da rotação de gameplay. Útil para corrigir orientação de modelos FBX.
- `mesh_id` (inteiro): ID da mesh
- `x, y, z` (decimal): ângulos em graus
- **Retorna**: 1=ok, 0=falha

### sm_move_forward(mesh_id, distancia)
Move a mesh na direção que ela aponta (baseado na rotação Y). Se a mesh é um veículo registrado, atualiza suspensão e inclinação automaticamente.
- `mesh_id` (inteiro): ID da mesh
- `distancia` (decimal): positivo=frente, negativo=trás
- **Retorna**: 1=ok, 0=falha

### sm_rotate_y(mesh_id, angulo)
Soma um ângulo à rotação Y atual da mesh. Útil para curvas suaves.
- `mesh_id` (inteiro): ID da mesh
- `angulo` (decimal): ângulo em graus a somar
- **Retorna**: 1=ok, 0=falha

---

## Consultas de Posição

### sm_position_x(mesh_id)
Retorna a posição X da mesh.
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: posição X (decimal)

### sm_position_y(mesh_id)
Retorna a posição Y da mesh.
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: posição Y (decimal)

### sm_position_z(mesh_id)
Retorna a posição Z da mesh.
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: posição Z (decimal)

### sm_rotation_y(mesh_id)
Retorna a rotação Y da mesh em radianos.
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: rotação Y (decimal, radianos)

---

## Aparência

### sm_color(mesh_id, r, g, b)
Define a cor da mesh.
- `mesh_id` (inteiro): ID da mesh
- `r, g, b` (inteiro): cor RGB (0-255)
- **Retorna**: 1=ok, 0=falha

### sm_visible(mesh_id, visivel)
Mostra ou esconde a mesh.
- `mesh_id` (inteiro): ID da mesh
- `visivel` (inteiro): 1=visível, 0=invisível
- **Retorna**: 1=ok, 0=falha

### sm_destroy(mesh_id)
Remove a mesh da cena e libera recursos.
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: 1=ok

---

## Parenting (Hierarquia)

### sm_parent(filho_id, pai_id)
Vincula uma mesh como filha de outra. Posição e rotação do filho passam a ser locais (relativas ao pai). Escala é independente. Use `pai_id=0` para desvincular.
- `filho_id` (inteiro): ID da mesh filha
- `pai_id` (inteiro): ID da mesh pai (0=desvincular)
- **Retorna**: 1=ok, 0=falha

---

## Texturas

### sm_texture(mesh_id, caminho)
Carrega uma textura (PNG/JPG) e aplica na mesh. Usa cache interno — se o mesmo arquivo já foi carregado, reutiliza.
- `mesh_id` (inteiro): ID da mesh
- `caminho` (texto): caminho do arquivo de imagem
- **Retorna**: 1=ok, 0=falha

### sm_texture_remove(mesh_id)
Remove a textura da mesh (volta para cor sólida).
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: 1=ok, 0=falha

### sm_texture_tile(mesh_id, repeat_x, repeat_y)
Define quantas vezes a textura repete em X e Y.
- `mesh_id` (inteiro): ID da mesh
- `repeat_x, repeat_y` (decimal): fator de repetição
- **Retorna**: 1=ok, 0=falha

---

## Modelos 3D

### sm_model(janela_id, caminho)
Carrega um modelo 3D (.obj ou .fbx). Detecta automaticamente o formato pela extensão. Para FBX, tenta carregar como animado primeiro (se tiver skeleton), senão carrega como estático. Extrai cor e textura do material automaticamente.
- `janela_id` (inteiro): ID da janela
- `caminho` (texto): caminho do arquivo (.obj ou .fbx)
- **Retorna**: ID da mesh (inteiro), 0=falha

---

## Câmera

### sm_camera(janela_id, px, py, pz, alvo_x, alvo_y, alvo_z)
Cria uma câmera e a define como ativa na janela.
- `janela_id` (inteiro): ID da janela
- `px, py, pz` (decimal): posição inicial
- `alvo_x, alvo_y, alvo_z` (decimal): ponto para onde a câmera olha
- **Retorna**: ID da câmera (inteiro)

### sm_camera_pos(cam_id, x, y, z)
Define a posição da câmera.
- `cam_id` (inteiro): ID da câmera
- `x, y, z` (decimal): nova posição
- **Retorna**: 1=ok, 0=falha

### sm_camera_look(cam_id, x, y, z)
Define o ponto alvo para onde a câmera olha.
- `cam_id` (inteiro): ID da câmera
- `x, y, z` (decimal): ponto alvo
- **Retorna**: 1=ok, 0=falha

### sm_camera_mode(cam_id, modo)
Define o modo da câmera.
- `cam_id` (inteiro): ID da câmera
- `modo` (inteiro): 0=livre (FPS), 1=orbital
- **Retorna**: 1=ok, 0=falha

### sm_camera_fov(cam_id, fov)
Define o campo de visão.
- `cam_id` (inteiro): ID da câmera
- `fov` (decimal): campo de visão em graus (padrão: 45)
- **Retorna**: 1=ok, 0=falha

### sm_camera_rotate(cam_id, yaw, pitch)
Define yaw e pitch da câmera (modo livre) em graus.
- `cam_id` (inteiro): ID da câmera
- `yaw, pitch` (decimal): ângulos em graus
- **Retorna**: 1=ok, 0=falha

### sm_camera_orbital_dist(cam_id, dist)
Define a distância orbital da câmera ao alvo.
- `cam_id` (inteiro): ID da câmera
- `dist` (decimal): distância
- **Retorna**: 1=ok, 0=falha

### sm_camera_orbital_angles(cam_id, yaw, pitch)
Define os ângulos orbitais da câmera em graus.
- `cam_id` (inteiro): ID da câmera
- `yaw, pitch` (decimal): ângulos em graus
- **Retorna**: 1=ok, 0=falha

### sm_camera_forward(cam_id, delta)
Move a câmera para frente/trás (modo livre).
- `cam_id` (inteiro): ID da câmera
- `delta` (decimal): distância (positivo=frente, negativo=trás)
- **Retorna**: 1=ok, 0=falha

### sm_camera_strafe(cam_id, delta)
Move a câmera para os lados (modo livre).
- `cam_id` (inteiro): ID da câmera
- `delta` (decimal): distância (positivo=direita, negativo=esquerda)
- **Retorna**: 1=ok, 0=falha

### sm_camera_up(cam_id, delta)
Move a câmera para cima/baixo (modo livre).
- `cam_id` (inteiro): ID da câmera
- `delta` (decimal): distância (positivo=cima, negativo=baixo)
- **Retorna**: 1=ok, 0=falha

### sm_camera_activate(cam_id)
Define esta câmera como a ativa da janela.
- `cam_id` (inteiro): ID da câmera
- **Retorna**: 1=ok, 0=falha

### sm_camera_follow(cam_id, mesh_id)
Faz a câmera orbital seguir uma mesh. Use `mesh_id=0` para desvincular.
- `cam_id` (inteiro): ID da câmera
- `mesh_id` (inteiro): ID da mesh a seguir (0=desvincular)
- **Retorna**: 1=ok, 0=falha

### sm_camera_follow_update(cam_id)
Atualiza posição do alvo da câmera E rotação da mesh seguida (mesh vira na direção da câmera). Ideal para personagens em terceira pessoa.
- `cam_id` (inteiro): ID da câmera
- **Retorna**: 1=ok

### sm_camera_follow_pos(cam_id)
Atualiza APENAS a posição do alvo da câmera, sem forçar rotação da mesh. Ideal para veículos ou quando a mesh tem rotação própria.
- `cam_id` (inteiro): ID da câmera
- **Retorna**: 1=ok

### sm_camera_collision(cam_id, ativar)
Ativa/desativa colisão de câmera. Quando ativa, a câmera não atravessa meshes visíveis (aproxima automaticamente).
- `cam_id` (inteiro): ID da câmera
- `ativar` (inteiro): 1=ativar, 0=desativar
- **Retorna**: 1=ok, 0=falha

### sm_move_relative(mesh_id, cam_id, frente, lado)
Move a mesh relativo à direção da câmera orbital. Ideal para personagens em terceira pessoa.
- `mesh_id` (inteiro): ID da mesh
- `cam_id` (inteiro): ID da câmera
- `frente` (decimal): distância frente/trás
- `lado` (decimal): distância esquerda/direita
- **Retorna**: 1=ok, 0=falha

---

## Input — Teclado

### sm_key_pressed(tecla)
Verifica se a tecla está pressionada agora.
- `tecla` (texto): nome da tecla ("w", "a", "space", "shift", "escape", "f1", etc.)
- **Retorna**: 1=pressionada, 0=não

### sm_key_down(tecla)
Verifica se a tecla acabou de ser pressionada neste frame.
- `tecla` (texto): nome da tecla
- **Retorna**: 1=sim, 0=não

### sm_key_up(tecla)
Verifica se a tecla acabou de ser solta neste frame.
- `tecla` (texto): nome da tecla
- **Retorna**: 1=sim, 0=não

**Teclas suportadas**: a-z, 0-9, space/espaco, enter, escape/esc, tab, backspace, delete, insert, up/cima, down/baixo, left/esquerda, right/direita, shift/lshift, rshift, ctrl/lctrl, rctrl, alt/lalt, ralt, f1-f12.

---

## Input — Mouse

### sm_mouse_x()
Retorna a posição X do cursor.
- **Retorna**: posição X (inteiro)

### sm_mouse_y()
Retorna a posição Y do cursor.
- **Retorna**: posição Y (inteiro)

### sm_mouse_dx()
Retorna o delta X do mouse no frame.
- **Retorna**: delta X (inteiro)

### sm_mouse_dy()
Retorna o delta Y do mouse no frame.
- **Retorna**: delta Y (inteiro)

### sm_mouse_button(botao)
Verifica se um botão do mouse está pressionado.
- `botao` (inteiro): 0=esquerdo, 1=direito, 2=meio
- **Retorna**: 1=pressionado, 0=não

### sm_mouse_button_down(botao)
Verifica se o botão acabou de ser clicado neste frame.
- `botao` (inteiro): 0=esquerdo, 1=direito, 2=meio
- **Retorna**: 1=sim, 0=não

### sm_mouse_button_up(botao)
Verifica se o botão acabou de ser solto neste frame.
- `botao` (inteiro): 0=esquerdo, 1=direito, 2=meio
- **Retorna**: 1=sim, 0=não

### sm_mouse_scroll()
Retorna o delta do scroll vertical no frame.
- **Retorna**: delta scroll (inteiro)

### sm_mouse_lock(janela_id, lock)
Trava/destrava o cursor na janela.
- `janela_id` (inteiro): ID da janela
- `lock` (inteiro): 1=travar, 0=destravar
- **Retorna**: 1=ok

### sm_mouse_grab(janela_id, grab)
Captura/libera o mouse para jogos (cursor escondido + raw input).
- `janela_id` (inteiro): ID da janela
- `grab` (inteiro): 1=capturar, 0=liberar
- **Retorna**: 1=ok

### sm_mouse_grab_toggle(janela_id, tecla)
Define tecla para alternar captura do mouse (ex: "escape").
- `janela_id` (inteiro): ID da janela
- `tecla` (texto): nome da tecla para toggle
- **Retorna**: 1=ok

### sm_mouse_grabbed()
Verifica se o mouse está capturado.
- **Retorna**: 1=capturado, 0=livre

---

## Colisão

### sm_collision(mesh_id_1, mesh_id_2)
Testa colisão AABB entre duas meshes.
- `mesh_id_1, mesh_id_2` (inteiro): IDs das meshes
- **Retorna**: 1=colidindo, 0=não

### sm_distance(mesh_id_1, mesh_id_2)
Calcula distância entre centros de duas meshes.
- `mesh_id_1, mesh_id_2` (inteiro): IDs das meshes
- **Retorna**: distância (decimal), -1 se falhar

### sm_raycast(cam_id, mesh_id)
Testa se o raio da câmera (direção que ela olha) acerta a bounding box da mesh.
- `cam_id` (inteiro): ID da câmera
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: 1=acerta, 0=não

### sm_bounds_min_x(mesh_id) / sm_bounds_min_y / sm_bounds_min_z
Retorna o valor mínimo da bounding box da mesh no eixo indicado (world space).
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: valor (decimal)

### sm_bounds_max_x(mesh_id) / sm_bounds_max_y / sm_bounds_max_z
Retorna o valor máximo da bounding box da mesh no eixo indicado (world space).
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: valor (decimal)

---

## Física / Gravidade

### sm_gravity(mesh_id, ativar)
Ativa/desativa gravidade numa mesh.
- `mesh_id` (inteiro): ID da mesh
- `ativar` (inteiro): 1=ativar, 0=desativar
- **Retorna**: 1=ok, 0=falha

### sm_gravity_force(forca)
Define a força da gravidade global (padrão: -9.8).
- `forca` (decimal): valor da gravidade (negativo=pra baixo)
- **Retorna**: 1=ok

### sm_velocity(mesh_id, vx, vy, vz)
Define a velocidade de uma mesh com física.
- `mesh_id` (inteiro): ID da mesh
- `vx, vy, vz` (decimal): velocidade nos 3 eixos
- **Retorna**: 1=ok, 0=falha

### sm_ground(mesh_id, y)
Define um chão manual (Y mínimo) para a mesh.
- `mesh_id` (inteiro): ID da mesh
- `y` (decimal): coordenada Y do chão
- **Retorna**: 1=ok, 0=falha

### sm_solid(mesh_id, ativar)
Marca uma mesh como sólida (outros objetos com gravidade param nela).
- `mesh_id` (inteiro): ID da mesh
- `ativar` (inteiro): 1=sólida, 0=não
- **Retorna**: 1=ok, 0=falha

### sm_pushable(mesh_id, ativar)
Marca uma mesh como empurrável por outros objetos.
- `mesh_id` (inteiro): ID da mesh
- `ativar` (inteiro): 1=empurrável, 0=não
- **Retorna**: 1=ok, 0=falha

### sm_bounce(mesh_id, fator)
Define fator de quique da mesh (0.0=sem bounce, 1.0=bounce total).
- `mesh_id` (inteiro): ID da mesh
- `fator` (decimal): fator de quique (0.0 a 1.0)
- **Retorna**: 1=ok, 0=falha

### sm_on_ground(mesh_id)
Verifica se a mesh está no chão.
- `mesh_id` (inteiro): ID da mesh
- **Retorna**: 1=no chão, 0=no ar

### sm_physics_update(janela_id)
Atualiza toda a física da janela (gravidade, colisões, resolução). Chamar uma vez por frame.
- `janela_id` (inteiro): ID da janela
- **Retorna**: 1=ok

### sm_delta_time()
Retorna o tempo do frame anterior em segundos.
- **Retorna**: delta time (decimal)

---

## Veículo

### sm_vehicle(corpo_id)
Registra uma mesh como veículo. Ativa suspensão e inclinação automáticas via `sm_move_forward`.
- `corpo_id` (inteiro): ID da mesh do corpo do veículo
- **Retorna**: 1=ok, 0=falha

### sm_vehicle_wheel(corpo_id, roda_id, off_x, off_y, off_z)
Registra uma roda no veículo com posição local relativa ao corpo.
- `corpo_id` (inteiro): ID do corpo do veículo
- `roda_id` (inteiro): ID da mesh da roda
- `off_x, off_y, off_z` (decimal): posição local da roda relativa ao corpo (off_y geralmente negativo)
- **Retorna**: 1=ok, 0=falha

### sm_vehicle_suspension(corpo_id, forca, amortecimento)
Configura parâmetros da suspensão do veículo.
- `corpo_id` (inteiro): ID do corpo
- `forca` (decimal): rigidez da mola (padrão: 30.0, maior=mais rígido)
- `amortecimento` (decimal): amortecimento (padrão: 15.0, maior=menos oscilação)
- **Retorna**: 1=ok, 0=falha

---

## Tempo / Timer

### sm_fps()
Retorna o FPS atual.
- **Retorna**: FPS (decimal)

### sm_fps_limit(limite)
Define limite de FPS (0=sem limite).
- `limite` (inteiro): máximo de frames por segundo
- **Retorna**: 1=ok

### sm_timer()
Retorna tempo total desde o início em segundos.
- **Retorna**: tempo (decimal)

### sm_timer_create()
Cria um cronômetro individual.
- **Retorna**: ID do cronômetro (inteiro)

### sm_timer_elapsed(timer_id)
Retorna tempo decorrido no cronômetro em segundos.
- `timer_id` (inteiro): ID do cronômetro
- **Retorna**: tempo (decimal)

### sm_timer_reset(timer_id)
Reinicia o cronômetro.
- `timer_id` (inteiro): ID do cronômetro
- **Retorna**: 1=ok, 0=falha

### sm_timer_pause(timer_id)
Pausa o cronômetro.
- `timer_id` (inteiro): ID do cronômetro
- **Retorna**: 1=ok, 0=falha

### sm_timer_resume(timer_id)
Retoma o cronômetro pausado.
- `timer_id` (inteiro): ID do cronômetro
- **Retorna**: 1=ok, 0=falha

### sm_timer_destroy(timer_id)
Destrói o cronômetro.
- `timer_id` (inteiro): ID do cronômetro
- **Retorna**: 1=ok

### sm_wait(ms)
Pausa a execução por N milissegundos.
- `ms` (inteiro): tempo em milissegundos
- **Retorna**: 1=ok

---

## Animação

### sm_anim_load(mesh_id, caminho)
Carrega animação de outro FBX e adiciona como clip no modelo. O FBX deve ter o mesmo skeleton (ex: mesmo personagem do Mixamo).
- `mesh_id` (inteiro): ID da mesh animada
- `caminho` (texto): caminho do FBX com a animação
- **Retorna**: índice do clip adicionado (inteiro), -1=falha

### sm_anim_play(mesh_id)
Inicia/retoma a animação.
- `mesh_id` (inteiro): ID da mesh animada
- **Retorna**: 1=ok, 0=falha

### sm_anim_pause(mesh_id)
Pausa a animação.
- `mesh_id` (inteiro): ID da mesh animada
- **Retorna**: 1=ok, 0=falha

### sm_anim_clip(mesh_id, clip_index)
Troca o clip de animação ativo.
- `mesh_id` (inteiro): ID da mesh animada
- `clip_index` (inteiro): índice do clip (0, 1, 2...)
- **Retorna**: 1=ok, 0=falha

### sm_anim_speed(mesh_id, velocidade)
Define velocidade da animação.
- `mesh_id` (inteiro): ID da mesh animada
- `velocidade` (decimal): 1.0=normal, 2.0=dobro, 0.5=metade
- **Retorna**: 1=ok, 0=falha

### sm_anim_loop(mesh_id, loop)
Ativa/desativa loop da animação.
- `mesh_id` (inteiro): ID da mesh animada
- `loop` (inteiro): 1=loop, 0=sem loop
- **Retorna**: 1=ok, 0=falha

### sm_anim_reset(mesh_id)
Reinicia a animação do começo.
- `mesh_id` (inteiro): ID da mesh animada
- **Retorna**: 1=ok, 0=falha

### sm_anim_blend(mesh_id, clip_b, fator)
Mistura o clip atual (A) com outro clip (B).
- `mesh_id` (inteiro): ID da mesh animada
- `clip_b` (inteiro): índice do clip B (-1=desligar blend)
- `fator` (decimal): 0.0=100% clip A, 1.0=100% clip B
- **Retorna**: 1=ok, 0=falha

### sm_anim_clips(mesh_id)
Retorna o número de clips disponíveis.
- `mesh_id` (inteiro): ID da mesh animada
- **Retorna**: quantidade de clips (inteiro)

### sm_anim_duration(mesh_id)
Retorna a duração do clip atual em segundos.
- `mesh_id` (inteiro): ID da mesh animada
- **Retorna**: duração (decimal)

### sm_anim_time(mesh_id)
Retorna o tempo atual da animação em segundos.
- `mesh_id` (inteiro): ID da mesh animada
- **Retorna**: tempo (decimal)

### sm_anim_root_motion(mesh_id, remover)
Controla remoção do root motion. Quando ativo, o personagem anda no lugar — movimento é controlado pelo jogo.
- `mesh_id` (inteiro): ID da mesh animada
- `remover` (inteiro): 1=remove root motion (anda no lugar), 0=animação move o modelo
- **Retorna**: 1=ok, 0=falha
