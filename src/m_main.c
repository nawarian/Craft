#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <curl/curl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// /deps
#include "noise.h"
#include "tinycthread.h"

// /src
#include "d_db.h"
#include "g_config.h"
#include "g_cube.h"
#include "g_item.h"
#include "g_matrix.h"
#include "g_sign.h"
#include "m_game.h"
#include "m_util.h"
#include "n_auth.h"
#include "n_client.h"
#include "p_map.h"
#include "p_world.h"

static Model model;
static Model *g = &model;

int chunked(float x) {
    return floorf(roundf(x) / CHUNK_SIZE);
}

float time_of_day() {
    if (g->day_length <= 0) {
        return 0.5;
    }
    float t;
    t = glfwGetTime();
    t = t / g->day_length;
    t = t - (int)t;
    return t;
}

float get_daylight() {
    float timer = time_of_day();
    if (timer < 0.5) {
        float t = (timer - 0.25) * 100;
        return 1 / (1 + powf(2, -t));
    }
    else {
        float t = (timer - 0.85) * 100;
        return 1 - 1 / (1 + powf(2, -t));
    }
}

int get_scale_factor() {
    int window_width, window_height;
    int buffer_width, buffer_height;
    glfwGetWindowSize(g->window, &window_width, &window_height);
    glfwGetFramebufferSize(g->window, &buffer_width, &buffer_height);
    int result = buffer_width / window_width;
    result = MAX(1, result);
    result = MIN(2, result);
    return result;
}

void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz) {
    float m = cosf(ry);
    *vx = cosf(rx - RADIANS(90)) * m;
    *vy = sinf(ry);
    *vz = sinf(rx - RADIANS(90)) * m;
}

GLuint gen_crosshair_buffer() {
    int x = g->width / 2;
    int y = g->height / 2;
    int p = 10 * g->scale;
    float data[] = {
        x, y - p, x, y + p,
        x - p, y, x + p, y
    };
    return m_util_buffer_gen(sizeof(data), data);
}

GLuint gen_wireframe_buffer(float x, float y, float z, float n) {
    float data[72];
    g_cube_make_wireframe(data, x, y, z, n);
    return m_util_buffer_gen(sizeof(data), data);
}

GLuint gen_sky_buffer() {
    float data[12288];
    g_cube_make_sphere(data, 1, 3);
    return m_util_buffer_gen(sizeof(data), data);
}

GLuint gen_cube_buffer(float x, float y, float z, float n, int w) {
    GLfloat *data = m_util_faces_malloc(10, 6);
    float ao[6][4] = {0};
    float light[6][4] = {
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5}
    };
    g_cube_make_cube(data, ao, light, 1, 1, 1, 1, 1, 1, x, y, z, n, w);
    return m_util_faces_gen(10, 6, data);
}

GLuint gen_plant_buffer(float x, float y, float z, float n, int w) {
    GLfloat *data = m_util_faces_malloc(10, 4);
    float ao = 0;
    float light = 1;
    g_cube_make_plant(data, ao, light, x, y, z, n, w, 45);
    return m_util_faces_gen(10, 4, data);
}

GLuint gen_player_buffer(float x, float y, float z, float rx, float ry) {
    GLfloat *data = m_util_faces_malloc(10, 6);
    g_cube_make_player(data, x, y, z, rx, ry);
    return m_util_faces_gen(10, 6, data);
}

GLuint gen_text_buffer(float x, float y, float n, char *text) {
    int length = strlen(text);
    GLfloat *data = m_util_faces_malloc(4, length);
    for (int i = 0; i < length; i++) {
        g_cube_make_character(data + i * 24, x, y, n / 2, n, text[i]);
        x += n;
    }
    return m_util_faces_gen(4, length, data);
}

void draw_triangles_3d_ao(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->normal);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 10, 0);
    glVertexAttribPointer(attrib->normal, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 10, (GLvoid *)(sizeof(GLfloat) * 3));
    glVertexAttribPointer(attrib->uv, 4, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 10, (GLvoid *)(sizeof(GLfloat) * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->normal);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_3d_text(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 5, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 5, (GLvoid *)(sizeof(GLfloat) * 3));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_3d(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->normal);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 8, 0);
    glVertexAttribPointer(attrib->normal, 3, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 8, (GLvoid *)(sizeof(GLfloat) * 3));
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 8, (GLvoid *)(sizeof(GLfloat) * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->normal);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_2d(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 4, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(GLfloat) * 4, (GLvoid *)(sizeof(GLfloat) * 2));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_lines(Attrib *attrib, GLuint buffer, int components, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glVertexAttribPointer(
        attrib->position, components, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_LINES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_chunk(Attrib *attrib, Chunk *chunk) {
    draw_triangles_3d_ao(attrib, chunk->buffer, chunk->faces * 6);
}

void draw_item(Attrib *attrib, GLuint buffer, int count) {
    draw_triangles_3d_ao(attrib, buffer, count);
}

void draw_text(Attrib *attrib, GLuint buffer, int length) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_triangles_2d(attrib, buffer, length * 6);
    glDisable(GL_BLEND);
}

void draw_signs(Attrib *attrib, Chunk *chunk) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-8, -1024);
    draw_triangles_3d_text(attrib, chunk->sign_buffer, chunk->sign_faces * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void draw_sign(Attrib *attrib, GLuint buffer, int length) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-8, -1024);
    draw_triangles_3d_text(attrib, buffer, length * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void draw_cube(Attrib *attrib, GLuint buffer) {
    draw_item(attrib, buffer, 36);
}

void draw_plant(Attrib *attrib, GLuint buffer) {
    draw_item(attrib, buffer, 24);
}

void draw_player(Attrib *attrib, Player *player) {
    draw_cube(attrib, player->buffer);
}

Player *find_player(int id) {
    for (int i = 0; i < g->player_count; i++) {
        Player *player = g->players + i;
        if (player->id == id) {
            return player;
        }
    }
    return 0;
}

void update_player(Player *player,
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
        player->buffer = gen_player_buffer(s->x, s->y, s->z, s->rx, s->ry);
    }
}

void delete_player(int id) {
    Player *player = find_player(id);
    if (!player) {
        return;
    }
    int count = g->player_count;
    m_util_buffer_del(player->buffer);
    Player *other = g->players + (--count);
    memcpy(player, other, sizeof(Player));
    g->player_count = count;
}

void delete_all_players() {
    for (int i = 0; i < g->player_count; i++) {
        Player *player = g->players + i;
        m_util_buffer_del(player->buffer);
    }
    g->player_count = 0;
}

float player_player_distance(Player *p1, Player *p2) {
    State *s1 = &p1->state;
    State *s2 = &p2->state;
    float x = s2->x - s1->x;
    float y = s2->y - s1->y;
    float z = s2->z - s1->z;
    return sqrtf(x * x + y * y + z * z);
}

float player_crosshair_distance(Player *p1, Player *p2) {
    State *s1 = &p1->state;
    State *s2 = &p2->state;
    float d = player_player_distance(p1, p2);
    float vx, vy, vz;
    get_sight_vector(s1->rx, s1->ry, &vx, &vy, &vz);
    vx *= d; vy *= d; vz *= d;
    float px, py, pz;
    px = s1->x + vx; py = s1->y + vy; pz = s1->z + vz;
    float x = s2->x - px;
    float y = s2->y - py;
    float z = s2->z - pz;
    return sqrtf(x * x + y * y + z * z);
}

