/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2021 -    Equinor ASA
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

#include "RimParameterGroup.h"

#include "cafPdmFieldScriptingCapability.h"
#include "cafPdmObjectScriptingCapability.h"
#include "cafPdmUiTableViewEditor.h"

#include "RimDoubleParameter.h"
#include "RimGenericParameter.h"
#include "RimIntegerParameter.h"
#include "RimStringParameter.h"

#include <cmath>

CAF_PDM_SOURCE_INIT( RimParameterGroup, "ParameterGroup" );

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimParameterGroup::RimParameterGroup()
{
    CAF_PDM_InitObject( "Parameter Group", ":/Bullet.png", "", "" );
    uiCapability()->setUiTreeChildrenHidden( true );

    CAF_PDM_InitFieldNoDefault( &m_parameters, "Parameters", "Parameters", "", "", "" );
    m_parameters.uiCapability()->setUiEditorTypeName( caf::PdmUiTableViewEditor::uiEditorTypeName() );
    m_parameters.uiCapability()->setUiLabelPosition( caf::PdmUiItemInfo::HIDDEN );
    m_parameters.uiCapability()->setCustomContextMenuEnabled( true );
    m_parameters.uiCapability()->setUiTreeChildrenHidden( true );

    CAF_PDM_InitFieldNoDefault( &m_name, "Name", "Name", "", "", "" );
    m_name.uiCapability()->setUiHidden( true );
    m_name.uiCapability()->setUiReadOnly( true );

    CAF_PDM_InitFieldNoDefault( &m_showExpanded, "Expanded", "Expanded", "", "", "" );
    m_name.uiCapability()->setUiHidden( true );
    m_name.uiCapability()->setUiReadOnly( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimParameterGroup::~RimParameterGroup()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
caf::PdmFieldHandle* RimParameterGroup::userDescriptionField()
{
    return &m_name;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::addParameter( RimGenericParameter* parameter )
{
    m_parameters.push_back( parameter );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::addParameter( QString name, int value )
{
    RimIntegerParameter* p = new RimIntegerParameter();
    p->setName( name );
    p->setLabel( name );
    p->setValue( value );

    m_parameters.push_back( p );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::addParameter( QString name, QString value )
{
    RimStringParameter* p = new RimStringParameter();
    p->setName( name );
    p->setLabel( name );
    p->setValue( value );

    m_parameters.push_back( p );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::addParameter( QString name, double value )
{
    RimDoubleParameter* p = new RimDoubleParameter();
    p->setName( name );
    p->setLabel( name );
    p->setValue( value );

    m_parameters.push_back( p );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::appendParametersToList( std::list<RimGenericParameter*>& parameterList )
{
    for ( auto p : m_parameters() )
    {
        parameterList.push_back( p );
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::defineEditorAttribute( const caf::PdmFieldHandle* field,
                                               QString                    uiConfigName,
                                               caf::PdmUiEditorAttribute* attribute )
{
    if ( field == &m_parameters )
    {
        auto tvAttribute = dynamic_cast<caf::PdmUiTableViewEditorAttribute*>( attribute );
        if ( tvAttribute )
        {
            tvAttribute->resizePolicy              = caf::PdmUiTableViewEditorAttribute::RESIZE_TO_FILL_CONTAINER;
            tvAttribute->alwaysEnforceResizePolicy = true;
            tvAttribute->minimumHeight             = 300;
        }
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::defineUiOrdering( QString uiConfigName, caf::PdmUiOrdering& uiOrdering )
{
    auto group = uiOrdering.addNewGroup( name() );
    group->add( &m_parameters );

    uiOrdering.skipRemainingFields( true );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::setName( QString name )
{
    m_name = name;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::setExpanded( bool expand )
{
    m_showExpanded = expand;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool RimParameterGroup::isExpanded() const
{
    return m_showExpanded;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
QString RimParameterGroup::name() const
{
    return m_name;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::vector<RimGenericParameter*> RimParameterGroup::parameters() const
{
    return m_parameters.childObjects();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::setParameterValue( QString name, int value )
{
    setParameterValue( name, QString::number( value ) );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::setParameterValue( QString name, double value )
{
    setParameterValue( name, QString::number( value ) );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RimParameterGroup::setParameterValue( QString name, QString value )
{
    RimGenericParameter* p = parameter( name );
    if ( p != nullptr ) p->setValue( value );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
RimGenericParameter* RimParameterGroup::parameter( QString name ) const
{
    for ( auto& p : m_parameters.childObjects() )
    {
        if ( p->name() == name )
        {
            return p;
        }
    }

    return nullptr;
}
