#include "csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define csv_malloc(size)    malloc(size)
#define csv_free(ptr)       free(ptr)
#define csv_print(s, ...)   printf(s "\n", __VA_ARGS__)

static void get_dimensions(FILE* fptr, int* num_rows, int* num_cols)
{
    char ch;
    int nr, nc, c;
    nr = nc = c = 0;
    while ((ch = fgetc(fptr)) != EOF) {
        if (ch == ',') {
            c += 1;
        }
        else if (ch == '\n') {
            nc = (c+1 > nc) ? c+1 : nc;
            c = 0;
            nr += 1;
        }
    }
    *num_rows = nr;
    *num_cols = nc;
}

// 0 = empty, 1 = int, 2 = float, 3 = string
static int substr_type(int n, const char* s)
{
    if (n == 0) 
        return 0;
    int res = 1;
    for (int i = 0; i < n; i++) {
        if (!isdigit(s[i])) {
            if (s[i] == '.')
                res = 2;
            else
                return 3;
        }
    }
    return res;
}

static Cell read_next_cell(FILE* fptr)
{
    Cell cell;
    int i, len, type;
    char c;
    char* substr;
    long long start;

    start = ftell(fptr);

    len = 0;

    do { 
        c = fgetc(fptr);
        len++;
    } while (c != ',' && c != '\n' && c != EOF);

    substr = csv_malloc(len * sizeof(char));

    fseek(fptr, start, SEEK_SET);

    for (i = 0; i < len; i++)
        substr[i] = fgetc(fptr);
    substr[len-1] = '\0';
    for (i = len-1; i >= 0; i--) {
        if (substr[i] == '\r') {
            substr[i] = '\0';
            len--;
        }
    }

    type = substr_type(len-1, substr);
    if (type == 0) {
        cell.type = CSV_EMPTY;
        csv_free(substr);
    } else if (type == 1) {
        cell.type = CSV_INT;
        cell.val_int = atol(substr);
        csv_free(substr);
    } else if (type == 2) {
        cell.type = CSV_FLOAT;
        cell.val_float = atof(substr);
        csv_free(substr);
    } else {
        cell.type = CSV_STRING;
        cell.val_string = substr;
    }

    return cell;
}

CSV* csv_read(const char* path)
{
    FILE* fptr;
    CSV* csv;
    Cell* cells;
    int num_rows, num_cols;
    int row, col;

    csv = NULL;

    fptr = fopen(path, "rb");
    if (fptr == NULL) {
        csv_print("Could not open csv file for reading: %s", path);
        return NULL;
    }

    get_dimensions(fptr, &num_rows, &num_cols);

    fseek(fptr, SEEK_SET, 0);

    cells = csv_malloc(num_rows * num_cols * sizeof(Cell));
    for (row = 0; row < num_rows; row++)
        for (col = 0; col < num_cols; col++)
            cells[row * num_cols + col] = read_next_cell(fptr);

    fclose(fptr);

    csv = csv_malloc(sizeof(CSV));
    csv->num_rows = num_rows;
    csv->num_cols = num_cols;
    csv->cells = cells;
    csv->trie = trie_create();

    return csv;
}

void csv_write(CSV* csv, const char* path)
{
    FILE* fptr;
    Cell cell;
    int row, col;

    fptr = fopen(path, "w");
    if (fptr == NULL) {
        csv_print("Could not open csv file for writing: %s", path);
        return;
    }

    for (row = 0; row < csv->num_rows; row++) {
        for (col = 0; col < csv->num_cols; col++) {
            cell = *csv_cell(csv, row, col);
            if (cell.type == CSV_EMPTY)
                ;
            else if (cell.type == CSV_INT)
                fprintf(fptr, "%lld", cell.val_int);
            else if (cell.type == CSV_FLOAT)
                fprintf(fptr, "%f", cell.val_float);
            else
                fprintf(fptr, cell.val_string);
            fprintf(fptr, ((col+1) % csv->num_cols == 0) ? "\n" : ",");
        }
    }

    fflush(fptr);
    fclose(fptr);
}

void csv_destroy(CSV* csv)
{
    for (int i = 0; i < csv->num_rows * csv->num_cols; i++)
        if (csv->cells[i].type == CSV_STRING)
            csv_free(csv->cells[i].val_string);
    trie_destroy(csv->trie);
    csv_free(csv->cells);
    csv_free(csv);
}

int* csv_column_int(CSV* csv, const char* col_name)
{
    int* arr;
    Cell cell;
    int row, col;
    int row_start, row_end;

    col = csv_column_id(csv, col_name);
    if (col == -1) {
        csv_print("Could not find column %s to flatten to int arr", col_name);
        return NULL;
    }
    row_start = 1;
    row_end = csv->num_rows - 1;

    arr = csv_malloc((row_end-row_start+1) * sizeof(int));

    for (row = row_start; row <= row_end; row++) {
        cell = *csv_cell(csv, row, col);
        if (cell.type != CSV_INT) {
            csv_print("Invalid cell type when flattening column %s to int array", col_name);
            csv_free(arr);
            return NULL;
        }
        arr[row-row_start] = cell.val_int;
    }

    return arr;
}