Player *player_crosshair(Player *player) {
    Player *result = 0;
    float threshold = RADIANS(5);
    float best = 0;
    for (int i = 0; i < g->player_count; i++) {
        Player *other = g->players + i;
        if (other == player) {
            continue;
        }
        float p = player_crosshair_distance(player, other);
        float d = player_player_distance(player, other);
        if (d < 96 && p / d < threshold) {
            if (best == 0 || d < best) {
                best = d;
                result = other;
            }
        }
    }
    return result;
}

Chunk *find_chunk(int p, int q) {
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        if (chunk->p == p && chunk->q == q) {
            return chunk;
        }
    }
    return 0;
}

int chunk_distance(Chunk *chunk, int p, int q) {
    int dp = ABS(chunk->p - p);
    int dq = ABS(chunk->q - q);
    return MAX(dp, dq);
}

int chunk_visible(float planes[6][4], int p, int q, int miny, int maxy) {
    int x = p * CHUNK_SIZE - 1;
    int z = q * CHUNK_SIZE - 1;
    int d = CHUNK_SIZE + 1;
    float points[8][3] = {
        {x + 0, miny, z + 0},
        {x + d, miny, z + 0},
        {x + 0, miny, z + d},
        {x + d, miny, z + d},
        {x + 0, maxy, z + 0},
        {x + d, maxy, z + 0},
        {x + 0, maxy, z + d},
        {x + d, maxy, z + d}
    };
    int n = g->ortho ? 4 : 6;
    for (int i = 0; i < n; i++) {
        int in = 0;
        int out = 0;
        for (int j = 0; j < 8; j++) {
            float d =
                planes[i][0] * points[j][0] +
                planes[i][1] * points[j][1] +
                planes[i][2] * points[j][2] +
                planes[i][3];
            if (d < 0) {
                out++;
            }
            else {
                in++;
            }
            if (in && out) {
                break;
            }
        }
        if (in == 0) {
            return 0;
        }
    }
    return 1;
}

int highest_block(float x, float z) {
    int result = -1;
    int nx = roundf(x);
    int nz = roundf(z);
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
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

int _hit_test(
    Map *map, float max_distance, int previous,
    float x, float y, float z,
    float vx, float vy, float vz,
    int *hx, int *hy, int *hz)
{
    int m = 32;
    int px = 0;
    int py = 0;
    int pz = 0;
    for (int i = 0; i < max_distance * m; i++) {
        int nx = roundf(x);
        int ny = roundf(y);
        int nz = roundf(z);
        if (nx != px || ny != py || nz != pz) {
            int hw = p_map_get(map, nx, ny, nz);
            if (hw > 0) {
                if (previous) {
                    *hx = px; *hy = py; *hz = pz;
                }
                else {
                    *hx = nx; *hy = ny; *hz = nz;
                }
                return hw;
            }
            px = nx; py = ny; pz = nz;
        }
        x += vx / m; y += vy / m; z += vz / m;
    }
    return 0;
}

int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz)
{
    int result = 0;
    float best = 0;
    int p = chunked(x);
    int q = chunked(z);
    float vx, vy, vz;
    get_sight_vector(rx, ry, &vx, &vy, &vz);
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        if (chunk_distance(chunk, p, q) > 1) {
            continue;
        }
        int hx, hy, hz;
        int hw = _hit_test(&chunk->map, 8, previous,
            x, y, z, vx, vy, vz, &hx, &hy, &hz);
        if (hw > 0) {
            float d = sqrtf(
                powf(hx - x, 2) + powf(hy - y, 2) + powf(hz - z, 2));
            if (best == 0 || d < best) {
                best = d;
                *bx = hx; *by = hy; *bz = hz;
                result = hw;
            }
        }
    }
    return result;
}

int hit_test_face(Player *player, int *x, int *y, int *z, int *face) {
    State *s = &player->state;
    int w = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, x, y, z);
    if (g_item_is_obstacle(w)) {
        int hx, hy, hz;
        hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
        int dx = hx - *x;
        int dy = hy - *y;
        int dz = hz - *z;
        if (dx == -1 && dy == 0 && dz == 0) {
            *face = 0; return 1;
        }
        if (dx == 1 && dy == 0 && dz == 0) {
            *face = 1; return 1;
        }
        if (dx == 0 && dy == 0 && dz == -1) {
            *face = 2; return 1;
        }
        if (dx == 0 && dy == 0 && dz == 1) {
            *face = 3; return 1;
        }
        if (dx == 0 && dy == 1 && dz == 0) {
            int degrees = roundf(DEGREES(atan2f(s->x - hx, s->z - hz)));
            if (degrees < 0) {
                degrees += 360;
            }
            int top = ((degrees + 45) / 90) % 4;
            *face = 4 + top; return 1;
        }
    }
    return 0;
}

int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz)
{
    int nx = roundf(x);
    int ny = roundf(y);
    int nz = roundf(z);
    for (int i = 0; i < height; i++) {
        if (nx == hx && ny - i == hy && nz == hz) {
            return 1;
        }
    }
    return 0;
}

int _gen_sign_buffer(
    GLfloat *data, float x, float y, float z, int face, const char *text)
{
    static const int glyph_dx[8] = {0, 0, -1, 1, 1, 0, -1, 0};
    static const int glyph_dz[8] = {1, -1, 0, 0, 0, -1, 0, 1};
    static const int line_dx[8] = {0, 0, 0, 0, 0, 1, 0, -1};
    static const int line_dy[8] = {-1, -1, -1, -1, 0, 0, 0, 0};
    static const int line_dz[8] = {0, 0, 0, 0, 1, 0, -1, 0};
    if (face < 0 || face >= 8) {
        return 0;
    }
    int count = 0;
    float max_width = 64;
    float line_height = 1.25;
    char lines[1024];
    int rows = m_util_wrap(text, max_width, lines, 1024);
    rows = MIN(rows, 5);
    int dx = glyph_dx[face];
    int dz = glyph_dz[face];
    int ldx = line_dx[face];
    int ldy = line_dy[face];
    int ldz = line_dz[face];
    float n = 1.0 / (max_width / 10);
    float sx = x - n * (rows - 1) * (line_height / 2) * ldx;
    float sy = y - n * (rows - 1) * (line_height / 2) * ldy;
    float sz = z - n * (rows - 1) * (line_height / 2) * ldz;
    char *key;
    char *line = m_util_tokenize(lines, "\n", &key);
    while (line) {
        int length = strlen(line);
        int line_width = m_util_string_width(line);
        line_width = MIN(line_width, max_width);
        float rx = sx - dx * line_width / max_width / 2;
        float ry = sy;
        float rz = sz - dz * line_width / max_width / 2;
        for (int i = 0; i < length; i++) {
            int width = m_util_char_width(line[i]);
            line_width -= width;
            if (line_width < 0) {
                break;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
            if (line[i] != ' ') {
                g_cube_make_character_3d(
                    data + count * 30, rx, ry, rz, n / 2, face, line[i]);
                count++;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
        }
        sx += n * line_height * ldx;
        sy += n * line_height * ldy;
        sz += n * line_height * ldz;
        line = m_util_tokenize(NULL, "\n", &key);
        rows--;
        if (rows <= 0) {
            break;
        }
    }
    return count;
}

void gen_sign_buffer(Chunk *chunk) {
    SignList *signs = &chunk->signs;

    // first pass - count characters
    int max_faces = 0;
    for (int i = 0; i < signs->size; i++) {
        Sign *e = signs->data + i;
        max_faces += strlen(e->text);
    }

    // second pass - generate geometry
    GLfloat *data = m_util_faces_malloc(5, max_faces);
    int faces = 0;
    for (int i = 0; i < signs->size; i++) {
        Sign *e = signs->data + i;
        faces += _gen_sign_buffer(
            data + faces * 30, e->x, e->y, e->z, e->face, e->text);
    }

    m_util_buffer_del(chunk->sign_buffer);
    chunk->sign_buffer = m_util_faces_gen(5, faces, data);
    chunk->sign_faces = faces;
}

int has_lights(Chunk *chunk) {
    if (!SHOW_LIGHTS) {
        return 0;
    }
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->p + dp, chunk->q + dq);
            }
            if (!other) {
                continue;
            }
            Map *map = &other->lights;
            if (map->size) {
                return 1;
            }
        }
    }
    return 0;
}

