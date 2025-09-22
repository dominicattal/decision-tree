#include "decisiontree.h"
#include <pthread.h>
#include <bitset.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct DTNode DTNode;

typedef struct DTNode {
    DTNode* left;
    DTNode* right;
    float base;
    int attr_idx;
    int discrete;
    int label;
} DTNode;

typedef struct DecisionTree {
    int num_attr;
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

DecisionTree* decision_tree_create(int num_attr)
{
    DecisionTree* dt = malloc(sizeof(DecisionTree));
    dt->num_attr = num_attr;
    dt->root = NULL;
    dt->config = decision_tree_default_config();
    return dt;
}

DTTrainConfig decision_tree_default_config(void)
{
    return (DTTrainConfig) {
        .type = DT_CLASSIFIER,
        .condition = DT_SPLIT_ENTROPY,
        .discrete_threshold = 5,
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

static int get_num_unique_labels(int num_labels, int* labels, Bitset* bitset)
{
    int i, j;
    int label1, label2;
    int num_unique_labels;

    num_unique_labels = 0;
    for (i = 0; i < num_labels; i++) {
        if (!bitset_isset(bitset, i))
            continue;
        label1 = labels[i];
        for (j = 0; j < i; j++) {
            if (!bitset_isset(bitset, j))
                continue;
            label2 = labels[j];
            if (label1 == label2)
                goto not_unqiue;
        }
        num_unique_labels++;
not_unqiue:
    }

    return num_unique_labels;
}

static int* get_unique_labels(int num_unique_labels, int num_labels, int* labels, Bitset* bitset)
{
    int uniq_idx, i, j, label;
    int* unique_labels = malloc(num_unique_labels * sizeof(int));

    uniq_idx = 0;
    for (i = 0; i < num_labels; i++) {
        if (!bitset_isset(bitset, i))
            continue;
        label = labels[i];
        for (j = 0; j < uniq_idx; j++) {
            if (label == unique_labels[j])
                goto not_unique;
        }
        unique_labels[uniq_idx++] = label;
not_unique:
    }

    return unique_labels;
}

typedef struct {
    DTTrainConfig*      config;
    int                 num_labels;
    int                 num_attr;
    float*              attr;
    int*                labels;
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

static float calculate_entropy(int num_labels, int* labels, Bitset* bitset)
{
    float entropy, p;
    int n, label_idx, uniq_idx;
    int num_unique_labels;
    int* unique_labels;
    int* unique_labels_count;

    n = bitset_numset(bitset);
    if (n == 0)
        return 0;

    num_unique_labels = get_num_unique_labels(num_labels, labels, bitset);
    unique_labels = get_unique_labels(num_unique_labels, num_labels, labels, bitset);
    unique_labels_count = calloc(num_unique_labels, sizeof(int));

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

    entropy = 0;
    for (uniq_idx = 0; uniq_idx < num_unique_labels; uniq_idx++) {
        if (unique_labels_count[uniq_idx] == 0)
            continue;
        p = (float)unique_labels_count[uniq_idx] / n;
        entropy -= p * log2(p);
    }

    free(unique_labels);
    free(unique_labels_count);

    return entropy;
}

static float calculate_gini(int num_labels, int* labels, Bitset* bitset)
{
    float gini, p;
    int n, label_idx, uniq_idx;
    int num_unique_labels;
    int* unique_labels;
    int* unique_labels_count;

    n = bitset_numset(bitset);
    if (n == 0)
        return 0;

    num_unique_labels = get_num_unique_labels(num_labels, labels, bitset);
    unique_labels = get_unique_labels(num_unique_labels, num_labels, labels, bitset);
    unique_labels_count = calloc(num_unique_labels, sizeof(int));

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

    gini = 1;
    for (uniq_idx = 0; uniq_idx < num_unique_labels; uniq_idx++) {
        if (unique_labels_count[uniq_idx] == 0)
            continue;
        p = (float)unique_labels_count[uniq_idx] / n;
        gini -= p * p;
    }

    free(unique_labels);
    free(unique_labels_count);

    return gini;
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

static int get_most_common_label(int num_labels, int* labels, Bitset* bitset)
{
    int i, j, most_common_idx, most_common;
    int num_unique_labels = get_num_unique_labels(num_labels, labels, bitset);
    if (num_unique_labels == 0)
        return -1;
    int* unique_labels = get_unique_labels(num_unique_labels, num_labels, labels, bitset);
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

    free(unique_labels);
    free(unique_labels_count);

    return most_common;
}

static void* decision_tree_train_helper(void* void_params)
{
    DTTrainParams*      params              = void_params;
    Bitset*             bitset              = params->bitset;
    DTTrainConfig*      config              = params->config;
    int                 num_labels          = params->num_labels;
    int                 num_attr            = params->num_attr;
    float*              attr                = params->attr;
    int*                labels              = params->labels;
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
    float information_gain, best_information_gain;
    float gini, best_gini;
    float gini_left, gini_right;
    float entropy_parent, entropy_children;
    float entropy_left, entropy_right;
    float best_base;
    pthread_t thid;
    void* async_left;

    new_params = malloc(sizeof(DTTrainParams));
    new_params->config = config;
    new_params->num_labels = num_labels;
    new_params->num_attr = num_attr;
    new_params->attr = attr;
    new_params->labels = labels;
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

    if (depth >= config->max_depth || all_labels_equal(num_labels, labels, bitset)) {
        node->label = get_most_common_label(num_labels, labels, bitset);
        return node;
    }

    bitset_left = bitset_create(num_labels);
    bitset_right = bitset_create(num_labels);
    new_params->bitset_left = &bitset_left;
    new_params->bitset_right = &bitset_right;

    best_information_gain = -1e9;
    best_gini = 1e9;
    best_attr_idx = -1;
    best_base = -1;
    best_discrete = -1;

    for (attr_idx = 0; attr_idx < num_attr; attr_idx++) {
        num_unique_values = get_num_unique_values(num_attr, num_labels, attr, attr_idx, bitset);
        unique_values = get_unique_values(num_unique_values, num_attr, num_labels, attr, attr_idx, bitset);
        n_parent = bitset_numset(bitset);
        new_params->discrete = num_unique_values <= config->discrete_threshold;
        new_params->attr_idx = attr_idx;
        for (uniq_idx = 0; uniq_idx < num_unique_values; uniq_idx++) {
            new_params->base = unique_values[uniq_idx];
            bitset_unsetall(bitset_left);
            bitset_unsetall(bitset_right);
            split(new_params);
            n_left = bitset_numset(bitset_left);
            n_right = bitset_numset(bitset_right);
            if (config->condition == DT_SPLIT_ENTROPY) {
                entropy_parent = calculate_entropy(num_labels, labels, bitset);
                entropy_left = calculate_entropy(num_labels, labels, bitset_left);
                entropy_right = calculate_entropy(num_labels, labels, bitset_right);
                entropy_children = (
                        ((float)n_left  / n_parent) * entropy_left
                    +   ((float)n_right / n_parent) * entropy_right
                );
                information_gain = entropy_parent - entropy_children;
                if (information_gain > best_information_gain) {
                    best_information_gain = information_gain;
                    best_attr_idx = new_params->attr_idx;
                    best_base = new_params->base;
                    best_discrete = new_params->discrete;
                }
            } else if (config->condition == DT_SPLIT_GINI) {
                gini_left = calculate_gini(num_labels, labels, bitset_left);
                gini_right = calculate_gini(num_labels, labels, bitset_right);
                gini = (
                        ((float)n_left  / n_parent) * gini_left
                    +   ((float)n_right / n_parent) * gini_right
                );
                if (gini < best_gini) {
                    best_gini = gini;
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
    if (depth == 2 && node->base == 6) {
        for (int i = 0; i < num_labels; i++) {
            if (!bitset_isset(bitset_right, i))
                continue;
        }
    }

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

    int condition_valid = (
            config->condition == DT_SPLIT_ENTROPY
        ||  config->condition == DT_SPLIT_GINI
        ||  config->condition == DT_SPLIT_ERROR
    );
    if (!condition_valid) {
        puts("Config condition must be DT_SPLIT_ENTROPY, DT_SPLIT_GINI, or DT_SPLIT_ERROR");
        return 0;
    }

    if (config->discrete_threshold < 2) {
        puts("Config discrete threshold must be at least 2");
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

void decision_tree_train(DecisionTree* dt, int num_labels, float* attr, int* labels)
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
    params->attr = attr;
    params->labels = labels;
    params->bitset = bitset;
    params->depth = 0;
    params->thread_mutex = NULL;
    params->num_threads_ptr = &num_threads;
    params->num_threads_mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(params->num_threads_mutex, NULL);
    dt->root = decision_tree_train_helper(params);

    pthread_mutex_destroy(params->num_threads_mutex);
    free(params->num_threads_mutex);
    free(params);
    bitset_destroy(bitset);
}

void decision_tree_destroy(DecisionTree* dt)
{
    dtnode_destroy(dt->root);
    free(dt); 
}

int decision_tree_predict(DecisionTree* dt, float* attr)
{
    if (dt->root == NULL)
        return -1;

    int (*cmp)(float, float);
    DTNode* cur = dt->root;
    const char* eq;
    const char* res;

    while (!dtnode_isleaf(cur)) {
        cmp = (cur->discrete) ? cmp_discrete : cmp_continuous;

        if (cur->discrete)
            eq = "==";
        else
            eq = "<=";

        if (cmp(cur->base, attr[cur->attr_idx]))
            res = "Yes";
        else
            res = "No";

        printf("Is (attr_idx[%d] = %f) %s %f? %s\n", cur->attr_idx, attr[cur->attr_idx], eq, cur->base, res);

        if (cmp(cur->base, attr[cur->attr_idx]))
            cur = cur->right;
        else
            cur = cur->left;
    }

    return cur->label;
}
