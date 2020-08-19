#ifndef _m_util_h_
#define _m_util_h_

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "g_config.h"

#define PI 3.14159265359
#define DEGREES(radians) ((radians) * 180 / PI)
#define RADIANS(degrees) ((degrees) * PI / 180)
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SIGN(x) (((x) > 0) - ((x) < 0))

#if DEBUG
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

typedef struct {
    unsigned int fps;
    unsigned int frames;
    double since;
} FPS;

// Random
int m_util_rand_int(int n);
double m_util_rand_double();

// FPS
void m_util_fps_update(FPS *fps);

// GL
GLuint m_util_buffer_gen(GLsizei size, GLfloat *data);
void m_util_buffer_del(GLuint buffer);
GLfloat *m_util_faces_malloc(int components, int faces);
GLuint m_util_faces_gen(int components, int faces, GLfloat *data);
GLuint m_util_shader_make(GLenum type, const char *source);
GLuint m_util_shader_load(GLenum type, const char *path);
GLuint m_util_program_make(GLuint shader1, GLuint shader2);
GLuint m_util_program_load(const char *path1, const char *path2);
void m_util_texture_load_png(const char *file_name);

// String
char *m_util_tokenize(char *str, const char *delim, char **key);
int m_util_char_width(char input);
int m_util_string_width(const char *input);
int m_util_wrap(const char *input, int max_width, char *output, int max_length);

#endif

