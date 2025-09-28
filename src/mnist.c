#include "tests.h"
#include "decisiontree.h"
#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <stb_image.h>
#include <stb_image_write.h>

#define IMAGE_LENGTH 28

#define MNIST_TRAIN_IMAGES_COUNT    60000
#define MNIST_TRAIN_IMAGES_PATH     "datasets/mnist/train-images.idx3-ubyte"
#define MNIST_TRAIN_LABELS_PATH     "datasets/mnist/train-labels.idx1-ubyte"
#define MNIST_T10K_IMAGES_COUNT     10000
#define MNIST_T10K_IMAGES_PATH      "datasets/mnist/t10k-images.idx3-ubyte"
#define MNIST_T10K_LABELS_PATH      "datasets/mnist/t10k-images.idx1-ubyte"

typedef unsigned char u8;

typedef struct {
    int id;
    u8 label;
    u8 data[IMAGE_LENGTH * IMAGE_LENGTH];
} Image;

static void image_write(Image image)
{
    char buf[128];
    sprintf(buf, "data/%d-%d.png", image.id, image.label);
    stbi_write_png(buf, IMAGE_LENGTH, IMAGE_LENGTH, 1, image.data, 0);
}

static Image* read_images(void)
{
    FILE* images_file = fopen(MNIST_TRAIN_IMAGES_PATH, "rb");
    FILE* labels_file = fopen(MNIST_TRAIN_LABELS_PATH, "rb");

    u8 garbage[16];
    fread(garbage, sizeof(u8), 16, images_file);
    fread(garbage, sizeof(u8), 8, labels_file);

    Image* images = malloc(MNIST_TRAIN_IMAGES_COUNT * sizeof(Image));
    for (int i = 0; i < MNIST_TRAIN_IMAGES_COUNT; i++) {
        Image image;
        image.id = i;
        fread(&image.label, sizeof(u8), 1, labels_file);
        fread(&image.data[0], sizeof(u8), IMAGE_LENGTH * IMAGE_LENGTH, images_file);
        images[i] = image;
    }

    fclose(labels_file);
    fclose(images_file);

    return images;
}

static void set_matrix_row(Matrix* mat, int row, u8* data)
{
    for (int i = 0; i < mat->cols; i ++)
        mat->buffer[row * mat->cols + i] = data[i];
}

void mnist_test()
{
    Image* images = read_images();    

    Matrix* mat = matrix_create(MNIST_TRAIN_IMAGES_COUNT, IMAGE_LENGTH * IMAGE_LENGTH);
    int* labels = malloc(MNIST_TRAIN_IMAGES_COUNT * sizeof(int));

    for (int i = 0; i < MNIST_TRAIN_IMAGES_COUNT; i++) {
        labels[i] = images[i].label;
        set_matrix_row(mat, i, images[i].data);
    }

    DecisionTree* dt = decision_tree_create(IMAGE_LENGTH * IMAGE_LENGTH, NULL);

    DTTrainConfig config = (DTTrainConfig) {
        .type = DT_CLASSIFIER,
        .splitter = DT_SPLIT_ENTROPY,
        .max_depth = 2,
        .max_num_threads = 20
    };

    decision_tree_config(dt, config);

    decision_tree_train(dt, MNIST_TRAIN_IMAGES_COUNT, mat->buffer, labels);

    free(images);
    free(labels);
    matrix_destroy(mat);
    decision_tree_destroy(dt);
}