long long* csv_column_long(CSV* csv, const char* col_name)
{
    long long* arr;
    Cell cell;
    int row, col;
    int row_start, row_end;

    col = csv_column_id(csv, col_name);
    if (col == -1) {
        csv_print("Could not find column %s to flatten to int arr", col_name);
        return NULL;
    }
    row_start = 1;
    row_end = csv->num_rows - 1;

    arr = csv_malloc((row_end-row_start+1) * sizeof(long long));

    for (row = row_start; row <= row_end; row++) {
        cell = *csv_cell(csv, row, col);
        if (cell.type != CSV_INT) {
            csv_print("Invalid cell type when flattening column %s to int array", col_name);
            csv_free(arr);
            return NULL;
        }
        arr[row-row_start] = cell.val_int;
    }

    return arr;
}

float* csv_column_float(CSV* csv, const char* col_name)
{
    float* arr;
    Cell cell;
    int row, col;
    int row_start, row_end;

    col = csv_column_id(csv, col_name);
    if (col == -1) {
        csv_print("Could not find column %s to flatten to float arr", col_name);
        return NULL;
    }
    row_start = 1;
    row_end = csv->num_rows - 1;

    arr = csv_malloc((row_end-row_start+1) * sizeof(float));

    for (row = row_start; row <= row_end; row++) {
        cell = *csv_cell(csv, row, col);
        if (cell.type == CSV_INT)
            arr[row-row_start] = (float)cell.val_int;
        else if (cell.type == CSV_FLOAT)
            arr[row-row_start] = cell.val_float;
        else {
            csv_print("Invalid cell type when flattening column %s to float array", col_name);
            csv_free(arr);
            return NULL;
        }
    }

    return arr;
}

double* csv_column_double(CSV* csv, const char* col_name)
{
    double* arr;
    Cell cell;
    int row, col;
    int row_start, row_end;

    col = csv_column_id(csv, col_name);
    if (col == -1) {
        csv_print("Could not find column %s to flatten to float arr", col_name);
        return NULL;
    }
    row_start = 1;
    row_end = csv->num_rows - 1;

    arr = csv_malloc((row_end-row_start+1) * sizeof(double));

    for (row = row_start; row <= row_end; row++) {
        cell = *csv_cell(csv, row, col);
        if (cell.type == CSV_INT)
            arr[row-row_start] = (double)cell.val_int;
        else if (cell.type == CSV_FLOAT)
            arr[row-row_start] = cell.val_float;
        else {
            csv_print("Invalid cell type when flattening column %s to float array", col_name);
            csv_free(arr);
            return NULL;
        }
    }

    return arr;
}

char** csv_column_string(CSV* csv, const char* col_name)
{
    char** arr;
    Cell cell;
    int col, row, n;
    char buf[64];
    char* string;
    int row_start, row_end;

    col = csv_column_id(csv, col_name);
    if (col == -1) {
        csv_print("Could not find column %s to flatten to string arr", col_name);
        return NULL;
    }
    row_start = 1;
    row_end = csv->num_rows - 1;

    arr = csv_malloc((row_end-row_start+1) * sizeof(char*));

    for (row = row_start; row <= row_end; row++) {
        cell = *csv_cell(csv, row, col);
        if (cell.type == CSV_EMPTY) {
            arr[row-row_start] = "";
        } else if (cell.type == CSV_INT) {
            sprintf(buf, "%lld", cell.val_int);
            n = strlen(buf);
            string = csv_malloc((n+1) * sizeof(char));
            strncpy(string, buf, n+1);
            arr[row-row_start] = string;
        } else if (cell.type == CSV_FLOAT) {
            sprintf(buf, "%f", cell.val_float);
            n = strlen(buf);
            string = csv_malloc((n+1) * sizeof(char));
            strncpy(string, buf, n+1);
            arr[row-row_start] = string;
        } else {
            n = strlen(cell.val_string);
            string = csv_malloc((n+1) * sizeof(char));
            strncpy(string, cell.val_string, n+1);
            arr[row-row_start] = string;
        }
    }

    return arr;
}

void csv_encode(CSV* csv, const char* col_name)
{
    Cell* cell;
    Trie* trie;
    char* tmp;
    int row, col;
    int row_start, row_end;

    trie = csv->trie;

    col = csv_column_id(csv, col_name);
    if (col == -1) {
        csv_print("Could not find column %s to encode", col_name);
        return;
    }
    row_start = 1;
    row_end = csv->num_rows - 1;

    for (row = row_start; row <= row_end; row++) {
        cell = csv_cell(csv, row, col);
        if (cell->type != CSV_STRING) {
            csv_print("Could not encode %s cell at (%d %d)", csv_cell_type_str(cell), row, col);
            continue;
        }
        tmp = cell->val_string;
        if (!trie_contains(trie, tmp))
            trie_insert(trie, tmp);
        cell->val_int = trie_key_id(trie, tmp);
        cell->type = CSV_INT;
        csv_free(tmp);
    }
}