void dirty_chunk(Chunk *chunk) {
    chunk->dirty = 1;
    if (has_lights(chunk)) {
        for (int dp = -1; dp <= 1; dp++) {
            for (int dq = -1; dq <= 1; dq++) {
                Chunk *other = find_chunk(chunk->p + dp, chunk->q + dq);
                if (other) {
                    other->dirty = 1;
                }
            }
        }
    }
}

void generate_chunk(Chunk *chunk, WorkerItem *item) {
    chunk->miny = item->miny;
    chunk->maxy = item->maxy;
    chunk->faces = item->faces;
    m_util_buffer_del(chunk->buffer);
    chunk->buffer = m_util_faces_gen(10, item->faces, item->data);
    gen_sign_buffer(chunk);
}

void gen_chunk_buffer(Chunk *chunk) {
    WorkerItem _item;
    WorkerItem *item = &_item;
    item->p = chunk->p;
    item->q = chunk->q;
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->p + dp, chunk->q + dq);
            }
            if (other) {
                item->block_maps[dp + 1][dq + 1] = &other->map;
                item->light_maps[dp + 1][dq + 1] = &other->lights;
            }
            else {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    m_game_compute_chunk(item);
    generate_chunk(chunk, item);
    chunk->dirty = 0;
}

void p_map_set_func(int x, int y, int z, int w, void *arg) {
    Map *map = (Map *)arg;
    p_map_set(map, x, y, z, w);
}


void request_chunk(int p, int q) {
    int key = d_db_get_key(p, q);
    n_client_chunk(p, q, key);
}

void init_chunk(Chunk *chunk, int p, int q) {
    chunk->p = p;
    chunk->q = q;
    chunk->faces = 0;
    chunk->sign_faces = 0;
    chunk->buffer = 0;
    chunk->sign_buffer = 0;
    dirty_chunk(chunk);
    SignList *signs = &chunk->signs;
    g_sign_list_alloc(signs, 16);
    d_db_load_signs(signs, p, q);
    Map *block_map = &chunk->map;
    Map *light_map = &chunk->lights;
    int dx = p * CHUNK_SIZE - 1;
    int dy = 0;
    int dz = q * CHUNK_SIZE - 1;
    p_map_alloc(block_map, dx, dy, dz, 0x7fff);
    p_map_alloc(light_map, dx, dy, dz, 0xf);
}

void create_chunk(Chunk *chunk, int p, int q) {
    init_chunk(chunk, p, q);

    WorkerItem _item;
    WorkerItem *item = &_item;
    item->p = chunk->p;
    item->q = chunk->q;
    item->block_maps[1][1] = &chunk->map;
    item->light_maps[1][1] = &chunk->lights;
    m_game_load_chunk(item);

    request_chunk(p, q);
}

void delete_chunks() {
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
            int p = chunked(s->x);
            int q = chunked(s->z);
            if (chunk_distance(chunk, p, q) < g->delete_radius) {
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

void delete_all_chunks() {
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        p_map_free(&chunk->map);
        p_map_free(&chunk->lights);
        g_sign_list_free(&chunk->signs);
        m_util_buffer_del(chunk->buffer);
        m_util_buffer_del(chunk->sign_buffer);
    }
    g->chunk_count = 0;
}

void check_workers() {
    for (int i = 0; i < WORKERS; i++) {
        Worker *worker = g->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_DONE) {
            WorkerItem *item = &worker->item;
            Chunk *chunk = find_chunk(item->p, item->q);
            if (chunk) {
                if (item->load) {
                    Map *block_map = item->block_maps[1][1];
                    Map *light_map = item->light_maps[1][1];
                    p_map_free(&chunk->map);
                    p_map_free(&chunk->lights);
                    p_map_copy(&chunk->map, block_map);
                    p_map_copy(&chunk->lights, light_map);
                    request_chunk(item->p, item->q);
                }
                generate_chunk(chunk, item);
            }
            for (int a = 0; a < 3; a++) {
                for (int b = 0; b < 3; b++) {
                    Map *block_map = item->block_maps[a][b];
                    Map *light_map = item->light_maps[a][b];
                    if (block_map) {
                        p_map_free(block_map);
                        free(block_map);
                    }
                    if (light_map) {
                        p_map_free(light_map);
                        free(light_map);
                    }
                }
            }
            worker->state = WORKER_IDLE;
        }
        mtx_unlock(&worker->mtx);
    }
}

void force_chunks(Player *player) {
    State *s = &player->state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    int r = 1;
    for (int dp = -r; dp <= r; dp++) {
        for (int dq = -r; dq <= r; dq++) {
            int a = p + dp;
            int b = q + dq;
            Chunk *chunk = find_chunk(a, b);
            if (chunk) {
                if (chunk->dirty) {
                    gen_chunk_buffer(chunk);
                }
            }
            else if (g->chunk_count < MAX_CHUNKS) {
                chunk = g->chunks + g->chunk_count++;
                create_chunk(chunk, a, b);
                gen_chunk_buffer(chunk);
            }
        }
    }
}

void ensure_chunks_worker(Player *player, Worker *worker) {
    State *s = &player->state;
    float matrix[16];
    g_mat_set_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    float planes[6][4];
    g_mat_frustum_planes(planes, g->render_radius, matrix);
    int p = chunked(s->x);
    int q = chunked(s->z);
    int r = g->create_radius;
    int start = 0x0fffffff;
    int best_score = start;
    int best_a = 0;
    int best_b = 0;
    for (int dp = -r; dp <= r; dp++) {
        for (int dq = -r; dq <= r; dq++) {
            int a = p + dp;
            int b = q + dq;
            int index = (ABS(a) ^ ABS(b)) % WORKERS;
            if (index != worker->index) {
                continue;
            }
            Chunk *chunk = find_chunk(a, b);
            if (chunk && !chunk->dirty) {
                continue;
            }
            int distance = MAX(ABS(dp), ABS(dq));
            int invisible = !chunk_visible(planes, a, b, 0, 256);
            int priority = 0;
            if (chunk) {
                priority = chunk->buffer && chunk->dirty;
            }
            int score = (invisible << 24) | (priority << 16) | distance;
            if (score < best_score) {
                best_score = score;
                best_a = a;
                best_b = b;
            }
        }
    }
    if (best_score == start) {
        return;
    }
    int a = best_a;
    int b = best_b;
    int load = 0;
    Chunk *chunk = find_chunk(a, b);
    if (!chunk) {
        load = 1;
        if (g->chunk_count < MAX_CHUNKS) {
            chunk = g->chunks + g->chunk_count++;
            init_chunk(chunk, a, b);
        }
        else {
            return;
        }
    }
    WorkerItem *item = &worker->item;
    item->p = chunk->p;
    item->q = chunk->q;
    item->load = load;
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->p + dp, chunk->q + dq);
            }
            if (other) {
                Map *block_map = malloc(sizeof(Map));
                p_map_copy(block_map, &other->map);
                Map *light_map = malloc(sizeof(Map));
                p_map_copy(light_map, &other->lights);
                item->block_maps[dp + 1][dq + 1] = block_map;
                item->light_maps[dp + 1][dq + 1] = light_map;
            }
            else {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    chunk->dirty = 0;
    worker->state = WORKER_BUSY;
    cnd_signal(&worker->cnd);
}

