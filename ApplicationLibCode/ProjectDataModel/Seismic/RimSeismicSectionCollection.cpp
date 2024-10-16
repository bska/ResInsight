/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2023     Equinor ASA
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

#include "RimSeismicSectionCollection.h"

#include "RiuViewer.h"

#include "Rim3dView.h"
#include "RimOilField.h"
#include "RimProject.h"
#include "RimRegularLegendConfig.h"
#include "RimSeismicData.h"
#include "RimSeismicDataCollection.h"
#include "RimSeismicSection.h"
#include "RimSurface.h"
#include "RimSurfaceCollection.h"
#include "RimTools.h"

#include "RivSeismicSectionPartMgr.h"

#include "cvfBoundingBox.h"
#include "cvfModelBasicList.h"

#include "cafDisplayCoordTransform.h"
#include "cafPdmUiTreeSelectionEditor.h"

CAF_PDM_SOURCE_INIT( RimSeismicSectionCollection, "SeismicSectionCollection" );

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimSeismicSectionCollection::RimSeismicSectionCollection()
{
    CAF_PDM_InitObject( "Seismic Sections", ":/Seismic16x16.png" );

    CAF_PDM_InitField( &m_userDescription, "UserDescription", QString( "Seismic Sections" ), "Name" );

    CAF_PDM_InitFieldNoDefault( &m_seismicSections, "SeismicSections", "SeismicSections" );

    CAF_PDM_InitField( &m_surfaceIntersectionLinesScaleFactor, "SurfaceIntersectionLinesScaleFactor", 5.0, "Line Scale Factor ( >= 1.0 )" );

    CAF_PDM_InitFieldNoDefault( &m_surfacesWithVisibleSurfaceLines, "SurfacesWithVisibleSurfaceLines", "Surface Lines" );
    m_surfacesWithVisibleSurfaceLines.uiCapability()->setUiEditorTypeName( caf::PdmUiTreeSelectionEditor::uiEditorTypeName() );

    setName( "Seismic Sections" );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimSeismicSectionCollection::~RimSeismicSectionCollection()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimSeismicSection* RimSeismicSectionCollection::addNewSection( RiaDefines::SeismicSectionType sectionType )
{
    auto view = firstAncestorOrThisOfType<Rim3dView>();
    if ( view == nullptr ) return nullptr;

    RimSeismicData* defaultSeis = nullptr;

    RimProject* proj = RimProject::current();
    if ( proj )
    {
        const auto& coll = proj->activeOilField()->seismicDataCollection().p();
        for ( auto* c : coll->seismicData() )
        {
            if ( c->boundingBox()->intersects( view->domainBoundingBox() ) )
            {
                defaultSeis = c;
                break;
            }
        }
    }

    RimSeismicSection* newSection = new RimSeismicSection();
    if ( defaultSeis != nullptr ) newSection->setSeismicData( defaultSeis );
    newSection->setSectionType( sectionType );
    m_seismicSections.push_back( newSection );
    updateAllRequiredEditors();
    updateView();
    return newSection;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimSeismicSectionCollection::empty()
{
    return m_seismicSections.empty();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
int RimSeismicSectionCollection::size()
{
    return static_cast<int>( m_seismicSections.size() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimSeismicSectionCollection::userDescription()
{
    return m_userDescription;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimSeismicSectionCollection::setUserDescription( QString description )
{
    m_userDescription = description;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
caf::PdmFieldHandle* RimSeismicSectionCollection::userDescriptionField()
{
    return &m_userDescription;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimSeismicSectionCollection::defineUiOrdering( QString uiConfigName, caf::PdmUiOrdering& uiOrdering )
{
    auto grp = uiOrdering.addNewGroup( "Surface Intersection Lines" );
    grp->add( &m_surfaceIntersectionLinesScaleFactor );
    grp->add( &m_surfacesWithVisibleSurfaceLines );

    uiOrdering.skipRemainingFields( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<RimSeismicSection*> RimSeismicSectionCollection::seismicSections() const
{
    return m_seismicSections.childrenByType();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimSeismicSectionCollection::appendPartsToModel( Rim3dView*                  view,
                                                      cvf::ModelBasicList*        model,
                                                      caf::DisplayCoordTransform* transform,
                                                      const cvf::BoundingBox&     boundingBox )
{
    if ( !isChecked() ) return;

    auto visibleSurfaces = m_surfacesWithVisibleSurfaceLines().ptrReferencedObjectsByType();

    for ( auto& section : m_seismicSections )
    {
        if ( section->isChecked() )
        {
            if ( section->seismicData() != nullptr )
            {
                section->partMgr()->appendGeometryPartsToModel( model, transform, boundingBox );

                section->partMgr()->appendSurfaceIntersectionLines( model, transform, m_surfaceIntersectionLinesScaleFactor(), visibleSurfaces );
            }
            section->partMgr()->appendPolylinePartsToModel( view, model, transform, boundingBox );
        }
    }

    model->updateBoundingBoxesRecursive();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimSeismicSectionCollection::fieldChangedByUi( const caf::PdmFieldHandle* changedField, const QVariant& oldValue, const QVariant& newValue )
{
    if ( changedField == &m_surfaceIntersectionLinesScaleFactor )
    {
        m_surfaceIntersectionLinesScaleFactor = std::max( 1.0, m_surfaceIntersectionLinesScaleFactor() );
    }

    updateView();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<RimRegularLegendConfig*> RimSeismicSectionCollection::legendConfigs()
{
    std::vector<RimRegularLegendConfig*> retVals;

    std::set<RimSeismicDataInterface*> usedSeisData;

    for ( auto& section : m_seismicSections )
    {
        auto seisData = section->seismicData();

        if ( usedSeisData.contains( seisData ) ) continue;

        retVals.push_back( seisData->legendConfig() );
        usedSeisData.insert( seisData );
    }

    return retVals;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimSeismicSectionCollection::updateLegendRangesTextAndVisibility( RiuViewer* nativeOrOverrideViewer, bool isUsingOverrideViewer )
{
    for ( auto& section : m_seismicSections )
    {
        if ( section->legendConfig() )
        {
            auto legendConfig = section->legendConfig();

            QString subtitle;
            if ( section->seismicData() )
            {
                subtitle = QString::fromStdString( section->seismicData()->userDescription() );

                const int maxChar = 20;
                if ( subtitle.size() > maxChar )
                {
                    subtitle = subtitle.left( maxChar - 2 );
                    subtitle += "..";
                }
            }

            legendConfig->setTitle( QString( "Seismic: \n" ) + subtitle );

            if ( section->isChecked() && legendConfig->showLegend() )
            {
                nativeOrOverrideViewer->addColorLegendToBottomLeftCorner( legendConfig->titledOverlayFrame(), isUsingOverrideViewer );
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimSeismicSectionCollection::setSurfacesVisible( const std::vector<RimSurface*>& surfaces )
{
    for ( auto surface : surfaces )
    {
        if ( std::find( m_surfacesWithVisibleSurfaceLines.begin(), m_surfacesWithVisibleSurfaceLines.end(), surface ) ==
             m_surfacesWithVisibleSurfaceLines.end() )
        {
            m_surfacesWithVisibleSurfaceLines.push_back( surface );
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QList<caf::PdmOptionItemInfo> RimSeismicSectionCollection::calculateValueOptions( const caf::PdmFieldHandle* fieldNeedingOptions )
{
    QList<caf::PdmOptionItemInfo> options;

    if ( fieldNeedingOptions == &m_surfacesWithVisibleSurfaceLines )
    {
        // If a surface is deleted, we need to remove it from the list of surfaces with visible surface lines
        m_surfacesWithVisibleSurfaceLines.removePtr( nullptr );

        auto surfaceCollection = RimTools::surfaceCollection();
        for ( auto surface : surfaceCollection->surfaces() )
        {
            options.push_back( caf::PdmOptionItemInfo( surface->fullName(), surface ) );
        }
    }

    return options;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimSeismicSectionCollection::onChildDeleted( caf::PdmChildArrayFieldHandle*      childArray,
                                                  std::vector<caf::PdmObjectHandle*>& referringObjects )
{
    updateView();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimSeismicSectionCollection::updateView()
{
    auto view = firstAncestorOrThisOfType<Rim3dView>();
    if ( view ) view->scheduleCreateDisplayModelAndRedraw();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimSeismicSectionCollection::shouldBeVisibleInTree() const
{
    RimProject* proj = RimProject::current();
    if ( proj == nullptr ) return false;

    return !proj->activeOilField()->seismicDataCollection()->isEmpty();
}
