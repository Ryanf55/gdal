/******************************************************************************
 *
 * Project:  PDS Translator
 * Purpose:  Implements OGRPDSDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_pds.h"

using namespace OGRPDS;

/************************************************************************/
/*                           OGRPDSDataSource()                         */
/************************************************************************/

OGRPDSDataSource::OGRPDSDataSource() : papoLayers(nullptr), nLayers(0)
{
}

/************************************************************************/
/*                        ~OGRPDSDataSource()                           */
/************************************************************************/

OGRPDSDataSource::~OGRPDSDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPDSDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetKeywordSub()                             */
/************************************************************************/

const char *OGRPDSDataSource::GetKeywordSub(const char *pszPath, int iSubscript,
                                            const char *pszDefault)

{
    const char *pszResult = oKeywords.GetKeyword(pszPath, nullptr);

    if (pszResult == nullptr)
        return pszDefault;

    if (pszResult[0] != '(')
        return pszDefault;

    char **papszTokens =
        CSLTokenizeString2(pszResult, "(,)", CSLT_HONOURSTRINGS);

    if (iSubscript <= CSLCount(papszTokens))
    {
        osTempResult = papszTokens[iSubscript - 1];
        CSLDestroy(papszTokens);
        return osTempResult.c_str();
    }

    CSLDestroy(papszTokens);
    return pszDefault;
}

/************************************************************************/
/*                            CleanString()                             */
/*                                                                      */
/* Removes single or double quotes, and converts spaces to underscores. */
/* The change is made in-place to CPLString.                            */
/************************************************************************/

void OGRPDSDataSource::CleanString(CPLString &osInput)

{
    if ((osInput.size() < 2) ||
        ((osInput.at(0) != '"' || osInput.at(osInput.size() - 1) != '"') &&
         (osInput.at(0) != '\'' || osInput.at(osInput.size() - 1) != '\'')))
        return;

    char *pszWrk = CPLStrdup(osInput.c_str() + 1);

    pszWrk[strlen(pszWrk) - 1] = '\0';

    for (int i = 0; pszWrk[i] != '\0'; i++)
    {
        if (pszWrk[i] == ' ')
            pszWrk[i] = '_';
    }

    osInput = pszWrk;
    CPLFree(pszWrk);
}

/************************************************************************/
/*                           LoadTable()                                */
/************************************************************************/

static CPLString MakeAttr(CPLString os1, CPLString os2)
{
    return os1 + "." + os2;
}

bool OGRPDSDataSource::LoadTable(const char *pszFilename, int nRecordSize,
                                 CPLString osTableID)
{
    CPLString osTableFilename;
    int nStartBytes = 0;

    CPLString osTableLink = "^";
    osTableLink += osTableID;

    CPLString osTable = oKeywords.GetKeyword(osTableLink, "");
    if (osTable[0] == '(')
    {
        osTableFilename = GetKeywordSub(osTableLink, 1, "");
        CPLString osStartRecord = GetKeywordSub(osTableLink, 2, "");
        nStartBytes = atoi(osStartRecord.c_str());
        if (nStartBytes <= 0 ||
            ((nRecordSize > 0 && nStartBytes > INT_MAX / nRecordSize)))
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Invalid StartBytes value");
            return false;
        }
        nStartBytes--;
        nStartBytes *= nRecordSize;
        if (osTableFilename.empty() || osStartRecord.empty() || nStartBytes < 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Cannot parse %s line",
                     osTableLink.c_str());
            return false;
        }
        CPLString osTPath = CPLGetPathSafe(pszFilename);
        CleanString(osTableFilename);
        osTableFilename =
            CPLFormCIFilenameSafe(osTPath, osTableFilename, nullptr);
    }
    else
    {
        osTableFilename = oKeywords.GetKeyword(osTableLink, "");
        if (!osTableFilename.empty() && osTableFilename[0] >= '0' &&
            osTableFilename[0] <= '9')
        {
            nStartBytes = atoi(osTableFilename.c_str());
            if (nStartBytes <= 1)
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Cannot parse %s line",
                         osTableFilename.c_str());
                return false;
            }
            nStartBytes = nStartBytes - 1;
            if (strstr(osTableFilename.c_str(), "<BYTES>") == nullptr)
            {
                if (nRecordSize > 0 && nStartBytes > INT_MAX / nRecordSize)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Too big StartBytes value");
                    return false;
                }
                nStartBytes *= nRecordSize;
            }
            osTableFilename = pszFilename;
        }
        else
        {
            CPLString osTPath = CPLGetPathSafe(pszFilename);
            CleanString(osTableFilename);
            osTableFilename =
                CPLFormCIFilenameSafe(osTPath, osTableFilename, nullptr);
            nStartBytes = 0;
        }
    }

    CPLString osTableName =
        oKeywords.GetKeyword(MakeAttr(osTableID, "NAME"), "");
    if (osTableName.empty())
    {
        if (GetLayerByName(osTableID.c_str()) == nullptr)
            osTableName = osTableID;
        else
            osTableName = CPLSPrintf("Layer_%d", nLayers + 1);
    }
    CleanString(osTableName);
    CPLString osTableInterchangeFormat =
        oKeywords.GetKeyword(MakeAttr(osTableID, "INTERCHANGE_FORMAT"), "");
    CPLString osTableRows =
        oKeywords.GetKeyword(MakeAttr(osTableID, "ROWS"), "");
    const int nRecords = atoi(osTableRows);
    if (osTableInterchangeFormat.empty() || osTableRows.empty() || nRecords < 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "One of TABLE.INTERCHANGE_FORMAT or TABLE.ROWS is missing");
        return false;
    }

    CleanString(osTableInterchangeFormat);
    if (osTableInterchangeFormat.compare("ASCII") != 0 &&
        osTableInterchangeFormat.compare("BINARY") != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only INTERCHANGE_FORMAT=ASCII or BINARY is supported");
        return false;
    }

    VSILFILE *fp = VSIFOpenL(osTableFilename, "rb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s",
                 osTableFilename.c_str());
        return false;
    }

    CPLString osTableStructure =
        oKeywords.GetKeyword(MakeAttr(osTableID, "^STRUCTURE"), "");
    if (!osTableStructure.empty())
    {
        CPLString osTPath = CPLGetPathSafe(pszFilename);
        CleanString(osTableStructure);
        osTableStructure =
            CPLFormCIFilenameSafe(osTPath, osTableStructure, nullptr);
    }

    GByte *pabyRecord = (GByte *)VSI_MALLOC_VERBOSE(nRecordSize + 1);
    if (pabyRecord == nullptr)
    {
        VSIFCloseL(fp);
        return false;
    }
    pabyRecord[nRecordSize] = 0;

    papoLayers = static_cast<OGRLayer **>(
        CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer *)));
    papoLayers[nLayers] = new OGRPDSLayer(
        osTableID, osTableName, fp, pszFilename, osTableStructure, nRecords,
        nStartBytes, nRecordSize, pabyRecord,
        osTableInterchangeFormat.compare("ASCII") == 0);
    nLayers++;

    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRPDSDataSource::Open(const char *pszFilename)

