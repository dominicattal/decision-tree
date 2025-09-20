#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>

Matrix* matrix_create(int rows, int cols)
{
    Matrix* mat = malloc(sizeof(Matrix));
    mat->buffer = calloc(rows * cols, sizeof(float));
    mat->rows = rows;
    mat->cols = cols;
    return mat;
}

void matrix_destroy(Matrix* mat)
{
    free(mat->buffer);
    free(mat);
}

void matrix_set_col(Matrix* mat, int col, float* buf)
{
    for (int i = 0; i < mat->rows; i++)
        mat->buffer[i * mat->cols + col] = buf[i];
}

void matrix_set_row(Matrix* mat, int row, float* buf)
{
    for (int i = 0; i < mat->cols; i ++)
        mat->buffer[row * mat->cols + i] = buf[i];
}

Matrix* matrix_multiply(Matrix* mat1, Matrix* mat2)
{
    int idx3, idx1, idx2;

    if (mat1->cols != mat2->rows)
        return NULL;

    Matrix* mat3 = matrix_create(mat1->rows, mat2->cols);
    for (int i = 0; i < mat1->rows; i++) {
        for (int k = 0; k < mat1->cols; k++) {
            for (int j = 0; j < mat2->cols; j++) {
                idx1 = i * mat1->cols + k;
                idx3 = i * mat2->cols + j;
                idx2 = k * mat1->cols + j;
                mat3->buffer[idx3] = mat1->buffer[idx1] * mat2->buffer[idx2];
            }
        }
    }

    return mat3;
}

void matrix_print(Matrix* mat, const char* fmt)
{
    for (int i = 0; i < mat->rows; i++)
        for (int j = 0; j < mat->cols; j++)
            printf(fmt, mat->buffer[i * mat->cols + j]);
}
