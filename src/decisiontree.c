#include "decisiontree.h"
#include <pthread.h>
#include <bitset.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef struct DTNode DTNode;

typedef struct DTNode {
    DTNode* left;
    DTNode* right;
    float base;
    int attr_idx;
    int discrete;
    union {
        int label;
        float avg;
    };
} DTNode;

typedef struct DecisionTree {
    int num_attr;
    char** attr_names;
    DTTrainConfig config;
    DTNode* root;
} DecisionTree;

static int dtnode_isleaf(DTNode* node)
{
    return node->left == NULL;
}

static void dtnode_destroy(DTNode* node)
{
    if (node == NULL)
        return;
    dtnode_destroy(node->left);
    dtnode_destroy(node->right);
    free(node);
}

static char** copy_attr_names(int num_attr, const char** attr_names)
{
    if (attr_names == NULL)
        return NULL;

    char** copy_attr_names = malloc(num_attr * sizeof(char*));
    for (int i = 0; i < num_attr; i++) {
        if (attr_names[i] == NULL) {
            copy_attr_names[i] = NULL;
            continue;
        }
        int n = strlen(attr_names[i]);
        copy_attr_names[i] = malloc((n+1) * sizeof(char));
        strncpy(copy_attr_names[i], attr_names[i], n+1);
    }

    return copy_attr_names;
}

DecisionTree* decision_tree_create(int num_attr, const char** attr_names)
{
    DecisionTree* dt = malloc(sizeof(DecisionTree));
    dt->num_attr = num_attr;
    dt->root = NULL;
    dt->config = decision_tree_default_config();
    dt->attr_names = copy_attr_names(num_attr, attr_names);
    return dt;
}

void decision_tree_set_attr(DecisionTree* dt, int num_attr, const char** attr_names)
{
    if (dt->root != NULL)
        dtnode_destroy(dt->root);
    dt->root = NULL;
    if (dt->attr_names != NULL)
        for (int i = 0; i < dt->num_attr; i++)
            free(dt->attr_names[i]);
    free(dt->attr_names); 
    dt->attr_names = copy_attr_names(num_attr, attr_names);
}

DTTrainConfig decision_tree_default_config(void)
{
    return (DTTrainConfig) {
        .type = DT_CLASSIFIER,
        .splitter = DT_SPLIT_ENTROPY,
        .min_samples_split = 5,
        .max_depth = 8,
        .max_num_threads = 1
    };
}

void decision_tree_config(DecisionTree* dt, DTTrainConfig config)
{
    dt->config = config;
}

static int get_attr_idx(int num_attr, int attr_idx, int label_idx)
{
    return num_attr * label_idx + attr_idx;
}

static int get_num_unique_values(int num_attr, int num_labels, float* attr, int attr_idx, Bitset* bitset)
{
    int i, j;
    float value1, value2;
    int num_unique_values;

    num_unique_values = 0;
    for (i = 0; i < num_labels; i++) {
        if (!bitset_isset(bitset, i))
            continue;
        value1 = attr[get_attr_idx(num_attr, attr_idx, i)];
        for (j = 0; j < i; j++) {
            if (!bitset_isset(bitset, j))
                continue;
            value2 = attr[get_attr_idx(num_attr, attr_idx, j)];
            if (value1 == value2)
                goto not_unique;
        }
        num_unique_values++;
not_unique:
    }

    return num_unique_values;
}

static float* get_unique_values(int num_unique_values, int num_attr, int num_labels, float* attr, int attr_idx, Bitset* bitset)
{
    int uniq_idx, i, j;
    float value;
    float* unique_values = calloc(num_unique_values, sizeof(float));

    uniq_idx = 0;
    for (i = 0; i < num_labels; i++) {
        if (!bitset_isset(bitset, i))
            continue;
        value = attr[get_attr_idx(num_attr, attr_idx, i)];
        for (j = 0; j < uniq_idx; j++) {
            if (value == unique_values[j])
                goto not_unique;
        }
        unique_values[uniq_idx++] = value;
not_unique:
    }

    return unique_values;
}