const char* csv_decode(CSV* csv, int id)
{
    return trie_id_key(csv->trie, id);
}

void csv_one_hot_encode(CSV* csv, const char* col_name)
{
    Trie* trie;
    Cell* new_cells;
    Cell* cell;
    char* string;
    char* string_copy;
    int i, j, n;
    int row, col, new_num_cols;
    int row_start, row_end;
    int new_idx, id, len;

    col = csv_column_id(csv, col_name);
    if (col == -1) {
        csv_print("Could not find column %s to one-hot encode", col_name);
        return;
    }
    row_start = 1;
    row_end = csv->num_rows - 1;

    trie = trie_create();

    for (row = row_start; row <= row_end; row++) {
        cell = csv_cell(csv, row, col);
        if (cell->type != CSV_STRING) {
            csv_print("Could not one-hot encode %s cell at (%d %d)", csv_cell_type_str(cell), row, col);
            continue;
        }
        if (!trie_contains(trie, cell->val_string))
            trie_insert(trie, cell->val_string);
    }
    
    n = trie_num_unique_keys(trie);
    new_num_cols = csv->num_cols + n - 1;
    new_cells = csv_malloc(csv->num_rows * new_num_cols * sizeof(Cell));

    for (i = 0; i < csv->num_rows * new_num_cols; i++) {
        new_cells[i] = (Cell) {
            .type = CSV_INT,
            .val_int = 0
        };
    }

    for (i = 0, j = 0; i < csv->num_cols; i++) {
        if (i == col) 
            continue;
        new_cells[j] = csv->cells[i];
        j++;
    }

    for (i = 0; i < n; i++) {
        string = trie_id_key(trie, i);
        len = strlen(string);
        string_copy = csv_malloc((len+1) * sizeof(char));
        strncpy(string_copy, string, len+1);
        new_cells[csv->num_cols+i-1] = (Cell) {
            .type = CSV_STRING,
            .val_string = string_copy
        };
    }

    for (row = 1; row < csv->num_rows; row++) {
        j = 0;
        for (i = 0; i < csv->num_cols; i++) {
            cell = csv_cell(csv, row, i);
            if (i == col) {
                if (cell->type != CSV_STRING)
                    continue;
                if (!trie_contains(trie, cell->val_string))
                    continue;
                id = trie_key_id(trie, cell->val_string);
                new_idx = row * new_num_cols + csv->num_cols + id - 1;
                new_cells[new_idx] = (Cell) {
                    .type = CSV_INT,
                    .val_int = 1
                };
                csv_free(cell->val_string);
            } else {
                new_idx = row * new_num_cols + j;
                new_cells[new_idx] = *cell;
                j++;
            }
        }
    }

    csv_free(csv->cells);
    csv->cells = new_cells;
    csv->num_cols = new_num_cols;

    trie_destroy(trie);
}

int csv_num_rows(CSV* csv)
{
    return csv->num_rows;
}

int csv_num_cols(CSV* csv)
{
    return csv->num_cols;
}

const char* csv_column_name(CSV* csv, int col)
{
    return csv_string(csv, 0, col);
}

int csv_column_id(CSV* csv, const char* col_name)
{
    Cell* cell;
    for (int i = 0; i < csv->num_rows; i++) {
        cell = csv_cell(csv, 0, i);
        if (cell->type != CSV_STRING)
            continue;
        if (strcmp(cell->val_string, col_name) != 0)
            continue;
        return i;
    }
    return -1;
}

Cell* csv_cell(CSV* csv, int row, int col)
{
    return &csv->cells[row * csv->num_cols + col];
}

CSVEnum csv_type(CSV* csv, int row, int col)
{
    return csv->cells[row * csv->num_cols + col].type;
}

long long csv_int(CSV* csv, int row, int col)
{
    return csv->cells[row * csv->num_cols + col].val_int;
}

double csv_float(CSV* csv, int row, int col)
{
    return csv->cells[row * csv->num_cols + col].val_float;
}

const char* csv_string(CSV* csv, int row, int col)
{
    return csv->cells[row * csv->num_cols + col].val_string;
}

const char* csv_cell_type_str(Cell* cell)
{
    switch (cell->type) {
        case CSV_EMPTY:
            return "empty";
        case CSV_INT:
            return "int";
        case CSV_FLOAT:
            return "float";
        case CSV_STRING:
            return "string";
    }
    return "error";
}

CSVEnum csv_cell_type(Cell* cell)
{
    return cell->type;
}

long long csv_cell_int(Cell* cell)
{
    return cell->val_int;
}

double csv_cell_float(Cell* cell)
{
    return cell->val_float;
}

const char* csv_cell_string(Cell* cell)
{
    return cell->val_string;
}

