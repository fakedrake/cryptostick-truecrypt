/*
* Author: Copyright (C) Andrzej Surowiec 2012
*						Parts Rudolf Boeddeker  Date: 2013-08-13
*
* This file is part of GPF Crypto Stick.
*
* GPF Crypto Stick is free software: you can redistribute it and/or modify
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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "device.h"
#include "response.h"
#include "string.h"
#include "sleep.h"
#include "base32.h"
#include "passworddialog.h"
#include "hotpdialog.h"
#include "stick20dialog.h"
#include "stick20debugdialog.h"
#include "aboutdialog.h"

#include "device.h"
#include "response.h"
#include "stick20responsedialog.h"
#include "stick20matrixpassworddialog.h"
#include "stick20setup.h"
#include "stick20updatedialog.h"
#include "stick20changepassworddialog.h"

#include <QTimer>
#include <QMenu>
#include <QDialog>
#include <QtGui>
#include <QDateTime>

enum DialogCode { Rejected, Accepted };     // Why not found ?

/*******************************************************************************

 External declarations

*******************************************************************************/

//extern "C" char DebugText_Stick20[600000];

extern "C" void DebugAppendText (char *Text);
extern "C" void DebugClearText (void);

/*******************************************************************************

 Local defines

*******************************************************************************/

#define VID_STICK_OTP 0x20A0
#define PID_STICK_OTP 0x4108

#define VID_STICK_20  0x20A0
#define PID_STICK_20  0x4109 // MSD + CCID + HID production id

//#define PID_STICK_20    0x220D // MSD + CCID + HID test id
//#define PID_STICK_20    0x2309 // MSD + HID test id
//#define PID_STICK_20    0x220B // HID test id

/*******************************************************************************

  MainWindow

  Constructor MainWindow

  Init the debug output dialog

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

MainWindow::MainWindow(int FlagDebug,int SConfigActive,QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    bool ret;

    trayMenu             = NULL;
    CryptedVolumeActive  = FALSE;
    HiddenVolumeActive   = FALSE;
    NormalVolumeRWActive = FALSE;

    SpecialConfigActive = SConfigActive;

    switch (FlagDebug)
    {
        case DEBUG_STATUS_LOCAL_DEBUG :
            DebugWindowActive            = TRUE;
            DebugingActive               = TRUE;
            DebugingStick20PoolingActive = FALSE;
            break;

        case DEBUG_STATUS_DEBUG_ALL :
            DebugWindowActive            = TRUE;
            DebugingActive               = TRUE;
            DebugingStick20PoolingActive = TRUE;
            break;

        case DEBUG_STATUS_NO_DEBUGGING :
        default :
            DebugWindowActive            = FALSE;
            DebugingActive               = FALSE;
            DebugingStick20PoolingActive = FALSE;
            break;
    }


    ui->setupUi(this);
    ui->statusBar->showMessage("Device disconnected.");

    cryptostick =  new Device(VID_STICK_OTP, PID_STICK_OTP,VID_STICK_20,PID_STICK_20);

    QTimer *timer = new QTimer(this);
    ret = connect(timer, SIGNAL(timeout()), this, SLOT(checkConnection()));
    timer->start(1000);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/images/CS_icon.png"));

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    trayIcon->show();

    if (TRUE == trayIcon->supportsMessages ())
    {
        trayIcon->showMessage ("Cryptostick GUI","active");
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Cryptostick GUI active");
        msgBox.exec();
    }

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    restoreAction = new QAction(tr("&Configure OTP"), this);
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(startConfiguration()));

    DebugAction = new QAction(tr("&Debug"), this);
    connect(DebugAction, SIGNAL(triggered()), this, SLOT(startStickDebug()));

    ActionAboutDialog = new QAction(tr("&About Crypto Stick"), this);
    connect(ActionAboutDialog, SIGNAL(triggered()), this, SLOT(startAboutDialog()));

    initActionsForStick20 ();

    // Init debug text
    DebugClearText ();
    DebugAppendText ("Start Debug - ");

#ifdef WIN32
    DebugAppendText ("WIN32 system\n");
#endif

#ifdef linux
    DebugAppendText ("LINUX system\n");
#endif

#ifdef MAC
    DebugAppendText ("MAC system\n");
#endif

    {
        union {
            unsigned char input[4];
            unsigned int  endianCheck;
        } uEndianCheck;

        unsigned char text[50];

        DebugAppendText ("\nEndian check\n\n");

        DebugAppendText ("Store 0x01 0x02 0x03 0x04 in memory locations x,x+1,x+2,x+3\n");
        DebugAppendText ("then read the location x - x+3 as an unsigned int\n\n");

        uEndianCheck.input[0] = 0x01;
        uEndianCheck.input[1] = 0x02;
        uEndianCheck.input[2] = 0x03;
        uEndianCheck.input[3] = 0x04;

        sprintf ((char*)text,"write u8  %02x%02x%02x%02x\n",uEndianCheck.input[0],uEndianCheck.input[1],uEndianCheck.input[2],uEndianCheck.input[3]);
        DebugAppendText ((char*)text);

        sprintf ((char*)text,"read  u32 %08x\n",uEndianCheck.endianCheck);
        DebugAppendText ((char*)text);

        DebugAppendText ("\n");

        if (0x01020304 == uEndianCheck.endianCheck)
        {
            DebugAppendText ("System is little endian\n");
        }
        if (0x04030201 == uEndianCheck.endianCheck)
        {
            DebugAppendText ("System is big endian\n");
        }
        DebugAppendText ("\n");
    }

    generateMenu();

}


/*******************************************************************************

  iconActivated

  Changes
  Date      Author        Info
  31.01.14  RB            Function created

  Reviews
  Date      Reviewer        Info


*******************************************************************************/

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Context:
//        trayMenu->hide();
//        trayMenu->close();
        break;
    case QSystemTrayIcon::Trigger:
        trayMenu->popup(QCursor::pos());
        break;
    case QSystemTrayIcon::DoubleClick:
        break;
    case QSystemTrayIcon::MiddleClick:
        break;
    default:
        ;
    }
}

/*******************************************************************************

  eventFilter

  Changes
  Date      Author        Info
  31.01.14  RB            Function created

  Reviews
  Date      Reviewer        Info


*******************************************************************************/

bool MainWindow::eventFilter (QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
      QMouseEvent *mEvent = static_cast<QMouseEvent *>(event);
      if(mEvent->button() == Qt::LeftButton)
      {
/*
        QMouseEvent my_event = new QMouseEvent ( mEvent->type(), mEvent->pos(), Qt::Rightbutton ,
        mEvent->buttons(), mEvent->modifiers() );
        QCoreApplication::postEvent ( trayIcon, my_event );
*/
        return true;
      }
    }
    return QObject::eventFilter(obj, event);
}