static int get_num_unique_labels(int num_labels, int* labels)
{
    int i, j;
    int label1, label2;
    int num_unique_labels;

    num_unique_labels = 0;
    for (i = 0; i < num_labels; i++) {
        label1 = labels[i];
        for (j = 0; j < i; j++) {
            label2 = labels[j];
            if (label1 == label2)
                goto not_unqiue;
        }
        num_unique_labels++;
not_unqiue:
    }

    return num_unique_labels;
}

static int* get_unique_labels(int num_unique_labels, int num_labels, int* labels)
{
    int uniq_idx, i, j, label;
    int* unique_labels = malloc(num_unique_labels * sizeof(int));

    uniq_idx = 0;
    for (i = 0; i < num_labels; i++) {
        label = labels[i];
        for (j = 0; j < uniq_idx; j++)
            if (label == unique_labels[j])
                goto not_unique;
        unique_labels[uniq_idx++] = label;
not_unique:
    }

    return unique_labels;
}

static int* get_unique_labels_count(int num_unique_labels, int* unique_labels, int num_labels, int* labels, Bitset* bitset)
{
    int label_idx, uniq_idx;
    int* unique_labels_count = calloc(num_unique_labels, sizeof(int));

    for (label_idx = 0; label_idx < num_labels; label_idx++) {
        if (!bitset_isset(bitset, label_idx))
            continue;
        for (uniq_idx = 0; uniq_idx < num_unique_labels; uniq_idx++) {
            if (labels[label_idx] == unique_labels[uniq_idx]) {
                unique_labels_count[uniq_idx]++;
                break;
            }
        }
    }

    return unique_labels_count;
}

typedef struct {
    DTTrainConfig*      config;
    int                 num_labels;
    int*                labels;
    int                 num_unique_labels;
    int*                unique_labels;
    int                 num_attr;
    float*              attr;
    int                 attr_idx;
    int                 discrete;
    float               base;
    Bitset*             bitset;
    Bitset**            bitset_left;
    Bitset**            bitset_right;
    int                 depth;
    int*                num_threads_ptr;
    pthread_mutex_t*    num_threads_mutex;
    pthread_mutex_t*    thread_mutex;
} DTTrainParams;

static int cmp_discrete(float base, float test)
{
    return test == base;
}

static int cmp_continuous(float base, float test)
{
    return test > base;
}

static void split(DTTrainParams* params)
{
    int        discrete      = params->discrete;
    int        num_labels    = params->num_labels;
    int        num_attr      = params->num_attr;
    int        attr_idx      = params->attr_idx;
    float*     attr          = params->attr;
    float      base          = params->base;
    Bitset*    bitset        = params->bitset;
    Bitset**   bitset_left   = params->bitset_left;
    Bitset**   bitset_right  = params->bitset_right;
    
    int (*cmp)(float, float);
    cmp = (discrete) ? cmp_discrete : cmp_continuous;

    float value;
    int label_idx;

    for (label_idx = 0; label_idx < num_labels; label_idx++) {
        if (!bitset_isset(bitset, label_idx))
            continue;
        value = attr[get_attr_idx(num_attr, attr_idx, label_idx)];
        if (cmp(base, value))
            bitset_set(*bitset_right, label_idx);
        else
            bitset_set(*bitset_left, label_idx);
    }
}

static float calculate_entropy(float res, float p)
{
    return res - p * log2(p);
}

static float calculate_gini(float res, float p)
{
    return res - p * p;
}

static float calculate_error(float res, float p)
{
    return (res > 1 - p) ? res : 1 - p;
}

static float calculate_split_classifier(DTTrainParams* params, Bitset* bitset)
{
    DTTrainConfig*  config              = params->config;
    int             num_unique_labels   = params->num_unique_labels;
    int*            unique_labels       = params->unique_labels;
    int             num_labels          = params->num_labels;
    int*            labels              = params->labels;

    float res, p;
    int n, uniq_idx;
    int* unique_labels_count;
    float (*calculate)(float, float);

    n = bitset_numset(bitset);
    if (n == 0)
        return 0;

    unique_labels_count = get_unique_labels_count(num_unique_labels, unique_labels, num_labels, labels, bitset);

    if (config->splitter == DT_SPLIT_ENTROPY)
        calculate = calculate_entropy;
    else if (config->splitter == DT_SPLIT_GINI)
        calculate = calculate_gini;
    else
        calculate = calculate_error;

    res = 0;
    for (uniq_idx = 0; uniq_idx < num_unique_labels; uniq_idx++) {
        if (unique_labels_count[uniq_idx] == 0)
            continue;
        p = (float)unique_labels_count[uniq_idx] / n;
        res = calculate(res, p);
    }

    free(unique_labels_count);

    return res;
}

