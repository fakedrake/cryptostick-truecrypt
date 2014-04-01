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

#include "stick20dialog.h"
#include "ui_stick20dialog.h"

#include "device.h"
#include "response.h"
#include "string.h"
#include "sleep.h"
#include "base32.h"

#include "stick20updatedialog.h"
#include "stick20responsedialog.h"
#include "stick20matrixpassworddialog.h"

/*******************************************************************************

 External declarations

*******************************************************************************/


/*******************************************************************************

 Local defines

*******************************************************************************/

typedef struct {
    char    *Text_pu8;
    uint8_t  Command_u8;
} typeOptionsComboboxStick20;

typeOptionsComboboxStick20 tOptionsComboboxStick20[] =
{
    "Enable crypted volume",                STICK20_CMD_ENABLE_CRYPTED_PARI,
    "Disable crypted volume",               STICK20_CMD_DISABLE_CRYPTED_PARI,
    "Enable hidden crypted volume",         STICK20_CMD_ENABLE_HIDDEN_CRYPTED_PARI,
//    "Disable hidden crypted volume",     STICK20_CMD_DISABLE_HIDDEN_CRYPTED_PARI,
    "Enable firmware update",               STICK20_CMD_ENABLE_FIRMWARE_UPDATE,
    "Export firmware to file",              STICK20_CMD_EXPORT_FIRMWARE_TO_FILE,
    "Generate new AES keys",                STICK20_CMD_GENERATE_NEW_KEYS,
    "Fill SD card with random chars",       STICK20_CMD_FILL_SD_CARD_WITH_RANDOM_CHARS,
    "Get stick status - Todo",              STICK20_CMD_GET_DEVICE_STATUS,
    "Set readonly uncrypted volume",        STICK20_CMD_ENABLE_READONLY_UNCRYPTED_LUN,
    "Set readwrite uncrypted volume",       STICK20_CMD_ENABLE_READWRITE_UNCRYPTED_LUN,
    NULL, 0
};


/*******************************************************************************

  Stick20Dialog

  Constructor Stick20Dialog

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

Stick20Dialog::Stick20Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Stick20Dialog)
{
    int i;
    ui->setupUi(this);

    // At start always on
    ui->PasswordEdit->setEchoMode(QLineEdit::Password);
    ui->checkBox->setChecked(false);

    i = 0;

    // Set the combobox entries
    while (NULL != tOptionsComboboxStick20[i].Text_pu8)
    {
        ui->comboBox->addItem (QString(tOptionsComboboxStick20[i].Text_pu8));
        i++;
    }
/*
    ui->comboBox->addItem (QString("Enable Crypted Partition"));
    ui->comboBox->addItem (QString("Disable Crypted Partition"));
    ui->comboBox->addItem (QString("Enable Hidden Crypted Partition"));
    ui->comboBox->addItem (QString("Disable Hidden Crypted Partition"));
    ui->comboBox->addItem (QString("Enable Firmware Update"));
    ui->comboBox->addItem (QString("Export Firmware To File"));
    ui->comboBox->addItem (QString("Generate New Keys"));
    ui->comboBox->addItem (QString("Fill SD Card With Random Chars"));
    ui->comboBox->addItem (QString("Get Stick Status - Todo"));
    ui->comboBox->addItem (QString("Set readonly Uncrypted Partition"));
    ui->comboBox->addItem (QString("Set readwrite Uncrypted Partition"));
*/
/*
    ui->comboBox->addItem (QString("Debug - Get Password Matrix"));
    ui->comboBox->addItem (QString("Debug - Send Password Matrix Pin Data"));
    ui->comboBox->addItem (QString("Debug - Send Password Matrix Setup"));
    ui->comboBox->addItem (QString("Debug - Get stick 20 status"));
*/
}