/*******************************************************************************

  checkConnection

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::checkConnection()
{
    currentTime = QDateTime::currentDateTime().toTime_t();

    int result = cryptostick->checkConnection();
    if (result==0)
    {
        if (false == cryptostick->activStick20) {
            ui->statusBar->showMessage("Device connected.");
        } else {
            ui->statusBar->showMessage("Device Stick 2.0 connected.");
        }
    }
    else if (result==-1){
        ui->statusBar->showMessage("Device disconnected.");
        CryptedVolumeActive = FALSE;
        HiddenVolumeActive  = FALSE;
        generateMenu();
        cryptostick->connect();
    }
    else if (result==1){ //recreate the settings and menus
        if (false == cryptostick->activStick20) {
            ui->statusBar->showMessage("Device connected.");
        } else {
            ui->statusBar->showMessage("Device Stick 2.0 connected.");
        }
        generateMenu();
    }

    if (TRUE == Stick20_ConfigurationChanged)
    {
        Stick20_ConfigurationChanged = FALSE;
        UpdateDynamicMenuEntrys ();
    }
}

/*******************************************************************************

  startTimer

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::startTimer()
{
}

/*******************************************************************************

  ~MainWindow

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

MainWindow::~MainWindow()
{
    delete ui;
}

/*******************************************************************************

  closeEvent

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::closeEvent(QCloseEvent *event)
{
    this->hide();
    event->ignore();
}

/*******************************************************************************

  on_pushButton_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_pushButton_clicked()
{
    if (cryptostick->isConnected){
        int64_t crc = cryptostick->getSlotName(0x11);

        Sleep::msleep(100);
        Response *testResponse=new Response();
        testResponse->getResponse(cryptostick);

        if (crc==testResponse->lastCommandCRC){

            QMessageBox message;
            QString str;
            //str.append(QString::number(testResponse->lastCommandCRC,16));
            QByteArray *data =new QByteArray(testResponse->data);
            str.append(QString(data->toHex()));

            message.setText(str);
            message.exec();

        }
    }
}

/*******************************************************************************

  on_pushButton_2_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_pushButton_2_clicked()
{
}

/*******************************************************************************

  getSlotNames

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::getSlotNames()
{
}

/*******************************************************************************

  generateMenu

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::generateMenu()
{
    int i;

// Delete old menu
    if (NULL != trayMenu)
    {
        delete trayMenu;
    }

// Setup the new menu
    trayMenu = new QMenu();

// About entry

    if (cryptostick->isConnected==false){
        trayMenu->addAction("Crypto Stick not connected");
    }
    else{
        if (false == cryptostick->activStick20)
        {
            // Stick 10 is connected
            generateMenuForStick10 ();
        }
        else {
            // Stick 20 is connected
            generateMenuForStick20 ();
        }
    }

    // Add debug window ?
    if (TRUE == DebugWindowActive)
    {
        trayMenu->addAction(DebugAction);
    }

    trayMenu->addSeparator();

    trayMenu->addAction(ActionAboutDialog);

    trayMenu->addAction(quitAction);
    trayIcon->setContextMenu(trayMenu);

    ui->slotComboBox->clear();

    for (i=0;i<HOTP_SLOT_COUNT;i++)
    {
        ui->slotComboBox->addItem(QString("HOTP slot ").append(QString::number(i+1,10)).append(" [").append((char *)cryptostick->HOTPSlots[i]->slotName).append("]"));
    }

    for (i=0;i<TOTP_SLOT_COUNT;i++)
    {
        ui->slotComboBox->addItem(QString("TOTP slot ").append(QString::number(i+1,10)).append(" [").append((char *)cryptostick->TOTPSlots[i]->slotName).append("]"));
    }
    ui->slotComboBox->setCurrentIndex(0);

    i = ui->slotComboBox->currentIndex();
}


/*******************************************************************************

  initActionsForStick20

  Changes
  Date      Author        Info
  03.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::initActionsForStick20()
{
    SecPasswordAction = new QAction(tr("&SecPassword"), this);
    connect(SecPasswordAction, SIGNAL(triggered()), this, SLOT(startMatrixPasswordDialog()));

    Stick20Action = new QAction(tr("&Stick 20"), this);
    connect(Stick20Action, SIGNAL(triggered()), this, SLOT(startStick20Configuration()));

    Stick20SetupAction = new QAction(tr("&Stick 20 Setup"), this);
    connect(Stick20SetupAction, SIGNAL(triggered()), this, SLOT(startStick20Setup()));

    Stick20ActionEnableCryptedVolume = new QAction(tr("&Unlock volume"), this);
    connect(Stick20ActionEnableCryptedVolume, SIGNAL(triggered()), this, SLOT(startStick20EnableCryptedVolume()));

    Stick20ActionDisableCryptedVolume = new QAction(tr("&Lock volume"), this);
    connect(Stick20ActionDisableCryptedVolume, SIGNAL(triggered()), this, SLOT(startStick20DisableCryptedVolume()));

    Stick20ActionEnableHiddenVolume = new QAction(tr("&Unlock hidden volume"), this);
    connect(Stick20ActionEnableHiddenVolume, SIGNAL(triggered()), this, SLOT(startStick20EnableHiddenVolume()));

    Stick20ActionDisableHiddenVolume = new QAction(tr("&Lock hidden volume"), this);
    connect(Stick20ActionDisableHiddenVolume, SIGNAL(triggered()), this, SLOT(startStick20DisableHiddenVolume()));

    Stick20ActionChangeUserPIN = new QAction(tr("&Change user PIN"), this);
    connect(Stick20ActionChangeUserPIN, SIGNAL(triggered()), this, SLOT(startStick20ActionChangeUserPIN()));

    Stick20ActionChangeAdminPIN = new QAction(tr("&Change admin PIN"), this);
    connect(Stick20ActionChangeAdminPIN, SIGNAL(triggered()), this, SLOT(startStick20ActionChangeAdminPIN()));

    Stick20ActionEnableFirmwareUpdate = new QAction(tr("&Enable firmware update"), this);
    connect(Stick20ActionEnableFirmwareUpdate, SIGNAL(triggered()), this, SLOT(startStick20EnableFirmwareUpdate()));

    Stick20ActionExportFirmwareToFile = new QAction(tr("&Export firmware to file"), this);
    connect(Stick20ActionExportFirmwareToFile, SIGNAL(triggered()), this, SLOT(startStick20ExportFirmwareToFile()));

    Stick20ActionDestroyCryptedVolume = new QAction(tr("&Destroy crypted volume"), this);
    connect(Stick20ActionDestroyCryptedVolume, SIGNAL(triggered()), this, SLOT(startStick20DestroyCryptedVolume()));

    Stick20ActionFillSDCardWithRandomChars = new QAction(tr("&Fill SD card with random chars"), this);
    connect(Stick20ActionFillSDCardWithRandomChars, SIGNAL(triggered()), this, SLOT(startStick20FillSDCardWithRandomChars()));

    Stick20ActionGetStickStatus = new QAction(tr("&Get stick status"), this);
    connect(Stick20ActionGetStickStatus, SIGNAL(triggered()), this, SLOT(startStick20GetStickStatus()));

    Stick20ActionSetReadonlyUncryptedVolume = new QAction(tr("&Set readonly uncrypted volume"), this);
    connect(Stick20ActionSetReadonlyUncryptedVolume, SIGNAL(triggered()), this, SLOT(startStick20SetReadonlyUncryptedVolume()));

    Stick20ActionSetReadWriteUncryptedVolume = new QAction(tr("&Set readwrite uncrypted volume"), this);
    connect(Stick20ActionSetReadWriteUncryptedVolume, SIGNAL(triggered()), this, SLOT(startStick20SetReadWriteUncryptedVolume()));

    Stick20ActionDebugAction = new QAction(tr("&Debug Action"), this);
    connect(Stick20ActionDebugAction, SIGNAL(triggered()), this, SLOT(startStick20DebugAction()));
}

/*******************************************************************************

  generateMenuOTP

  Changes
  Date      Author        Info
  24.03.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::generateMenuOTP()
{
    if (cryptostick->HOTPSlots[0]->isProgrammed==true){
        QString actionName("HOTP slot 1 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->HOTPSlots[0]->slotName),this,SLOT(getHOTP1()));
    }
    if (cryptostick->HOTPSlots[1]->isProgrammed==true){
        QString actionName("HOTP slot 2 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->HOTPSlots[1]->slotName),this,SLOT(getHOTP2()));
    }
#if (HOTP_SLOT_COUNT >= 3)
    if (cryptostick->HOTPSlots[2]->isProgrammed==true){
        QString actionName("HOTP slot 3 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->HOTPSlots[2]->slotName),this,SLOT(getHOTP3()));
    }
#endif

    if (cryptostick->TOTPSlots[0]->isProgrammed==true){
        QString actionName("TOTP slot 1 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[0]->slotName),this,SLOT(getTOTP1()));
    }
    if (cryptostick->TOTPSlots[1]->isProgrammed==true){
        QString actionName("TOTP slot 2 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[1]->slotName),this,SLOT(getTOTP2()));
    }
    if (cryptostick->TOTPSlots[2]->isProgrammed==true){
        QString actionName("TOTP slot 3 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[2]->slotName),this,SLOT(getTOTP3()));
    }
    if (cryptostick->TOTPSlots[3]->isProgrammed==true){
        QString actionName("TOTP slot 4 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[3]->slotName),this,SLOT(getTOTP4()));
    }

#if (TOTP_SLOT_COUNT > 4)
    if (cryptostick->TOTPSlots[4]->isProgrammed==true){
        QString actionName("TOTP slot 5 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[4]->slotName),this,SLOT(getTOTP5()));
    }
#endif
#if (TOTP_SLOT_COUNT > 5)
    if (cryptostick->TOTPSlots[5]->isProgrammed==true){
        QString actionName("TOTP slot 6 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[5]->slotName),this,SLOT(getTOTP6()));
    }
#endif
#if (TOTP_SLOT_COUNT > 6)
    if (cryptostick->TOTPSlots[6]->isProgrammed==true){
        QString actionName("TOTP slot 7 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[6]->slotName),this,SLOT(getTOTP7()));
    }
#endif
#if (TOTP_SLOT_COUNT > 7)
    if (cryptostick->TOTPSlots[7]->isProgrammed==true){
        QString actionName("TOTP slot 8 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[7]->slotName),this,SLOT(getTOTP8()));
    }
#endif
#if (TOTP_SLOT_COUNT > 8)
    if (cryptostick->TOTPSlots[8]->isProgrammed==true){
        QString actionName("TOTP slot 9 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[8]->slotName),this,SLOT(getTOTP9()));
    }
#endif
#if (TOTP_SLOT_COUNT > 9)
    if (cryptostick->TOTPSlots[9]->isProgrammed==true){
        QString actionName("TOTP slot 10 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[8]->slotName),this,SLOT(getTOTP10()));
    }
#endif
#if (TOTP_SLOT_COUNT > 10)
    if (cryptostick->TOTPSlots[10]->isProgrammed==true){
        QString actionName("TOTP slot 11 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[10]->slotName),this,SLOT(getTOTP11()));
    }
#endif
#if (TOTP_SLOT_COUNT > 11)
    if (cryptostick->TOTPSlots[11]->isProgrammed==true){
        QString actionName("TOTP slot 12 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[11]->slotName),this,SLOT(getTOTP12()));
    }
#endif
#if (TOTP_SLOT_COUNT > 12)
    if (cryptostick->TOTPSlots[12]->isProgrammed==true){
        QString actionName("TOTP slot 13 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[12]->slotName),this,SLOT(getTOTP13()));
    }
#endif
#if (TOTP_SLOT_COUNT > 13)
    if (cryptostick->TOTPSlots[13]->isProgrammed==true){
        QString actionName("TOTP slot 14 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[13]->slotName),this,SLOT(getTOTP14()));
    }
#endif
#if (TOTP_SLOT_COUNT > 14)
    if (cryptostick->TOTPSlots[14]->isProgrammed==true){
        QString actionName("TOTP slot 15 ");
        trayMenu->addAction(actionName.append((char *)cryptostick->TOTPSlots[14]->slotName),this,SLOT(getTOTP15()));
    }
#endif


    trayMenu->addSeparator();
}

/*******************************************************************************

  generateMenuForStick10

  Changes
  Date      Author        Info
  27.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::generateMenuForStick10()
{
    generateMenuOTP ();

    trayMenu->addAction(restoreAction);
}

/*******************************************************************************

  generateMenuForStick20

  Changes
  Date      Author        Info
  03.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::generateMenuForStick20()
{
    generateMenuOTP ();

    if (FALSE == CryptedVolumeActive)
    {
        trayMenu->addAction(Stick20ActionEnableCryptedVolume        );
    }
    else
    {
        trayMenu->addAction(Stick20ActionDisableCryptedVolume       );
    }

    if (FALSE == HiddenVolumeActive)
    {
        trayMenu->addAction(Stick20ActionEnableHiddenVolume         );
    }
    else
    {
        trayMenu->addAction(Stick20ActionDisableHiddenVolume        );
    }


    trayMenuSubConfigure  = trayMenu->addMenu( "Configure" );
    trayMenuSubConfigure->addAction(restoreAction);
    trayMenuSubConfigure->addAction(Stick20ActionChangeUserPIN);
    trayMenuSubConfigure->addAction(Stick20ActionChangeAdminPIN);
    trayMenuSubConfigure->addAction(Stick20ActionDestroyCryptedVolume       );
    trayMenuSubConfigure->addAction(Stick20ActionGetStickStatus             );


    if (TRUE == SpecialConfigActive)
    {
        trayMenuSubSpecialConfigure = trayMenuSubConfigure->addMenu( "Special Configure" );
        trayMenuSubSpecialConfigure->addAction(Stick20ActionEnableFirmwareUpdate       );
        trayMenuSubSpecialConfigure->addAction(Stick20ActionExportFirmwareToFile       );
        trayMenuSubSpecialConfigure->addAction(Stick20ActionFillSDCardWithRandomChars  );

        if (FALSE == NormalVolumeRWActive)
        {
            trayMenuSubSpecialConfigure->addAction(Stick20ActionSetReadonlyUncryptedVolume );      // Set RW active
        }
        else
        {
            trayMenuSubSpecialConfigure->addAction(Stick20ActionSetReadWriteUncryptedVolume);      // Set readonly active
        }
    }


    // Add secure password dialog test
//    trayMenu->addAction(SecPasswordAction);

    // Add debug window ?
    if (TRUE == DebugWindowActive)
    {
        trayMenu->addSeparator();
        trayMenu->addAction(Stick20Action);
        trayMenu->addAction(Stick20ActionDebugAction);
    }

}


/*******************************************************************************

  generateHOTPConfig

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::generateHOTPConfig(HOTPSlot *slot)
{
    int selectedSlot = ui->slotComboBox->currentIndex();

    if ((selectedSlot >= 0) && (selectedSlot < HOTP_SLOT_COUNT))
    {
        slot->slotNumber=selectedSlot+0x10;


        QByteArray secretFromGUI = QByteArray::fromHex(ui->secretEdit->text().toAscii());
        memset(slot->secret,0,20);
        memcpy(slot->secret,secretFromGUI.data(),secretFromGUI.size());

        QByteArray slotNameFromGUI = QByteArray(ui->nameEdit->text().toAscii());
        memset(slot->slotName,0,15);
        memcpy(slot->slotName,slotNameFromGUI.data(),slotNameFromGUI.size());

        memset(slot->tokenID,0,13);
        QByteArray ompFromGUI = (ui->ompEdit->text().toAscii());
        memcpy(slot->tokenID,ompFromGUI,2);

        QByteArray ttFromGUI = (ui->ttEdit->text().toAscii());
        memcpy(slot->tokenID+2,ttFromGUI,2);

        QByteArray muiFromGUI = (ui->muiEdit->text().toAscii());
        memcpy(slot->tokenID+4,muiFromGUI,8);

        slot->tokenID[12]=ui->keyboardComboBox->currentIndex()&0xFF;

        QByteArray counterFromGUI = QByteArray::fromHex(ui->counterEdit->text().toAscii());
        memset(slot->counter,0,8);
        memcpy(slot->counter,counterFromGUI.data(),counterFromGUI.length());

        slot->config=0;

        if (ui->digits8radioButton->isChecked())
            slot->config+=(1<<0);
        if (ui->enterCheckBox->isChecked())
            slot->config+=(1<<1);
        if (ui->tokenIDCheckBox->isChecked())
            slot->config+=(1<<2);


    }
   // qDebug() << slot->counter;

}

/*******************************************************************************

  generateTOTPConfig

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::generateTOTPConfig(TOTPSlot *slot)
{
    int selectedSlot = ui->slotComboBox->currentIndex();

    // get the TOTP slot number
    selectedSlot -= HOTP_SLOT_COUNT;

    if ((selectedSlot >= 0) && (selectedSlot < TOTP_SLOT_COUNT))
    {
        slot->slotNumber = selectedSlot + 0x20;

        QByteArray secretFromGUI = QByteArray::fromHex(ui->secretEdit->text().toAscii());
        memset(slot->secret,0,20);
        memcpy(slot->secret,secretFromGUI.data(),secretFromGUI.size());

        QByteArray slotNameFromGUI = QByteArray(ui->nameEdit->text().toAscii());
        memset(slot->slotName,0,15);
        memcpy(slot->slotName,slotNameFromGUI.data(),slotNameFromGUI.size());

        memset(slot->tokenID,0,13);
        QByteArray ompFromGUI = (ui->ompEdit->text().toAscii());
        memcpy(slot->tokenID,ompFromGUI,2);

        QByteArray ttFromGUI = (ui->ttEdit->text().toAscii());
        memcpy(slot->tokenID+2,ttFromGUI,2);

        QByteArray muiFromGUI = (ui->muiEdit->text().toAscii());
        memcpy(slot->tokenID+4,muiFromGUI,8);

        slot->config=0;

        if (ui->digits8radioButton->isChecked())
            slot->config+=(1<<0);
        if (ui->enterCheckBox->isChecked())
            slot->config+=(1<<1);
        if (ui->tokenIDCheckBox->isChecked())
            slot->config+=(1<<2);


    }
}

/*******************************************************************************

  generateAllConfigs

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::generateAllConfigs()
{
    cryptostick->initializeConfig();
    cryptostick->getSlotConfigs();
    displayCurrentSlotConfig();
    generateMenu();
}

/*******************************************************************************

  displayCurrentSlotConfig

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::displayCurrentSlotConfig()
{
    uint8_t slotNo = ui->slotComboBox->currentIndex();

    if (slotNo == 255)
    {
        return;
    }

    if ((slotNo >=0) && (slotNo < HOTP_SLOT_COUNT))
    {
        //ui->hotpGroupBox->show();
        ui->hotpGroupBox->setTitle("OATH-HOTP Parameters");
        ui->label_5->setText("HOTP length:");
        ui->label_6->show();
        ui->counterEdit->show();
        ui->setToZeroButton->show();
        ui->setToRandomButton->show();
        ui->enterCheckBox->show();

        //slotNo=slotNo+0x10;
        ui->nameEdit->setText(QString((char *)cryptostick->HOTPSlots[slotNo]->slotName));

        QByteArray secret((char *) cryptostick->HOTPSlots[slotNo]->secret,20);
        ui->hexRadioButton->setChecked(true);
        ui->secretEdit->setText(secret.toHex());

        QByteArray counter((char *) cryptostick->HOTPSlots[slotNo]->counter,8);
        ui->counterEdit->setText(counter.toHex());

        if (cryptostick->HOTPSlots[slotNo]->counter==0)
            ui->counterEdit->setText("0");

        QByteArray omp((char *)cryptostick->HOTPSlots[slotNo]->tokenID,2);
        ui->ompEdit->setText(QString(omp));

        QByteArray tt((char *)cryptostick->HOTPSlots[slotNo]->tokenID+2,2);
        ui->ttEdit->setText(QString(tt));

        QByteArray mui((char *)cryptostick->HOTPSlots[slotNo]->tokenID+4,8);
        ui->muiEdit->setText(QString(mui));

        if (cryptostick->HOTPSlots[slotNo]->config&(1<<0))
            ui->digits8radioButton->setChecked(true);
        else ui->digits6radioButton->setChecked(true);

        if (cryptostick->HOTPSlots[slotNo]->config&(1<<1))
            ui->enterCheckBox->setChecked(true);
        else ui->enterCheckBox->setChecked(false);

        if (cryptostick->HOTPSlots[slotNo]->config&(1<<2))
            ui->tokenIDCheckBox->setChecked(true);
        else ui->tokenIDCheckBox->setChecked(false);


        //qDebug() << "Counter value:" << cryptostick->HOTPSlots[slotNo]->counter;

    }
    else if ((slotNo >= HOTP_SLOT_COUNT) && (slotNo < HOTP_SLOT_COUNT + TOTP_SLOT_COUNT))
    {
        slotNo -= HOTP_SLOT_COUNT;
        //ui->hotpGroupBox->hide();
        ui->hotpGroupBox->setTitle("OATH-TOTP Parameters");
        ui->label_5->setText("TOTP length:");
        ui->label_6->hide();
        ui->counterEdit->hide();
        ui->setToZeroButton->hide();
        ui->setToRandomButton->hide();
        ui->enterCheckBox->hide();


        ui->nameEdit->setText(QString((char *)cryptostick->TOTPSlots[slotNo]->slotName));


        QByteArray secret((char *) cryptostick->TOTPSlots[slotNo]->secret,20);
        ui->hexRadioButton->setChecked(true);
        ui->secretEdit->setText(secret.toHex());

        ui->counterEdit->setText("0");

    QByteArray omp((char *)cryptostick->TOTPSlots[slotNo]->tokenID,2);
    ui->ompEdit->setText(QString(omp));

    QByteArray tt((char *)cryptostick->TOTPSlots[slotNo]->tokenID+2,2);
    ui->ttEdit->setText(QString(tt));

    QByteArray mui((char *)cryptostick->TOTPSlots[slotNo]->tokenID+4,8);
    ui->muiEdit->setText(QString(mui));

    if (!cryptostick->TOTPSlots[slotNo]->isProgrammed){
        ui->ompEdit->setText("CS");
        ui->ttEdit->setText("01");
        QByteArray cardSerial = QByteArray((char *) cryptostick->cardSerial).toHex();
        ui->muiEdit->setText(QString( "%1" ).arg(QString(cardSerial),8,'0'));
    }

    if (cryptostick->TOTPSlots[slotNo]->config&(1<<0))
        ui->digits8radioButton->setChecked(true);
    else ui->digits6radioButton->setChecked(true);

    if (cryptostick->TOTPSlots[slotNo]->config&(1<<1))
        ui->enterCheckBox->setChecked(true);
    else ui->enterCheckBox->setChecked(false);

    if (cryptostick->TOTPSlots[slotNo]->config&(1<<2))
        ui->tokenIDCheckBox->setChecked(true);
    else ui->tokenIDCheckBox->setChecked(false);
    }


    if (!cryptostick->TOTPSlots[slotNo]->isProgrammed){
        ui->ompEdit->setText("CS");
        ui->ttEdit->setText("01");
        QByteArray cardSerial = QByteArray((char *) cryptostick->cardSerial).toHex();
        ui->muiEdit->setText(QString( "%1" ).arg(QString(cardSerial),8,'0'));
    }
}

/*******************************************************************************

  displayCurrentGeneralConfig

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/


void MainWindow::displayCurrentGeneralConfig()
{
    QByteArray firmware = QByteArray((char *) cryptostick->firmwareVersion).toHex();
    ui->firmwareEdit->setText(QString(firmware));
   // qDebug() << QString(firmware);
    QByteArray cardSerial = QByteArray((char *) cryptostick->cardSerial).toHex();

    ui->serialEdit->setText(QString( "%1" ).arg(QString(cardSerial),8,'0'));

    ui->numLockComboBox->setCurrentIndex(0);
    ui->capsLockComboBox->setCurrentIndex(0);
    ui->scrollLockComboBox->setCurrentIndex(0);

    if (cryptostick->generalConfig[0]==0||cryptostick->generalConfig[0]==1)
        ui->numLockComboBox->setCurrentIndex(cryptostick->generalConfig[0]+1);
    if (cryptostick->generalConfig[1]==0||cryptostick->generalConfig[1]==1)
        ui->capsLockComboBox->setCurrentIndex(cryptostick->generalConfig[1]+1);
    if (cryptostick->generalConfig[2]==0||cryptostick->generalConfig[2]==1)
        ui->scrollLockComboBox->setCurrentIndex(cryptostick->generalConfig[2]+1);



}

/*******************************************************************************

  startConfiguration

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::startConfiguration()
{

    //PasswordDialog pd;
    //pd.exec();
    bool ok;

    if (!cryptostick->validPassword){
        cryptostick->getPasswordRetryCount();

        QString password = QInputDialog::getText(this, tr("Enter card admin password"),tr("Admin password: ")+tr("(Tries left: ")+QString::number(cryptostick->passwordRetryCount)+")", QLineEdit::Password,"", &ok);

        if (ok){

            uint8_t tempPassword[25];

            for (int i=0;i<25;i++)
                tempPassword[i]=qrand()&0xFF;

            cryptostick->firstAuthenticate((uint8_t *)password.toAscii().data(),tempPassword);
            password.clear();
        }
    }
    if (cryptostick->validPassword){

        cryptostick->getSlotConfigs();
        displayCurrentSlotConfig();

        cryptostick->getStatus();
        displayCurrentGeneralConfig();

        showNormal();

   }
    else if (ok){
        QMessageBox msgBox;
         msgBox.setText("Invalid password!");
         msgBox.exec();
    }
}

/*******************************************************************************

  startStick20Configuration

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::startStick20Configuration()
{
    Stick20Dialog dialog(this);

    dialog.cryptostick=cryptostick;

    dialog.exec();
}

/*******************************************************************************

  startStickDebug

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::startStickDebug()
{
    DebugDialog dialog(this);

    dialog.cryptostick=cryptostick;

    dialog.SetNewText (DebugText_Stick20);

    dialog.exec();
}

/*******************************************************************************

  startAboutDialog

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::startAboutDialog()
{
    AboutDialog dialog(this);

    dialog.exec();
}


/*******************************************************************************

  startStick20Setup

  For testing only

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::startStick20Setup()
{
    Stick20Setup dialog(this);

    dialog.cryptostick=cryptostick;

    dialog.exec();
}

/*******************************************************************************

  startMatrixPasswordDialog

  For testing only

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::startMatrixPasswordDialog()
{
    MatrixPasswordDialog dialog(this);

    dialog.cryptostick=cryptostick;
    dialog.PasswordLen=6;
    dialog.SetupInterfaceFlag = FALSE;

    dialog.InitSecurePasswordDialog ();

    dialog.exec();
}


/*******************************************************************************

  startStick20EnableCryptedVolume

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20EnableCryptedVolume()
{
    uint8_t password[40];
    bool    ret;

    PasswordDialog dialog(this);
    dialog.init("Enter user password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_ENABLE_CRYPTED_PARI,password);
    }
}

/*******************************************************************************

  startStick20DisableCryptedVolume

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20DisableCryptedVolume()
{
    uint8_t password[40];

    password[0] = 0;
    stick20SendCommand (STICK20_CMD_DISABLE_CRYPTED_PARI,password);
}

/*******************************************************************************

  startStick20EnableHiddenVolume

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20EnableHiddenVolume()
{
    uint8_t password[40];
    bool    ret;

    PasswordDialog dialog(this);
    dialog.init("Enter hidden password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_ENABLE_HIDDEN_CRYPTED_PARI,password);
    }
}

/*******************************************************************************

  startStick20DisableHiddenVolume

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20DisableHiddenVolume()
{
    uint8_t password[40];

    password[0] = 0;
    stick20SendCommand (STICK20_CMD_DISABLE_HIDDEN_CRYPTED_PARI,password);

}


/*******************************************************************************

  startStick20EnableCryptedVolume

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20EnableFirmwareUpdate()
{
    uint8_t password[40];
    bool    ret;

    PasswordDialog dialog(this);
    dialog.init("Enter admin password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_ENABLE_FIRMWARE_UPDATE,password);
    }
}

/*******************************************************************************

  startStick20ActionChangeUserPIN

  Changes
  Date      Author        Info
  24.03.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/


void MainWindow::startStick20ActionChangeUserPIN()
{
    DialogChangePassword dialog(this);

    dialog.setModal (TRUE);

    dialog.cryptostick        = cryptostick;

    dialog.PasswordKind       = STICK20_PASSWORD_KIND_USER;

    dialog.InitData ();
    dialog.exec();
}


/*******************************************************************************

  startStick20ActionChangeAdminPIN

  Changes
  Date      Author        Info
  24.03.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/


void MainWindow::startStick20ActionChangeAdminPIN()
{
    DialogChangePassword dialog(this);

    dialog.setModal (TRUE);

    dialog.cryptostick        = cryptostick;

    dialog.PasswordKind       = STICK20_PASSWORD_KIND_ADMIN;

    dialog.InitData ();
    dialog.exec();
}




/*******************************************************************************

  startStick20ExportFirmwareToFile

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20ExportFirmwareToFile()
{
    uint8_t password[40];
    bool    ret;

    PasswordDialog dialog(this);
    dialog.init("Enter admin password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_EXPORT_FIRMWARE_TO_FILE,password);
    }
}

/*******************************************************************************

  startStick20DestroyCryptedVolume

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20DestroyCryptedVolume()
{
    uint8_t password[40];
    bool    ret;

    PasswordDialog dialog(this);
    dialog.init("Enter admin password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_GENERATE_NEW_KEYS,password);
    }

}

/*******************************************************************************

  startStick20EnableCryptedVolume

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20FillSDCardWithRandomChars()
{
    uint8_t password[40];
    bool    ret;

    PasswordDialog dialog(this);
    dialog.init("Enter admin password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_FILL_SD_CARD_WITH_RANDOM_CHARS,password);
    }
}


/*******************************************************************************

  startStick20GetStickStatus

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20GetStickStatus()
{
    uint8_t password[40];
    bool    ret;
/*
    PasswordDialog dialog(this);
    dialog.init("Enter admin password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_GET_DEVICE_STATUS,password);
    }
*/

    password[0] = 'P';
    password[1] = 0;
    stick20SendCommand (STICK20_CMD_GET_DEVICE_STATUS,password);

}