static float calculate_mse(float res, float val)
{
    return res + val * val;
}

static float calculate_abs_error(float res, float val)
{
    return res + val;
}

static float calculate_split_regressor(DTTrainParams* params, Bitset* bitset)
{
    DTTrainConfig*  config  = params->config;
    int     num_labels      = params->num_labels;
    float*  labels          = (float*)params->labels;

    float avg, res, val;
    int n, label_idx;
    float (*calculate)(float, float);

    n = bitset_numset(bitset);
    if (n == 0)
        return 0;

    if (config->splitter == DT_SPLIT_MSE)
        calculate = calculate_mse;
    else
        calculate = calculate_abs_error;

    res = 0;
    for (label_idx = 0; label_idx < num_labels; label_idx++) {
        if (!bitset_isset(bitset, label_idx))
            continue;
        res += labels[label_idx];
    }
    avg = res / n;
    res = 0;
    for (label_idx = 0; label_idx < num_labels; label_idx++) {
        if (!bitset_isset(bitset, label_idx))
            continue;
        val = fabsf(labels[label_idx] - avg);
        res = calculate(res, val);
    }
    res /= n;

    return res;
}

static int all_labels_equal(int num_labels, int* labels, Bitset* bitset)
{
    int i, base = -1;
    for (i = 0; i < num_labels; i++) {
        if (bitset_isset(bitset, i)) {
            base = labels[i];
            break;
        }
    }
    while (i < num_labels) {
        if (bitset_isset(bitset, i) && labels[i] != base)
            return 0;
        i++;
    }
    return 1;
}

static int get_most_common_label(int num_unique_labels, int* unique_labels, int num_labels, int* labels, Bitset* bitset)
{
    int i, j, most_common_idx, most_common;
    if (num_unique_labels == 0)
        return -1;
    int* unique_labels_count = calloc(num_unique_labels, sizeof(int));

    for (i = 0; i < num_labels; i++) {
        if (!bitset_isset(bitset, i))
            continue;
        for (j = 0; j < num_unique_labels; j++) {
            if (labels[i] == unique_labels[j]) {
                unique_labels_count[j]++;
                break;
            }
        }
    }

    most_common_idx = 0;
    for (i = 1; i < num_unique_labels; i++)
        if (unique_labels_count[i] > unique_labels_count[most_common_idx])
            most_common_idx = i;
    most_common = unique_labels[most_common_idx];

    free(unique_labels_count);

    return most_common;
}

static float get_labels_average(int num_labels, float* labels, Bitset* bitset)
{
    int i, cnt;
    float avg;

    avg = cnt = 0;
    for (i = 0; i < num_labels; i++) {
        if (!bitset_isset(bitset, i))
            continue;
        avg = (avg * cnt + labels[i]) / (cnt+1);
        cnt++;
    }

    return avg;
}

