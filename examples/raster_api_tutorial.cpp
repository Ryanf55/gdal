#include "gdal_priv.h"

#include <iostream>
#include <errno.h>

int main(int argc, const char* argv[])
{
    if (argc != 2) {
        return EINVAL;
    }
    const char* pszFilename = argv[1];

    GDALDatasetUniquePtr poDataset;
    GDALAllRegister();
    const GDALAccess eAccess = GA_ReadOnly;
    poDataset = GDALDatasetUniquePtr(GDALDataset::FromHandle(GDALOpen( pszFilename, eAccess )));
    if( !poDataset )
    {
        std::cerr << "Unable to open " << pszFilename << "\n";
    }
    return 0;
}