void ensure_chunks(Player *player) {
    check_workers();
    force_chunks(player);
    for (int i = 0; i < WORKERS; i++) {
        Worker *worker = g->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_IDLE) {
            ensure_chunks_worker(player, worker);
        }
        mtx_unlock(&worker->mtx);
    }
}

void unset_sign(int x, int y, int z) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        if (g_sign_list_remove_all(signs, x, y, z)) {
            chunk->dirty = 1;
            d_db_delete_signs(x, y, z);
        }
    }
    else {
        d_db_delete_signs(x, y, z);
    }
}

void unset_sign_face(int x, int y, int z, int face) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        if (g_sign_list_remove(signs, x, y, z, face)) {
            chunk->dirty = 1;
            d_db_delete_sign(x, y, z, face);
        }
    }
    else {
        d_db_delete_sign(x, y, z, face);
    }
}

void _set_sign(
    int p, int q, int x, int y, int z, int face, const char *text, int dirty)
{
    if (strlen(text) == 0) {
        unset_sign_face(x, y, z, face);
        return;
    }
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        g_sign_list_add(signs, x, y, z, face, text);
        if (dirty) {
            chunk->dirty = 1;
        }
    }
    d_db_insert_sign(p, q, x, y, z, face, text);
}

void set_sign(int x, int y, int z, int face, const char *text) {
    int p = chunked(x);
    int q = chunked(z);
    _set_sign(p, q, x, y, z, face, text, 1);
    n_client_sign(x, y, z, face, text);
}

void toggle_light(int x, int y, int z) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->lights;
        int w = p_map_get(map, x, y, z) ? 0 : 15;
        p_map_set(map, x, y, z, w);
        d_db_insert_light(p, q, x, y, z, w);
        n_client_light(x, y, z, w);
        dirty_chunk(chunk);
    }
}

void set_light(int p, int q, int x, int y, int z, int w) {
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->lights;
        if (p_map_set(map, x, y, z, w)) {
            dirty_chunk(chunk);
            d_db_insert_light(p, q, x, y, z, w);
        }
    }
    else {
        d_db_insert_light(p, q, x, y, z, w);
    }
}

void _set_block(int p, int q, int x, int y, int z, int w, int dirty) {
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        if (p_map_set(map, x, y, z, w)) {
            if (dirty) {
                dirty_chunk(chunk);
            }
            d_db_insert_block(p, q, x, y, z, w);
        }
    }
    else {
        d_db_insert_block(p, q, x, y, z, w);
    }
    if (w == 0 && chunked(x) == p && chunked(z) == q) {
        unset_sign(x, y, z);
        set_light(p, q, x, y, z, 0);
    }
}

void set_block(int x, int y, int z, int w) {
    int p = chunked(x);
    int q = chunked(z);
    _set_block(p, q, x, y, z, w, 1);
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            if (dx == 0 && dz == 0) {
                continue;
            }
            if (dx && chunked(x + dx) == p) {
                continue;
            }
            if (dz && chunked(z + dz) == q) {
                continue;
            }
            _set_block(p + dx, q + dz, x, y, z, -w, 1);
        }
    }
    n_client_block(x, y, z, w);
}

void record_block(int x, int y, int z, int w) {
    memcpy(&g->block1, &g->block0, sizeof(Block));
    g->block0.x = x;
    g->block0.y = y;
    g->block0.z = z;
    g->block0.w = w;
}

int get_block(int x, int y, int z) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        return p_map_get(map, x, y, z);
    }
    return 0;
}

void builder_block(int x, int y, int z, int w) {
    if (y <= 0 || y >= 256) {
        return;
    }
    if (g_item_is_destructable(get_block(x, y, z))) {
        set_block(x, y, z, 0);
    }
    if (w) {
        set_block(x, y, z, w);
    }
}

int render_chunks(Attrib *attrib, Player *player) {
    int result = 0;
    State *s = &player->state;
    ensure_chunks(player);
    int p = chunked(s->x);
    int q = chunked(s->z);
    float light = get_daylight();
    float matrix[16];
    g_mat_set_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    float planes[6][4];
    g_mat_frustum_planes(planes, g->render_radius, matrix);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, s->x, s->y, s->z);
    glUniform1i(attrib->sampler, 0);
    glUniform1i(attrib->extra1, 2);
    glUniform1f(attrib->extra2, light);
    glUniform1f(attrib->extra3, g->render_radius * CHUNK_SIZE);
    glUniform1i(attrib->extra4, g->ortho);
    glUniform1f(attrib->timer, time_of_day());
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        if (chunk_distance(chunk, p, q) > g->render_radius) {
            continue;
        }
        if (!chunk_visible(
            planes, chunk->p, chunk->q, chunk->miny, chunk->maxy))
        {
            continue;
        }
        draw_chunk(attrib, chunk);
        result += chunk->faces;
    }
    return result;
}

void render_signs(Attrib *attrib, Player *player) {
    State *s = &player->state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    float matrix[16];
    g_mat_set_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    float planes[6][4];
    g_mat_frustum_planes(planes, g->render_radius, matrix);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 3);
    glUniform1i(attrib->extra1, 1);
    for (int i = 0; i < g->chunk_count; i++) {
        Chunk *chunk = g->chunks + i;
        if (chunk_distance(chunk, p, q) > g->sign_radius) {
            continue;
        }
        if (!chunk_visible(
            planes, chunk->p, chunk->q, chunk->miny, chunk->maxy))
        {
            continue;
        }
        draw_signs(attrib, chunk);
    }
}

void render_sign(Attrib *attrib, Player *player) {
    if (!g->typing || g->typing_buffer[0] != CRAFT_KEY_SIGN) {
        return;
    }
    int x, y, z, face;
    if (!hit_test_face(player, &x, &y, &z, &face)) {
        return;
    }
    State *s = &player->state;
    float matrix[16];
    g_mat_set_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 3);
    glUniform1i(attrib->extra1, 1);
    char text[MAX_SIGN_LENGTH];
    strncpy(text, g->typing_buffer + 1, MAX_SIGN_LENGTH);
    text[MAX_SIGN_LENGTH - 1] = '\0';
    GLfloat *data = m_util_faces_malloc(5, strlen(text));
    int length = _gen_sign_buffer(data, x, y, z, face, text);
    GLuint buffer = m_util_faces_gen(5, length, data);
    draw_sign(attrib, buffer, length);
    m_util_buffer_del(buffer);
}

void render_players(Attrib *attrib, Player *player) {
    State *s = &player->state;
    float matrix[16];
    g_mat_set_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, s->x, s->y, s->z);
    glUniform1i(attrib->sampler, 0);
    glUniform1f(attrib->timer, time_of_day());
    for (int i = 0; i < g->player_count; i++) {
        Player *other = g->players + i;
        if (other != player) {
            draw_player(attrib, other);
        }
    }
}

void render_sky(Attrib *attrib, Player *player, GLuint buffer) {
    State *s = &player->state;
    float matrix[16];
    g_mat_set_3d(
        matrix, g->width, g->height,
        0, 0, 0, s->rx, s->ry, g->fov, 0, g->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 2);
    glUniform1f(attrib->timer, time_of_day());
    draw_triangles_3d(attrib, buffer, 512 * 3);
}