static void* decision_tree_train_helper(void* void_params)
{
    DTTrainParams*      params              = void_params;
    Bitset*             bitset              = params->bitset;
    DTTrainConfig*      config              = params->config;
    int                 num_labels          = params->num_labels;
    int*                labels              = params->labels;
    int                 num_unique_labels   = params->num_unique_labels;
    int*                unique_labels       = params->unique_labels;
    int                 num_attr            = params->num_attr;
    float*              attr                = params->attr;
    int                 depth               = params->depth;
    int*                num_threads_ptr     = params->num_threads_ptr;
    pthread_mutex_t*    num_threads_mutex   = params->num_threads_mutex;

    if (params->thread_mutex != NULL)
        pthread_mutex_unlock(params->thread_mutex);

    DTNode* node;
    DTTrainParams* new_params;
    Bitset* bitset_left;
    Bitset* bitset_right;
    float* unique_values;
    int best_attr_idx, attr_idx;
    int best_discrete;
    int uniq_idx, num_unique_values;
    int n_parent, n_left, n_right;
    float score, best_score;
    float score_left, score_right;
    float best_base;
    bool classifier_condition;
    pthread_t thid;
    void* async_left;

    new_params = malloc(sizeof(DTTrainParams));
    new_params->config = config;
    new_params->num_labels = num_labels;
    new_params->labels = labels;
    new_params->num_unique_labels = num_unique_labels;
    new_params->unique_labels = unique_labels;
    new_params->num_attr = num_attr;
    new_params->attr = attr;
    new_params->depth = depth + 1;
    new_params->bitset = bitset;
    new_params->num_threads_ptr = num_threads_ptr;
    new_params->num_threads_mutex = num_threads_mutex;
    new_params->thread_mutex = NULL;

    node = malloc(sizeof(DTNode));
    node->left = node->right = NULL;
    node->base = 0;
    node->attr_idx = -2;
    node->discrete = -1;
    node->label = -1;

    classifier_condition = config->type == DT_CLASSIFIER && all_labels_equal(num_labels, labels, bitset);
    if (depth >= config->max_depth || classifier_condition) {
        if (config->type == DT_CLASSIFIER)
            node->label = get_most_common_label(num_unique_labels, unique_labels, num_labels, labels, bitset);
        else
            node->avg = get_labels_average(num_labels, (float*)labels, bitset);
        return node;
    }

    bitset_left = bitset_create(num_labels);
    bitset_right = bitset_create(num_labels);
    new_params->bitset_left = &bitset_left;
    new_params->bitset_right = &bitset_right;

    best_score = 1e9;
    best_attr_idx = -1;
    best_base = -1;
    best_discrete = -1;

    for (attr_idx = 0; attr_idx < num_attr; attr_idx++) {
        num_unique_values = get_num_unique_values(num_attr, num_labels, attr, attr_idx, bitset);
        unique_values = get_unique_values(num_unique_values, num_attr, num_labels, attr, attr_idx, bitset);
        n_parent = bitset_numset(bitset);
        new_params->discrete = num_unique_values <= config->min_samples_split;
        new_params->attr_idx = attr_idx;
        for (uniq_idx = 0; uniq_idx < num_unique_values; uniq_idx++) {
            new_params->base = unique_values[uniq_idx];
            bitset_unsetall(bitset_left);
            bitset_unsetall(bitset_right);
            split(new_params);
            n_left = bitset_numset(bitset_left);
            n_right = bitset_numset(bitset_right);
            if (n_left == 0 || n_right == 0)
                continue;
            if (config->type == DT_CLASSIFIER) {
                score_left = calculate_split_classifier(new_params, bitset_left);
                score_right = calculate_split_classifier(new_params, bitset_right);
                score = (
                            ((float)n_left  / n_parent) * score_left
                        +   ((float)n_right / n_parent) * score_right
                );
                if (score < best_score) {
                    best_score = score;
                    best_attr_idx = new_params->attr_idx;
                    best_base = new_params->base;
                    best_discrete = new_params->discrete;
                }
            } else if (config->type == DT_REGRESSOR) {
                score_left = calculate_split_regressor(new_params, bitset_left);
                score_right = calculate_split_regressor(new_params, bitset_right);
                score = (
                            ((float)n_left  / n_parent) * score_left
                        +   ((float)n_right / n_parent) * score_right
                );
                if (score < best_score) {
                    best_score = score;
                    best_attr_idx = new_params->attr_idx;
                    best_base = new_params->base;
                    best_discrete = new_params->discrete;
                }
            }
        }
        free(unique_values);
    }

    new_params->discrete = best_discrete;
    new_params->attr_idx = best_attr_idx;
    new_params->base = best_base;

    bitset_unsetall(bitset_left);
    bitset_unsetall(bitset_right);
    split(new_params);

    node->discrete = best_discrete;
    node->attr_idx = best_attr_idx;
    node->base = best_base;

    n_left = bitset_numset(bitset_left);
    n_right = bitset_numset(bitset_right);

    new_params->bitset_left = NULL;
    new_params->bitset_right = NULL;

    int num_threads;
    pthread_mutex_lock(num_threads_mutex);
    num_threads = *num_threads_ptr;
    if (num_threads < config->max_num_threads)
        (*num_threads_ptr)++;
    pthread_mutex_unlock(num_threads_mutex);

    if (num_threads < config->max_num_threads) {
        new_params->thread_mutex = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(new_params->thread_mutex, NULL);
        pthread_mutex_lock(new_params->thread_mutex);
        new_params->bitset = bitset_left;
        pthread_create(&thid, NULL, decision_tree_train_helper, new_params);
        pthread_mutex_lock(new_params->thread_mutex);
        pthread_mutex_destroy(new_params->thread_mutex);
        free(new_params->thread_mutex);
        new_params->thread_mutex = NULL;
        new_params->bitset = bitset_right;
        node->right = decision_tree_train_helper(new_params);
        pthread_join(thid, &async_left);
        node->left = async_left;
        pthread_mutex_lock(num_threads_mutex);
        (*num_threads_ptr)--;
        pthread_mutex_unlock(num_threads_mutex);
    } else {
        new_params->bitset = bitset_left;
        node->left = decision_tree_train_helper(new_params);
        new_params->bitset = bitset_right;
        node->right = decision_tree_train_helper(new_params);
    }

    bitset_destroy(bitset_left);
    bitset_destroy(bitset_right);
    free(new_params);

    return node;
}

