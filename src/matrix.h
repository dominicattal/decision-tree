#ifndef MATRIX_H
#define MATRIX_H

typedef struct {
    int rows, cols;
    float* buffer;
} Matrix;

Matrix*     matrix_create(int rows, int cols);
void        matrix_destroy(Matrix* mat);
void        matrix_set_col(Matrix* mat, int col, float* buf);
void        matrix_set_row(Matrix* mat, int row, float* buf);
Matrix*     matrix_multiply(Matrix* mat1, Matrix* mat2);
void        matrix_print(Matrix* mat, const char* fmt);

#endif
