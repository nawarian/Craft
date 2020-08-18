#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <curl/curl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noise.h"
#include "tinycthread.h"

#include "m_game.h"
#include "m_util.h"
#include "d_db.h"
#include "g_cube.h"
#include "g_item.h"
#include "p_world.h"

#define XZ_SIZE (CHUNK_SIZE * 3 + 2)
#define XZ_LO (CHUNK_SIZE)
#define XZ_HI (CHUNK_SIZE * 2 + 1)
#define Y_SIZE 258
#define XYZ(x, y, z) ((y) * XZ_SIZE * XZ_SIZE + (x) * XZ_SIZE + (z))
#define XZ(x, z) ((x) * XZ_SIZE + (z))

static Model model;
static Model *g;

void _create_window() {
    int window_width = WINDOW_WIDTH;
    int window_height = WINDOW_HEIGHT;
    GLFWmonitor *monitor = NULL;
    if (FULLSCREEN) {
        int mode_count;
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *modes = glfwGetVideoModes(monitor, &mode_count);
        window_width = modes[mode_count - 1].width;
        window_height = modes[mode_count - 1].height;
    }
    g->window = glfwCreateWindow(
        window_width, window_height, "Craft", monitor, NULL);
}

void _load_textures() {
    // Textures
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_util_texture_load_png("textures/texture.png");

    // Sky
    GLuint sky;
    glGenTextures(1, &sky);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sky);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_util_texture_load_png("textures/sky.png");

    // Font
    GLuint font;
    glGenTextures(1, &font);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, font);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_util_texture_load_png("textures/font.png");

    // Sign
    GLuint sign;
    glGenTextures(1, &sign);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, sign);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_util_texture_load_png("textures/sign.png");
}

// @todo
void _load_shaders(Attrib *block, Attrib *line, Attrib *text, Attrib *sky) {
    GLuint program;

    program = m_util_program_load(
        "shaders/block_vertex.glsl", "shaders/block_fragment.glsl");
    block->program = program;
    block->position = glGetAttribLocation(program, "position");
    block->normal = glGetAttribLocation(program, "normal");
    block->uv = glGetAttribLocation(program, "uv");
    block->matrix = glGetUniformLocation(program, "matrix");
    block->sampler = glGetUniformLocation(program, "sampler");
    block->extra1 = glGetUniformLocation(program, "sky_sampler");
    block->extra2 = glGetUniformLocation(program, "daylight");
    block->extra3 = glGetUniformLocation(program, "fog_distance");
    block->extra4 = glGetUniformLocation(program, "ortho");
    block->camera = glGetUniformLocation(program, "camera");
    block->timer = glGetUniformLocation(program, "timer");

    program = m_util_program_load(
        "shaders/line_vertex.glsl", "shaders/line_fragment.glsl");
    line->program = program;
    line->position = glGetAttribLocation(program, "position");
    line->matrix = glGetUniformLocation(program, "matrix");

    program = m_util_program_load(
        "shaders/text_vertex.glsl", "shaders/text_fragment.glsl");
    text->program = program;
    text->position = glGetAttribLocation(program, "position");
    text->uv = glGetAttribLocation(program, "uv");
    text->matrix = glGetUniformLocation(program, "matrix");
    text->sampler = glGetUniformLocation(program, "sampler");
    text->extra1 = glGetUniformLocation(program, "is_sign");

    program = m_util_program_load(
        "shaders/sky_vertex.glsl", "shaders/sky_fragment.glsl");
    sky->program = program;
    sky->position = glGetAttribLocation(program, "position");
    sky->normal = glGetAttribLocation(program, "normal");
    sky->uv = glGetAttribLocation(program, "uv");
    sky->matrix = glGetUniformLocation(program, "matrix");
    sky->sampler = glGetUniformLocation(program, "sampler");
    sky->timer = glGetUniformLocation(program, "timer");
}

void light_fill(
    char *opaque, char *light,
    int x, int y, int z, int w, int force)
{
    if (x + w < XZ_LO || z + w < XZ_LO) {
        return;
    }
    if (x - w > XZ_HI || z - w > XZ_HI) {
        return;
    }
    if (y < 0 || y >= Y_SIZE) {
        return;
    }
    if (light[XYZ(x, y, z)] >= w) {
        return;
    }
    if (!force && opaque[XYZ(x, y, z)]) {
        return;
    }
    light[XYZ(x, y, z)] = w--;
    light_fill(opaque, light, x - 1, y, z, w, 0);
    light_fill(opaque, light, x + 1, y, z, w, 0);
    light_fill(opaque, light, x, y - 1, z, w, 0);
    light_fill(opaque, light, x, y + 1, z, w, 0);
    light_fill(opaque, light, x, y, z - 1, w, 0);
    light_fill(opaque, light, x, y, z + 1, w, 0);
}