static int validate_config(DTTrainConfig* config)
{
    int type_valid = (
            config->type == DT_CLASSIFIER
        ||  config->type == DT_REGRESSOR
    );
    if (!type_valid) {
        puts("Config type must be DT_CLASSIFIER or DT_REGRESSOR");
        return 0;
    }

    int classifier_condition_valid = (
            config->splitter == DT_SPLIT_ENTROPY
        ||  config->splitter == DT_SPLIT_GINI
        ||  config->splitter == DT_SPLIT_ERROR
    );
    if (config->type == DT_CLASSIFIER && !classifier_condition_valid) {
        puts("Config condition for classifier must be DT_SPLIT_ENTROPY, DT_SPLIT_GINI, or DT_SPLIT_ERROR");
        return 0;
    }

    int regressor_condition_valid = (
            config->splitter == DT_SPLIT_MSE
        ||  config->splitter == DT_SPLIT_ABS_ERROR
    );

    if (config->type == DT_REGRESSOR && !regressor_condition_valid) {
        puts("Config condition for regressor must be DT_SPLIT_RMSE or DT_SPLIT_ABS_ERROR");
        return 0;
    }

    if (config->min_samples_split < 2) {
        puts("Config minimum samples split must be at least 2");
        return 0;
    }

    if (config->max_depth <= 0) {
        puts("Config max depth must be greater than 0");
        return 0;
    }

    if (config->max_num_threads <= 0) {
        puts("Config max num threads must be greater than 0");
        return 0;
    }

    return 1;
}

void decision_tree_train(DecisionTree* dt, int num_labels, float* attr, void* labels)
{
    Bitset* bitset = bitset_create(num_labels);
    bitset_setall(bitset);
    if (dt->root != NULL)
        dtnode_destroy(dt->root);
    DTTrainParams* params = malloc(sizeof(DTTrainParams));
    int num_threads = 1;
    params->config = &dt->config;
    if (!validate_config(params->config)) {
        free(params);
        return;
    }
    params->num_attr = dt->num_attr;
    params->num_labels = num_labels;;
    params->labels = (int*)labels;
    params->num_unique_labels = get_num_unique_labels(num_labels, labels);
    params->unique_labels = get_unique_labels(params->num_unique_labels, num_labels, labels);
    params->attr = attr;
    params->bitset = bitset;
    params->depth = 0;
    params->thread_mutex = NULL;
    params->num_threads_ptr = &num_threads;
    params->num_threads_mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(params->num_threads_mutex, NULL);

    puts("Training decision tree");
    clock_t t = clock();
    dt->root = decision_tree_train_helper(params);
    t = clock() - t;
    printf("Trained in %f s\n", ((double)t)/CLOCKS_PER_SEC);

    pthread_mutex_destroy(params->num_threads_mutex);
    free(params->unique_labels);
    free(params->num_threads_mutex);
    free(params);
    bitset_destroy(bitset);
}