/*******************************************************************************

  startStick20SetReadonlyUncryptedVolume

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20SetReadonlyUncryptedVolume()
{
    uint8_t password[40];
    bool    ret;

    PasswordDialog dialog(this);
    dialog.init("Enter user password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_ENABLE_READONLY_UNCRYPTED_LUN,password);
    }

}

/*******************************************************************************

  startStick20SetReadWriteUncryptedVolume

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20SetReadWriteUncryptedVolume()
{
    uint8_t password[40];
    bool    ret;

    PasswordDialog dialog(this);
    dialog.init("Enter user password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_ENABLE_READWRITE_UNCRYPTED_LUN,password);
    }

}


/*******************************************************************************

  startStick20DebugAction

  Function to start a action to test functions of firmware

  Changes
  Date      Author        Info
  24.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

void MainWindow::startStick20DebugAction()
{
    uint8_t password[40];
    bool    ret;
    int64_t crc;


    if (1)
    {
          cryptostick->getPasswordRetryCount();
//        crc = cryptostick->getSlotName(0x10);

//        Sleep::msleep(100);
//        Response *testResponse=new Response();
//        testResponse->getResponse(cryptostick);

//        if (crc==testResponse->lastCommandCRC)
/*
        {

            QMessageBox message;
            QString str;
            QByteArray *data =new QByteArray((char*)testResponse->reportBuffer,REPORT_SIZE+1);

//            str.append(QString::number(testResponse->lastCommandCRC,16));
            str.append(QString(data->toHex()));

            message.setText(str);
            message.exec();

            str.clear();
        }
*/
    }

