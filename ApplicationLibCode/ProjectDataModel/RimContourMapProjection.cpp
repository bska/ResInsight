/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2018- Equinor ASA
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

#include "RimContourMapProjection.h"

#include "RiaOpenMPTools.h"
#include "RiaWeightedMeanCalculator.h"

#include "RigCellGeometryTools.h"
#include "RigContourMapCalculator.h"
#include "RigContourMapGrid.h"

#include "RimCase.h"
#include "RimGridView.h"
#include "RimProject.h"
#include "RimRegularLegendConfig.h"
#include "RimTextAnnotation.h"

#include "cafContourLines.h"
#include "cafPdmUiDoubleSliderEditor.h"
#include "cafPdmUiTreeOrdering.h"
#include "cafProgressInfo.h"

#include "cvfArray.h"
#include "cvfGeometryTools.h"
#include "cvfGeometryUtils.h"
#include "cvfScalarMapper.h"
#include "cvfStructGridGeometryGenerator.h"

#include <algorithm>

namespace caf
{
template <>
void RimContourMapProjection::ResultAggregation::setUp()
{
    addItem( RigContourMapCalculator::RESULTS_OIL_COLUMN, "OIL_COLUMN", "Oil Column" );
    addItem( RigContourMapCalculator::RESULTS_GAS_COLUMN, "GAS_COLUMN", "Gas Column" );
    addItem( RigContourMapCalculator::RESULTS_HC_COLUMN, "HC_COLUMN", "Hydrocarbon Column" );

    addItem( RigContourMapCalculator::RESULTS_MEAN_VALUE, "MEAN_VALUE", "Arithmetic Mean" );
    addItem( RigContourMapCalculator::RESULTS_HARM_VALUE, "HARM_VALUE", "Harmonic Mean" );
    addItem( RigContourMapCalculator::RESULTS_GEOM_VALUE, "GEOM_VALUE", "Geometric Mean" );
    addItem( RigContourMapCalculator::RESULTS_VOLUME_SUM, "VOLUME_SUM", "Volume Weighted Sum" );
    addItem( RigContourMapCalculator::RESULTS_SUM, "SUM", "Sum" );

    addItem( RigContourMapCalculator::RESULTS_TOP_VALUE, "TOP_VALUE", "Top  Value" );
    addItem( RigContourMapCalculator::RESULTS_MIN_VALUE, "MIN_VALUE", "Min Value" );
    addItem( RigContourMapCalculator::RESULTS_MAX_VALUE, "MAX_VALUE", "Max Value" );

    setDefault( RigContourMapCalculator::RESULTS_MEAN_VALUE );
}
} // namespace caf
CAF_PDM_ABSTRACT_SOURCE_INIT( RimContourMapProjection, "RimContourMapProjection" );

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimContourMapProjection::RimContourMapProjection()
    : m_pickPoint( cvf::Vec2d::UNDEFINED )
    , m_currentResultTimestep( -1 )
    , m_minResultAllTimeSteps( std::numeric_limits<double>::infinity() )
    , m_maxResultAllTimeSteps( -std::numeric_limits<double>::infinity() )
{
    CAF_PDM_InitObject( "RimContourMapProjection", ":/2DMapProjection16x16.png" );

    CAF_PDM_InitField( &m_relativeSampleSpacing, "SampleSpacing", 0.9, "Sample Spacing Factor" );
    m_relativeSampleSpacing.uiCapability()->setUiEditorTypeName( caf::PdmUiDoubleSliderEditor::uiEditorTypeName() );

    CAF_PDM_InitFieldNoDefault( &m_resultAggregation, "ResultAggregation", "Result Aggregation" );

    CAF_PDM_InitField( &m_showContourLines, "ContourLines", true, "Show Contour Lines" );
    CAF_PDM_InitField( &m_showContourLabels, "ContourLabels", true, "Show Contour Labels" );
    CAF_PDM_InitField( &m_smoothContourLines, "SmoothContourLines", true, "Smooth Contour Lines" );

    setName( "Map Projection" );
    nameField()->uiCapability()->setUiReadOnly( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimContourMapProjection::~RimContourMapProjection()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::generateResultsIfNecessary( int timeStep )
{
    caf::ProgressInfo progress( 100, "Generate Results", true );

    updateGridInformation();
    progress.setProgress( 10 );

    if ( gridMappingNeedsUpdating() || mapCellVisibilityNeedsUpdating() || resultVariableChanged() )
    {
        clearResults();
        clearTimeStepRange();

        m_cellGridIdxVisibility = getCellVisibility();

        if ( gridMappingNeedsUpdating() )
        {
            m_projected3dGridIndices = RigContourMapCalculator::generateGridMapping( *this, *m_contourMapGrid );
        }
        progress.setProgress( 20 );
        m_mapCellVisibility = getMapCellVisibility();
        progress.setProgress( 30 );
    }
    else
    {
        progress.setProgress( 30 );
    }

    if ( resultsNeedsUpdating( timeStep ) )
    {
        clearGeometry();
        m_aggregatedResults = generateResults( timeStep );
        progress.setProgress( 80 );
        generateVertexResults();
    }
    progress.setProgress( 100 );
    updateAfterResultGeneration( timeStep );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::generateGeometryIfNecessary()
{
    caf::ProgressInfo progress( 100, "Generate Geometry", true );

    if ( geometryNeedsUpdating() )
    {
        generateContourPolygons();
        progress.setProgress( 25 );
        generateTrianglesWithVertexValues();
    }
    progress.setProgress( 100 );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<cvf::Vec3d> RimContourMapProjection::generatePickPointPolygon()
{
    std::vector<cvf::Vec3d> points;

    if ( !m_pickPoint.isUndefined() )
    {
#ifndef NDEBUG
        cvf::Vec2d  cellDiagonal( sampleSpacing() * 0.5, sampleSpacing() * 0.5 );
        cvf::Vec2ui pickedCell = m_contourMapGrid->ijFromLocalPos( m_pickPoint );
        cvf::Vec2d  cellCenter = m_contourMapGrid->cellCenterPosition( pickedCell.x(), pickedCell.y() );
        cvf::Vec2d  cellCorner = cellCenter - cellDiagonal;
        points.push_back( cvf::Vec3d( cellCorner, 0.0 ) );
        points.push_back( cvf::Vec3d( cellCorner + cvf::Vec2d( sampleSpacing(), 0.0 ), 0.0 ) );
        points.push_back( cvf::Vec3d( cellCorner + cvf::Vec2d( sampleSpacing(), 0.0 ), 0.0 ) );
        points.push_back( cvf::Vec3d( cellCorner + cvf::Vec2d( sampleSpacing(), sampleSpacing() ), 0.0 ) );
        points.push_back( cvf::Vec3d( cellCorner + cvf::Vec2d( sampleSpacing(), sampleSpacing() ), 0.0 ) );
        points.push_back( cvf::Vec3d( cellCorner + cvf::Vec2d( 0.0, sampleSpacing() ), 0.0 ) );
        points.push_back( cvf::Vec3d( cellCorner + cvf::Vec2d( 0.0, sampleSpacing() ), 0.0 ) );
        points.push_back( cvf::Vec3d( cellCorner, 0.0 ) );
#endif
        points.push_back( cvf::Vec3d( m_pickPoint - cvf::Vec2d( 0.5 * sampleSpacing(), 0.0 ), 0.0 ) );
        points.push_back( cvf::Vec3d( m_pickPoint + cvf::Vec2d( 0.5 * sampleSpacing(), 0.0 ), 0.0 ) );
        points.push_back( cvf::Vec3d( m_pickPoint - cvf::Vec2d( 0.0, 0.5 * sampleSpacing() ), 0.0 ) );
        points.push_back( cvf::Vec3d( m_pickPoint + cvf::Vec2d( 0.0, 0.5 * sampleSpacing() ), 0.0 ) );
    }
    return points;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::clearGeometry()
{
    m_contourPolygons.clear();
    m_trianglesWithVertexValues.clear();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const std::vector<RimContourMapProjection::ContourPolygons>& RimContourMapProjection::contourPolygons() const
{
    return m_contourPolygons;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const std::vector<cvf::Vec4d>& RimContourMapProjection::trianglesWithVertexValues()
{
    return m_trianglesWithVertexValues;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::sampleSpacingFactor() const
{
    return m_relativeSampleSpacing();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::setSampleSpacingFactor( double spacingFactor )
{
    m_relativeSampleSpacing = spacingFactor;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::showContourLines() const
{
    return m_showContourLines();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::showContourLabels() const
{
    return m_showContourLabels();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimContourMapProjection::resultAggregationText() const
{
    return m_resultAggregation().uiText();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimContourMapProjection::caseName() const
{
    RimCase* rimCase = baseView()->ownerCase();
    if ( !rimCase ) return QString();

    return rimCase->caseUserDescription();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimContourMapProjection::currentTimeStepName() const
{
    RimCase* rimCase = baseView()->ownerCase();
    if ( !rimCase || m_currentResultTimestep == -1 ) return QString();

    return rimCase->timeStepName( m_currentResultTimestep );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::maxValue() const
{
    return maxValue( m_aggregatedResults );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::minValue() const
{
    return minValue( m_aggregatedResults );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::meanValue() const
{
    return sumAllValues() / numberOfValidCells();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::sumAllValues() const
{
    double sum = 0.0;

    for ( size_t index = 0; index < m_aggregatedResults.size(); ++index )
    {
        if ( m_aggregatedResults[index] != std::numeric_limits<double>::infinity() )
        {
            sum += m_aggregatedResults[index];
        }
    }
    return sum;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
cvf::Vec2ui RimContourMapProjection::numberOfElementsIJ() const
{
    return m_contourMapGrid->numberOfElementsIJ();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
cvf::Vec2ui RimContourMapProjection::numberOfVerticesIJ() const
{
    return m_contourMapGrid->numberOfVerticesIJ();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::isColumnResult() const
{
    return RigContourMapCalculator::isColumnResult( m_resultAggregation() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::valueAtVertex( uint i, uint j ) const
{
    size_t index = m_contourMapGrid->vertexIndexFromIJ( i, j );
    if ( index < numberOfVertices() )
    {
        return m_aggregatedVertexResults.at( index );
    }
    return std::numeric_limits<double>::infinity();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
uint RimContourMapProjection::numberOfCells() const
{
    return m_contourMapGrid->numberOfCells();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
uint RimContourMapProjection::numberOfValidCells() const
{
    uint validCount = 0u;
    for ( uint i = 0; i < numberOfCells(); ++i )
    {
        cvf::Vec2ui ij = m_contourMapGrid->ijFromCellIndex( i );
        if ( hasResultInCell( ij.x(), ij.y() ) )
        {
            validCount++;
        }
    }
    return validCount;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
size_t RimContourMapProjection::numberOfVertices() const
{
    cvf::Vec2ui gridSize = numberOfVerticesIJ();
    return static_cast<size_t>( gridSize.x() ) * static_cast<size_t>( gridSize.y() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::checkForMapIntersection( const cvf::Vec3d& domainPoint3d, cvf::Vec2d* contourMapPoint, double* valueAtPoint ) const
{
    CVF_TIGHT_ASSERT( contourMapPoint );
    CVF_TIGHT_ASSERT( valueAtPoint );

    const cvf::Vec3d& minPoint = m_contourMapGrid->expandedBoundingBox().min();
    cvf::Vec3d        mapPos3d = domainPoint3d - minPoint;
    cvf::Vec2d        mapPos2d( mapPos3d.x(), mapPos3d.y() );
    cvf::Vec2d        gridorigin( minPoint.x(), minPoint.y() );

    double value = interpolateValue( mapPos2d );
    if ( value != std::numeric_limits<double>::infinity() )
    {
        *valueAtPoint    = value;
        *contourMapPoint = mapPos2d + gridorigin;

        return true;
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::setPickPoint( cvf::Vec2d globalPickPoint )
{
    m_pickPoint = globalPickPoint - m_contourMapGrid->origin2d();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
cvf::Vec3d RimContourMapProjection::origin3d() const
{
    return m_contourMapGrid->expandedBoundingBox().min();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
size_t RimContourMapProjection::gridResultIndex( size_t globalCellIdx ) const
{
    return globalCellIdx;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::calculateValueInMapCell( uint i, uint j, const std::vector<double>& gridCellValues ) const
{
    const std::vector<std::pair<size_t, double>>& matchingCells = cellsAtIJ( i, j );
    return RigContourMapCalculator::calculateValueInMapCell( *this, matchingCells, gridCellValues, m_resultAggregation() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::gridMappingNeedsUpdating() const
{
    if ( m_projected3dGridIndices.size() != numberOfCells() ) return true;

    if ( m_cellGridIdxVisibility.isNull() ) return true;

    cvf::ref<cvf::UByteArray> currentVisibility = getCellVisibility();

    CVF_ASSERT( currentVisibility->size() == m_cellGridIdxVisibility->size() );
    for ( size_t i = 0; i < currentVisibility->size(); ++i )
    {
        if ( ( *currentVisibility )[i] != ( *m_cellGridIdxVisibility )[i] ) return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::resultsNeedsUpdating( int timeStep ) const
{
    return ( m_aggregatedResults.size() != numberOfCells() || m_aggregatedVertexResults.size() != numberOfVertices() ||
             timeStep != m_currentResultTimestep );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::geometryNeedsUpdating() const
{
    return m_contourPolygons.empty() || m_trianglesWithVertexValues.empty();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::resultRangeIsValid() const
{
    return m_minResultAllTimeSteps != std::numeric_limits<double>::infinity() &&
           m_maxResultAllTimeSteps != -std::numeric_limits<double>::infinity();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::clearGridMapping()
{
    clearResults();
    clearTimeStepRange();
    m_projected3dGridIndices.clear();
    m_mapCellVisibility.clear();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::clearResults()
{
    clearGeometry();

    m_aggregatedResults.clear();
    m_aggregatedVertexResults.clear();
    m_currentResultTimestep = -1;

    clearResultVariable();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::clearTimeStepRange()
{
    m_minResultAllTimeSteps = std::numeric_limits<double>::infinity();
    m_maxResultAllTimeSteps = -std::numeric_limits<double>::infinity();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::maxValue( const std::vector<double>& aggregatedResults ) const
{
    double maxV = -std::numeric_limits<double>::infinity();

    for ( size_t index = 0; index < aggregatedResults.size(); ++index )
    {
        if ( aggregatedResults[index] != std::numeric_limits<double>::infinity() )
        {
            maxV = std::max( maxV, aggregatedResults[index] );
        }
    }
    return maxV;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::minValue( const std::vector<double>& aggregatedResults ) const
{
    double minV = std::numeric_limits<double>::infinity();

    for ( size_t index = 0; index < aggregatedResults.size(); ++index )
    {
        if ( aggregatedResults[index] != std::numeric_limits<double>::infinity() )
        {
            minV = std::min( minV, aggregatedResults[index] );
        }
    }
    return minV;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::pair<double, double> RimContourMapProjection::minmaxValuesAllTimeSteps()
{
    if ( !resultRangeIsValid() )
    {
        clearTimeStepRange();

        m_minResultAllTimeSteps = std::min( m_minResultAllTimeSteps, minValue( m_aggregatedResults ) );
        m_maxResultAllTimeSteps = std::max( m_maxResultAllTimeSteps, maxValue( m_aggregatedResults ) );

        for ( int i = 0; i < (int)baseView()->ownerCase()->timeStepStrings().size() - 1; ++i )
        {
            if ( i != m_currentResultTimestep )
            {
                std::vector<double> aggregatedResults = generateResults( i );
                m_minResultAllTimeSteps               = std::min( m_minResultAllTimeSteps, minValue( aggregatedResults ) );
                m_maxResultAllTimeSteps               = std::max( m_maxResultAllTimeSteps, maxValue( aggregatedResults ) );
            }
        }
    }
    return std::make_pair( m_minResultAllTimeSteps, m_maxResultAllTimeSteps );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
cvf::ref<cvf::UByteArray> RimContourMapProjection::getCellVisibility() const
{
    return baseView()->currentTotalCellVisibility();
}

//--------------------------------------------------------------------------------------------------
/// Empty default implementation
//--------------------------------------------------------------------------------------------------
std::vector<bool> RimContourMapProjection::getMapCellVisibility()
{
    return std::vector<bool>( numberOfCells(), true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::mapCellVisibilityNeedsUpdating()
{
    std::vector<bool> mapCellVisiblity = getMapCellVisibility();
    return !( mapCellVisiblity == m_mapCellVisibility );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::generateVertexResults()
{
    size_t nCells = numberOfCells();
    if ( nCells != m_aggregatedResults.size() ) return;

    size_t nVertices          = numberOfVertices();
    m_aggregatedVertexResults = std::vector<double>( nVertices, std::numeric_limits<double>::infinity() );
#pragma omp parallel for
    for ( int index = 0; index < static_cast<int>( nVertices ); ++index )
    {
        cvf::Vec2ui ij                   = m_contourMapGrid->ijFromVertexIndex( index );
        m_aggregatedVertexResults[index] = calculateValueAtVertex( ij.x(), ij.y() );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::generateTrianglesWithVertexValues()
{
    std::vector<cvf::Vec3d> vertices = m_contourMapGrid->generateVertices();

    cvf::Vec2ui              patchSize = numberOfVerticesIJ();
    cvf::ref<cvf::UIntArray> faceList  = new cvf::UIntArray;
    cvf::GeometryUtils::tesselatePatchAsTriangles( patchSize.x(), patchSize.y(), 0u, true, faceList.p() );

    bool                discrete = false;
    std::vector<double> contourLevels;
    if ( legendConfig()->mappingMode() != RimRegularLegendConfig::MappingType::CATEGORY_INTEGER )
    {
        legendConfig()->scalarMapper()->majorTickValues( &contourLevels );
        if ( legendConfig()->mappingMode() == RimRegularLegendConfig::MappingType::LINEAR_DISCRETE ||
             legendConfig()->mappingMode() == RimRegularLegendConfig::MappingType::LOG10_DISCRETE )
        {
            discrete = true;
        }
    }

    const double cellArea      = sampleSpacing() * sampleSpacing();
    const double areaThreshold = 1.0e-5 * 0.5 * cellArea;

    std::vector<std::vector<std::vector<cvf::Vec3d>>> subtractPolygons;
    if ( !m_contourPolygons.empty() )
    {
        subtractPolygons.resize( m_contourPolygons.size() );
        for ( size_t i = 0; i < m_contourPolygons.size() - 1; ++i )
        {
            for ( size_t j = 0; j < m_contourPolygons[i + 1].size(); ++j )
            {
                subtractPolygons[i].push_back( m_contourPolygons[i + 1][j].vertices );
            }
        }
    }

    int numberOfThreads = RiaOpenMPTools::availableThreadCount();

    std::vector<std::vector<std::vector<cvf::Vec4d>>> threadTriangles( numberOfThreads );

#pragma omp parallel
    {
        int myThread = RiaOpenMPTools::currentThreadIndex();
        threadTriangles[myThread].resize( std::max( (size_t)1, m_contourPolygons.size() ) );

#pragma omp for schedule( dynamic )
        for ( int64_t i = 0; i < (int64_t)faceList->size(); i += 3 )
        {
            std::vector<cvf::Vec3d> triangle( 3 );
            std::vector<cvf::Vec4d> triangleWithValues( 3 );
            bool                    anyValidVertex = false;
            for ( size_t n = 0; n < 3; ++n )
            {
                uint vn = ( *faceList )[i + n];
                double value = vn < m_aggregatedVertexResults.size() ? m_aggregatedVertexResults[vn] : std::numeric_limits<double>::infinity();
                triangle[n]           = vertices[vn];
                triangleWithValues[n] = cvf::Vec4d( vertices[vn], value );
                if ( value != std::numeric_limits<double>::infinity() )
                {
                    anyValidVertex = true;
                }
            }

            if ( !anyValidVertex )
            {
                continue;
            }

            if ( m_contourPolygons.empty() )
            {
                threadTriangles[myThread][0].insert( threadTriangles[myThread][0].end(), triangleWithValues.begin(), triangleWithValues.end() );
                continue;
            }

            bool outsideOuterLimit = false;
            for ( size_t c = 0; c < m_contourPolygons.size() && !outsideOuterLimit; ++c )
            {
                std::vector<std::vector<cvf::Vec3d>> intersectPolygons;
                for ( size_t j = 0; j < m_contourPolygons[c].size(); ++j )
                {
                    bool containsAtLeastOne = false;
                    for ( size_t t = 0; t < 3; ++t )
                    {
                        if ( m_contourPolygons[c][j].bbox.contains( triangle[t] ) )
                        {
                            containsAtLeastOne = true;
                        }
                    }
                    if ( containsAtLeastOne )
                    {
                        std::vector<std::vector<cvf::Vec3d>> clippedPolygons =
                            RigCellGeometryTools::intersectionWithPolygon( triangle, m_contourPolygons[c][j].vertices );
                        intersectPolygons.insert( intersectPolygons.end(), clippedPolygons.begin(), clippedPolygons.end() );
                    }
                }

                if ( intersectPolygons.empty() )
                {
                    outsideOuterLimit = true;
                    continue;
                }

                std::vector<std::vector<cvf::Vec3d>> clippedPolygons;

                if ( !subtractPolygons[c].empty() )
                {
                    for ( const std::vector<cvf::Vec3d>& polygon : intersectPolygons )
                    {
                        std::vector<std::vector<cvf::Vec3d>> fullyClippedPolygons =
                            RigCellGeometryTools::subtractPolygons( polygon, subtractPolygons[c] );
                        clippedPolygons.insert( clippedPolygons.end(), fullyClippedPolygons.begin(), fullyClippedPolygons.end() );
                    }
                }
                else
                {
                    clippedPolygons.swap( intersectPolygons );
                }

                {
                    std::vector<cvf::Vec4d> clippedTriangles;
                    for ( std::vector<cvf::Vec3d>& clippedPolygon : clippedPolygons )
                    {
                        std::vector<std::vector<cvf::Vec3d>> polygonTriangles;
                        if ( clippedPolygon.size() == 3u )
                        {
                            polygonTriangles.push_back( clippedPolygon );
                        }
                        else
                        {
                            cvf::Vec3d baryCenter = cvf::Vec3d::ZERO;
                            for ( size_t v = 0; v < clippedPolygon.size(); ++v )
                            {
                                cvf::Vec3d& clippedVertex = clippedPolygon[v];
                                baryCenter += clippedVertex;
                            }
                            baryCenter /= clippedPolygon.size();
                            for ( size_t v = 0; v < clippedPolygon.size(); ++v )
                            {
                                std::vector<cvf::Vec3d> clippedTriangle;
                                if ( v == clippedPolygon.size() - 1 )
                                {
                                    clippedTriangle = { clippedPolygon[v], clippedPolygon[0], baryCenter };
                                }
                                else
                                {
                                    clippedTriangle = { clippedPolygon[v], clippedPolygon[v + 1], baryCenter };
                                }
                                polygonTriangles.push_back( clippedTriangle );
                            }
                        }
                        for ( const std::vector<cvf::Vec3d>& polygonTriangle : polygonTriangles )
                        {
                            // Check triangle area
                            double area =
                                0.5 * ( ( polygonTriangle[1] - polygonTriangle[0] ) ^ ( polygonTriangle[2] - polygonTriangle[0] ) ).length();
                            if ( area < areaThreshold ) continue;
                            for ( const cvf::Vec3d& localVertex : polygonTriangle )
                            {
                                double value = std::numeric_limits<double>::infinity();
                                if ( discrete )
                                {
                                    value = contourLevels[c] + 0.01 * ( contourLevels.back() - contourLevels.front() ) / contourLevels.size();
                                }
                                else
                                {
                                    for ( size_t n = 0; n < 3; ++n )
                                    {
                                        if ( ( triangle[n] - localVertex ).length() < sampleSpacing() * 0.01 &&
                                             triangleWithValues[n].w() != std::numeric_limits<double>::infinity() )
                                        {
                                            value = triangleWithValues[n].w();
                                            break;
                                        }
                                    }
                                    if ( value == std::numeric_limits<double>::infinity() )
                                    {
                                        value = interpolateValue( cvf::Vec2d( localVertex.x(), localVertex.y() ) );
                                        if ( value == std::numeric_limits<double>::infinity() )
                                        {
                                            value = contourLevels[c];
                                        }
                                    }
                                }

                                cvf::Vec4d globalVertex( localVertex, value );
                                clippedTriangles.push_back( globalVertex );
                            }
                        }
                    }

                    {
                        // Add critical section here due to a weird bug when running in a single thread
                        // Running multi threaded does not require this critical section, as we use a thread local data
                        // structure
#pragma omp critical
                        threadTriangles[myThread][c].insert( threadTriangles[myThread][c].end(),
                                                             clippedTriangles.begin(),
                                                             clippedTriangles.end() );
                    }
                }
            }
        }
    }

    std::vector<std::vector<cvf::Vec4d>> trianglesPerLevel( std::max( (size_t)1, m_contourPolygons.size() ) );
    for ( size_t c = 0; c < trianglesPerLevel.size(); ++c )
    {
        std::vector<cvf::Vec4d> allTrianglesThisLevel;
        for ( size_t i = 0; i < threadTriangles.size(); ++i )
        {
            allTrianglesThisLevel.insert( allTrianglesThisLevel.end(), threadTriangles[i][c].begin(), threadTriangles[i][c].end() );
        }

        double triangleAreasThisLevel = sumTriangleAreas( allTrianglesThisLevel );
        if ( c >= m_contourLevelCumulativeAreas.size() || triangleAreasThisLevel > 1.0e-3 * m_contourLevelCumulativeAreas[c] )
        {
            trianglesPerLevel[c] = allTrianglesThisLevel;
        }
    }

    std::vector<cvf::Vec4d> finalTriangles;
    for ( size_t i = 0; i < trianglesPerLevel.size(); ++i )
    {
        finalTriangles.insert( finalTriangles.end(), trianglesPerLevel[i].begin(), trianglesPerLevel[i].end() );
    }

    m_trianglesWithVertexValues = finalTriangles;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::generateContourPolygons()
{
    std::vector<ContourPolygons> contourPolygons;

    std::vector<double> contourLevels;
    if ( resultRangeIsValid() && legendConfig()->mappingMode() != RimRegularLegendConfig::MappingType::CATEGORY_INTEGER )
    {
        legendConfig()->scalarMapper()->majorTickValues( &contourLevels );
        int nContourLevels = static_cast<int>( contourLevels.size() );

        if ( minValue() != std::numeric_limits<double>::infinity() && maxValue() != -std::numeric_limits<double>::infinity() &&
             std::fabs( maxValue() - minValue() ) > 1.0e-8 )
        {
            if ( nContourLevels > 2 )
            {
                const size_t N = contourLevels.size();
                // Adjust contour levels slightly to avoid weird visual artifacts due to numerical error.
                double fudgeFactor    = 1.0e-3;
                double fudgeAmountMin = fudgeFactor * ( contourLevels[1] - contourLevels[0] );
                double fudgeAmountMax = fudgeFactor * ( contourLevels[N - 1u] - contourLevels[N - 2u] );

                contourLevels.front() += fudgeAmountMin;
                contourLevels.back() -= fudgeAmountMax;

                double simplifyEpsilon = m_smoothContourLines() ? 5.0e-2 * sampleSpacing() : 1.0e-3 * sampleSpacing();

                if ( nContourLevels >= 10 )
                {
                    simplifyEpsilon *= 2.0;
                }
                if ( numberOfCells() > 100000 )
                {
                    simplifyEpsilon *= 2.0;
                }
                else if ( numberOfCells() > 1000000 )
                {
                    simplifyEpsilon *= 4.0;
                }

                std::vector<caf::ContourLines::ListOfLineSegments> unorderedLineSegmentsPerLevel =
                    caf::ContourLines::create( m_aggregatedVertexResults, xVertexPositions(), yVertexPositions(), contourLevels );

                contourPolygons = std::vector<ContourPolygons>( unorderedLineSegmentsPerLevel.size() );
                const double areaThreshold = 1.5 * ( sampleSpacing() * sampleSpacing() ) / ( sampleSpacingFactor() * sampleSpacingFactor() );

#pragma omp parallel for
                for ( int i = 0; i < (int)unorderedLineSegmentsPerLevel.size(); ++i )
                {
                    contourPolygons[i] = RigContourPolygonsTools::createContourPolygonsFromLineSegments( unorderedLineSegmentsPerLevel[i],
                                                                                                         contourLevels[i],
                                                                                                         areaThreshold );

                    if ( m_smoothContourLines() )
                    {
                        RigContourPolygonsTools::smoothContourPolygons( contourPolygons[i], true, sampleSpacing() );
                    }

                    for ( RigContourPolygonsTools::ContourPolygon& polygon : contourPolygons[i] )
                    {
                        RigCellGeometryTools::simplifyPolygon( &polygon.vertices, simplifyEpsilon );
                    }
                }

                // The clipping of contour polygons is intended to detect and fix a smoothed contour polygons
                // crossing into an outer contour line. The current implementation has some side effects causing
                // several contour lines to disappear. Disable this clipping for now
                /*
                if ( m_smoothContourLines() )
                {
                    for ( size_t i = 1; i < contourPolygons.size(); ++i )
                    {
                        RigContourPolygonsTools::clipContourPolygons(&contourPolygons[i], &contourPolygons[i - 1] );
                    }
                }
                */

                m_contourLevelCumulativeAreas.resize( contourPolygons.size(), 0.0 );
                for ( int64_t i = (int64_t)contourPolygons.size() - 1; i >= 0; --i )
                {
                    double levelOuterArea            = RigContourPolygonsTools::sumPolygonArea( contourPolygons[i] );
                    m_contourLevelCumulativeAreas[i] = levelOuterArea;
                }
            }
        }
    }
    m_contourPolygons = contourPolygons;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::sumTriangleAreas( const std::vector<cvf::Vec4d>& triangles )
{
    double sumArea = 0.0;
    for ( size_t i = 0; i < triangles.size(); i += 3 )
    {
        cvf::Vec3d v1( triangles[i].x(), triangles[i].y(), triangles[i].z() );
        cvf::Vec3d v2( triangles[i + 1].x(), triangles[i + 1].y(), triangles[i + 1].z() );
        cvf::Vec3d v3( triangles[i + 2].x(), triangles[i + 2].y(), triangles[i + 2].z() );
        double     area = 0.5 * ( ( v3 - v1 ) ^ ( v2 - v1 ) ).length();
        sumArea += area;
    }
    return sumArea;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::isMeanResult() const
{
    return RigContourMapCalculator::isMeanResult( m_resultAggregation() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::isStraightSummationResult() const
{
    return RigContourMapCalculator::isStraightSummationResult( m_resultAggregation() );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::interpolateValue( const cvf::Vec2d& gridPos2d ) const
{
    cvf::Vec2ui cellContainingPoint = m_contourMapGrid->ijFromLocalPos( gridPos2d );
    cvf::Vec2d  cellCenter          = m_contourMapGrid->cellCenterPosition( cellContainingPoint.x(), cellContainingPoint.y() );

    std::array<cvf::Vec3d, 4> x;
    x[0] = cvf::Vec3d( cellCenter + cvf::Vec2d( -sampleSpacing() * 0.5, -sampleSpacing() * 0.5 ), 0.0 );
    x[1] = cvf::Vec3d( cellCenter + cvf::Vec2d( sampleSpacing() * 0.5, -sampleSpacing() * 0.5 ), 0.0 );
    x[2] = cvf::Vec3d( cellCenter + cvf::Vec2d( sampleSpacing() * 0.5, sampleSpacing() * 0.5 ), 0.0 );
    x[3] = cvf::Vec3d( cellCenter + cvf::Vec2d( -sampleSpacing() * 0.5, sampleSpacing() * 0.5 ), 0.0 );

    cvf::Vec4d baryCentricCoords = cvf::GeometryTools::barycentricCoords( x[0], x[1], x[2], x[3], cvf::Vec3d( gridPos2d, 0.0 ) );

    std::array<cvf::Vec2ui, 4> v;
    v[0] = cellContainingPoint;
    v[1] = cvf::Vec2ui( cellContainingPoint.x() + 1u, cellContainingPoint.y() );
    v[2] = cvf::Vec2ui( cellContainingPoint.x() + 1u, cellContainingPoint.y() + 1u );
    v[3] = cvf::Vec2ui( cellContainingPoint.x(), cellContainingPoint.y() + 1u );

    std::array<double, 4> vertexValues;
    double                validBarycentricCoordsSum = 0.0;
    for ( int i = 0; i < 4; ++i )
    {
        double vertexValue = valueAtVertex( v[i].x(), v[i].y() );
        if ( vertexValue == std::numeric_limits<double>::infinity() )
        {
            return std::numeric_limits<double>::infinity();
        }
        else
        {
            vertexValues[i] = vertexValue;
            validBarycentricCoordsSum += baryCentricCoords[i];
        }
    }

    if ( validBarycentricCoordsSum < 1.0e-8 )
    {
        return std::numeric_limits<double>::infinity();
    }

    // Calculate final value
    double value = 0.0;
    for ( int i = 0; i < 4; ++i )
    {
        value += baryCentricCoords[i] / validBarycentricCoordsSum * vertexValues[i];
    }

    return value;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::valueInCell( uint i, uint j ) const
{
    size_t index = m_contourMapGrid->cellIndexFromIJ( i, j );
    if ( index < numberOfCells() )
    {
        return m_aggregatedResults.at( index );
    }
    return std::numeric_limits<double>::infinity();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimContourMapProjection::hasResultInCell( uint i, uint j ) const
{
    return !cellsAtIJ( i, j ).empty();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::calculateValueAtVertex( uint vi, uint vj ) const
{
    std::vector<uint> averageIs;
    std::vector<uint> averageJs;

    if ( vi > 0u ) averageIs.push_back( vi - 1 );
    if ( vj > 0u ) averageJs.push_back( vj - 1 );
    if ( vi < m_contourMapGrid->mapSize().x() ) averageIs.push_back( vi );
    if ( vj < m_contourMapGrid->mapSize().y() ) averageJs.push_back( vj );

    RiaWeightedMeanCalculator<double> calc;
    for ( uint j : averageJs )
    {
        for ( uint i : averageIs )
        {
            if ( hasResultInCell( i, j ) )
            {
                calc.addValueAndWeight( valueInCell( i, j ), 1.0 );
            }
        }
    }
    if ( calc.validAggregatedWeight() )
    {
        return calc.weightedMean();
    }
    return std::numeric_limits<double>::infinity();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<std::pair<size_t, double>> RimContourMapProjection::cellsAtIJ( uint i, uint j ) const
{
    size_t cellIndex = m_contourMapGrid->cellIndexFromIJ( i, j );
    if ( cellIndex < m_projected3dGridIndices.size() )
    {
        return m_projected3dGridIndices[cellIndex];
    }
    return std::vector<std::pair<size_t, double>>();
}

//--------------------------------------------------------------------------------------------------
/// Vertex positions in local coordinates (add origin2d.x() for UTM x)
//--------------------------------------------------------------------------------------------------
std::vector<double> RimContourMapProjection::xVertexPositions() const
{
    return m_contourMapGrid->xVertexPositions();
}

//--------------------------------------------------------------------------------------------------
/// Vertex positions in local coordinates (add origin2d.y() for UTM y)
//--------------------------------------------------------------------------------------------------
std::vector<double> RimContourMapProjection::yVertexPositions() const
{
    return m_contourMapGrid->yVertexPositions();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
double RimContourMapProjection::gridEdgeOffset() const
{
    return sampleSpacing() * 2.0;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::fieldChangedByUi( const caf::PdmFieldHandle* changedField, const QVariant& oldValue, const QVariant& newValue )
{
    if ( changedField == &m_resultAggregation )
    {
        ResultAggregation previousAggregation = static_cast<RigContourMapCalculator::ResultAggregationEnum>( oldValue.toInt() );
        if ( RigContourMapCalculator::isStraightSummationResult( previousAggregation ) != isStraightSummationResult() )
        {
            clearGridMapping();
        }
        else
        {
            clearResults();
        }
        clearTimeStepRange();
    }
    else if ( changedField == &m_smoothContourLines )
    {
        clearGeometry();
    }
    else if ( changedField == &m_relativeSampleSpacing )
    {
        clearGridMapping();
        clearResults();
        clearTimeStepRange();
    }

    baseView()->updateConnectedEditors();

    RimProject* proj = RimProject::current();
    proj->scheduleCreateDisplayModelAndRedrawAllViews();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::defineEditorAttribute( const caf::PdmFieldHandle* field, QString uiConfigName, caf::PdmUiEditorAttribute* attribute )
{
    if ( &m_relativeSampleSpacing == field )
    {
        caf::PdmUiDoubleSliderEditorAttribute* myAttr = dynamic_cast<caf::PdmUiDoubleSliderEditorAttribute*>( attribute );
        if ( myAttr )
        {
            myAttr->m_minimum                       = 0.2;
            myAttr->m_maximum                       = 2.0;
            myAttr->m_sliderTickCount               = 9;
            myAttr->m_delaySliderUpdateUntilRelease = true;
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::defineUiOrdering( QString uiConfigName, caf::PdmUiOrdering& uiOrdering )
{
    caf::PdmUiGroup* mainGroup = uiOrdering.addNewGroup( "Projection Settings" );
    mainGroup->add( &m_resultAggregation );
    legendConfig()->uiOrdering( "NumLevelsOnly", *mainGroup );
    mainGroup->add( &m_relativeSampleSpacing );
    mainGroup->add( &m_showContourLines );
    mainGroup->add( &m_showContourLabels );
    m_showContourLabels.uiCapability()->setUiReadOnly( !m_showContourLines() );
    mainGroup->add( &m_smoothContourLines );
    m_smoothContourLines.uiCapability()->setUiReadOnly( !m_showContourLines() );
    uiOrdering.skipRemainingFields( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::defineUiTreeOrdering( caf::PdmUiTreeOrdering& uiTreeOrdering, QString uiConfigName /*= ""*/ )
{
    uiTreeOrdering.skipRemainingChildren( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimContourMapProjection::initAfterRead()
{
}
