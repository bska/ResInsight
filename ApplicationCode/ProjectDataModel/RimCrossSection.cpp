/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2015-     Statoil ASA
//  Copyright (C) 2015-     Ceetron Solutions AS
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

#include "RimCrossSection.h"

#include "RiaApplication.h"

#include "RimEclipseView.h"
#include "RimEclipseWell.h"
#include "RimEclipseWellCollection.h"
#include "RimOilField.h"
#include "RimProject.h"
#include "RimWellPath.h"

#include "RivCrossSectionPartMgr.h"
#include "RigSimulationWellCenterLineCalculator.h"


namespace caf {

template<>
void caf::AppEnum< RimCrossSection::CrossSectionEnum >::setUp()
{
    addItem(RimCrossSection::CS_WELL_PATH,       "CS_WELL_PATH",       "Well Path");
    addItem(RimCrossSection::CS_SIMULATION_WELL, "CS_SIMULATION_WELL", "Simulation Well");
//    addItem(RimCrossSection::CS_USER_DEFINED,    "CS_USER_DEFINED",    "User defined");
    setDefault(RimCrossSection::CS_WELL_PATH);
}

template<>
void caf::AppEnum< RimCrossSection::CrossSectionDirEnum >::setUp()
{
    addItem(RimCrossSection::CS_VERTICAL,   "CS_VERTICAL",      "Vertical");
    addItem(RimCrossSection::CS_HORIZONTAL, "CS_HORIZONTAL",    "Horizontal");
    setDefault(RimCrossSection::CS_VERTICAL);
}

} 


