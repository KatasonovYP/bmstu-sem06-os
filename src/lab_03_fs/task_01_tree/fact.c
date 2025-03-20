#include <stdio.h>

int factorial(int n, int depth)
{
    for (int i = 0; i < depth; i++)
    {
        printf("  ");
    }

    printf("CALL: factorial(%d)\n", n);

    int result;
    if (n <= 1)
    {
        result = 1;
    }
    result = n * factorial(n - 1, depth + 1);

    for (int i = 0; i < depth; i++)
    {
        printf("  ");
    }
    printf("RETURN: factorial(%d) = %d\n", n, result);

    return result;
}

int main()
{
    int n = 5;
    printf("calc factorial(%d):\n", n);
    int result = factorial(n, 0);

    printf("\nresult: factorial(%d) = %d\n", n, result);

    return 0;
}
