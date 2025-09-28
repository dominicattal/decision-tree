#ifndef DECISIONTREE_H
#define DECISIONTREE_H

typedef struct DecisionTree DecisionTree;

typedef enum {

    // Decision tree types
    DT_CLASSIFIER,
    DT_REGRESSOR,

    // DT_CLASSIFIER split conditions
    DT_SPLIT_ENTROPY,
    DT_SPLIT_GINI,
    DT_SPLIT_ERROR,

    // DT_REGRESSOR split conditions
    DT_SPLIT_RMSE,

} DTEnum;

typedef struct {
    DTEnum  type;
    DTEnum  condition;
    int     discrete_threshold;
    int     max_depth;
    int     max_num_threads;
} DTTrainConfig;

// Create a decision tree with the default config and num_attr names, specified in attr_names
// Passing NULL as attr_names will make unnamed attributes
// Any element in attr_names that is NULL will remain unnamed
DecisionTree*   decision_tree_create(int num_attr, const char** attr_names);

// Destroy a decision tree
void            decision_tree_destroy(DecisionTree* dt); 

// Read and write a decision tree to avoid retraining
DecisionTree*   decision_tree_read(const char* path);
void            decision_tree_write(DecisionTree* dt, const char* path);

// Refit the decision tree to new attributes. Forgets old tree.
void            decision_tree_set_attr(DecisionTree* dt, int num_attr, const char** attr_names);

// Returns the default config:
//      type = DT_CLASSIFIER
//      condition = DT_SPLIT_ENTROPY
//      discrete_threshold = 5
//      max_depth = 8
//      max_num_threads = 1
DTTrainConfig   decision_tree_default_config(void);

// Sets the config for the current decision tree
void            decision_tree_config(DecisionTree* dt, DTTrainConfig config);

// Trains the decision tree with attributes and labels
// The decision tree's config is validated before training. If it fails, the decision tree is not
// altered and a message is printed.
// num_labels is the number of labels used for testing
// attr is a num_labels * num_attr flattened 2D array. each row corresponds to the element in labels
// labels can be either an integer array or a float array for classifiers or regressors respectively
void            decision_tree_train(DecisionTree* dt, int num_labels, float* attr, void* labels);

// Returns the predicted label for a decision tree classifier
int             decision_tree_classifier_predict(DecisionTree* dt, float* attr);

// Returns the predicted value for a decision tree regressor
float           decision_tree_regressor_predict(DecisionTree* dt, float* attr);

#endif