CAF_PDM_SOURCE_INIT(RimCrossSection, "CrossSection");

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
RimCrossSection::RimCrossSection()
{
    CAF_PDM_InitObject("Intersection", ":/CrossSection16x16.png", "", "");

    CAF_PDM_InitField(&name,        "UserDescription",  QString("Intersection Name"), "Name", "", "", "");
    CAF_PDM_InitField(&isActive,    "Active",           true, "Active", "", "", "");
    isActive.uiCapability()->setUiHidden(true);

    CAF_PDM_InitFieldNoDefault(&type,           "Type",             "Type", "", "", "");
    CAF_PDM_InitFieldNoDefault(&direction,      "Direction",        "Direction", "", "", "");
    CAF_PDM_InitFieldNoDefault(&wellPath,       "WellPath",         "Well Path", "", "", "");
    CAF_PDM_InitFieldNoDefault(&simulationWell, "SimulationWell",   "Simulation Well", "", "", "");
    CAF_PDM_InitField         (&branchIndex,    "Branch",        -1, "Branch", "", "", "");
    
    uiCapability()->setUiChildrenHidden(true);
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
void RimCrossSection::fieldChangedByUi(const caf::PdmFieldHandle* changedField, const QVariant& oldValue, const QVariant& newValue)
{
    if (changedField == &isActive ||
        changedField == &type ||
        changedField == &direction ||
        changedField == &wellPath ||
        changedField == &simulationWell ||
        changedField == &branchIndex)
    {
        m_crossSectionPartMgr = NULL;
    
        RimView* rimView = NULL;
        this->firstAnchestorOrThisOfType(rimView);
        if (rimView)
        {
            rimView->scheduleCreateDisplayModelAndRedraw();
        }
    }

    if (changedField == &simulationWell 
        || changedField == &isActive 
        || changedField == &type)
    {
        m_wellBranchCenterlines.clear();
        updateWellCenterline();
        branchIndex = -1;
    }
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
void RimCrossSection::defineUiOrdering(QString uiConfigName, caf::PdmUiOrdering& uiOrdering)
{
    uiOrdering.add(&name);
    uiOrdering.add(&type);
    uiOrdering.add(&direction);

    if (type == CS_WELL_PATH)
    {
        uiOrdering.add(&wellPath);
    }
    else if (type == CS_SIMULATION_WELL)
    {
        uiOrdering.add(&simulationWell);
        updateWellCenterline();
        if (simulationWell() && m_wellBranchCenterlines.size() > 1)
        {
            uiOrdering.add(&branchIndex);
        }
    }
    else
    {
        // User defined poly line
    }

    uiOrdering.setForgetRemainingFields(true);
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
QList<caf::PdmOptionItemInfo> RimCrossSection::calculateValueOptions(const caf::PdmFieldHandle* fieldNeedingOptions, bool * useOptionsOnly)
{
    QList<caf::PdmOptionItemInfo> options;

    if (fieldNeedingOptions == &wellPath)
    {
        RimProject* proj = RiaApplication::instance()->project();
        if (proj->activeOilField()->wellPathCollection())
        {
            caf::PdmChildArrayField<RimWellPath*>& wellPaths = proj->activeOilField()->wellPathCollection()->wellPaths;
            
            QIcon wellIcon(":/Well.png");
            for (size_t i = 0; i < wellPaths.size(); i++)
            {
                options.push_back(caf::PdmOptionItemInfo(wellPaths[i]->name(), QVariant::fromValue(caf::PdmPointer<caf::PdmObjectHandle>(wellPaths[i])), false, wellIcon));
            }
        }

        if (options.size() == 0)
        {
            options.push_front(caf::PdmOptionItemInfo("None", QVariant::fromValue(caf::PdmPointer<caf::PdmObjectHandle>(NULL))));
        }
    }
    else if (fieldNeedingOptions == &simulationWell)
    {
        RimEclipseWellCollection* coll = simulationWellCollection();
        if (coll)
        {
            caf::PdmChildArrayField<RimEclipseWell*>& eclWells = coll->wells;

            QIcon simWellIcon(":/Well.png");
            for (size_t i = 0; i < eclWells.size(); i++)
            {
                options.push_back(caf::PdmOptionItemInfo(eclWells[i]->name(), QVariant::fromValue(caf::PdmPointer<caf::PdmObjectHandle>(eclWells[i])), false, simWellIcon));
            }

        }

        if (options.size() == 0)
        {
            options.push_front(caf::PdmOptionItemInfo("None", QVariant::fromValue(caf::PdmPointer<caf::PdmObjectHandle>(NULL))));
        }
    }
    else if (fieldNeedingOptions == &branchIndex)
    {
        updateWellCenterline();

        size_t branchCount = m_wellBranchCenterlines.size();
        
        options.push_back(caf::PdmOptionItemInfo("All", -1));

        for (int bIdx = 0; bIdx < branchCount; ++bIdx)
        {
            options.push_back(caf::PdmOptionItemInfo(QString::number(bIdx + 1), QVariant::fromValue(bIdx)));
        }
    }
    return options;
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
caf::PdmFieldHandle* RimCrossSection::userDescriptionField()
{
    return &name;
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
caf::PdmFieldHandle* RimCrossSection::objectToggleField()
{
    return &isActive;
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
RimEclipseWellCollection* RimCrossSection::simulationWellCollection()
{
    RimEclipseView* eclipseView = NULL;
    firstAnchestorOrThisOfType(eclipseView);

    if (eclipseView)
    {
        return eclipseView->wellCollection;
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
std::vector< std::vector <cvf::Vec3d> > RimCrossSection::polyLines() const
{
    std::vector< std::vector <cvf::Vec3d> > lines;
    if (type == CS_WELL_PATH)
    {
        if (wellPath())
        {
            lines.push_back(wellPath->wellPathGeometry()->m_wellPathPoints);
        }
    }
    else if (type == CS_SIMULATION_WELL)
    {
        if (simulationWell())
        {
            updateWellCenterline();

            if (0 <= branchIndex && branchIndex < m_wellBranchCenterlines.size())
            {
                lines.push_back(m_wellBranchCenterlines[branchIndex]);
            }

            if (branchIndex == -1)
            {
                lines = m_wellBranchCenterlines;
            }
        }
    }
    else
    {

    }

    if (type == CS_WELL_PATH || type == CS_SIMULATION_WELL)
    {
        for (int lIdx = 0; lIdx < lines.size(); ++lIdx)
        {
            cvf::Vec3d startToEnd = (lines[lIdx].back() - lines[lIdx].front());
            startToEnd[2] = 0.0;
            
            cvf::Vec3d newStart = lines[lIdx].front() - startToEnd * 0.1;
            cvf::Vec3d newEnd = lines[lIdx].back() + startToEnd * 0.1;

            lines[lIdx].insert(lines[lIdx].begin(), newStart);
            lines[lIdx].push_back(newEnd);
        }
    }

    return lines;
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
RivCrossSectionPartMgr* RimCrossSection::crossSectionPartMgr()
{
    if (m_crossSectionPartMgr.isNull()) m_crossSectionPartMgr = new RivCrossSectionPartMgr(this);

    return m_crossSectionPartMgr.p();
}

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
void RimCrossSection::updateWellCenterline() const
{
    if (isActive() && type == CS_SIMULATION_WELL && simulationWell())
    {
        if (m_wellBranchCenterlines.size() == 0)
        {
            std::vector< std::vector <RigWellResultPoint> > pipeBranchesCellIds;

            RigSimulationWellCenterLineCalculator::calculateWellPipeCenterline(simulationWell(), m_wellBranchCenterlines, pipeBranchesCellIds);
        }
    }
    else
    {
        m_wellBranchCenterlines.clear();
    }
}