void occlusion(
    char neighbors[27], char lights[27], float shades[27],
    float ao[6][4], float light[6][4])
{
    static const int lookup3[6][4][3] = {
        {{0, 1, 3}, {2, 1, 5}, {6, 3, 7}, {8, 5, 7}},
        {{18, 19, 21}, {20, 19, 23}, {24, 21, 25}, {26, 23, 25}},
        {{6, 7, 15}, {8, 7, 17}, {24, 15, 25}, {26, 17, 25}},
        {{0, 1, 9}, {2, 1, 11}, {18, 9, 19}, {20, 11, 19}},
        {{0, 3, 9}, {6, 3, 15}, {18, 9, 21}, {24, 15, 21}},
        {{2, 5, 11}, {8, 5, 17}, {20, 11, 23}, {26, 17, 23}}
    };
   static const int lookup4[6][4][4] = {
        {{0, 1, 3, 4}, {1, 2, 4, 5}, {3, 4, 6, 7}, {4, 5, 7, 8}},
        {{18, 19, 21, 22}, {19, 20, 22, 23}, {21, 22, 24, 25}, {22, 23, 25, 26}},
        {{6, 7, 15, 16}, {7, 8, 16, 17}, {15, 16, 24, 25}, {16, 17, 25, 26}},
        {{0, 1, 9, 10}, {1, 2, 10, 11}, {9, 10, 18, 19}, {10, 11, 19, 20}},
        {{0, 3, 9, 12}, {3, 6, 12, 15}, {9, 12, 18, 21}, {12, 15, 21, 24}},
        {{2, 5, 11, 14}, {5, 8, 14, 17}, {11, 14, 20, 23}, {14, 17, 23, 26}}
    };
    static const float curve[4] = {0.0, 0.25, 0.5, 0.75};
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 4; j++) {
            int corner = neighbors[lookup3[i][j][0]];
            int side1 = neighbors[lookup3[i][j][1]];
            int side2 = neighbors[lookup3[i][j][2]];
            int value = side1 && side2 ? 3 : corner + side1 + side2;
            float shade_sum = 0;
            float light_sum = 0;
            int is_light = lights[13] == 15;
            for (int k = 0; k < 4; k++) {
                shade_sum += shades[lookup4[i][j][k]];
                light_sum += lights[lookup4[i][j][k]];
            }
            if (is_light) {
                light_sum = 15 * 4 * 10;
            }
            float total = curve[value] + shade_sum / 4.0;
            ao[i][j] = MIN(total, 1.0);
            light[i][j] = light_sum / 15.0 / 4.0;
        }
    }
}