{
    // --------------------------------------------------------------------
    //      Does this appear to be a .PDS table file?
    // --------------------------------------------------------------------

    VSILFILE *fp = VSIFOpenL(pszFilename, "rb");
    if (fp == nullptr)
        return FALSE;

    char szBuffer[512];
    int nbRead =
        static_cast<int>(VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fp));
    szBuffer[nbRead] = '\0';

    const char *pszPos = strstr(szBuffer, "PDS_VERSION_ID");
    const bool bIsPDS = pszPos != nullptr;

    if (!bIsPDS)
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    if (!oKeywords.Ingest(fp, static_cast<int>(pszPos - szBuffer)))
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    VSIFCloseL(fp);
    CPLString osRecordType = oKeywords.GetKeyword("RECORD_TYPE", "");
    CPLString osFileRecords = oKeywords.GetKeyword("FILE_RECORDS", "");
    CPLString osRecordBytes = oKeywords.GetKeyword("RECORD_BYTES", "");
    int nRecordSize = atoi(osRecordBytes);
    if (osRecordType.empty() || osFileRecords.empty() ||
        osRecordBytes.empty() || nRecordSize <= 0 ||
        nRecordSize > 10 * 1024 * 1024)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "One of RECORD_TYPE, FILE_RECORDS or RECORD_BYTES is missing");
        return FALSE;
    }
    CleanString(osRecordType);
    if (osRecordType.compare("FIXED_LENGTH") != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only RECORD_TYPE=FIXED_LENGTH is supported");
        return FALSE;
    }

    CPLString osTable = oKeywords.GetKeyword("^TABLE", "");
    if (!osTable.empty())
    {
        LoadTable(pszFilename, nRecordSize, "TABLE");
    }
    else
    {
        fp = VSIFOpenL(pszFilename, "rb");
        if (fp == nullptr)
            return FALSE;

        // To avoid performance issues with datasets generated by oss-fuzz
        int nErrors = 0;
        while (nErrors < 10)
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            const char *pszLine = CPLReadLine2L(fp, 256, nullptr);
            CPLPopErrorHandler();
            CPLErrorReset();
            if (pszLine == nullptr)
                break;
            char **papszTokens =
                CSLTokenizeString2(pszLine, " =", CSLT_HONOURSTRINGS);
            int nTokens = CSLCount(papszTokens);
            if (nTokens == 2 && papszTokens[0][0] == '^' &&
                strstr(papszTokens[0], "TABLE") != nullptr)
            {
                if (!LoadTable(pszFilename, nRecordSize, papszTokens[0] + 1))
                {
                    nErrors++;
                }
            }
            CSLDestroy(papszTokens);
            papszTokens = nullptr;
        }
        VSIFCloseL(fp);
    }

    return nLayers != 0;
}