void decision_tree_destroy(DecisionTree* dt)
{
    if (dt->attr_names != NULL)
        for (int i = 0; i < dt->num_attr; i++)
            free(dt->attr_names[i]);
    free(dt->attr_names);
    dtnode_destroy(dt->root);
    free(dt); 
}

static void* decision_tree_predict(DecisionTree* dt, float* attr)
{
    int (*cmp)(float, float);
    int attr_idx;
    float value;
    DTNode* cur = dt->root;

    while (!dtnode_isleaf(cur)) {
        cmp = (cur->discrete) ? cmp_discrete : cmp_continuous;
        attr_idx = cur->attr_idx;
        value = attr[attr_idx];
        if (cmp(cur->base, value))
            cur = cur->right;
        else
            cur = cur->left;
    }

    return &cur->label;
}

static void* decision_tree_predict_verbose(DecisionTree* dt, float* attr)
{
    int (*cmp)(float, float);
    int attr_idx;
    float value;
    DTNode* cur = dt->root;
    const char* eq;
    const char* res;
    const char* name;
    char buf[32];

    while (!dtnode_isleaf(cur)) {
        cmp = (cur->discrete) ? cmp_discrete : cmp_continuous;
        attr_idx = cur->attr_idx;
        value = attr[attr_idx];

        if (cur->discrete)
            eq = "==";
        else
            eq = "<=";

        if (cmp(cur->base, value))
            res = "Yes";
        else
            res = "No";

        if (dt->attr_names != NULL && dt->attr_names[attr_idx] != NULL)
            name = dt->attr_names[attr_idx];
        else {
            sprintf(buf, "attribute %d", attr_idx+1);        
            name = buf;
        }

        printf("Is %20s = %8.4f %s %8.4f? %s\n", name, value, eq, cur->base, res);

        if (cmp(cur->base, value))
            cur = cur->right;
        else
            cur = cur->left;
    }

    return &cur->label;
}

int decision_tree_classifier_predict(DecisionTree* dt, float* attr)
{
    if (dt->root == NULL)
        return 0;
    return *(int*)decision_tree_predict(dt, attr);
}

int decision_tree_classifier_predict_verbose(DecisionTree* dt, float* attr)
{
    if (dt->root == NULL)
        return 0;
    return *(int*)decision_tree_predict_verbose(dt, attr);
}

float decision_tree_regressor_predict(DecisionTree* dt, float* attr)
{
    if (dt->root == NULL)
        return 0;
    return *(float*)decision_tree_predict(dt, attr);
}

float decision_tree_regressor_predict_verbose(DecisionTree* dt, float* attr)
{
    if (dt->root == NULL)
        return 0;
    return *(float*)decision_tree_predict_verbose(dt, attr);
}

int* decision_tree_classifier_test(DecisionTree* dt, int num_labels, float* attr)
{
    int* predictions = malloc(num_labels * sizeof(int));
    for (int i = 0; i < num_labels; i++)
        predictions[i] = decision_tree_classifier_predict(dt, attr+(i*dt->num_attr));
    return predictions;
}

float* decision_tree_regressor_test(DecisionTree* dt, int num_labels, float* attr)
{
    float* predictions = malloc(num_labels * sizeof(float));
    for (int i = 0; i < num_labels; i++)
        predictions[i] = decision_tree_regressor_predict(dt, attr+(i*dt->num_attr));
    return predictions;
}

static int get_num_nodes(DTNode* node)
{
    if (node == NULL) return 0;
    return 1 + get_num_nodes(node->left) + get_num_nodes(node->right);
}

static void write_preorder(FILE* fptr, DTNode* node, int* idx, DTNode** preorder)
{
    if (node == NULL)
        return;
    preorder[(*idx)++] = node;
    fwrite(&node->base, sizeof(int), 1, fptr);
    fwrite(&node->attr_idx, sizeof(int), 1, fptr);
    fwrite(&node->discrete, sizeof(int), 1, fptr);
    fwrite(&node->label, sizeof(int), 1, fptr);
    write_preorder(fptr, node->left, idx, preorder);
    write_preorder(fptr, node->right, idx, preorder);
}