/*******************************************************************************

  Stick20Dialog

  Destructor Stick20Dialog

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

Stick20Dialog::~Stick20Dialog()
{
    delete ui;
}

/*******************************************************************************

  on_buttonBox_accepted

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

void Stick20Dialog::on_buttonBox_accepted()
{
    bool        ret;
    bool        waitForAnswerFromStick20;
    bool        stopWhenStatusOKFromStick20;
    int         n;
    uint8_t     password[50];
    QByteArray  passwordString;
    QMessageBox msgBox;

    waitForAnswerFromStick20    = FALSE;
    stopWhenStatusOKFromStick20 = FALSE;

    // No Stick no work
    if (false == cryptostick->isConnected){
        msgBox.setText("Stick20Dialog: No stick 2.0 connected!");
        msgBox.exec();
        return;
    }

    if (false == ui->PasswordEdit->isHidden())
    {
        passwordString = ui->PasswordEdit->text().toAscii();
        // No password entered ?
        if (0 == passwordString.size()){
            msgBox.setText("Please enter a password");
            msgBox.exec();
            return;
        }
    }

    if (false == ui->checkBox_Matrix->isChecked())
    {
        // Send normal password
        password[0] = 'P';          // For normal password

        // Check the password length
        passwordString = ui->PasswordEdit->text().toAscii();
        n = passwordString.size();
        if (30 <= n) {
            msgBox.setText("Password too long! (Max = 30 char)");
            msgBox.exec();
            return;
        }
        memset (&password[1],0,49);
        memcpy(&password[1],passwordString.data(),n);
    }
    else
    {
        // Get matrix password
        MatrixPasswordDialog dialog (this);

        dialog.setModal (TRUE);

        dialog.cryptostick        = cryptostick;
        dialog.PasswordLen        = 6;
        dialog.SetupInterfaceFlag = false;

        dialog.InitSecurePasswordDialog ();

        if (false == dialog.exec())
        {
            return;
        }

        // Copy the matrix password
        password[0] = 'M';          // For matrix password
        dialog.CopyMatrixPassword((char*)&password[1],49);
    }
/*
    {
        int i;
        i = ui->comboBox->currentIndex();
        i++;
    }
*/
// get the command nr
    n = ui->comboBox->currentIndex();
    n = tOptionsComboboxStick20[n].Command_u8;
    switch (n)
    {
        case STICK20_CMD_ENABLE_CRYPTED_PARI            :
            ret = cryptostick->stick20EnableCryptedPartition (password);
            if (TRUE == ret)
            {
                waitForAnswerFromStick20 = TRUE;
            }
            break;
        case STICK20_CMD_DISABLE_CRYPTED_PARI           :
            ret = cryptostick->stick20DisableCryptedPartition ();
            if (TRUE == ret)
            {
                waitForAnswerFromStick20 = TRUE;
            }
            break;
        case STICK20_CMD_ENABLE_HIDDEN_CRYPTED_PARI     :
            ret = cryptostick->stick20EnableHiddenCryptedPartition (password);
            if (TRUE == ret)
            {
                waitForAnswerFromStick20 = TRUE;
            }
            break;
        case STICK20_CMD_DISABLE_HIDDEN_CRYPTED_PARI    :
            ret = cryptostick->stick20DisableHiddenCryptedPartition ();
            if (TRUE == ret)
            {
                waitForAnswerFromStick20 = TRUE;
            }
            break;
        case STICK20_CMD_ENABLE_FIRMWARE_UPDATE         :
            {
                UpdateDialog dialog(this);
                ret = dialog.exec();
                if (Accepted == ret)
                {
                    ret = cryptostick->stick20EnableFirmwareUpdate (password);
                    if (TRUE == ret)
                    {
                        waitForAnswerFromStick20 = TRUE;
                    }
                }
            }
            break;
        case STICK20_CMD_EXPORT_FIRMWARE_TO_FILE        :        
            ret = cryptostick->stick20ExportFirmware (password);
            if (TRUE == ret)
            {
                waitForAnswerFromStick20 = TRUE;
            }
            break;
        case STICK20_CMD_GENERATE_NEW_KEYS              :   
            {
                msgBox.setText("The generation of new AES keys will destroy the crypted volume!");
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                msgBox.setDefaultButton(QMessageBox::No);
                ret = msgBox.exec();
                if (Accepted == ret)
                {
                    ret = cryptostick->stick20CreateNewKeys (password);
                    if (TRUE == ret)
                    {
                        waitForAnswerFromStick20 = TRUE;
                    }
                }
            }
            break;
        case STICK20_CMD_FILL_SD_CARD_WITH_RANDOM_CHARS :
            {
                msgBox.setText("This command fills the hole sd card with random chars. This will destroy all volumes!");
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                msgBox.setDefaultButton(QMessageBox::No);
                ret = msgBox.exec();
                if (Accepted == ret)
                {
                    ret = cryptostick->stick20FillSDCardWithRandomChars (password,STICK20_FILL_SD_CARD_WITH_RANDOM_CHARS_ENCRYPTED_VOL);
                    if (TRUE == ret)
                    {
                        waitForAnswerFromStick20 = TRUE;
                    }
                }
            }
            break;
         case STICK20_CMD_WRITE_STATUS_DATA        :
            msgBox.setText("Not implemented");
            ret = msgBox.exec();
            break;
        case STICK20_CMD_ENABLE_READONLY_UNCRYPTED_LUN :
            ret = cryptostick->stick20SendSetReadonlyToUncryptedVolume (password);
            if (TRUE == ret)
            {
                waitForAnswerFromStick20 = TRUE;
            }
            break;
        case STICK20_CMD_ENABLE_READWRITE_UNCRYPTED_LUN :
            ret = cryptostick->stick20SendSetReadwriteToUncryptedVolume (password);
            if (TRUE == ret)
            {
                waitForAnswerFromStick20 = TRUE;
            }
            break;
        case STICK20_CMD_SEND_PASSWORD_MATRIX        :
            ret = cryptostick->stick20GetPasswordMatrix ();
            if (TRUE == ret)
            {
                waitForAnswerFromStick20 = TRUE;
            }
            break;
        case STICK20_CMD_SEND_PASSWORD_MATRIX_PINDATA        :
            ret = cryptostick->stick20SendPasswordMatrixPinData (password);
            if (TRUE == ret)
            {
                waitForAnswerFromStick20 = TRUE;
            }
            break;

        case STICK20_CMD_GET_DEVICE_STATUS        :
            ret = cryptostick->stick20GetStatusData ();
            if (TRUE == ret)
            {
                waitForAnswerFromStick20    = TRUE;
                stopWhenStatusOKFromStick20 = TRUE;
            }
            break;

        default :
        // ui->comboBox->currentIndex()
            msgBox.setText("Stick20Dialog: Wrong combobox value! ");
            msgBox.exec();
            break;

    }

    if (TRUE == waitForAnswerFromStick20)
    {
        Stick20ResponseDialog ResponseDialog(this);

        if (FALSE == stopWhenStatusOKFromStick20)
        {
            ResponseDialog.NoStopWhenStatusOK ();
        }
        ResponseDialog.cryptostick=cryptostick;

        ResponseDialog.exec();
    }

}