void render_wireframe(Attrib *attrib, Player *player) {
    State *s = &player->state;
    float matrix[16];
    g_mat_set_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (g_item_is_obstacle(hw)) {
        glUseProgram(attrib->program);
        glLineWidth(1);
        glEnable(GL_COLOR_LOGIC_OP);
        glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
        GLuint wireframe_buffer = gen_wireframe_buffer(hx, hy, hz, 0.53);
        draw_lines(attrib, wireframe_buffer, 3, 24);
        m_util_buffer_del(wireframe_buffer);
        glDisable(GL_COLOR_LOGIC_OP);
    }
}

void render_crosshairs(Attrib *attrib) {
    float matrix[16];
    g_mat_set_2d(matrix, g->width, g->height);
    glUseProgram(attrib->program);
    glLineWidth(4 * g->scale);
    glEnable(GL_COLOR_LOGIC_OP);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    GLuint crosshair_buffer = gen_crosshair_buffer();
    draw_lines(attrib, crosshair_buffer, 2, 4);
    m_util_buffer_del(crosshair_buffer);
    glDisable(GL_COLOR_LOGIC_OP);
}

void render_item(Attrib *attrib) {
    float matrix[16];
    g_mat_matrix_set_item(matrix, g->width, g->height, g->scale);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, 0, 0, 5);
    glUniform1i(attrib->sampler, 0);
    glUniform1f(attrib->timer, time_of_day());
    int w = items[g->item_index];
    if (g_item_is_plant(w)) {
        GLuint buffer = gen_plant_buffer(0, 0, 0, 0.5, w);
        draw_plant(attrib, buffer);
        m_util_buffer_del(buffer);
    }
    else {
        GLuint buffer = gen_cube_buffer(0, 0, 0, 0.5, w);
        draw_cube(attrib, buffer);
        m_util_buffer_del(buffer);
    }
}

void render_text(
    Attrib *attrib, int justify, float x, float y, float n, char *text)
{
    float matrix[16];
    g_mat_set_2d(matrix, g->width, g->height);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 1);
    glUniform1i(attrib->extra1, 0);
    int length = strlen(text);
    x -= n * justify * (length - 1) / 2;
    GLuint buffer = gen_text_buffer(x, y, n, text);
    draw_text(attrib, buffer, length);
    m_util_buffer_del(buffer);
}

void add_message(const char *text) {
    printf("%s\n", text);
    snprintf(
        g->messages[g->message_index], MAX_TEXT_LENGTH, "%s", text);
    g->message_index = (g->message_index + 1) % MAX_MESSAGES;
}

void copy() {
    memcpy(&g->copy0, &g->block0, sizeof(Block));
    memcpy(&g->copy1, &g->block1, sizeof(Block));
}

void paste() {
    Block *c1 = &g->copy1;
    Block *c2 = &g->copy0;
    Block *p1 = &g->block1;
    Block *p2 = &g->block0;
    int scx = SIGN(c2->x - c1->x);
    int scz = SIGN(c2->z - c1->z);
    int spx = SIGN(p2->x - p1->x);
    int spz = SIGN(p2->z - p1->z);
    int oy = p1->y - c1->y;
    int dx = ABS(c2->x - c1->x);
    int dz = ABS(c2->z - c1->z);
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x <= dx; x++) {
            for (int z = 0; z <= dz; z++) {
                int w = get_block(c1->x + x * scx, y, c1->z + z * scz);
                builder_block(p1->x + x * spx, y + oy, p1->z + z * spz, w);
            }
        }
    }
}

void array(Block *b1, Block *b2, int xc, int yc, int zc) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int dx = b2->x - b1->x;
    int dy = b2->y - b1->y;
    int dz = b2->z - b1->z;
    xc = dx ? xc : 1;
    yc = dy ? yc : 1;
    zc = dz ? zc : 1;
    for (int i = 0; i < xc; i++) {
        int x = b1->x + dx * i;
        for (int j = 0; j < yc; j++) {
            int y = b1->y + dy * j;
            for (int k = 0; k < zc; k++) {
                int z = b1->z + dz * k;
                builder_block(x, y, z, w);
            }
        }
    }
}

void cube(Block *b1, Block *b2, int fill) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int x1 = MIN(b1->x, b2->x);
    int y1 = MIN(b1->y, b2->y);
    int z1 = MIN(b1->z, b2->z);
    int x2 = MAX(b1->x, b2->x);
    int y2 = MAX(b1->y, b2->y);
    int z2 = MAX(b1->z, b2->z);
    int a = (x1 == x2) + (y1 == y2) + (z1 == z2);
    for (int x = x1; x <= x2; x++) {
        for (int y = y1; y <= y2; y++) {
            for (int z = z1; z <= z2; z++) {
                if (!fill) {
                    int n = 0;
                    n += x == x1 || x == x2;
                    n += y == y1 || y == y2;
                    n += z == z1 || z == z2;
                    if (n <= a) {
                        continue;
                    }
                }
                builder_block(x, y, z, w);
            }
        }
    }
}

void sphere(Block *center, int radius, int fill, int fx, int fy, int fz) {
    static const float offsets[8][3] = {
        {-0.5, -0.5, -0.5},
        {-0.5, -0.5, 0.5},
        {-0.5, 0.5, -0.5},
        {-0.5, 0.5, 0.5},
        {0.5, -0.5, -0.5},
        {0.5, -0.5, 0.5},
        {0.5, 0.5, -0.5},
        {0.5, 0.5, 0.5}
    };
    int cx = center->x;
    int cy = center->y;
    int cz = center->z;
    int w = center->w;
    for (int x = cx - radius; x <= cx + radius; x++) {
        if (fx && x != cx) {
            continue;
        }
        for (int y = cy - radius; y <= cy + radius; y++) {
            if (fy && y != cy) {
                continue;
            }
            for (int z = cz - radius; z <= cz + radius; z++) {
                if (fz && z != cz) {
                    continue;
                }
                int inside = 0;
                int outside = fill;
                for (int i = 0; i < 8; i++) {
                    float dx = x + offsets[i][0] - cx;
                    float dy = y + offsets[i][1] - cy;
                    float dz = z + offsets[i][2] - cz;
                    float d = sqrtf(dx * dx + dy * dy + dz * dz);
                    if (d < radius) {
                        inside = 1;
                    }
                    else {
                        outside = 1;
                    }
                }
                if (inside && outside) {
                    builder_block(x, y, z, w);
                }
            }
        }
    }
}

void cylinder(Block *b1, Block *b2, int radius, int fill) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int x1 = MIN(b1->x, b2->x);
    int y1 = MIN(b1->y, b2->y);
    int z1 = MIN(b1->z, b2->z);
    int x2 = MAX(b1->x, b2->x);
    int y2 = MAX(b1->y, b2->y);
    int z2 = MAX(b1->z, b2->z);
    int fx = x1 != x2;
    int fy = y1 != y2;
    int fz = z1 != z2;
    if (fx + fy + fz != 1) {
        return;
    }
    Block block = {x1, y1, z1, w};
    if (fx) {
        for (int x = x1; x <= x2; x++) {
            block.x = x;
            sphere(&block, radius, fill, 1, 0, 0);
        }
    }
    if (fy) {
        for (int y = y1; y <= y2; y++) {
            block.y = y;
            sphere(&block, radius, fill, 0, 1, 0);
        }
    }
    if (fz) {
        for (int z = z1; z <= z2; z++) {
            block.z = z;
            sphere(&block, radius, fill, 0, 0, 1);
        }
    }
}

void tree(Block *block) {
    int bx = block->x;
    int by = block->y;
    int bz = block->z;
    for (int y = by + 3; y < by + 8; y++) {
        for (int dx = -3; dx <= 3; dx++) {
            for (int dz = -3; dz <= 3; dz++) {
                int dy = y - (by + 4);
                int d = (dx * dx) + (dy * dy) + (dz * dz);
                if (d < 11) {
                    builder_block(bx + dx, y, bz + dz, 15);
                }
            }
        }
    }
    for (int y = by; y < by + 7; y++) {
        builder_block(bx, y, bz, 5);
    }
}

