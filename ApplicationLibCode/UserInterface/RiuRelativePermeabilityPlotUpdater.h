/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2017     Statoil ASA
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

#pragma once

#include <QPointer>
#include <QString>

class RiuSelectionItem;
class RiuRelativePermeabilityPlotPanel;
class Rim3dView;
class RimEclipseView;
class RigEclipseCaseData;
class RimEclipseResultDefinition;
class RiuSelectionItem;
class RiuEclipseSelectionItem;

//==================================================================================================
//
//
//
//==================================================================================================
class RiuRelativePermeabilityPlotUpdater
{
public:
    RiuRelativePermeabilityPlotUpdater( RiuRelativePermeabilityPlotPanel* targetPlotPanel );

    void updateOnSelectionChanged( const RiuSelectionItem* selectionItem );
    void updateOnTimeStepChanged( Rim3dView* changedView );

    static RiuEclipseSelectionItem* extractEclipseSelectionItem( const RiuSelectionItem* selectionItem, Rim3dView*& newFollowAnimView );
    static QString                  constructCellReferenceText( const RigEclipseCaseData* eclipseCaseData,
                                                                size_t                    gridIndex,
                                                                size_t                    gridLocalCellIndex,
                                                                const QString&            valueName,
                                                                double                    cellValue );

private:
    static bool queryDataAndUpdatePlot( const RimEclipseResultDefinition* eclipseResDef,
                                        size_t                            timeStepIndex,
                                        size_t                            gridIndex,
                                        size_t                            gridLocalCellIndex,
                                        RiuRelativePermeabilityPlotPanel* plotPanel );

private:
    QPointer<RiuRelativePermeabilityPlotPanel> m_targetPlotPanel;
    const Rim3dView*                           m_viewToFollowAnimationFrom;
};

//==================================================================================================
//
//
//
//==================================================================================================
class CellLookupHelper
{
public:
    static size_t mapToActiveCellIndex( const RigEclipseCaseData* eclipseCaseData, size_t gridIndex, size_t gridLocalCellIndex );
};