void m_game_compute_chunk(WorkerItem *item) {
    char *opaque = (char *)calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *light = (char *)calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *highest = (char *)calloc(XZ_SIZE * XZ_SIZE, sizeof(char));

    int ox = item->p * CHUNK_SIZE - CHUNK_SIZE - 1;
    int oy = -1;
    int oz = item->q * CHUNK_SIZE - CHUNK_SIZE - 1;

    // check for lights
    int has_light = 0;
    if (SHOW_LIGHTS) {
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) {
                Map *map = item->light_maps[a][b];
                if (map && map->size) {
                    has_light = 1;
                }
            }
        }
    }

    // populate opaque array
    for (int a = 0; a < 3; a++) {
        for (int b = 0; b < 3; b++) {
            Map *map = item->block_maps[a][b];
            if (!map) {
                continue;
            }
            MAP_FOR_EACH(map, ex, ey, ez, ew) {
                int x = ex - ox;
                int y = ey - oy;
                int z = ez - oz;
                int w = ew;
                // TODO: this should be unnecessary
                if (x < 0 || y < 0 || z < 0) {
                    continue;
                }
                if (x >= XZ_SIZE || y >= Y_SIZE || z >= XZ_SIZE) {
                    continue;
                }
                // END TODO
                opaque[XYZ(x, y, z)] = !g_item_is_transparent(w);
                if (opaque[XYZ(x, y, z)]) {
                    highest[XZ(x, z)] = MAX(highest[XZ(x, z)], y);
                }
            } END_MAP_FOR_EACH;
        }
    }

    // flood fill light intensities
    if (has_light) {
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) {
                Map *map = item->light_maps[a][b];
                if (!map) {
                    continue;
                }
                MAP_FOR_EACH(map, ex, ey, ez, ew) {
                    int x = ex - ox;
                    int y = ey - oy;
                    int z = ez - oz;
                    light_fill(opaque, light, x, y, z, ew, 1);
                } END_MAP_FOR_EACH;
            }
        }
    }

    Map *map = item->block_maps[1][1];

    // count exposed faces
    int miny = 256;
    int maxy = 0;
    int faces = 0;
    MAP_FOR_EACH(map, ex, ey, ez, ew) {
        if (ew <= 0) {
            continue;
        }
        int x = ex - ox;
        int y = ey - oy;
        int z = ez - oz;
        int f1 = !opaque[XYZ(x - 1, y, z)];
        int f2 = !opaque[XYZ(x + 1, y, z)];
        int f3 = !opaque[XYZ(x, y + 1, z)];
        int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
        int f5 = !opaque[XYZ(x, y, z - 1)];
        int f6 = !opaque[XYZ(x, y, z + 1)];
        int total = f1 + f2 + f3 + f4 + f5 + f6;
        if (total == 0) {
            continue;
        }
        if (g_item_is_plant(ew)) {
            total = 4;
        }
        miny = MIN(miny, ey);
        maxy = MAX(maxy, ey);
        faces += total;
    } END_MAP_FOR_EACH;

    // generate geometry
    GLfloat *data = m_util_faces_malloc(10, faces);
    int offset = 0;
    MAP_FOR_EACH(map, ex, ey, ez, ew) {
        if (ew <= 0) {
            continue;
        }
        int x = ex - ox;
        int y = ey - oy;
        int z = ez - oz;
        int f1 = !opaque[XYZ(x - 1, y, z)];
        int f2 = !opaque[XYZ(x + 1, y, z)];
        int f3 = !opaque[XYZ(x, y + 1, z)];
        int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
        int f5 = !opaque[XYZ(x, y, z - 1)];
        int f6 = !opaque[XYZ(x, y, z + 1)];
        int total = f1 + f2 + f3 + f4 + f5 + f6;
        if (total == 0) {
            continue;
        }
        char neighbors[27] = {0};
        char lights[27] = {0};
        float shades[27] = {0};
        int index = 0;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    neighbors[index] = opaque[XYZ(x + dx, y + dy, z + dz)];
                    lights[index] = light[XYZ(x + dx, y + dy, z + dz)];
                    shades[index] = 0;
                    if (y + dy <= highest[XZ(x + dx, z + dz)]) {
                        for (int oy = 0; oy < 8; oy++) {
                            if (opaque[XYZ(x + dx, y + dy + oy, z + dz)]) {
                                shades[index] = 1.0 - oy * 0.125;
                                break;
                            }
                        }
                    }
                    index++;
                }
            }
        }
        float ao[6][4];
        float light[6][4];
        occlusion(neighbors, lights, shades, ao, light);
        if (g_item_is_plant(ew)) {
            total = 4;
            float min_ao = 1;
            float max_light = 0;
            for (int a = 0; a < 6; a++) {
                for (int b = 0; b < 4; b++) {
                    min_ao = MIN(min_ao, ao[a][b]);
                    max_light = MAX(max_light, light[a][b]);
                }
            }
            float rotation = simplex2(ex, ez, 4, 0.5, 2) * 360;
            g_cube_make_plant(
                data + offset, min_ao, max_light,
                ex, ey, ez, 0.5, ew, rotation);
        }
        else {
            g_cube_make_cube(
                data + offset, ao, light,
                f1, f2, f3, f4, f5, f6,
                ex, ey, ez, 0.5, ew);
        }
        offset += total * 60;
    } END_MAP_FOR_EACH;

    free(opaque);
    free(light);
    free(highest);

    item->miny = miny;
    item->maxy = maxy;
    item->faces = faces;
    item->data = data;
}

void map_set_func(int x, int y, int z, int w, void *arg) {
    Map *map = (Map *) arg;
    p_map_set(map, x, y, z, w);
}