void parse_command(const char *buffer, int forward) {
    char username[128] = {0};
    char token[128] = {0};
    char server_addr[MAX_ADDR_LENGTH];
    int server_port = DEFAULT_PORT;
    char filename[MAX_PATH_LENGTH];
    int radius, count, xc, yc, zc;
    if (sscanf(buffer, "/identity %128s %128s", username, token) == 2) {
        d_db_auth_set(username, token);
        add_message("Successfully imported identity token!");
        n_auth_login();
    }
    else if (strcmp(buffer, "/logout") == 0) {
        d_db_auth_select_none();
        n_auth_login();
    }
    else if (sscanf(buffer, "/login %128s", username) == 1) {
        if (d_db_auth_select(username)) {
            n_auth_login();
        }
        else {
            add_message("Unknown username.");
        }
    }
    else if (sscanf(buffer,
        "/online %128s %d", server_addr, &server_port) >= 1)
    {
        g->mode_changed = 1;
        g->mode = MODE_ONLINE;
        strncpy(g->server_addr, server_addr, MAX_ADDR_LENGTH);
        g->server_port = server_port;
        snprintf(g->d_db_path, MAX_PATH_LENGTH,
            "cache.%s.%d.db", g->server_addr, g->server_port);
    }
    else if (sscanf(buffer, "/offline %128s", filename) == 1) {
        g->mode_changed = 1;
        g->mode = MODE_OFFLINE;
        snprintf(g->d_db_path, MAX_PATH_LENGTH, "%s.db", filename);
    }
    else if (strcmp(buffer, "/offline") == 0) {
        g->mode_changed = 1;
        g->mode = MODE_OFFLINE;
        snprintf(g->d_db_path, MAX_PATH_LENGTH, "%s", DB_PATH);
    }
    else if (sscanf(buffer, "/view %d", &radius) == 1) {
        if (radius >= 1 && radius <= 24) {
            g->create_radius = radius;
            g->render_radius = radius;
            g->delete_radius = radius + 4;
        }
        else {
            add_message("Viewing distance must be between 1 and 24.");
        }
    }
    else if (strcmp(buffer, "/copy") == 0) {
        copy();
    }
    else if (strcmp(buffer, "/paste") == 0) {
        paste();
    }
    else if (strcmp(buffer, "/tree") == 0) {
        tree(&g->block0);
    }
    else if (sscanf(buffer, "/array %d %d %d", &xc, &yc, &zc) == 3) {
        array(&g->block1, &g->block0, xc, yc, zc);
    }
    else if (sscanf(buffer, "/array %d", &count) == 1) {
        array(&g->block1, &g->block0, count, count, count);
    }
    else if (strcmp(buffer, "/fcube") == 0) {
        cube(&g->block0, &g->block1, 1);
    }
    else if (strcmp(buffer, "/cube") == 0) {
        cube(&g->block0, &g->block1, 0);
    }
    else if (sscanf(buffer, "/fsphere %d", &radius) == 1) {
        sphere(&g->block0, radius, 1, 0, 0, 0);
    }
    else if (sscanf(buffer, "/sphere %d", &radius) == 1) {
        sphere(&g->block0, radius, 0, 0, 0, 0);
    }
    else if (sscanf(buffer, "/fcirclex %d", &radius) == 1) {
        sphere(&g->block0, radius, 1, 1, 0, 0);
    }
    else if (sscanf(buffer, "/circlex %d", &radius) == 1) {
        sphere(&g->block0, radius, 0, 1, 0, 0);
    }
    else if (sscanf(buffer, "/fcircley %d", &radius) == 1) {
        sphere(&g->block0, radius, 1, 0, 1, 0);
    }
    else if (sscanf(buffer, "/circley %d", &radius) == 1) {
        sphere(&g->block0, radius, 0, 0, 1, 0);
    }
    else if (sscanf(buffer, "/fcirclez %d", &radius) == 1) {
        sphere(&g->block0, radius, 1, 0, 0, 1);
    }
    else if (sscanf(buffer, "/circlez %d", &radius) == 1) {
        sphere(&g->block0, radius, 0, 0, 0, 1);
    }
    else if (sscanf(buffer, "/fcylinder %d", &radius) == 1) {
        cylinder(&g->block0, &g->block1, radius, 1);
    }
    else if (sscanf(buffer, "/cylinder %d", &radius) == 1) {
        cylinder(&g->block0, &g->block1, radius, 0);
    }
    else if (forward) {
        n_client_talk(buffer);
    }
}

void on_light() {
    State *s = &g->players->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && g_item_is_destructable(hw)) {
        toggle_light(hx, hy, hz);
    }
}

void on_left_click() {
    State *s = &g->players->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && g_item_is_destructable(hw)) {
        set_block(hx, hy, hz, 0);
        record_block(hx, hy, hz, 0);
        if (g_item_is_plant(get_block(hx, hy + 1, hz))) {
            set_block(hx, hy + 1, hz, 0);
        }
    }
}

void on_right_click() {
    State *s = &g->players->state;
    int hx, hy, hz;
    int hw = hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && g_item_is_obstacle(hw)) {
        if (!player_intersects_block(2, s->x, s->y, s->z, hx, hy, hz)) {
            set_block(hx, hy, hz, items[g->item_index]);
            record_block(hx, hy, hz, items[g->item_index]);
        }
    }
}

void on_middle_click() {
    State *s = &g->players->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    for (int i = 0; i < item_count; i++) {
        if (items[i] == hw) {
            g->item_index = i;
            break;
        }
    }
}

void on_key(GLFWwindow *window, int key, int scancode, int action, int mods) {
    int control = mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER);
    int exclusive =
        glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    if (action == GLFW_RELEASE) {
        return;
    }
    if (key == GLFW_KEY_BACKSPACE) {
        if (g->typing) {
            int n = strlen(g->typing_buffer);
            if (n > 0) {
                g->typing_buffer[n - 1] = '\0';
            }
        }
    }
    if (action != GLFW_PRESS) {
        return;
    }
    if (key == GLFW_KEY_ESCAPE) {
        if (g->typing) {
            g->typing = 0;
        }
        else if (exclusive) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    if (key == GLFW_KEY_ENTER) {
        if (g->typing) {
            if (mods & GLFW_MOD_SHIFT) {
                int n = strlen(g->typing_buffer);
                if (n < MAX_TEXT_LENGTH - 1) {
                    g->typing_buffer[n] = '\r';
                    g->typing_buffer[n + 1] = '\0';
                }
            }
            else {
                g->typing = 0;
                if (g->typing_buffer[0] == CRAFT_KEY_SIGN) {
                    Player *player = g->players;
                    int x, y, z, face;
                    if (hit_test_face(player, &x, &y, &z, &face)) {
                        set_sign(x, y, z, face, g->typing_buffer + 1);
                    }
                }
                else if (g->typing_buffer[0] == '/') {
                    parse_command(g->typing_buffer, 1);
                }
                else {
                    n_client_talk(g->typing_buffer);
                }
            }
        }
        else {
            if (control) {
                on_right_click();
            }
            else {
                on_left_click();
            }
        }
    }
    if (control && key == 'V') {
        const char *buffer = glfwGetClipboardString(window);
        if (g->typing) {
            g->suppress_char = 1;
            strncat(g->typing_buffer, buffer,
                MAX_TEXT_LENGTH - strlen(g->typing_buffer) - 1);
        }
        else {
            parse_command(buffer, 0);
        }
    }
    if (!g->typing) {
        if (key == CRAFT_KEY_FLY) {
            g->flying = !g->flying;
        }
        if (key >= '1' && key <= '9') {
            g->item_index = key - '1';
        }
        if (key == '0') {
            g->item_index = 9;
        }
        if (key == CRAFT_KEY_ITEM_NEXT) {
            g->item_index = (g->item_index + 1) % item_count;
        }
        if (key == CRAFT_KEY_ITEM_PREV) {
            g->item_index--;
            if (g->item_index < 0) {
                g->item_index = item_count - 1;
            }
        }
        if (key == CRAFT_KEY_OBSERVE) {
            g->observe1 = (g->observe1 + 1) % g->player_count;
        }
        if (key == CRAFT_KEY_OBSERVE_INSET) {
            g->observe2 = (g->observe2 + 1) % g->player_count;
        }
    }
}