/*
    PasswordDialog dialog(this);
    dialog.init("Enter user password");
    ret = dialog.exec();

    if (Accepted == ret)
    {
        password[0] = 'P';
        dialog.getPassword ((char*)&password[1]);

        stick20SendCommand (STICK20_CMD_ENABLE_READWRITE_UNCRYPTED_LUN,password);
    }
*/
}

/*******************************************************************************

  UpdateDynamicMenuEntrys

  Changes
  Date      Author        Info
  31.03.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

int MainWindow::UpdateDynamicMenuEntrys (void)
{
    if (READ_WRITE_ACTIVE == HID_Stick20Configuration_st.ReadWriteFlagUncryptedVolume_u8)
    {
        NormalVolumeRWActive = FALSE;
    }
    else
    {
        NormalVolumeRWActive = TRUE;
    }

    if (0 != (HID_Stick20Configuration_st.VolumeActiceFlag_u8 & (1 << SD_CRYPTED_VOLUME_BIT_PLACE)))
    {
        CryptedVolumeActive = TRUE;
    }
    else
    {
        CryptedVolumeActive = FALSE;
    }

    if (0 != (HID_Stick20Configuration_st.VolumeActiceFlag_u8 & (1 << SD_HIDDEN_VOLUME_BIT_PLACE)))
    {
        HiddenVolumeActive  = TRUE;
    }
    else
    {
        HiddenVolumeActive  = FALSE;
    }
    generateMenu();

    return (TRUE);
}

/*******************************************************************************

  stick20SendCommand

  Changes
  Date      Author        Info
  04.02.14  RB            Function created

  Reviews
  Date      Reviewer        Info

*******************************************************************************/

