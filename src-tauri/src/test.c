// src/test.c
/******** Notes about FFI and testing in rust ********
 * Its not bad i guess, definitely an ok way to test if a c program will run, maybe even cpp
 * One caveat is the build sometimes fucks up when you try and do it live from tests
 * cargo build --tests to rebuild, do need to clean out target everytime this is changed
*/

#include <stdio.h>

void c_test_rust_ffi(int num)
{
    printf("[  c ] num is %d", num);
}

void c_test_increment_ptr()
{
    int arr[] = {1, 2, 3, 4, 5};
    int *ptr = arr;      // Initialize pointer to the beginning of the array
    printf("%d ", *ptr); // Pri
    // Iterate through the array using pointer arithmetic
    ptr += 2;
    printf("%d ", *ptr); // Pri
    printf("\n");
}

void c_print_hex()
{
    char *thing = "a\n";
    printf("thing, %p hey: %s", *(thing + 1), thing);
}

void c_test_crashing()
{
    printf("Crashing\n");
    
    char *test_buf = malloc(2000);
    // Following crashes
    for (int i = 0; i < 5; i++)
    {
        free(test_buf);
    }

    printf("\n");
}