static void write_inorder(FILE* fptr, DTNode* node, DTNode** preorder)
{
    if (node == NULL)
        return;
    write_inorder(fptr, node->left, preorder);
    int idx = 0;
    while (preorder[idx] != node)
        idx++;
    fwrite(&idx, sizeof(int), 1, fptr);
    write_inorder(fptr, node->right, preorder);
}

void decision_tree_write(DecisionTree* dt, const char* path)
{
    int i, n, m, idx;
    DTNode** preorder;
    FILE* fptr;
    
    if (dt->root == NULL) {
        puts("Nothing to write for decision tree");
        return;
    }

    fptr = fopen(path, "wb");
    if (fptr == NULL) {
        printf("Failed to open path: %s\n", path);
        return;
    }

    fwrite(&dt->config, sizeof(DTTrainConfig), 1, fptr);
    fwrite(&dt->num_attr, sizeof(int), 1, fptr);
    n = (dt->attr_names == NULL) ? 0 : dt->num_attr;
    fwrite(&n, sizeof(int), 1, fptr);
    for (i = 0; i < dt->num_attr; i++) {
        m = (dt->attr_names[i]) ? strlen(dt->attr_names[i]) : 0;
        fwrite(&m, sizeof(int), 1, fptr);
        fwrite(dt->attr_names[i], sizeof(char), m, fptr);
    }

    idx = 0;
    n = get_num_nodes(dt->root);
    preorder = malloc(n * sizeof(DTNode*));
    fwrite(&n, sizeof(int), 1, fptr);
    write_preorder(fptr, dt->root, &idx, preorder);
    write_inorder(fptr, dt->root, preorder);

    free(preorder);
    fclose(fptr);

    printf("Successfully wrote decision tree to %s\n", path);
}

static void read_attr_names(FILE* fptr, DecisionTree* dt)
{
    int i, n, m;
    fread(&dt->num_attr, sizeof(int), 1, fptr);
    fread(&n, sizeof(int), 1, fptr);
    if (n == 0) {
        dt->attr_names = NULL;
        return;
    }
    dt->attr_names = malloc(dt->num_attr * sizeof(char*));
    for (i = 0; i < dt->num_attr; i++) {
        fread(&m, sizeof(int), 1, fptr);
        if (m == 0) {
            dt->attr_names[i] = NULL;
            continue;
        }
        dt->attr_names[i] = malloc((m+1) * sizeof(char));
        fread(dt->attr_names[i], sizeof(char), m, fptr);
        dt->attr_names[i][m] = '\0';
    }
}

static DTNode* construct_tree(DTNode** preorder, int* inorder, int pl, int pr, int il, int ir)
{
    int i, n_left;
    DTNode* root = preorder[pl];

    if (pl == pr) {
        root->left = NULL;
        root->right = NULL;
        return root;
    }

    for (i = il; i <= ir; i++)
        if (inorder[i] == pl)
            break;

    n_left = i - il;
    root->left = construct_tree(preorder, inorder, pl+1, pl+n_left, il, i-1);
    root->right = construct_tree(preorder, inorder, pl+n_left+1, pr, i+1, ir);

    return root;
}

DecisionTree* decision_tree_read(const char* path)
{
    int i, n;
    DTNode* node;
    DTNode** preorder;
    int* inorder;
    DecisionTree* dt;

    FILE* fptr = fopen(path, "rb");
    if (fptr == NULL) {
        printf("Could not open %s\n", path);
        return NULL;
    }

    dt = malloc(sizeof(DecisionTree));
    fread(&dt->config, sizeof(DTTrainConfig), 1, fptr);
    read_attr_names(fptr, dt);
    fread(&n, sizeof(int), 1, fptr);
    preorder = malloc(n * sizeof(DTNode*));
    for (i = 0; i < n; i++) {
        node = malloc(sizeof(DTNode));
        fread(&node->base, sizeof(int), 1, fptr);
        fread(&node->attr_idx, sizeof(int), 1, fptr);
        fread(&node->discrete, sizeof(int), 1, fptr);
        fread(&node->label, sizeof(int), 1, fptr);
        preorder[i] = node;
    }
    inorder = malloc(n * sizeof(int));
    fread(inorder, sizeof(int), n, fptr);

    dt->root = construct_tree(preorder, inorder, 0, n-1, 0, n-1);

    free(preorder);
    free(inorder);
    return dt;
}