/*******************************************************************************

  on_checkBox_toggled

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

void Stick20Dialog::on_checkBox_toggled(bool checked)
{
    if (checked)
        ui->PasswordEdit->setEchoMode(QLineEdit::Normal);
    else
        ui->PasswordEdit->setEchoMode(QLineEdit::Password);
}

// Change dialog context

/*******************************************************************************

  InitEnterPasswordGui

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/


void Stick20Dialog::InitEnterPasswordGui(char *PasswordKind)
{
    ui->PasswordEdit->setText("");
    ui->PasswordEdit->setEnabled(true);
    ui->label->setText(PasswordKind);
    ui->PasswordEdit->show();
    ui->label->show();
    ui->checkBox->setChecked(false);
    ui->checkBox->show();

    ui->checkBox_Matrix->hide();                // Feature not used

//    ui->checkBox_Matrix->setChecked(false);
//    ui->checkBox_Matrix->show();

}

/*******************************************************************************

  InitNoPasswordGui

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/


void Stick20Dialog::InitNoPasswordGui(void)
{
    ui->label->hide();
    ui->PasswordEdit->setEnabled(false);
    ui->PasswordEdit->hide();
    ui->checkBox->hide();
    ui->checkBox_Matrix->hide();
}
/*******************************************************************************

  on_comboBox_currentIndexChanged

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

void Stick20Dialog::on_comboBox_currentIndexChanged(int index)
{

    index = ui->comboBox->currentIndex();
    index = tOptionsComboboxStick20[index].Command_u8;

    switch (index)
    {
        case STICK20_CMD_ENABLE_CRYPTED_PARI            :
            InitEnterPasswordGui ("User-Password");
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            break;
        case STICK20_CMD_DISABLE_CRYPTED_PARI           :
            InitNoPasswordGui ();
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
            break;
        case STICK20_CMD_ENABLE_HIDDEN_CRYPTED_PARI     :
            InitEnterPasswordGui ("User-Password");
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            break;
        case STICK20_CMD_DISABLE_HIDDEN_CRYPTED_PARI    :
            InitNoPasswordGui ();
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
            break;
        case STICK20_CMD_ENABLE_FIRMWARE_UPDATE         :
            InitEnterPasswordGui ("Admin-Password");
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            break;
        case STICK20_CMD_EXPORT_FIRMWARE_TO_FILE        :
            InitEnterPasswordGui ("Admin-Password");
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            break;
        case STICK20_CMD_GENERATE_NEW_KEYS              :
            InitEnterPasswordGui ("Admin-Password");
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            break;
        case STICK20_CMD_FILL_SD_CARD_WITH_RANDOM_CHARS :
            InitEnterPasswordGui ("Admin-Password");
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            break;
        case STICK20_CMD_ENABLE_READONLY_UNCRYPTED_LUN :
            InitEnterPasswordGui ("Admin-Password");
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            break;
        case STICK20_CMD_ENABLE_READWRITE_UNCRYPTED_LUN :
            InitEnterPasswordGui ("Admin-Password");
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            break;
        default :
            break;

    }
}


/*******************************************************************************

  on_checkBox_Matrix_toggled

  Reviews
  Date      Reviewer        Info
  12.08.13  RB              First review

*******************************************************************************/

void Stick20Dialog::on_checkBox_Matrix_toggled(bool checked)
{
    if (checked)
    {
        ui->PasswordEdit->setEnabled(false);
        ui->checkBox->setEnabled(false);
    }
    else
    {
        ui->PasswordEdit->setEnabled(true);
        ui->checkBox->setEnabled(true);
    }
}

/*******************************************************************************

  on_PasswordEdit_textChanged

  Changes
  Date      Author        Info
  03.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void Stick20Dialog::on_PasswordEdit_textChanged(const QString &arg1)
{
    if (0 ==  ui->PasswordEdit->text().size())
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    else
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }

}