void on_char(GLFWwindow *window, unsigned int u) {
    if (g->suppress_char) {
        g->suppress_char = 0;
        return;
    }
    if (g->typing) {
        if (u >= 32 && u < 128) {
            char c = (char)u;
            int n = strlen(g->typing_buffer);
            if (n < MAX_TEXT_LENGTH - 1) {
                g->typing_buffer[n] = c;
                g->typing_buffer[n + 1] = '\0';
            }
        }
    }
    else {
        if (u == CRAFT_KEY_CHAT) {
            g->typing = 1;
            g->typing_buffer[0] = '\0';
        }
        if (u == CRAFT_KEY_COMMAND) {
            g->typing = 1;
            g->typing_buffer[0] = '/';
            g->typing_buffer[1] = '\0';
        }
        if (u == CRAFT_KEY_SIGN) {
            g->typing = 1;
            g->typing_buffer[0] = CRAFT_KEY_SIGN;
            g->typing_buffer[1] = '\0';
        }
    }
}

void on_scroll(GLFWwindow *window, double xdelta, double ydelta) {
    static double ypos = 0;
    ypos += ydelta;
    if (ypos < -SCROLL_THRESHOLD) {
        g->item_index = (g->item_index + 1) % item_count;
        ypos = 0;
    }
    if (ypos > SCROLL_THRESHOLD) {
        g->item_index--;
        if (g->item_index < 0) {
            g->item_index = item_count - 1;
        }
        ypos = 0;
    }
}

void on_mouse_button(GLFWwindow *window, int button, int action, int mods) {
    int control = mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER);
    int exclusive =
        glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    if (action != GLFW_PRESS) {
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (exclusive) {
            if (control) {
                on_right_click();
            }
            else {
                on_left_click();
            }
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (exclusive) {
            if (control) {
                on_light();
            }
            else {
                on_right_click();
            }
        }
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (exclusive) {
            on_middle_click();
        }
    }
}

