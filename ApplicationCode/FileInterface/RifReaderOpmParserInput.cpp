/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2016 Statoil ASA
// 
//  ResInsight is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
// 
//  ResInsight is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.
// 
//  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html> 
//  for more details.
//
/////////////////////////////////////////////////////////////////////////////////

#include "RifReaderOpmParserInput.h"

#include "RifReaderEclipseOutput.h"
#include "RigCaseCellResultsData.h"
#include "RigCaseData.h"
#include "RimEclipseInputCaseOpm.h"
#include "RimEclipseInputProperty.h"
#include "RimEclipseInputPropertyCollection.h"
#include "RiuMainWindow.h"
#include "RiuProcessMonitor.h"

#include "cafProgressInfo.h"

#include "opm/parser/eclipse/Deck/DeckItem.hpp"
#include "opm/parser/eclipse/Parser/MessageContainer.hpp"
#include "opm/parser/eclipse/Parser/ParseContext.hpp"
#include "opm/parser/eclipse/Parser/Parser.hpp"


//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
void RifReaderOpmParserInput::importGridAndProperties(const QString& fileName, RigCaseData* caseData, std::map<QString, QString>* mapFromResultNameToKeyword)
{
    RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("\nStarted reading of grid and properties from file : " + fileName + "\n"));

    {
        std::shared_ptr<const Opm::EclipseGrid> eclipseGrid;
        std::string errorMessage;

        std::shared_ptr<const Opm::Deck> deck;

        try
        {
            Opm::Parser parser;

            // A default ParseContext will set up all parsing errors to throw exceptions
            Opm::ParseContext parseContext;

            for (auto state : allParserConfigKeys())
            {
                parseContext.addKey(state);
            }

            // Treat all parsing errors as warnings
            parseContext.update(Opm::InputError::WARN);

            deck = parser.parseFile(fileName.toStdString(), parseContext);

            if (deck)
            {
                eclipseGrid = Opm::Parser::parseGrid(*deck, parseContext);

                if (eclipseGrid && eclipseGrid->hasCellInfo())
                {
                    if (eclipseGrid->c_ptr())
                    {
                        RifReaderEclipseOutput::transferGeometry(eclipseGrid->c_ptr(), caseData);
                    }
                    else
                    {
                        throw std::invalid_argument("No valid 3D grid detected");
                    }

                    Opm::TableManager tableManager(*deck);

                    Opm::Eclipse3DProperties properties(*deck, tableManager, *eclipseGrid);

                    std::vector<std::string> predefKeywords = RifReaderOpmParserInput::knownPropertyKeywords();
                    for (auto keyword : predefKeywords)
                    {
                        if (properties.supportsGridProperty(keyword))
                        {
                            if (properties.hasDeckDoubleGridProperty(keyword))
                            {
                                auto allValues = properties.getDoubleGridProperty(keyword).getData();

                                QString newResultName = caseData->results(RifReaderInterface::MATRIX_RESULTS)->makeResultNameUnique(QString::fromStdString(keyword));
                                size_t resultIndex = RifReaderOpmParserPropertyReader::findOrCreateResult(newResultName, caseData);

                                if (resultIndex != cvf::UNDEFINED_SIZE_T)
                                {
                                    std::vector< std::vector<double> >& newPropertyData = caseData->results(RifReaderInterface::MATRIX_RESULTS)->cellScalarResults(resultIndex);
                                    newPropertyData.push_back(allValues);

                                    mapFromResultNameToKeyword->insert(std::make_pair(newResultName, QString::fromStdString(keyword)));
                                }
                            }
                            else if (properties.hasDeckIntGridProperty(keyword))
                            {
                                auto intValues = properties.getIntGridProperty(keyword).getData();

                                QString newResultName = caseData->results(RifReaderInterface::MATRIX_RESULTS)->makeResultNameUnique(QString::fromStdString(keyword));
                                size_t resultIndex = RifReaderOpmParserPropertyReader::findOrCreateResult(newResultName, caseData);

                                if (resultIndex != cvf::UNDEFINED_SIZE_T)
                                {
                                    std::vector< std::vector<double> >& newPropertyData = caseData->results(RifReaderInterface::MATRIX_RESULTS)->cellScalarResults(resultIndex);

                                    std::vector<double> doubleValues;

                                    doubleValues.insert(std::end(doubleValues), std::begin(intValues), std::end(intValues));

                                    newPropertyData.push_back(doubleValues);

                                    mapFromResultNameToKeyword->insert(std::make_pair(newResultName, QString::fromStdString(keyword)));
                                }
                            }

                        }
                    }
                }
            }
        }
        catch (std::exception& e)
        {
            errorMessage = e.what();
        }
        catch (...)
        {
            errorMessage = "Unknown exception throwm from Opm::Parser";
        }

        if (deck)
        {
            const Opm::MessageContainer& messages = deck->getMessageContainer();
            if (messages.size() > 0)
            {
                RiuMainWindow::instance()->processMonitor()->addStringToLog("\n\nLog messages from Deck : \n");
            }
            for (auto m : messages)
            {
                RiuMainWindow::instance()->processMonitor()->addStringToLog("  Deck : " + QString::fromStdString(m.message) + "\n");
            }
        }

        if (eclipseGrid)
        {
            const Opm::MessageContainer& messages = eclipseGrid->getMessageContainer();
            if (messages.size() > 0)
            {
                RiuMainWindow::instance()->processMonitor()->addStringToLog("\n\nLog messages from EclipseGrid : \n");
            }
            for (auto m : messages)
            {
                RiuMainWindow::instance()->processMonitor()->addStringToLog("  EclipseGrid :" + QString::fromStdString(m.message) + "\n");
            }
        }

        if (errorMessage.size() > 0)
        {
            RiuMainWindow::instance()->processMonitor()->addStringToLog("\n\nError messages : \n");
            RiuMainWindow::instance()->processMonitor()->addStringToLog("  " + QString::fromStdString(errorMessage) + "\n");
            RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Failed reading of grid and properties from file : " + fileName + "\n"));
        }
        else
        {
            RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Completed reading of grid and properties from file : " + fileName + "\n"));
        }
    }
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
std::vector<std::string> RifReaderOpmParserInput::knownPropertyKeywords()
{
    std::vector<std::string> knownKeywords;
    knownKeywords.push_back("AQUIFERA");
    knownKeywords.push_back("ACTNUM");
    knownKeywords.push_back("EQLNUM");
    knownKeywords.push_back("FIPNUM");
    knownKeywords.push_back("KRG");
    knownKeywords.push_back("KRGR");
    knownKeywords.push_back("KRO");
    knownKeywords.push_back("KRORG");
    knownKeywords.push_back("KRORW");
    knownKeywords.push_back("KRW");
    knownKeywords.push_back("KRWR");
    knownKeywords.push_back("MINPVV");
    knownKeywords.push_back("MULTPV");
    knownKeywords.push_back("MULTX");
    knownKeywords.push_back("MULTX-");
    knownKeywords.push_back("MULTY");
    knownKeywords.push_back("MULTY-");
    knownKeywords.push_back("MULTZ");
    knownKeywords.push_back("NTG");
    knownKeywords.push_back("PCG");
    knownKeywords.push_back("PCW");
    knownKeywords.push_back("PERMX");
    knownKeywords.push_back("PERMY");
    knownKeywords.push_back("PERMZ");
    knownKeywords.push_back("PORO");
    knownKeywords.push_back("PVTNUM");
    knownKeywords.push_back("SATNUM");
    knownKeywords.push_back("SGCR");
    knownKeywords.push_back("SGL");
    knownKeywords.push_back("SGLPC");
    knownKeywords.push_back("SGU");
    knownKeywords.push_back("SGWCR");
    knownKeywords.push_back("SWATINIT");
    knownKeywords.push_back("SWCR");
    knownKeywords.push_back("SWGCR");
    knownKeywords.push_back("SWL");
    knownKeywords.push_back("SWLPC");
    knownKeywords.push_back("TRANX");
    knownKeywords.push_back("TRANY");
    knownKeywords.push_back("TRANZ");

    return knownKeywords;
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
std::vector<std::string> RifReaderOpmParserInput::allParserConfigKeys()
{
    std::vector<std::string> configKeys;
    configKeys.push_back(Opm::ParseContext::PARSE_UNKNOWN_KEYWORD);
    configKeys.push_back(Opm::ParseContext::PARSE_RANDOM_TEXT);
    configKeys.push_back(Opm::ParseContext::PARSE_RANDOM_SLASH);
    configKeys.push_back(Opm::ParseContext::PARSE_MISSING_DIMS_KEYWORD);
    configKeys.push_back(Opm::ParseContext::PARSE_EXTRA_DATA);
    configKeys.push_back(Opm::ParseContext::PARSE_MISSING_INCLUDE);
    configKeys.push_back(Opm::ParseContext::UNSUPPORTED_SCHEDULE_GEO_MODIFIER);
    configKeys.push_back(Opm::ParseContext::UNSUPPORTED_COMPORD_TYPE);
    configKeys.push_back(Opm::ParseContext::UNSUPPORTED_INITIAL_THPRES);
    configKeys.push_back(Opm::ParseContext::INTERNAL_ERROR_UNINITIALIZED_THPRES);
    configKeys.push_back(Opm::ParseContext::PARSE_MISSING_SECTIONS);

    return configKeys;
}


//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
/*
bool RifReaderOpmParserInput::openGridFile(const QString& fileName, bool importProperties, RigCaseData* caseData, RimEclipseInputCaseOpm* rimInputCase)
{
    RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Started grid reading from file : " + fileName + "\n"));

    {
        std::shared_ptr<const Opm::EclipseGrid> eclipseGrid;
        std::string errorMessage;

        try
        {
            Opm::Parser parser;

            // A default ParseContext will set up all parsing errors to throw exceptions
            Opm::ParseContext parseContext;
            parseContext.addKey("PARSE_MISSING_SECTIONS");

            // Treat all parsing errors as warnings
            parseContext.update(Opm::InputError::WARN);
     
            auto deck = parser.parseFile(fileName.toStdString(), parseContext);

            eclipseGrid = parser.parseGrid(*deck, parseContext);
            if (eclipseGrid && eclipseGrid->c_ptr())
            {
                RifReaderEclipseOutput::transferGeometry(eclipseGrid->c_ptr(), caseData);

                if (importProperties)
                {
                    std::map<QString, QString> allPropertiesOnFile;
                    RifReaderOpmParserPropertyReader::readAllProperties(deck, caseData, &allPropertiesOnFile);
                    for (auto it = allPropertiesOnFile.begin(); it != allPropertiesOnFile.end(); ++it)
                    {
                        RimEclipseInputProperty* inputProperty = new RimEclipseInputProperty;
                        inputProperty->resultName = it->first;
                        inputProperty->eclipseKeyword = it->second;
                        inputProperty->fileName = fileName;
                        inputProperty->resolvedState = RimEclipseInputProperty::RESOLVED;
                        rimInputCase->m_inputPropertyCollection->inputProperties.push_back(inputProperty);
                    }
                }
            }
        }
        catch (std::exception& e)
        {
            errorMessage = e.what();
        }
        catch (...)
        {
            errorMessage = "Unknown exception throwm from Opm::Parser";
        }

        if (eclipseGrid)
        {
            const Opm::MessageContainer& messages = eclipseGrid->getMessageContainer();
            for (auto m : messages)
            {
                RiuMainWindow::instance()->processMonitor()->addStringToLog("  " + QString::fromStdString(m.message) + "\n");
            }
        }

        if (errorMessage.size() > 0)
        {
            RiuMainWindow::instance()->processMonitor()->addStringToLog("  " + QString::fromStdString(errorMessage) + "\n");
            RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Failed grid reading from file : " + fileName + "\n"));
        }
        else
        {
            RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Completed grid reading from file : " + fileName + "\n"));
        }
    }

    return true;
}
*/

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
size_t RifReaderOpmParserPropertyReader::findOrCreateResult(const QString& newResultName, RigCaseData* reservoir)
{
    size_t resultIndex = reservoir->results(RifReaderInterface::MATRIX_RESULTS)->findScalarResultIndex(newResultName);
    if (resultIndex == cvf::UNDEFINED_SIZE_T)
    {
        resultIndex = reservoir->results(RifReaderInterface::MATRIX_RESULTS)->addEmptyScalarResult(RimDefines::INPUT_PROPERTY, newResultName, false);
    }

    return resultIndex;
}


/*
//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
std::map<QString, QString> RifReaderOpmParserInput::copyPropertiesToCaseData(const QString& fileName, RigCaseData* caseData)
{
    RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Started reading of properties from file : " + fileName + "\n"));

    std::map<QString, QString> newResults;
    
    {
        std::shared_ptr<const Opm::EclipseGrid> eclipseGrid;
        std::string errorMessage;

        try
        {
            Opm::Parser parser;

            // A default ParseContext will set up all parsing errors to throw exceptions
            Opm::ParseContext parseContext;

            // Treat all parsing errors as warnings
            parseContext.update(Opm::InputError::WARN);

            auto deck = parser.parseFile(fileName.toStdString(), parseContext);

            RifReaderOpmParserPropertyReader::readAllProperties(deck, caseData, &newResults);
        }
        catch (std::exception& e)
        {
            errorMessage = e.what();
        }
        catch (...)
        {
            errorMessage = "Unknown exception throwm from Opm::Parser";
        }

        if (eclipseGrid)
        {
            const Opm::MessageContainer& messages = eclipseGrid->getMessageContainer();
            for (auto m : messages)
            {
                RiuMainWindow::instance()->processMonitor()->addStringToLog("  " + QString::fromStdString(m.message) + "\n");
            }
        }

        if (errorMessage.size() > 0)
        {
            RiuMainWindow::instance()->processMonitor()->addStringToLog("  " + QString::fromStdString(errorMessage) + "\n");
            RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Failed reading of properties from file : " + fileName + "\n"));
        }
        else
        {
            RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Completed reading of properties from file : " + fileName + "\n"));
        }
    }

    return newResults;
}
*/



//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
void RifReaderOpmParserPropertyReader::readAllProperties(std::shared_ptr< Opm::Deck > deck, RigCaseData* caseData, std::map<QString, QString>* newResults)
{
    std::set<std::string> uniqueKeywords;
    for (auto it = deck->begin(); it != deck->end(); it++)
    {
        uniqueKeywords.insert(it->name());
    }

    for (auto keyword : uniqueKeywords)
    {
        bool isItemCountEqual = RifReaderOpmParserPropertyReader::isDataItemCountIdenticalToMainGridCellCount(deck, keyword, caseData);

        if (isItemCountEqual)
        {
            std::vector<double> allValues;

            RifReaderOpmParserPropertyReader::getAllValuesForKeyword(deck, keyword, allValues);

            QString keywordName = QString::fromStdString(keyword);
            QString newResultName = caseData->results(RifReaderInterface::MATRIX_RESULTS)->makeResultNameUnique(keywordName);
            size_t resultIndex = findOrCreateResult(newResultName, caseData);
            if (resultIndex != cvf::UNDEFINED_SIZE_T)
            {
                std::vector< std::vector<double> >& newPropertyData = caseData->results(RifReaderInterface::MATRIX_RESULTS)->cellScalarResults(resultIndex);
                newPropertyData.push_back(allValues);
            }

            newResults->insert(std::make_pair(newResultName, keywordName));
        }
    }
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
RifReaderOpmParserPropertyReader::RifReaderOpmParserPropertyReader()
{

}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
RifReaderOpmParserPropertyReader::~RifReaderOpmParserPropertyReader()
{

}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
bool RifReaderOpmParserPropertyReader::open(const QString& fileName)
{
    {
        std::string errorMessage;

        try
        {
            Opm::Parser parser;

            // A default ParseContext will set up all parsing errors to throw exceptions
            Opm::ParseContext parseContext;

            // Treat all parsing errors as warnings
            parseContext.update(Opm::InputError::WARN);

            m_deck = parser.parseFile(fileName.toStdString(), parseContext);
        }
        catch (std::exception& e)
        {
            errorMessage = e.what();
        }
        catch (...)
        {
            errorMessage = "Unknown exception throwm from Opm::Parser";
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
std::set<QString> RifReaderOpmParserPropertyReader::keywords() const
{
    std::set<QString> ids;

    if (m_deck)
    {
        for (auto it = m_deck->begin(); it != m_deck->end(); it++)
        {
            ids.insert(QString::fromStdString(it->name()));
        }
    }

    return ids;
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
bool RifReaderOpmParserPropertyReader::copyPropertyToCaseData(const QString& keywordName, RigCaseData* caseData, const QString& resultName)
{
    {
        std::string errorMessage;

        try
        {
            std::string stdKeywordName = keywordName.toStdString();

            if (m_deck->hasKeyword(stdKeywordName))
            {
                bool isItemCountEqual = isDataItemCountIdenticalToMainGridCellCount(m_deck, stdKeywordName, caseData);
                if (isItemCountEqual)
                {
                    std::vector<double> allValues;

                    getAllValuesForKeyword(m_deck, stdKeywordName, allValues);

                    size_t resultIndex = RifReaderOpmParserPropertyReader::findOrCreateResult(resultName, caseData);
                    if (resultIndex != cvf::UNDEFINED_SIZE_T)
                    {
                        std::vector< std::vector<double> >& newPropertyData = caseData->results(RifReaderInterface::MATRIX_RESULTS)->cellScalarResults(resultIndex);
                        newPropertyData.push_back(allValues);
                    }
                }
            }
        }
        catch (std::exception& e)
        {
            errorMessage = e.what();
        }
        catch (...)
        {
            errorMessage = "Unknown exception throwm from Opm::Parser";
        }

        QString fileName = QString::fromStdString(m_deck->getDataFile());

        if (errorMessage.size() > 0)
        {
            RiuMainWindow::instance()->processMonitor()->addStringToLog("  " + QString::fromStdString(errorMessage) + "\n");
            RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Error detected while reading property %1 from file : %2\n").arg(keywordName).arg(fileName));
        }
        else
        {
            RiuMainWindow::instance()->processMonitor()->addStringToLog(QString("Completed reading of property %1 from file : %2\n").arg(keywordName).arg(fileName));
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
void RifReaderOpmParserPropertyReader::getAllValuesForKeyword(std::shared_ptr< Opm::Deck > deck, const std::string& keyword, std::vector<double>& allValues)
{
    for (auto deckKeyword : deck->getKeywordList(keyword))
    {
        if (deckKeyword->isDataKeyword() && deckKeyword->size() == 1)
        {
            auto deckRecord = deckKeyword->getDataRecord();
            if (deckRecord.size() == 1)
            {
                if (deckRecord.getDataItem().typeof() == Opm::DeckItem::integer)
                {
                    auto opmData = deckKeyword->getIntData();
                    allValues.insert(std::end(allValues), std::begin(opmData), std::end(opmData));
                }
                else if (deckRecord.getDataItem().typeof() == Opm::DeckItem::fdouble)
                {
                    auto opmData = deckKeyword->getRawDoubleData();
                    allValues.insert(std::end(allValues), std::begin(opmData), std::end(opmData));
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
bool RifReaderOpmParserPropertyReader::isDataItemCountIdenticalToMainGridCellCount(std::shared_ptr< Opm::Deck > deck, const std::string& keyword, RigCaseData* caseData)
{
    bool isEqual = false;
    {
        size_t valueCount = 0;
        for (auto deckKeyword : deck->getKeywordList(keyword))
        {
            if (deckKeyword->isDataKeyword())
            {
                valueCount += deckKeyword->getDataSize();
            }
        }

        if (valueCount == caseData->mainGrid()->cellCount())
        {
            isEqual = true;
        }
    }

    return isEqual;
}
