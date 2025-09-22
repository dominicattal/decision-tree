#ifndef DECISIONTREE_H
#define DECISIONTREE_H

typedef struct DecisionTree DecisionTree;

typedef enum {
    DT_CLASSIFIER,
    DT_REGRESSOR,
    DT_INT,
    DT_FLOAT,
    DT_SPLIT_ENTROPY,
    DT_SPLIT_GINI,
    DT_SPLIT_ERROR
} DTEnum;

typedef struct {
    DTEnum  type;
    DTEnum  condition;
    int     discrete_threshold;
    int     max_depth;
    int     max_num_threads;
} DTTrainConfig;

// creates and fits a decision tree to the attributes and labels provided
// num_labels is the number of data points to fit from the attr and labels arrays
// num_attr is the number of attributes used for each prediction
// attr_types are the types of each array in attr
//      must be DT_INT, DT_LONG, DT_FLOAT, or DT_DOUBLE
// attr is the data being passed through. should match attr_types but there isnt any checking
// labels are for each corresponding attribute set
// cond is the split condition, either DT_SPLIT_ENTROPY, DT_SPLIT_GINI, DT_SPLIT_ERROR
DecisionTree*   decision_tree_create(int num_attr);
DTTrainConfig   decision_tree_default_config(void);
void            decision_tree_config(DecisionTree* dt, DTTrainConfig config);
void            decision_tree_train(DecisionTree* dt, int num_labels, float* attr, int* labels);
int             decision_tree_predict(DecisionTree* dt, float* attr);
void            decision_tree_destroy(DecisionTree* dt); 



#endif
