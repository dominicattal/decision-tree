#ifndef MODELS_H
#define MODELS_H

#include <stdbool.h>

// values taken from https://en.wikipedia.org/wiki/Confusion_matrix

typedef struct {
    int label, uniq_label;
    union {
        int true_positives;
        int tp;
    };
    union {
        int true_negatives;
        int tn;
    };
    union {
        int false_positives;
        int fp;
    };
    union {
        int false_negatives;
        int fn;
    };

} ConfusionMatrix;

typedef struct {
    int                 num_unique_labels;
    ConfusionMatrix*    matrices;
} ClassifierMetrics;

typedef struct {
    float   rmse;
} RegressorMetrics;

typedef struct {
    int     num_unique_labels;
    int*    unique_labels;
    int     num_labels;
    int*    labels_given;
    int*    labels_predicted;
} ClassifierEvaluateParams;

typedef struct {
    bool tp, tn, fp, fn;
    union {
        bool precision;
        bool positive_predictive_value;
        bool ppv;
    };
    union {
        bool recall;
        bool sensitivity;
        bool true_positive_rate;
        bool tpr;
    };
    union {
        bool accuracy;
        bool acc;
    };
    union {
        bool negative_predictive_value;
        bool npv;
    };
    union {
        bool false_discovery_rate;
        bool freddy_delano_roosevelt;
        bool fdr;
    };
} ConfusionMatrixPrintParams;

ClassifierMetrics*              model_classifier_evaluate(ClassifierEvaluateParams* params);
ConfusionMatrix*                model_classifier_metrics_matrix(ClassifierMetrics* metrics, int label);
ConfusionMatrixPrintParams      model_classifier_metrics_print_params_default(void);
void                            model_classifier_metrics_print(ClassifierMetrics* metrics, ConfusionMatrixPrintParams* params);
void                            model_confusion_matrix_print(ConfusionMatrix* matrix, ConfusionMatrixPrintParams* params);
void                            model_classifier_metrics_destroy(ClassifierMetrics* metrics);

#endif