int MainWindow::stick20SendCommand (uint8_t stick20Command, uint8_t *password)
{
    bool        ret;
    bool        waitForAnswerFromStick20;
    bool        stopWhenStatusOKFromStick20;
    int         Result;

    QByteArray  passwordString;
    QMessageBox msgBox;

    waitForAnswerFromStick20    = FALSE;
    stopWhenStatusOKFromStick20 = FALSE;

    switch (stick20Command)
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
        case STICK20_CMD_SEND_STARTUP                   :
            break;
        default :
            msgBox.setText("Stick20Dialog: Wrong combobox value! ");
            msgBox.exec();
            break;

    }

    Result = FALSE;
    if (TRUE == waitForAnswerFromStick20)
    {
        Stick20ResponseDialog ResponseDialog(this);

        if (FALSE == stopWhenStatusOKFromStick20)
        {
            ResponseDialog.NoStopWhenStatusOK ();
        }
        ResponseDialog.cryptostick=cryptostick;

        ResponseDialog.exec();
        Result = ResponseDialog.ResultValue;
    }

    if (TRUE == Result)
    {
        switch (stick20Command)
        {
            case STICK20_CMD_ENABLE_CRYPTED_PARI            :
                CryptedVolumeActive = TRUE;
                HiddenVolumeActive  = FALSE;
                generateMenu();
                break;
            case STICK20_CMD_DISABLE_CRYPTED_PARI           :
                CryptedVolumeActive = FALSE;
                HiddenVolumeActive  = FALSE;
                generateMenu();
                break;
            case STICK20_CMD_ENABLE_HIDDEN_CRYPTED_PARI     :
                HiddenVolumeActive  = TRUE;
                CryptedVolumeActive = FALSE;
                generateMenu();
                break;
            case STICK20_CMD_DISABLE_HIDDEN_CRYPTED_PARI    :
                CryptedVolumeActive = FALSE;
                HiddenVolumeActive  = FALSE;
                generateMenu();
                break;
            case STICK20_CMD_GET_DEVICE_STATUS              :
            case STICK20_CMD_SEND_STARTUP                   :
                UpdateDynamicMenuEntrys ();
                break;
            case STICK20_CMD_ENABLE_READWRITE_UNCRYPTED_LUN :
               NormalVolumeRWActive = TRUE;
               break;
            case STICK20_CMD_ENABLE_READONLY_UNCRYPTED_LUN:
               NormalVolumeRWActive = FALSE;
               break;
            default :
                break;
        }
    }


    return (true);
}