void m_game_load_chunk(WorkerItem *item) {
    int p = item->p;
    int q = item->q;
    Map *block_map = item->block_maps[1][1];
    Map *light_map = item->light_maps[1][1];
    p_world_create(p, q, map_set_func, block_map);
    d_db_load_blocks(block_map, p, q);
    d_db_load_lights(light_map, p, q);
}

int _worker_run(void *arg) {
    Worker *worker = (Worker *)arg;
    int running = 1;
    while (running) {
        mtx_lock(&worker->mtx);
        while (worker->state != WORKER_BUSY) {
            cnd_wait(&worker->cnd, &worker->mtx);
        }
        mtx_unlock(&worker->mtx);
        WorkerItem *item = &worker->item;
        if (item->load) {
            m_game_load_chunk(item);
        }
        m_game_compute_chunk(item);
        mtx_lock(&worker->mtx);
        worker->state = WORKER_DONE;
        mtx_unlock(&worker->mtx);
    }
    return 0;
}

int m_game_init(
    Model *model,
    void* callback_on_key,
    void* callback_on_char,
    void* callback_on_mouse_button,
    void* callback_on_scroll,
    Attrib *block,
    Attrib *line,
    Attrib *text,
    Attrib *sky
) {
    // Sets model from m_main.c (refactor in progress...)
    g = model;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    srand(time(NULL));
    rand();

    // Initialize Window
    if (!glfwInit()) {
        return -1;
    }

    _create_window();
    if (!g->window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(g->window);
    glfwSwapInterval(VSYNC);

    // Registering input callbacks
    glfwSetInputMode(g->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(g->window, callback_on_key);
    glfwSetCharCallback(g->window, callback_on_char);
    glfwSetMouseButtonCallback(g->window, callback_on_mouse_button);
    glfwSetScrollCallback(g->window, callback_on_scroll);

    if (glewInit() != GLEW_OK) {
        return -1;
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glLogicOp(GL_INVERT);
    glClearColor(0, 0, 0, 1);

    _load_textures();
    _load_shaders(block, line, text, sky);

    g->create_radius = CREATE_CHUNK_RADIUS;
    g->render_radius = RENDER_CHUNK_RADIUS;
    g->delete_radius = DELETE_CHUNK_RADIUS;
    g->sign_radius = RENDER_SIGN_RADIUS;

    // INITIALIZE WORKER THREADS
    for (int i = 0; i < WORKERS; i++) {
        Worker *worker = g->workers + i;
        worker->index = i;
        worker->state = WORKER_IDLE;
        mtx_init(&worker->mtx, mtx_plain);
        cnd_init(&worker->cnd);
        thrd_create(&worker->thrd, _worker_run, worker);
    }

    return 0;
}

void m_game_create() {
}

void _get_motion_vector(
    int flying,
    int sz,
    int sx,
    float rx,
    float ry,
    float *vx,
    float *vy,
    float *vz
) {
    *vx = 0;
    *vy = 0;
    *vz = 0;

    if (!sz && !sx) {
        return;
    }

    float strafe = atan2f(sz, sx);
    if (flying) {
        float m = cosf(ry);
        float y = sinf(ry);

        if (sx) {
            if  (!sz) {
                y = 0;
            }
            m = 1;
        }

        if (sz > 0) {
            y = -y;
        }

        *vx = cosf(rx + strafe) * m;
        *vy = y;
        *vz = sinf(rx + strafe) * m;
    } else {
        *vx = cosf(rx + strafe);
        *vy = 0;
        *vz = sinf(rx + strafe);
    }
}

int _chunked(float x) {
    return floorf(roundf(x) / CHUNK_SIZE);
}

Chunk *_find_chunk(int p, int q) {
    for (int i = 0; i < g->chunk_count; ++i) {
        Chunk *chunk = g->chunks + i;

        if (chunk->p == p && chunk->q == q) {
            return chunk;
        }
    }

    return 0;
}

int _collide(int height, float *x, float *y, float *z) {
    int result = 0;
    int p = _chunked(*x);
    int q = _chunked(*z);
    Chunk *chunk = _find_chunk(p, q);
    if (!chunk) {
        return result;
    }

    Map *map = &chunk->map;
    int nx = roundf(*x);
    int ny = roundf(*y);
    int nz = roundf(*z);
    float px = *x - nx;
    float py = *y - ny;
    float pz = *z - nz;
    float pad = 0.25;

    for (int dy = 0; dy < height; ++dy) {
        if (px < -pad && g_item_is_obstacle(p_map_get(map, nx - 1, ny - dy, nz))) {
            *x = nx - pad;
        }
        if (px > pad && g_item_is_obstacle(p_map_get(map, nx + 1, ny - dy, nz))) {
            *x = nx + pad;
        }
        if (py < -pad && g_item_is_obstacle(p_map_get(map, nx, ny - dy - 1, nz))) {
            *y = ny - pad;
            result = 1;
        }
        if (py > pad && g_item_is_obstacle(p_map_get(map, nx, ny - dy + 1, nz))) {
            *y = ny + pad;
            result = 1;
        }
        if (pz < -pad && g_item_is_obstacle(p_map_get(map, nx, ny - dy, nz - 1))) {
            *z = nz - pad;
        }
        if (pz > pad && g_item_is_obstacle(p_map_get(map, nx, ny - dy, nz + 1))) {
            *z = nz + pad;
        }
    }

    return result;
}

int _highest_block(float x, float z) {
    int result = -1;
    int nx = roundf(x);
    int nz = roundf(z);
    int p = _chunked(x);
    int q = _chunked(z);

    Chunk *chunk = _find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        MAP_FOR_EACH(map, ex, ey, ez, ew) {
            if (g_item_is_obstacle(ew) && ex == nx && ez == nz) {
                result = MAX(result, ey);
            }
        } END_MAP_FOR_EACH;
    }

    return result;
}

void m_game_update(double dt) {
    // Handle Mouse Movement
    int exclusive = glfwGetInputMode(g->window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    static double px = 0;
    static double py = 0;
    State *s = &g->players->state;

    if (exclusive && (px || py)) {
        double mx, my;
        glfwGetCursorPos(g->window, &mx, &my);
        float m = 0.0025;
        s->rx += (mx - px) * m;

        if (INVERT_MOUSE) {
            s->ry += (my - py) * m;
        } else {
            s->ry -= (my - py) * m;
        }

        if (s->rx < 0) {
            s->rx += RADIANS(360);
        }

        if (s->rx >= RADIANS(360)) {
            s->rx -= RADIANS(360);
        }

        s->ry = MAX(s->ry, -RADIANS(90));
        s->ry = MIN(s->ry, RADIANS(90));
        px = mx;
        py = my;
    } else {
        glfwGetCursorPos(g->window, &px, &py);
    }

    // Handle Input
    static float dy = 0;
    int sz = 0;
    int sx = 0;

    if (!g->typing) {
        float m = dt * 1.0;
        g->ortho = glfwGetKey(g->window, CRAFT_KEY_ORTHO) ? 64 : 0;
        g->fov = glfwGetKey(g->window, CRAFT_KEY_ZOOM) ? 15 : 65;
        if (glfwGetKey(g->window, CRAFT_KEY_FORWARD)) --sz;
        if (glfwGetKey(g->window, CRAFT_KEY_BACKWARD)) ++sz;
        if (glfwGetKey(g->window, CRAFT_KEY_LEFT)) --sx;
        if (glfwGetKey(g->window, CRAFT_KEY_RIGHT)) ++sx;
        if (glfwGetKey(g->window, GLFW_KEY_LEFT)) s->rx -= m;
        if (glfwGetKey(g->window, GLFW_KEY_RIGHT)) s->rx += m;
        if (glfwGetKey(g->window, GLFW_KEY_UP)) s->ry += m;
        if (glfwGetKey(g->window, GLFW_KEY_DOWN)) s->ry -= m;
    }

    float vx, vy, vz;
    _get_motion_vector(g->flying, sz, sx, s->rx, s->ry, &vx, &vy, &vz);
    if (!g->typing) {
        if (glfwGetKey(g->window, CRAFT_KEY_JUMP)) {
            if (g->flying) {
                vy = 1;
            } else if (dy == 0) {
                dy = 8;
            }
        }
    }

    float speed = g->flying ? 20 : 5;
    int estimate = roundf(sqrtf(
        powf(vx * speed, 2) +
        powf(vy * speed + ABS(dy) * 2, 2) +
        powf(vz * speed, 2)
    ) * dt * 8);
    int step = MAX(8, estimate);
    float ut = dt / step;
    vx = vx * ut * speed;
    vy = vy * ut * speed;
    vz = vz * ut * speed;

    for (int i = 0; i < step; ++i) {
        if (g->flying) {
            dy = 0;
        } else {
            dy -= ut * 25;
            dy = MAX(dy, -250);
        }

        s->x += vx;
        s->y += vy + dy * ut;
        s->z += vz;

        if (_collide(2, &s->x, &s->y, &s->z)) {
            dy = 0;
        }
    }

    if (s->y < 0) {
         s->y = _highest_block(s->x, s->z) + 2;
    }

    // Receive data from server

    // Flush database

    // Send position to server
}

int _chunk_distance(Chunk *chunk, int p, int q) {
    int dp = ABS(chunk->p - p);
    int dq = ABS(chunk->q - q);
    return MAX(dp, dq);
}

void _delete_chunks() {
    int count = g->chunk_count;
    State *s1 = &g->players->state;
    State *s2 = &(g->players + g->observe1)->state;
    State *s3 = &(g->players + g->observe2)->state;
    State *states[3] = {s1, s2, s3};
    for (int i = 0; i < count; i++) {
        Chunk *chunk = g->chunks + i;
        int delete = 1;
        for (int j = 0; j < 3; j++) {
            State *s = states[j];
            int p = _chunked(s->x);
            int q = _chunked(s->z);
            if (_chunk_distance(chunk, p, q) < g->delete_radius) {
                delete = 0;
                break;
            }
        }
        if (delete) {
            p_map_free(&chunk->map);
            p_map_free(&chunk->lights);
            g_sign_list_free(&chunk->signs);
            m_util_buffer_del(chunk->buffer);
            m_util_buffer_del(chunk->sign_buffer);
            Chunk *other = g->chunks + (--count);
            memcpy(chunk, other, sizeof(Chunk));
        }
    }
    g->chunk_count = count;
}

GLuint _gen_player_buffer(float x, float y, float z, float rx, float ry) {
    GLfloat *data = m_util_faces_malloc(10, 6);
    g_cube_make_player(data, x, y, z, rx, ry);

    return m_util_faces_gen(10, 6, data);
}

void _update_player(Player *player,
    float x, float y, float z, float rx, float ry, int interpolate)
{
    if (interpolate) {
        State *s1 = &player->state1;
        State *s2 = &player->state2;
        memcpy(s1, s2, sizeof(State));
        s2->x = x; s2->y = y; s2->z = z; s2->rx = rx; s2->ry = ry;
        s2->t = glfwGetTime();
        if (s2->rx - s1->rx > PI) {
            s1->rx += 2 * PI;
        }
        if (s1->rx - s2->rx > PI) {
            s1->rx -= 2 * PI;
        }
    }
    else {
        State *s = &player->state;
        s->x = x; s->y = y; s->z = z; s->rx = rx; s->ry = ry;
        m_util_buffer_del(player->buffer);
        player->buffer = _gen_player_buffer(s->x, s->y, s->z, s->rx, s->ry);
    }
}


void _interpolate_player(Player *player) {
    State *s1 = &player->state1;
    State *s2 = &player->state2;
    float t1 = s2->t - s1->t;
    float t2 = glfwGetTime() - s2->t;
    t1 = MIN(t1, 1);
    t1 = MAX(t1, 0.1);
    float p = MIN(t2 / t1, 1);
    _update_player(
        player,
        s1->x + (s2->x - s1->x) * p,
        s1->y + (s2->y - s1->y) * p,
        s1->z + (s2->z - s1->z) * p,
        s1->rx + (s2->rx - s1->rx) * p,
        s1->ry + (s2->ry - s1->ry) * p,
        0);
}

void m_game_render() {
    State *s = &g->players->state;
    Player *me = g->players;
    // Prepare rendering
    g->observe1 = g->observe1 % g->player_count;
    g->observe2 = g->observe2 % g->player_count;

    _delete_chunks();
    m_util_buffer_del(me->buffer);
    me->buffer = _gen_player_buffer(s->x, s->y, s->z, s->rx, s->ry);

    for (int i = 1; i < g->player_count; ++i) {
        _interpolate_player(g->players + i);
    }

    Player *player = g->players + g->observe1;

    // Render 3D scene
    // Render HUD
    // Render Text
    // Render Picture in Picture


    // Swap and Poll
}

