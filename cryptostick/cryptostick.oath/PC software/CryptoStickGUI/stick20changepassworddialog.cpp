/*
* Author: Copyright (C) Rudolf Boeddeker  Date: 2013-08-13
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

#include "stick20changepassworddialog.h"
#include "ui_stick20changepassworddialog.h"
#include "stick20responsedialog.h"



/*******************************************************************************

 External declarations

*******************************************************************************/

/*******************************************************************************

 Local defines

*******************************************************************************/


/*******************************************************************************

  DialogChangePassword

  Constructor DialogChangePassword

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

DialogChangePassword::DialogChangePassword(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogChangePassword)
{
    ui->setupUi(this);

    ui->lineEdit_OldPW->setEchoMode(QLineEdit::Password);
    ui->lineEdit_NewPW_1->setEchoMode(QLineEdit::Password);
    ui->lineEdit_NewPW_2->setEchoMode(QLineEdit::Password);
}

/*******************************************************************************

  DialogChangePassword

  Destructor DialogChangePassword

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

DialogChangePassword::~DialogChangePassword()
{
    delete ui;
}

/*******************************************************************************

  InitData

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void DialogChangePassword::InitData(void)
{
    switch (PasswordKind)
    {
        case STICK20_PASSWORD_KIND_USER :
            ui->label->setText("Change user PIN");
            break;
        case STICK20_PASSWORD_KIND_ADMIN :
            ui->label->setText("Change admin PIN");
            break;
    }
}

/*******************************************************************************

  CheckResponse

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

int DialogChangePassword::CheckResponse(bool NoStopFlag)
{
    Stick20ResponseDialog ResponseDialog(this);

    ResponseDialog.setModal(TRUE);

    if (TRUE == NoStopFlag)
    {
        ResponseDialog.NoStopWhenStatusOK ();
    }

    ResponseDialog.cryptostick=cryptostick;

    return (ResponseDialog.exec());
}

/*******************************************************************************

  SendNewPassword

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void DialogChangePassword::SendNewPassword(void)
{
    int ret;
    QByteArray PasswordString;

    unsigned char Data[STICK20_PASSOWRD_LEN+2];

// Set kind of password
    switch (PasswordKind)
    {
        case STICK20_PASSWORD_KIND_USER :
            Data[0] = 'P';
            break;
        case STICK20_PASSWORD_KIND_ADMIN :
            Data[0] = 'A';
            break;
        default :
            Data[0] = '?';
            break;
    }

// Send old password
    PasswordString = ui->lineEdit_OldPW->text().toAscii();

    strncpy ((char*)&Data[1],PasswordString.data(),STICK20_PASSOWRD_LEN);
    Data[STICK20_PASSOWRD_LEN+1] = 0;

    ret = cryptostick->stick20SendPassword (Data);

    if ((int)true == ret)
    {
        CheckResponse (TRUE);
    }
    else
    {
        // Todo
        return;
    }

// Change password
    PasswordString = ui->lineEdit_NewPW_1->text().toAscii();

    strncpy ((char*)&Data[1],PasswordString.data(),STICK20_PASSOWRD_LEN);
    Data[STICK20_PASSOWRD_LEN+1] = 0;

    ret = cryptostick->stick20SendNewPassword (Data);

    if ((int)true == ret)
    {
        CheckResponse (FALSE);
    }
    else
    {
        // Todo
        return;
    }
}

/*******************************************************************************

  accept

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void DialogChangePassword::accept()
{
// Check for correct new password entrys
    if (0 != strcmp (ui->lineEdit_NewPW_1->text().toAscii(),ui->lineEdit_NewPW_2->text().toAscii()))
    {
        QMessageBox msgBox;

        msgBox.setText("The new password entrys are not the same");
        msgBox.exec();
        return;
    }

// Check the new length of password
    if (STICK20_PASSOWRD_LEN <= strlen (ui->lineEdit_NewPW_1->text().toAscii()))
    {
        QMessageBox msgBox;
        QString OutputText;

        OutputText = "The maximum length of a password is " + QString("%1").arg(STICK20_PASSOWRD_LEN)+ "chars";

        msgBox.setText(OutputText);
        msgBox.exec();
        return;
    }

// Send password to Stick 2.0
    SendNewPassword();

    done (true);
}

/*******************************************************************************

  on_checkBox_clicked

  Set echo mode for password edit

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/


void DialogChangePassword::on_checkBox_clicked(bool checked)
{
    if (checked)
    {
        ui->lineEdit_OldPW->setEchoMode(QLineEdit::Normal);
        ui->lineEdit_NewPW_1->setEchoMode(QLineEdit::Normal);
        ui->lineEdit_NewPW_2->setEchoMode(QLineEdit::Normal);
    }
    else
    {
        ui->lineEdit_OldPW->setEchoMode(QLineEdit::Password);
        ui->lineEdit_NewPW_1->setEchoMode(QLineEdit::Password);
        ui->lineEdit_NewPW_2->setEchoMode(QLineEdit::Password);
    }
}
