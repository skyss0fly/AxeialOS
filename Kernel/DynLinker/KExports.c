#include <KExports.h>
#include <KrnPrintf.h>
#include <String.h>

void*
KexpLookup(const char* __Name__)
{
    /* Validate input parameter */
    if (!__Name__)
    {
        return 0;
    }

    /* Iterate through the export table */
    const KExport* Cur = __start_kexports;
    const KExport* End = __stop_kexports;

    while (Cur < End)
    {
        /* Check if symbol name matches */
        if (Cur->Name && strcmp(Cur->Name, __Name__) == 0)
        {
            return Cur->Addr;
        }

        Cur++;
    }

    /* Symbol not found */
    return 0;
}

void
KexpDump(void)
{
    /* Get pointers to export table boundaries */
    const KExport* Cur = __start_kexports;
    const KExport* End = __stop_kexports;

    /* Print header message */
    PInfo("KExports: Listing all kernel exports:\n");

    /* Iterate and print each symbol */
    while (Cur < End)
    {
        KrnPrintf("  %s => %p\n", Cur->Name, Cur->Addr);
        Cur++;
    }
}
