#include "models.h"
#include <stdio.h>
#include <stdlib.h>

static int get_uniq_idx(int label, int num_unique_labels, int* unique_labels)
{
    for (int i = 0; i < num_unique_labels; i++)
        if (label == unique_labels[i])
            return i;
    return -1;
}

ClassifierMetrics* model_classifier_evaluate(ClassifierEvaluateParams* params)
{
    int     num_unique_labels   =   params->num_unique_labels;
    int*    unique_labels       =   params->unique_labels;
    int     num_labels          =   params->num_labels;
    int*    labels_given        =   params->labels_given;
    int*    labels_predicted    =   params->labels_predicted;

    ClassifierMetrics* cm;
    int tp, tn, fp, fn;
    int uniq_label, label_idx;
    int given_label, given_uniq_idx;
    int pred_label, pred_uniq_idx;
    int given_eq, pred_eq;
    int confusion_idx, count;
    int* confusion_matrix;
    ConfusionMatrix* matrix;
    ConfusionMatrix* matrices;

    confusion_matrix = calloc(num_unique_labels * num_unique_labels, sizeof(int));
    for (label_idx = 0; label_idx < num_labels; label_idx++) {
        given_label = labels_given[label_idx];
        pred_label = labels_predicted[label_idx];
        given_uniq_idx = get_uniq_idx(given_label, num_unique_labels, unique_labels);
        pred_uniq_idx = get_uniq_idx(pred_label, num_unique_labels, unique_labels);
        confusion_idx = given_uniq_idx * num_unique_labels + pred_uniq_idx;
        confusion_matrix[confusion_idx]++;
    }

    matrices = calloc(num_unique_labels, sizeof(ConfusionMatrix));
    for (uniq_label = 0; uniq_label < num_unique_labels; uniq_label++) {
        matrix = &matrices[uniq_label];
        matrix->uniq_label = uniq_label;
        matrix->label = unique_labels[uniq_label];
        tp = tn = fp = fn = 0;
        for (given_uniq_idx = 0; given_uniq_idx < num_unique_labels; given_uniq_idx++) {
            for (pred_uniq_idx = 0; pred_uniq_idx < num_unique_labels; pred_uniq_idx++) {
                confusion_idx = given_uniq_idx * num_unique_labels + pred_uniq_idx;
                count = confusion_matrix[confusion_idx];
                given_eq = given_uniq_idx == uniq_label;
                pred_eq = pred_uniq_idx == uniq_label;
                if (given_eq && pred_eq)
                    tp += count;
                else if (given_eq && !pred_eq)
                    fn += count;
                else if (!given_eq && !pred_eq)
                    tn += count;
                else
                    fp += count;
            }
        }
        matrix->true_positives = tp;
        matrix->true_negatives = tn;
        matrix->false_positives = fp;
        matrix->false_negatives = fn;
        //matrix->prevalence = (float)(tp + fp) / (tn + fn);
        //matrix->ppv = (float)tp / (tp + fp);
        //matrix->tpr = (float)tp / (tp + fn);
        //matrix->acc = (float)(tp + tn) / (tp + fn + tn + fp);
        //matrix->npv = (float)tn / (tn + fn);
        //matrix->f1 = (float)(2 * matrix->ppv * matrix->tpr) / (matrix->ppv + matrix->tpr);
        //matrix->fdr = (float)fp / (tp + fp);
    }

    free(confusion_matrix);

    cm = malloc(sizeof(ClassifierMetrics));
    cm->num_unique_labels = num_unique_labels;
    cm->matrices = matrices;

    return cm;
}

ConfusionMatrix* model_classifier_metrics_matrix(ClassifierMetrics* metrics, int label)
{
    for (int i = 0; i < metrics->num_unique_labels; i++)
        if (metrics->matrices[i].label == label)
            return &metrics->matrices[i];
    return NULL;
}

ConfusionMatrixPrintParams model_classifier_metrics_print_params_default(void)
{
    return (ConfusionMatrixPrintParams) {
        .tp = true,
        .tn = true,
        .fp = true,
        .fn = true,
        .ppv = true,
        .tpr = false,
        .acc = true,
        .npv = false,
        .fdr = false
    };
}

static void print_int(const char* name, int num)
{
    printf("\t%30s = %-d\n", name, num);
}

void model_confusion_matrix_print(ConfusionMatrix* matrix, ConfusionMatrixPrintParams* params)
{
    int tp, tn, fp, fn;
    tp = matrix->tp;
    tn = matrix->tn;
    fp = matrix->fp;
    fn = matrix->fn;
    printf("Matrix: %d %d\n", matrix->label, matrix->uniq_label);
    if (params->tp)
        print_int("True Positives", tp);
    if (params->tn)
        print_int("True Negatives", tn);
    if (params->fp)
        print_int("True Positives", fp);
    if (params->fn)
        print_int("True Negatives", fn);
}

void model_classifier_metrics_print(ClassifierMetrics* metrics, ConfusionMatrixPrintParams* params)
{
    for (int i = 0; i < metrics->num_unique_labels; i++)
        model_confusion_matrix_print(&metrics->matrices[i], params);
}

void model_classifier_metrics_destroy(ClassifierMetrics* metrics)
{
    free(metrics->matrices);
    free(metrics);
}
