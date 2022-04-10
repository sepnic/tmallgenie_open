#include <stdio.h>
#include <string.h>
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"

#define LOG_TAG "memdbgtest"

class TEST {
public:
    TEST() { value = 0; };
    TEST(int i) { value = i; };
    ~TEST() {};
private:
    int value;
};

int main()
{
    void *ptr1 = OS_MALLOC(1);
    void *ptr2 = OS_CALLOC(1, 2);
    ptr2 = OS_REALLOC(ptr2, 3);
    OS_MEMORY_DUMP();

    OS_FREE(ptr1);
    OS_FREE(ptr2);
    OS_MEMORY_DUMP();

    const char *str = "we product overflow here";
    void *ptr4 = OS_MALLOC(strlen(str));
    sprintf((char *)ptr4, "%s", str);
    OS_FREE(ptr4);
    OS_MEMORY_DUMP();

    TEST *test1, *test2;
    OS_NEW(test1, TEST);
    OS_NEW(test2, TEST, 2);
    OS_CLASS_DUMP();

    OS_DELETE(test1);
    OS_DELETE(test2);
    OS_CLASS_DUMP();
    return 0;
}