void parse_buffer(char *buffer) {
    Player *me = g->players;
    State *s = &g->players->state;
    char *key;
    char *line = m_util_tokenize(buffer, "\n", &key);
    while (line) {
        int pid;
        float ux, uy, uz, urx, ury;
        if (sscanf(line, "U,%d,%f,%f,%f,%f,%f",
            &pid, &ux, &uy, &uz, &urx, &ury) == 6)
        {
            me->id = pid;
            s->x = ux; s->y = uy; s->z = uz; s->rx = urx; s->ry = ury;
            force_chunks(me);
            if (uy == 0) {
                s->y = highest_block(s->x, s->z) + 2;
            }
        }
        int bp, bq, bx, by, bz, bw;
        if (sscanf(line, "B,%d,%d,%d,%d,%d,%d",
            &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            _set_block(bp, bq, bx, by, bz, bw, 0);
            if (player_intersects_block(2, s->x, s->y, s->z, bx, by, bz)) {
                s->y = highest_block(s->x, s->z) + 2;
            }
        }
        if (sscanf(line, "L,%d,%d,%d,%d,%d,%d",
            &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            set_light(bp, bq, bx, by, bz, bw);
        }
        float px, py, pz, prx, pry;
        if (sscanf(line, "P,%d,%f,%f,%f,%f,%f",
            &pid, &px, &py, &pz, &prx, &pry) == 6)
        {
            Player *player = find_player(pid);
            if (!player && g->player_count < MAX_PLAYERS) {
                player = g->players + g->player_count;
                g->player_count++;
                player->id = pid;
                player->buffer = 0;
                snprintf(player->name, MAX_NAME_LENGTH, "player%d", pid);
                update_player(player, px, py, pz, prx, pry, 1); // twice
            }
            if (player) {
                update_player(player, px, py, pz, prx, pry, 1);
            }
        }
        if (sscanf(line, "D,%d", &pid) == 1) {
            delete_player(pid);
        }
        int kp, kq, kk;
        if (sscanf(line, "K,%d,%d,%d", &kp, &kq, &kk) == 3) {
            d_db_set_key(kp, kq, kk);
        }
        if (sscanf(line, "R,%d,%d", &kp, &kq) == 2) {
            Chunk *chunk = find_chunk(kp, kq);
            if (chunk) {
                dirty_chunk(chunk);
            }
        }
        double elapsed;
        int day_length;
        if (sscanf(line, "E,%lf,%d", &elapsed, &day_length) == 2) {
            glfwSetTime(fmod(elapsed, day_length));
            g->day_length = day_length;
            g->time_changed = 1;
        }
        if (line[0] == 'T' && line[1] == ',') {
            char *text = line + 2;
            add_message(text);
        }
        char format[64];
        snprintf(
            format, sizeof(format), "N,%%d,%%%ds", MAX_NAME_LENGTH - 1);
        char name[MAX_NAME_LENGTH];
        if (sscanf(line, format, &pid, name) == 2) {
            Player *player = find_player(pid);
            if (player) {
                strncpy(player->name, name, MAX_NAME_LENGTH);
            }
        }
        snprintf(
            format, sizeof(format),
            "S,%%d,%%d,%%d,%%d,%%d,%%d,%%%d[^\n]", MAX_SIGN_LENGTH - 1);
        int face;
        char text[MAX_SIGN_LENGTH] = {0};
        if (sscanf(line, format,
            &bp, &bq, &bx, &by, &bz, &face, text) >= 6)
        {
            _set_sign(bp, bq, bx, by, bz, face, text, 0);
        }
        line = m_util_tokenize(NULL, "\n", &key);
    }
}

void reset_model() {
    memset(g->chunks, 0, sizeof(Chunk) * MAX_CHUNKS);
    g->chunk_count = 0;
    memset(g->players, 0, sizeof(Player) * MAX_PLAYERS);
    g->player_count = 0;
    g->observe1 = 0;
    g->observe2 = 0;
    g->flying = 0;
    g->item_index = 0;
    memset(g->typing_buffer, 0, sizeof(char) * MAX_TEXT_LENGTH);
    g->typing = 0;
    memset(g->messages, 0, sizeof(char) * MAX_MESSAGES * MAX_TEXT_LENGTH);
    g->message_index = 0;
    g->day_length = DAY_LENGTH;
    glfwSetTime(g->day_length / 3.0);
    g->time_changed = 1;
}

int main(int argc, char **argv) {
    // CHECK COMMAND LINE ARGUMENTS //
    if (argc == 2 || argc == 3) {
        g->mode = MODE_ONLINE;
        strncpy(g->server_addr, argv[1], MAX_ADDR_LENGTH);
        g->server_port = argc == 3 ? atoi(argv[2]) : DEFAULT_PORT;
        snprintf(g->d_db_path, MAX_PATH_LENGTH,
            "cache.%s.%d.db", g->server_addr, g->server_port);
    } else {
        g->mode = MODE_OFFLINE;
        snprintf(g->d_db_path, MAX_PATH_LENGTH, "%s", DB_PATH);
    }

    Attrib block_attrib = {0};
    Attrib line_attrib = {0};
    Attrib text_attrib = {0};
    Attrib sky_attrib = {0};

    // @todo -> Move callbacks to m_game instead of passing here
    int init = m_game_init(
        &model,
        on_key,
        on_char,
        on_mouse_button,
        on_scroll,
        &block_attrib,
        &line_attrib,
        &text_attrib,
        &sky_attrib
    );

    // Non-zero return, end here.
    if (init != 0) {
        return init;
    }

    // OUTER LOOP //
    int running = 1;
    while (running) {
        // DATABASE INITIALIZATION //
        if (g->mode == MODE_OFFLINE || USE_CACHE) {
            d_db_enable();
            if (d_db_init(g->d_db_path)) {
                return -1;
            }
            if (g->mode == MODE_ONLINE) {
                // TODO: support proper caching of signs (handle deletions)
                d_db_delete_all_signs();
            }
        }

        // CLIENT INITIALIZATION //
        if (g->mode == MODE_ONLINE) {
            n_client_enable();
            n_client_connect(g->server_addr, g->server_port);
            n_client_start();
            n_client_version(1);
            n_auth_login();
        }

        // LOCAL VARIABLES //
        reset_model();
        FPS fps = {0, 0, 0};
        double last_commit = glfwGetTime();
        double last_update = glfwGetTime();
        GLuint sky_buffer = gen_sky_buffer();

        Player *me = g->players;
        State *s = &g->players->state;
        me->id = 0;
        me->name[0] = '\0';
        me->buffer = 0;
        g->player_count = 1;

        // LOAD STATE FROM DATABASE //
        int loaded = d_db_load_state(&s->x, &s->y, &s->z, &s->rx, &s->ry);
        force_chunks(me);
        if (!loaded) {
            s->y = highest_block(s->x, s->z) + 2;
        }

        // BEGIN MAIN LOOP //
        double previous = glfwGetTime();
        while (1) {
            // WINDOW SIZE AND SCALE //
            g->scale = get_scale_factor();
            glfwGetFramebufferSize(g->window, &g->width, &g->height);
            glViewport(0, 0, g->width, g->height);

            // FRAME RATE //
            if (g->time_changed) {
                g->time_changed = 0;
                last_commit = glfwGetTime();
                last_update = glfwGetTime();
                memset(&fps, 0, sizeof(fps));
            }
            m_util_fps_update(&fps);
            double now = glfwGetTime();
            double dt = now - previous;
            dt = MIN(dt, 0.2);
            dt = MAX(dt, 0.0);
            previous = now;

            m_game_update(dt);

            // HANDLE DATA FROM SERVER //
            char *buffer = n_client_recv();
            if (buffer) {
                parse_buffer(buffer);
                free(buffer);
            }

            // FLUSH DATABASE //
            if (now - last_commit > COMMIT_INTERVAL) {
                last_commit = now;
                d_db_commit();
            }

            // SEND POSITION TO SERVER //
            if (now - last_update > 0.1) {
                last_update = now;
                n_client_position(s->x, s->y, s->z, s->rx, s->ry);
            }

            // PREPARE TO RENDER //
            m_game_render();
            Player *player = g->players + g->observe1;

            // RENDER 3-D SCENE //
            glClear(GL_DEPTH_BUFFER_BIT);
            int face_count = render_chunks(&block_attrib, player);
            render_signs(&text_attrib, player);
            render_sign(&text_attrib, player);
            render_players(&block_attrib, player);
            if (SHOW_WIREFRAME) {
                render_wireframe(&line_attrib, player);
            }

            // RENDER HUD //
            glClear(GL_DEPTH_BUFFER_BIT);
            if (SHOW_CROSSHAIRS) {
                render_crosshairs(&line_attrib);
            }
            if (SHOW_ITEM) {
                render_item(&block_attrib);
            }

            // RENDER TEXT //
            char text_buffer[1024];
            float ts = 12 * g->scale;
            float tx = ts / 2;
            float ty = g->height - ts;
            if (SHOW_INFO_TEXT) {
                int hour = time_of_day() * 24;
                char am_pm = hour < 12 ? 'a' : 'p';
                hour = hour % 12;
                hour = hour ? hour : 12;
                snprintf(
                    text_buffer, 1024,
                    "(%d, %d) (%.2f, %.2f, %.2f) [%d, %d, %d] %d%cm %dfps",
                    chunked(s->x), chunked(s->z), s->x, s->y, s->z,
                    g->player_count, g->chunk_count,
                    face_count * 2, hour, am_pm, fps.fps);
                render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts, text_buffer);
                ty -= ts * 2;
            }
            if (SHOW_CHAT_TEXT) {
                for (int i = 0; i < MAX_MESSAGES; i++) {
                    int index = (g->message_index + i) % MAX_MESSAGES;
                    if (strlen(g->messages[index])) {
                        render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts,
                            g->messages[index]);
                        ty -= ts * 2;
                    }
                }
            }
            if (g->typing) {
                snprintf(text_buffer, 1024, "> %s", g->typing_buffer);
                render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts, text_buffer);
                ty -= ts * 2;
            }
            if (SHOW_PLAYER_NAMES) {
                if (player != me) {
                    render_text(&text_attrib, ALIGN_CENTER,
                        g->width / 2, ts, ts, player->name);
                }
                Player *other = player_crosshair(player);
                if (other) {
                    render_text(&text_attrib, ALIGN_CENTER,
                        g->width / 2, g->height / 2 - ts - 24, ts,
                        other->name);
                }
            }

            // RENDER PICTURE IN PICTURE //
            if (g->observe2) {
                player = g->players + g->observe2;

                int pw = 256 * g->scale;
                int ph = 256 * g->scale;
                int offset = 32 * g->scale;
                int pad = 3 * g->scale;
                int sw = pw + pad * 2;
                int sh = ph + pad * 2;

                glEnable(GL_SCISSOR_TEST);
                glScissor(g->width - sw - offset + pad, offset - pad, sw, sh);
                glClear(GL_COLOR_BUFFER_BIT);
                glDisable(GL_SCISSOR_TEST);
                glClear(GL_DEPTH_BUFFER_BIT);
                glViewport(g->width - pw - offset, offset, pw, ph);

                g->width = pw;
                g->height = ph;
                g->ortho = 0;
                g->fov = 65;

                render_sky(&sky_attrib, player, sky_buffer);
                glClear(GL_DEPTH_BUFFER_BIT);
                render_chunks(&block_attrib, player);
                render_signs(&text_attrib, player);
                render_players(&block_attrib, player);
                glClear(GL_DEPTH_BUFFER_BIT);
                if (SHOW_PLAYER_NAMES) {
                    render_text(&text_attrib, ALIGN_CENTER,
                        pw / 2, ts, ts, player->name);
                }
            }

            // SWAP AND POLL //
            glfwSwapBuffers(g->window);
            glfwPollEvents();
            if (glfwWindowShouldClose(g->window)) {
                running = 0;
                break;
            }
            if (g->mode_changed) {
                g->mode_changed = 0;
                break;
            }
        }

        // SHUTDOWN //
        d_db_save_state(s->x, s->y, s->z, s->rx, s->ry);
        d_db_close();
        d_db_disable();
        n_client_stop();
        n_client_disable();
        m_util_buffer_del(sky_buffer);
        delete_all_chunks();
        delete_all_players();
    }

    glfwTerminate();
    curl_global_cleanup();
    return 0;
}
