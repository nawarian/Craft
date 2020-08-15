#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>

#include "m_game.h"
#include "m_util.h"

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

int m_game_init(
    Model *model,
    void* callback_on_key,
    void* callback_on_char,
    void* callback_on_mouse_button,
    void* callback_on_scroll
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
    _load_shaders();

    return 0;
}

void m_game_create() {
}

void m_game_update() {
    // Handle Mouse Movement
    // Handle Input
}

