/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2024     Equinor ASA
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

#include "cafCmdFeature.h"

#include "Cloud/RiaSumoConnector.h"

#include <QDialog>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

class SimpleDialog : public QDialog
{
    Q_OBJECT

public:
    SimpleDialog( QWidget* parent = nullptr );
    ~SimpleDialog() override;

    void createConnection();

private:
    void onOkClicked();
    void onCancelClicked();
    void onTokenReady( const QString& token );
    void onAuthClicked();
    void onAssetsClicked();
    void onCasesClicked();
    void onVectorNamesClicked();
    void onFindBlobIdClicked();
    void onParquetClicked();
    void onShowContentParquetClicked();
    void onRealizationsClicked();

    bool isTokenValid();

private:
    QLabel*      label;
    QPushButton* okButton;
    QPushButton* cancelButton;
    QPushButton* authButton;
    QPushButton* assetsButton;
    QPushButton* casesButton;
    QPushButton* vectorNamesButton;
    QPushButton* blobIdButton;
    QPushButton* parquetDownloadButton;
    QPushButton* showContentParquetButton;
    QPushButton* realizationIdsButton;

    QPointer<RiaSumoConnector> m_sumoConnector;
};

//==================================================================================================
///
//==================================================================================================
class RicSumoDataFeature : public caf::CmdFeature
{
    CAF_CMD_HEADER_INIT;

protected:
    void onActionTriggered( bool isChecked ) override;
    void setupActionLook( QAction* actionToSetup ) override;

private:
    SimpleDialog* m_dialog = nullptr;
};
