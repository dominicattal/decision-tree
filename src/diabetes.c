#include "tests.h"
#include "models.h"
#include "models/decisiontree.h"
#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <csv.h>

#define DIABETES_CSV_PATH "datasets/diabetes/diabetes.csv"

void diabetes_test_write(void)
{
    CSV* csv = csv_read(DIABETES_CSV_PATH);

    const char* columns[] = {
        "Pregnancies",
        "Glucose",
        "BloodPressure",
        "SkinThickness",
        "Insulin",
        "BMI",
        "DiabetesPedigreeFunction",
        "Age"
    };

    int n = sizeof(columns) / sizeof(*columns);

    puts("==== Classifier ====");
    int* labels = csv_column_int(csv, "Outcome");
    DecisionTree* dt = decision_tree_create(n, columns);
    Matrix* matrix = matrix_create(csv->num_rows-1, n);

    DTTrainConfig config = decision_tree_default_config();
    config.max_num_threads = 20;
    config.max_depth = 50;
    config.discrete_threshold = 5;
    config.condition = DT_SPLIT_ENTROPY;

    decision_tree_config(dt, config);

    for (int col = 0; col < n; col++) {
        float* buf = csv_column_float(csv, columns[col]);
        matrix_set_col(matrix, col, buf);
        free(buf);
    }

    decision_tree_train(dt, csv->num_rows-1, matrix->buffer, labels);

    decision_tree_write(dt, "models/diabetes.dt");

    float test[8] = {6, 148, 72, 35, 0, 33.6, 0.627, 50};
    int out = decision_tree_classifier_predict(dt, test);
    printf("Prediction: %s\n", (out) ? "Yes" : "No");

    free(labels);
    matrix_destroy(matrix);
    decision_tree_destroy(dt);
    csv_destroy(csv);
}

void diabetes_test_read(void)
{
    DecisionTree* dt = decision_tree_read("models/diabetes.dt");
    float test[8] = {6, 148, 72, 35, 0, 33.6, 0.627, 50};
    int out = decision_tree_classifier_predict(dt, test);
    printf("Prediction: %s\n", (out) ? "Yes" : "No");
    decision_tree_destroy(dt);
}

void diabetes_test(void)
{
    diabetes_test_write();
    puts("");
    diabetes_test_read();
}
