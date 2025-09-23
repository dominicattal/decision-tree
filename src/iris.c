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

    const char* columns[4] = {
        "SepalLengthCm",
        "SepalWidthCm",
        "PetalLengthCm",
        "PetalWidthCm"
    };

    int num_attr = sizeof(columns) / sizeof(char*);

    csv_encode(csv, "Species");
    int* labels = csv_column_int(csv, "Species");

    DecisionTree* dt = decision_tree_create(num_attr, columns);
    Matrix* matrix = matrix_create(csv->num_rows-1, num_attr);

    puts("==== Classifier ====");

    for (int i = 0; i < num_attr; i++) {
        float* column = csv_column_float(csv, columns[i]);
        matrix_set_col(matrix, i, column);
        free(column);
    }

    DTTrainConfig config = decision_tree_default_config();
    config.max_num_threads = 20;
    config.discrete_threshold = 2;
    config.condition = DT_SPLIT_GINI;

    decision_tree_config(dt, config);
    decision_tree_train(dt, csv->num_rows-1, matrix->buffer, labels);

    float test[4] = {7.9, 3.1, 2.0, 0.1};
    int label = decision_tree_classifier_predict(dt, test);
    printf("Prediction: %s\n", csv_decode(csv, label));

    puts("==== Regressor ====");

    config.type = DT_REGRESSOR;
    config.condition = DT_SPLIT_RMSE;

    decision_tree_config(dt, config);

    float* labels_float = csv_column_float(csv, "Species");
    matrix_set_col(matrix, 3, labels_float);

    float* petal_width_cm = csv_column_float(csv, "PetalWidthCm");

    decision_tree_set_attr(dt, 4, NULL);
    decision_tree_train(dt, csv->num_rows-1, matrix->buffer, petal_width_cm);

    float test2[4] = {7, 3.2, 4.7, 1};
    float value = decision_tree_regressor_predict(dt, test2);
    printf("Prediction: %f\n", value);

    decision_tree_destroy(dt);

    matrix_destroy(matrix);

    free(petal_width_cm);
    free(labels_float);
    free(labels);

    csv_destroy(csv);
}