/*******************************************************************************

  getCode

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::getCode(uint8_t slotNo)
{
    uint8_t result[18];
    memset(result,0,18);
    uint32_t code;


     cryptostick->getCode(slotNo,currentTime/30,currentTime,30,result);
     //cryptostick->getCode(slotNo,1,result);
     code=result[0]+(result[1]<<8)+(result[2]<<16)+(result[3]<<24);
     code=code%100000000;

     qDebug() << "Current time:" << currentTime;
     qDebug() << "Counter:" << currentTime/30;
     qDebug() << "TOTP:" << code;

}

/*******************************************************************************

  on_writeButton_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_writeButton_clicked()
{
    QMessageBox msgBox;
    int res;

    if (cryptostick->isConnected){

        ui->hexRadioButton->toggle();

        if (ui->slotComboBox->currentIndex() < HOTP_SLOT_COUNT){//HOTP slot
            HOTPSlot *hotp=new HOTPSlot();

            generateHOTPConfig(hotp);
            //HOTPSlot *hotp=new HOTPSlot(0x10,(uint8_t *)"Herp",(uint8_t *)"123456",(uint8_t *)"0",0);
            res = cryptostick->writeToHOTPSlot(hotp);

        }
        else{//TOTP slot
            TOTPSlot *totp=new TOTPSlot();
            generateTOTPConfig(totp);
            res = cryptostick->writeToTOTPSlot(totp);
        }



        if (res==0)
            msgBox.setText("Config written successfully!");
        else
            msgBox.setText("Error writing config!");

        msgBox.exec();


        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        Sleep::msleep(500);
        QApplication::restoreOverrideCursor();

        generateAllConfigs();

    }
    else{
        msgBox.setText("Crypto stick not connected!");
        msgBox.exec();
    }
}

/*******************************************************************************

  on_slotComboBox_currentIndexChanged

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_slotComboBox_currentIndexChanged(int index)
{
    index = index;      // avoid warning

    displayCurrentSlotConfig();
}

/*******************************************************************************

  on_resetButton_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_resetButton_clicked()
{
    displayCurrentSlotConfig();
}

/*******************************************************************************

  on_hexRadioButton_toggled

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/


void MainWindow::on_hexRadioButton_toggled(bool checked)
{
    if (checked){

        QByteArray secret;
        secret = ui->secretEdit->text().toAscii();

        qDebug() << "encoded secret:" << QString(secret);

        uint8_t encoded[128];
        uint8_t decoded[20];
        memset(encoded,'A',32);
        memcpy(encoded,secret.data(),secret.length());

        base32_decode(encoded,decoded,20);

        //ui->secretEdit->setInputMask("HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH HH;");
        ui->secretEdit->setInputMask("hh hh hh hh hh hh hh hh hh hh hh hh hh hh hh hh hh hh hh hh;");
        //ui->secretEdit->setInputMask("HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH;0");
        //ui->secretEdit->setMaxLength(59);

        secret = QByteArray((char *)decoded,20).toHex();

        ui->secretEdit->setText(QString(secret));
        //qDebug() << QString(secret);

    }
}

/*******************************************************************************

  on_base32RadioButton_toggled

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_base32RadioButton_toggled(bool checked)
{
    if (checked){

        QByteArray secret;
        secret = QByteArray::fromHex(ui->secretEdit->text().toAscii());


        uint8_t encoded[128];
        uint8_t decoded[20];
        memset(decoded,0,20);
        memcpy(decoded,secret.data(),secret.length());

        base32_encode(decoded,secret.length(),encoded,128);

        //ui->secretEdit->setInputMask("NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN;");
        ui->secretEdit->setInputMask("nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn;");
        ui->secretEdit->setMaxLength(32);
        ui->secretEdit->setText(QString((char *)encoded));
        //qDebug() << QString((char *)encoded);

    }

}

/*******************************************************************************

  on_setToZeroButton_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/


void MainWindow::on_setToZeroButton_clicked()
{
    ui->counterEdit->setText("0");
}

/*******************************************************************************

  on_setToRandomButton_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_setToRandomButton_clicked()
{
    quint64 counter=qrand();
    counter<<=16;
    counter+=qrand();
    counter<<=16;
    counter+=qrand();
    counter<<=16;
    counter+=qrand();
    //qDebug() << counter;
    ui->counterEdit->setText(QString(QByteArray::number(counter,16)));
}

/*******************************************************************************

  on_checkBox_2_toggled

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_checkBox_2_toggled(bool checked)
{
    checked = checked;      // avoid warning
}

/*******************************************************************************

  on_tokenIDCheckBox_toggled

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_tokenIDCheckBox_toggled(bool checked)
{

    if (checked){
        ui->ompEdit->setEnabled(true);
        ui->ttEdit->setEnabled(true);
        ui->muiEdit->setEnabled(true);


    }
    else{
        ui->ompEdit->setEnabled(false);
        ui->ttEdit->setEnabled(false);
        ui->muiEdit->setEnabled(false);


    }
}

/*******************************************************************************

  on_writeGeneralConfigButton_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_writeGeneralConfigButton_clicked()
{
    QMessageBox msgBox;
    int res;
    uint8_t data[3];

    if (cryptostick->isConnected){

        data[0]=ui->numLockComboBox->currentIndex()-1;
        data[1]=ui->capsLockComboBox->currentIndex()-1;
        data[2]=ui->scrollLockComboBox->currentIndex()-1;

        res =cryptostick->writeGeneralConfig(data);

        if (res==0)
            msgBox.setText("Config written successfully!");
        else
            msgBox.setText("Error writing config!");

        msgBox.exec();

        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        Sleep::msleep(500);
        QApplication::restoreOverrideCursor();
        cryptostick->getStatus();
        displayCurrentGeneralConfig();

    }
    else{
        msgBox.setText("Crypto stick not connected!");
        msgBox.exec();
    }

}
/*******************************************************************************

  getHOTPDialog

  Changes
  Date      Author          Info
  25.03.14  RB              Dynamic slot counts

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::getHOTPDialog(int slot)
{
    HOTPDialog dialog(this);
    dialog.device=cryptostick;
    dialog.slotNumber=0x10 + slot;
    dialog.title=QString("HOTP slot ").append(QString::number(slot+1,10)).append(" [").append((char *)cryptostick->HOTPSlots[slot]->slotName).append("]");
    dialog.setToHOTP();
    dialog.getNextCode();
    dialog.exec();
}

void MainWindow::getHOTP1()
{
    getHOTPDialog (0);
}
void MainWindow::getHOTP2()
{
    getHOTPDialog (1);
}
void MainWindow::getHOTP3()
{
    getHOTPDialog (2);
}

/*******************************************************************************

  getHOTP1

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/
/*
void MainWindow::getHOTP1()
{
    HOTPDialog dialog(this);
    dialog.device=cryptostick;
    dialog.slotNumber=0x10;
    dialog.title=QString("HOTP slot 1 [").append((char *)cryptostick->HOTPSlots[0]->slotName).append("]");
    dialog.setToHOTP();
    dialog.getNextCode();
    dialog.exec();
}
*/
/*******************************************************************************

  getHOTP2

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/
/*
void MainWindow::getHOTP2()
{
    HOTPDialog dialog(this);
    dialog.device=cryptostick;
    dialog.slotNumber=0x11;
    dialog.title=QString("HOTP slot 2 [").append((char *)cryptostick->HOTPSlots[1]->slotName).append("]");
    dialog.setToHOTP();
    dialog.getNextCode();
    dialog.exec();
}
*/

