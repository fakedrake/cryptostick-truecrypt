/*
* Author: Copyright (C) Rudolf Boeddeker  Date: 2013-08-12
*
* This file is part of GPF Crypto Stick 2
*
* GPF Crypto Stick 2  is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* GPF Crypto Stick is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with GPF Crypto Stick. If not, see <http://www.gnu.org/licenses/>.
*/

#include "stick20debugdialog.h"
#include "ui_stick20debugdialog.h"

#include "device.h"
#include "response.h"


#include <QTimer>
#include <QMenu>
#include <QtGui>
#include <QDateTime>

/*******************************************************************************

 External declarations

*******************************************************************************/

extern "C" char DebugText_Stick20[600000];          // todo move to header
extern "C" unsigned long DebugTextlen_Stick20;      // todo move to header
extern "C" char DebugTextHasChanged;                // todo move to header
extern "C" int DebugingActive;                      // todo move to header
extern "C" int DebugingStick20PoolingActive;;       // todo move to header

/*******************************************************************************

 Local defines

*******************************************************************************/

#define CS20_DEBUG_DIALOG_POLL_TIME        100      // in ms

//#define LOCAL_DEBUG                               // activate for debugging

/*******************************************************************************

  DebugDialog

  Constructor DebugDialog

  Init the debug output dialog

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

DebugDialog::DebugDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DebugDialog)
{
    bool ret;

    ui->setupUi(this);

    // Create timer for polling the cryptostick 2.0
    RefreshTimer = new QTimer(this);

    ret = connect(RefreshTimer, SIGNAL(timeout()), this, SLOT(UpdateDebugText()));

    RefreshTimer->start(CS20_DEBUG_DIALOG_POLL_TIME);
}


/*******************************************************************************

  ~DebugDialog

  Destructor DebugDialog

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

DebugDialog::~DebugDialog()
{
    delete ui;

    RefreshTimer->stop();

    delete RefreshTimer;
}


/*******************************************************************************

  UpdateDebugText

  Get debug response from cryptostick 2.0 and update the dialog

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

void DebugDialog::UpdateDebugText()
{
    static int nCallCounter = 0;
    QString OutputText;
    int ret;


    if (true == DebugingStick20PoolingActive)
    {
        // Poll data from stick 20
        Response *stick20Response = new Response();

        ret = stick20Response->getResponse(cryptostick);
    }

#ifdef LOCAL_DEBUG
    { // For debugging
        char text[1000];
        sprintf(text,"Calls %5d, Bytes %7d", nCallCounter,DebugTextlen_Stick20);

        ui->label->setText(text);
    }
#endif

    // Check for auto scroll activ
    if (0 != ui->checkBox->checkState())
    {
        if (TRUE == DebugTextHasChanged)
        {
            ui->plainTextEdit->setPlainText(DebugText_Stick20);

            ui->plainTextEdit->moveCursor (QTextCursor::End);
       }
    }

    nCallCounter++;
}

/*******************************************************************************

  SetNewText

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

void DebugDialog::SetNewText(QString Text)
{
    ui->plainTextEdit->setPlainText(Text);
}


/*******************************************************************************

  on_pushButton_clicked

  Excecute when update button was pressed

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

void DebugDialog::on_pushButton_clicked()
{
    ui->plainTextEdit->setPlainText(DebugText_Stick20);

    ui->plainTextEdit->moveCursor (QTextCursor::End);
}
