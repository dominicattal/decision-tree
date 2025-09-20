#include "tests.h"
#include "models/decisiontree.h"
#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <csv.h>

#define IRIS_CSV_PATH "datasets/iris/iris.csv"

void iris_test(void)
{
    CSV* csv = csv_read(IRIS_CSV_PATH);

    float* sepal_length_cm = csv_column_float(csv, "SepalLengthCm");
    float* sepal_width_cm = csv_column_float(csv, "SepalWidthCm");
    float* petal_length_cm = csv_column_float(csv, "PetalLengthCm");
    float* petal_width_cm = csv_column_float(csv, "PetalWidthCm");

    csv_encode(csv, "Species");
    int* labels = csv_column_int(csv, "Species");

    DecisionTree* dt = decision_tree_create(4);

    Matrix* matrix = matrix_create(csv->num_rows-1, 4);

    matrix_set_col(matrix, 0, sepal_length_cm);
    matrix_set_col(matrix, 1, sepal_width_cm);
    matrix_set_col(matrix, 2, petal_length_cm);
    matrix_set_col(matrix, 3, petal_width_cm);

    decision_tree_train(dt, csv->num_rows-1, matrix->buffer, labels);

    matrix_destroy(matrix);

    float test[4] = {4.9, 3.1, 1.5, 0.1};
    int label = decision_tree_predict(dt, test);
    printf("%s\n", csv_decode(csv, label));

    decision_tree_destroy(dt);

    free(sepal_length_cm);
    free(sepal_width_cm);
    free(petal_length_cm);
    free(petal_width_cm);

    csv_destroy(csv);
}