/*******************************************************************************

  getTOTPDialog

  Changes
  Date      Author          Info
  25.03.14  RB              Dynamic slot counts

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::getTOTPDialog(int slot)
{
    HOTPDialog dialog(this);
    dialog.device=cryptostick;
    dialog.slotNumber=0x20 + slot;
    dialog.title=QString("TOTP slot ").append(QString::number(slot+1,10)).append(" [").append((char *)cryptostick->TOTPSlots[slot]->slotName).append("]");
    dialog.setToTOTP();
    dialog.getNextCode();
    dialog.exec();
}


/*******************************************************************************

  getTOTP1

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::getTOTP1()
{
    getTOTPDialog (0);
}
void MainWindow::getTOTP2()
{
    getTOTPDialog (1);
}
void MainWindow::getTOTP3()
{
    getTOTPDialog (2);
}
void MainWindow::getTOTP4()
{
    getTOTPDialog (3);
}
void MainWindow::getTOTP5()
{
    getTOTPDialog (4);
}
void MainWindow::getTOTP6()
{
    getTOTPDialog (5);
}
void MainWindow::getTOTP7()
{
    getTOTPDialog (6);
}
void MainWindow::getTOTP8()
{
    getTOTPDialog (7);
}
void MainWindow::getTOTP9()
{
    getTOTPDialog (8);
}
void MainWindow::getTOTP10()
{
    getTOTPDialog (9);
}
void MainWindow::getTOTP11()
{
    getTOTPDialog (10);
}
void MainWindow::getTOTP12()
{
    getTOTPDialog (11);
}
void MainWindow::getTOTP13()
{
    getTOTPDialog (12);
}
void MainWindow::getTOTP14()
{
    getTOTPDialog (13);
}
void MainWindow::getTOTP15()
{
    getTOTPDialog (14);
}


/*******************************************************************************

  getTOTP2

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/
/*
void MainWindow::getTOTP2()
{
    HOTPDialog dialog(this);
    dialog.device=cryptostick;
    dialog.slotNumber=0x21;
    dialog.title=QString("TOTP slot 2 [").append((char *)cryptostick->TOTPSlots[1]->slotName).append("]");
    dialog.setToTOTP();
    dialog.getNextCode();
    dialog.exec();
}
*/
/*******************************************************************************

  getTOTP3

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/
/*
void MainWindow::getTOTP3()
{
    HOTPDialog dialog(this);
    dialog.device=cryptostick;
    dialog.slotNumber=0x22;
    dialog.title=QString("TOTP slot 3 [").append((char *)cryptostick->TOTPSlots[2]->slotName).append("]");
    dialog.setToTOTP();
    dialog.getNextCode();
    dialog.exec();
}
*/
/*******************************************************************************

  getTOTP4

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/
/*
void MainWindow::getTOTP4()
{
    HOTPDialog dialog(this);
    dialog.device=cryptostick;
    dialog.slotNumber=0x23;
    dialog.title=QString("TOTP slot 4 [").append((char *)cryptostick->TOTPSlots[3]->slotName).append("]");
    dialog.setToTOTP();
    dialog.getNextCode();
    dialog.exec();
}
*/



