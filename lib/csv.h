#ifndef CSV_H
#define CSV_H

#include "trie.h"

typedef enum {
    
    // Cell types
    CSV_EMPTY, 
    CSV_INT,
    CSV_FLOAT,
    CSV_STRING,

} CSVEnum;

typedef struct {
    CSVEnum type;
    union {
        long long val_int;
        double val_float;
        char* val_string;
    };
} Cell;

typedef struct {
    int num_rows;
    int num_cols;
    Cell* cells;
} CSV;

// Object creation/deletion
CSV*        csv_read(const char* path);
void        csv_write(CSV* csv, const char* path);
void        csv_destroy(CSV* csv);

// ----- CSV Queries -----
// does no type checking
int         csv_num_rows(CSV* csv);
int         csv_num_cols(CSV* csv);
Cell*       csv_cell(CSV* csv, int row, int col);
CSVEnum     csv_type(CSV* csv, int row, int col);
long long   csv_int(CSV* csv, int row, int col);
double      csv_float(CSV* csv, int row, int col);
const char* csv_string(CSV* csv, int row, int col);
const char* csv_column_name(CSV* csv, int col);
int         csv_column_id(CSV* csv, const char* col_name);

// ----- Cell Queries -----
// does no type checking
const char* csv_cell_type_str(Cell* cell);
CSVEnum     csv_cell_type(Cell* cell);
long long   csv_cell_int(Cell* cell);
double      csv_cell_float(Cell* cell);
const char* csv_cell_string(Cell* cell);

// ----- CSV flatten -----
// row_start and row_end are 0-indexed
// flatten a row into a 1D list

// returns NULL if cell is not an int
int*        csv_column_int(CSV* csv, const char* col_name);
long long*  csv_column_long(CSV* csv, const char* col_name);

// returns NULL if cell is not an int or float
float*      csv_column_float(CSV* csv, const char* col_name);
double*     csv_column_double(CSV* csv, const char* col_name);

// works for all types
// each string is copied. you are responsible for freeing memory
char**      csv_column_string(CSV* csv, const char* col_name);

// ----- CSV operations -----

// maps strings to ints for classifying
void        csv_encode(CSV* csv, const char* col_name);

// one hot encode to remove biases
void        csv_one_hot_encode(CSV* csv, const char* col_name);

#endif
