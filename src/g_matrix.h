#ifndef _g_matrix_h_
#define _g_matrix_h_

void g_mat_normalize(float *x, float *y, float *z);
void g_mat_identity(float *matrix);
void g_mat_translate(float *matrix, float dx, float dy, float dz);
void g_mat_rotate(float *matrix, float x, float y, float z, float angle);
void g_mat_vec_multiply(float *vector, float *a, float *b);
void g_mat_multiply(float *matrix, float *a, float *b);
void g_mat_apply(float *data, float *matrix, int count, int offset, int stride);
void g_mat_frustum_planes(float planes[6][4], int radius, float *matrix);
void g_mat_frustum(
    float *matrix, float left, float right, float bottom,
    float top, float znear, float zfar);
void g_mat_perspective(
    float *matrix, float fov, float aspect,
    float near, float far);
void g_mat_ortho(
    float *matrix,
    float left, float right, float bottom, float top, float near, float far);
void g_mat_set_2d(float *matrix, int width, int height);
void g_mat_set_3d(
    float *matrix, int width, int height,
    float x, float y, float z, float rx, float ry,
    float fov, int ortho, int radius);
void g_mat_matrix_set_item(float *matrix, int width, int height, int scale);

#endif