/*******************************************************************************

  on_eraseButton_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_eraseButton_clicked()
{
    QMessageBox msgBox;
     msgBox.setText("Are you sure you want to erase the slot?");
     msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
     msgBox.setDefaultButton(QMessageBox::No);
     int ret = msgBox.exec();

     uint8_t slotNo=ui->slotComboBox->currentIndex();
     if (slotNo>=0&&slotNo<=1)
         slotNo=slotNo+0x10;
     else if (slotNo>=2&&slotNo<=5)
         slotNo=slotNo+0x1E;

     switch (ret) {
       case QMessageBox::Yes:
            cryptostick->eraseSlot(slotNo);
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
            Sleep::msleep(1000);
            QApplication::restoreOverrideCursor();
            generateAllConfigs();
             msgBox.setText("Slot erased!");
              msgBox.setStandardButtons(QMessageBox::Ok);
             msgBox.exec();
           break;
       case QMessageBox::No:

           break;
       default:
           // should never be reached
           break;
     }
}

/*******************************************************************************

  on_resetGeneralConfigButton_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_resetGeneralConfigButton_clicked()
{
    displayCurrentGeneralConfig();
}

/*******************************************************************************

  on_randomSecretButton_clicked

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_randomSecretButton_clicked()
{

    int i;
    uint8_t secret[20];

    for (i=0;i<20;i++)
        secret[i]=qrand()&0xFF;

    QByteArray secretArray((char *) secret,20);
    ui->hexRadioButton->setChecked(true);
    ui->secretEdit->setText(secretArray.toHex());

}

/*******************************************************************************

  on_checkBox_toggled

  Reviews
  Date      Reviewer        Info
  13.08.13  RB              First review

*******************************************************************************/

void MainWindow::on_checkBox_toggled(bool checked)
{
    if (checked)
        ui->secretEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    else
        ui->secretEdit->setEchoMode(QLineEdit::Normal);


}


