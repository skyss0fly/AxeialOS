#include <AllTypes.h>

/** @internal Internal String and Libc Functions for the kernel. */

/*Strcpy*/
void
StringCopy(char *__Dest__, const char *__Src__, uint32_t __MaxLen__)
{
    uint32_t Index = 0;
    while (__Src__[Index] && Index < (__MaxLen__ - 1))
    {
        __Dest__[Index] = __Src__[Index];
        Index++;
    }
    __Dest__[Index] = '\0';
}

/*memcpy*/
void 
*__builtin_memcpy(void *__Dest__, const void *__Src__, size_t __Size__)
{
    char *Dest = __Dest__;
    const char *Src = __Src__;

    while (__Size__--) {
        *Dest++ = *Src++;
    }

    return __Dest__;
}

/*Memset*/
void*
memset(void* __Dest__, int __Value__, size_t __Index__)
{
    unsigned char* ptr = (unsigned char*)__Dest__;

    while (__Index__--)
        *ptr++ = (unsigned char)__Value__;

    return __Dest__;
}

/*Strcmp*/
int
strcmp(const char *__Str1__, const char *__Str2__)
{
    while (*__Str1__ && (*__Str1__ == *__Str2__))
    {
        __Str1__++;
        __Str2__++;
    }

    return (unsigned char)(*__Str1__) - (unsigned char)(*__Str2__);
}

/*Strlen*/
size_t
strlen(const char *__Str__)
{
    size_t __Size__ = 0;
    while (__Str__[__Size__] != '\0')
        __Size__++;
    return __Size__;
}

/*Strncmp*/
int
strncmp(const char *__S1__, const char *__S2__, size_t __Size__)
{
    for (size_t __i = 0; __i < __Size__; __i++)
    {
        unsigned char c1 = (unsigned char)__S1__[__i];
        unsigned char c2 = (unsigned char)__S2__[__i];

        if (c1 != c2)
            return (c1 < c2) ? -1 : 1;

        if (c1 == '\0')
            return 0;
    }
    return 0;
}

/*Strchr*/
char *
strrchr(const char *__Str__, int __Idx__)
{
    const char *LastChar = 0;
    char Char = (char)__Idx__;

    if (!__Str__) return 0;

    while (*__Str__)
    {
        if (*__Str__ == Char)
            LastChar = __Str__;
        __Str__++;
    }

    /* Also check terminating null if __Idx__ == '\0' */
    if (Char == '\0')
        return (char*)__Str__;

    return (char*)LastChar;
}
