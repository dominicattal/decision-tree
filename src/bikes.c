#include "tests.h"
#include "models/decisiontree.h"
#include "matrix.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <csv.h>

#define BIKES_CSV_PATH "datasets/bike-share/hour.csv"

static float* normalize_col(int n, float* buf)
{
    float mean = 0;
    float variance = 0;
    float std;
    float* norm;
    for (int i = 0; i < n; i++)
        mean += buf[i];
    mean /= n;
    for (int i = 0; i < n; i++)
        variance += fabsf(buf[i] - mean);
    std = sqrt(variance);
    norm = malloc(n * sizeof(float));
    for (int i = 0; i < n; i++)
        norm[i] = (buf[i] - mean) / std;
    return norm;
}

void bikes_test(void)
{
    printf("%lld\n", sizeof(DTTrainConfig));
    puts("Reading csv");
    CSV* csv = csv_read(BIKES_CSV_PATH);

    const char* columns[4] = {
        "weekday",
        "hr",
        "atemp",
        "hum"
    };

    int num_attr = sizeof(columns) / sizeof(char*);
    int num_labels = csv->num_rows - 1;
    int num_labels_train = (num_labels / 10 * 8);

    DecisionTree* dt = decision_tree_create(num_attr, columns);
    Matrix* matrix = matrix_create(num_labels, num_attr);
    DTTrainConfig config = decision_tree_default_config();
    config.type = DT_REGRESSOR;
    config.max_num_threads = 20;
    config.max_depth = 32;
    config.discrete_threshold = 2;
    config.condition = DT_SPLIT_RMSE;

    // load data
    for (int i = 0; i < num_attr; i++) {
        float* col = csv_column_float(csv, columns[i]);
        float* col_normalized = normalize_col(num_labels_train, col);
        matrix_set_col(matrix, i, col_normalized);
        free(col);
        free(col_normalized);
    }
    float* counts = csv_column_float(csv, "cnt");

    decision_tree_config(dt, config);

    decision_tree_train(dt, num_labels_train, matrix->buffer, counts);

    float test[4] = {0.99398, -0.3697, 0.2321, 1.6161};
    float value = decision_tree_regressor_predict(dt, test);
    printf("Prediction: %f\n", value);
    
    free(counts);
    matrix_destroy(matrix);
    decision_tree_destroy(dt);
}
