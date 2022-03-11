/*
 * Copyright (C) 2020-2022 Roy Qu (royqh1979@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "compilersetoptionwidget.h"
#include "ui_compilersetoptionwidget.h"
#include "../settings.h"
#include "../mainwindow.h"
#include "compilersetdirectorieswidget.h"
#include <QMessageBox>
#include "../utils.h"
#include "../iconsmanager.h"
#include "../platform.h"
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>

CompilerSetOptionWidget::CompilerSetOptionWidget(const QString& name, const QString& group, QWidget* parent) :
    SettingsWidget(name,group,parent),
    ui(new Ui::CompilerSetOptionWidget)
{
    ui->setupUi(this);

    mBinDirWidget = new CompilerSetDirectoriesWidget();
    ui->dirTabs->addTab(mBinDirWidget,QObject::tr("Binaries"));
    mLibDirWidget = new CompilerSetDirectoriesWidget();
    ui->dirTabs->addTab(mLibDirWidget,QObject::tr("Libraries"));
    mCIncludeDirWidget = new CompilerSetDirectoriesWidget();
    ui->dirTabs->addTab(mCIncludeDirWidget,QObject::tr("C Includes"));
    mCppIncludeDirWidget = new CompilerSetDirectoriesWidget();
    ui->dirTabs->addTab(mCppIncludeDirWidget,QObject::tr("C++ Includes"));

    connect(ui->chkUseCustomCompilerParams, &QCheckBox::stateChanged,
             ui->txtCustomCompileParams, &QPlainTextEdit::setEnabled);
    connect(ui->chkUseCustomLinkParams, &QCheckBox::stateChanged,
             ui->txtCustomLinkParams, &QPlainTextEdit::setEnabled);

    connect(pIconsManager, &IconsManager::actionIconsUpdated,
            this, &CompilerSetOptionWidget::updateIcons);
    updateIcons();
}

CompilerSetOptionWidget::~CompilerSetOptionWidget()
{
    delete ui;
}

void CompilerSetOptionWidget::init()
{
    ui->cbEncodingDetails->setVisible(false);
    ui->cbEncoding->clear();
    ui->cbEncoding->addItem(tr("ANSI"),ENCODING_SYSTEM_DEFAULT);
    ui->cbEncoding->addItem(tr("UTF-8"),ENCODING_UTF8);
    foreach (const QString& langName, pCharsetInfoManager->languageNames()) {
        ui->cbEncoding->addItem(langName,langName);
    }
    SettingsWidget::init();
}

void resetOptionTabs(Settings::PCompilerSet pSet,QTabWidget* pTab)
{
    while (pTab->count()>0) {
        QWidget* p=pTab->widget(0);
        if (p!=nullptr) {
            pTab->removeTab(0);
            p->setParent(nullptr);
            delete p;
        }
    }

    for (PCompilerOption pOption: pSet->options()) {
        QWidget* pWidget = nullptr;
        for (int i=0;i<pTab->count();i++) {
            if (pOption->section == pTab->tabText(i)) {
                pWidget = pTab->widget(i);
                break;
            }
        }
        if (pWidget == nullptr) {
            pWidget = new QWidget();
            pTab->addTab(pWidget,pOption->section);
            pWidget->setLayout(new QGridLayout());
        }
        QGridLayout *pLayout = static_cast<QGridLayout*>(pWidget->layout());
        int row = pLayout->rowCount();
        pLayout->addWidget(new QLabel(pOption->name),row,0);
        QComboBox* pCombo = new QComboBox();
        if (pOption->choices.count()>0) {
            for (int i=0;i<pOption->choices.count();i++) {
                QString choice = pOption->choices[i];
                QStringList valueName=choice.split("=");
                if (valueName.length()<2) {
                    pCombo->addItem("");
                } else {
                    pCombo->addItem(valueName[0]);
                }
            }
        } else {
            pCombo->addItem(QObject::tr("No"));
            pCombo->addItem(QObject::tr("Yes"));
        }
        pCombo->setCurrentIndex(pOption->value);
        pLayout->addWidget(pCombo,row,1);
    }
    for (int i=0;i<pTab->count();i++) {
        QWidget* pWidget = pTab->widget(i);
        QGridLayout *pLayout = static_cast<QGridLayout*>(pWidget->layout());
        int row = pLayout->rowCount();
        QSpacerItem* horizontalSpacer = new QSpacerItem(10, 100, QSizePolicy::Minimum, QSizePolicy::Expanding);
        pLayout->addItem(horizontalSpacer,row,0);
    }
}

static void loadCompilerSetSettings(Settings::PCompilerSet pSet, Ui::CompilerSetOptionWidget* ui) {
    ui->chkUseCustomCompilerParams->setChecked(pSet->useCustomCompileParams());
    ui->txtCustomCompileParams->setPlainText(pSet->customCompileParams());
    ui->txtCustomCompileParams->setEnabled(pSet->useCustomCompileParams());
    ui->chkUseCustomLinkParams->setChecked(pSet->useCustomLinkParams());
    ui->txtCustomLinkParams->setPlainText(pSet->customLinkParams());
    ui->txtCustomLinkParams->setEnabled(pSet->useCustomLinkParams());
    ui->chkAutoAddCharset->setChecked(pSet->autoAddCharsetParams());
    ui->chkStaticLink->setChecked(pSet->staticLink());
    //rest tabs in the options widget
    resetOptionTabs(pSet,ui->optionTabs);

    ui->txtCCompiler->setText(pSet->CCompiler());
    ui->txtCppCompiler->setText(pSet->cppCompiler());
    ui->txtMake->setText(pSet->make());
    ui->txtDebugger->setText(pSet->debugger());
    ui->txtGDBServer->setText(pSet->debugServer());
    ui->txtResourceCompiler->setText(pSet->resourceCompiler());
    ui->txtProfiler->setText(pSet->profiler());

    if (pSet->execCharset() == ENCODING_AUTO_DETECT
            || pSet->execCharset() == ENCODING_SYSTEM_DEFAULT
            || pSet->execCharset() == ENCODING_UTF8) {
        ui->cbEncoding->setCurrentText(pSet->execCharset());
        ui->cbEncodingDetails->clear();
        ui->cbEncodingDetails->setVisible(false);
    } else {
        QString encoding = pSet->execCharset();
        QString language = pCharsetInfoManager->findLanguageByCharsetName(encoding);
        ui->cbEncoding->setCurrentText(language);
        ui->cbEncodingDetails->setVisible(true);
        ui->cbEncodingDetails->clear();
        QList<PCharsetInfo> infos = pCharsetInfoManager->findCharsetsByLanguageName(language);
        foreach (const PCharsetInfo& info, infos) {
            ui->cbEncodingDetails->addItem(info->name);
        }
        ui->cbEncodingDetails->setCurrentText(encoding);
    }
}

void CompilerSetOptionWidget::doLoad()
{
    disconnectInputs();
    ui->cbCompilerSet->clear();
    if (pSettings->compilerSets().size()<=0) {
        ui->btnRenameCompilerSet->setEnabled(false);
        ui->btnRemoveCompilerSet->setEnabled(false);
        return;
    } else {
        ui->btnRenameCompilerSet->setEnabled(true);
        ui->btnRemoveCompilerSet->setEnabled(true);
    }
    int index=pSettings->compilerSets().defaultIndex();
    for (int i=0;i<pSettings->compilerSets().size();i++) {
        ui->cbCompilerSet->addItem(pSettings->compilerSets().getSet(i)->name());
    }
    if (index < 0 || index>=ui->cbCompilerSet->count()) {
        index = 0;
    }
    ui->cbCompilerSet->setCurrentIndex(index);
    reloadCurrentCompilerSet();
    connectInputs();
}

void CompilerSetOptionWidget::doSave()
{
    if (pSettings->compilerSets().size()>0) {
        saveCurrentCompilerSet();
    }
    pSettings->compilerSets().saveSets();
    pMainWindow->updateCompilerSet();
}

void CompilerSetOptionWidget::on_cbCompilerSet_currentIndexChanged(int index)
{
    if (index<0)
        return;
    setSettingsChanged();
    pSettings->compilerSets().setDefaultIndex(index);
    disconnectInputs();
    reloadCurrentCompilerSet();
    connectInputs();
}

void CompilerSetOptionWidget::reloadCurrentCompilerSet()
{
    Settings::PCompilerSet pSet = pSettings->compilerSets().defaultSet();
    loadCompilerSetSettings(pSet, ui);

    mBinDirWidget->setDirList(pSet->binDirs());
    mLibDirWidget->setDirList(pSet->libDirs());
    mCIncludeDirWidget->setDirList(pSet->CIncludeDirs());
    mCppIncludeDirWidget->setDirList(pSet->CppIncludeDirs());
}

void CompilerSetOptionWidget::saveCurrentCompilerSet()
{
    Settings::PCompilerSet pSet = pSettings->compilerSets().defaultSet();

    pSet->setUseCustomCompileParams(ui->chkUseCustomCompilerParams->isChecked());
    pSet->setCustomCompileParams(ui->txtCustomCompileParams->toPlainText().trimmed());
    pSet->setUseCustomLinkParams(ui->chkUseCustomLinkParams->isChecked());
    pSet->setCustomLinkParams(ui->txtCustomLinkParams->toPlainText().trimmed());
    pSet->setAutoAddCharsetParams(ui->chkAutoAddCharset->isChecked());
    pSet->setStaticLink(ui->chkStaticLink->isChecked());

    pSet->setCCompiler(ui->txtCCompiler->text().trimmed());
    pSet->setCppCompiler(ui->txtCppCompiler->text().trimmed());
    pSet->setMake(ui->txtMake->text().trimmed());
    pSet->setDebugger(ui->txtDebugger->text().trimmed());
    pSet->setDebugServer(ui->txtGDBServer->text().trimmed());
    pSet->setResourceCompiler(ui->txtResourceCompiler->text().trimmed());
    pSet->setProfiler(ui->txtProfiler->text().trimmed());

    pSet->binDirs()=mBinDirWidget->dirList();

    pSet->libDirs()=mLibDirWidget->dirList();
    pSet->CIncludeDirs()=mCIncludeDirWidget->dirList();
    pSet->CppIncludeDirs()=mCppIncludeDirWidget->dirList();

    if (ui->cbEncodingDetails->isVisible()) {
        pSet->setExecCharset(ui->cbEncodingDetails->currentText());
    } else {
        pSet->setExecCharset(ui->cbEncoding->currentData().toString());
    }

    //read values in the options widget
    QTabWidget* pTab = ui->optionTabs;
    for (int i=0;i<pTab->count();i++) {
        QString section = pTab->tabText(i);
        QWidget* pWidget = pTab->widget(i);
        QGridLayout* pLayout = static_cast<QGridLayout*>(pWidget->layout());
        if (pLayout != nullptr) {
            for (int j=1;j<pLayout->rowCount()-1;j++) {
                QString name = static_cast<QLabel *>(pLayout->itemAtPosition(j,0)->widget())->text();
                QComboBox* pCombo = static_cast<QComboBox *>(pLayout->itemAtPosition(j,1)->widget());
                for (PCompilerOption pOption: pSet->options()) {
                    if (pOption->section == section && pOption->name == name) {
                        pOption->value = pCombo->currentIndex();
                    }
                }
            }
        }
    }
}

void CompilerSetOptionWidget::on_btnFindCompilers_pressed()
{
#ifdef Q_OS_WIN
        QString msg = tr("Red Panda C++ will clear current compiler list and search"
                      " for compilers in the following locations:<br /> '%1'<br /> '%2'<br />Are you really want to continue?")
                                 .arg(includeTrailingPathDelimiter(pSettings->dirs().appDir()) + "MinGW32")
                                 .arg(includeTrailingPathDelimiter(pSettings->dirs().appDir()) + "MinGW64");
#else
        QString msg = tr("Red Panda C++ will clear current compiler list and search"
                      " for compilers in the the PATH. <br />Are you really want to continue?");
#endif
    if (QMessageBox::warning(this,tr("Confirm"),msg,
                                 QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok )
        return;
    pSettings->compilerSets().clearSets();
    pSettings->compilerSets().findSets();
    doLoad();
    setSettingsChanged();
    if (pSettings->compilerSets().size()==0) {
        QMessageBox::warning(this,tr("Failed"),tr("Can't find any compiler."));
    }
}

void CompilerSetOptionWidget::on_btnAddBlankCompilerSet_pressed()
{
    QString name = QInputDialog::getText(this,tr("Compiler Set Name"),tr("Name"));
    pSettings->compilerSets().addSet();
    pSettings->compilerSets().setDefaultIndex(pSettings->compilerSets().size()-1);
    pSettings->compilerSets().defaultSet()->setName(name);
    doLoad();
}

void CompilerSetOptionWidget::on_btnAddCompilerSetByFolder_pressed()
{
    QString folder = QFileDialog::getExistingDirectory(this, tr("Compiler Set Folder"));
    int oldSize = pSettings->compilerSets().size();

    if (!pSettings->compilerSets().addSets(folder)) {
        pSettings->compilerSets().addSets(folder+QDir::separator()+"bin");
    }
    doLoad();
    int newSize = pSettings->compilerSets().size();
    if (oldSize == newSize) {
        QMessageBox::warning(this,tr("Failed"),tr("Can't find any compiler."));
    }
}

void CompilerSetOptionWidget::on_btnRenameCompilerSet_pressed()
{
    QString name = QInputDialog::getText(this,tr("Compiler Set Name"),tr("New name"),QLineEdit::Normal,
                                         pSettings->compilerSets().defaultSet()->name());
    if (!name.isEmpty())
        pSettings->compilerSets().defaultSet()->setName(name);
    doLoad();
}

void CompilerSetOptionWidget::on_btnRemoveCompilerSet_pressed()
{
    pSettings->compilerSets().deleteSet(ui->cbCompilerSet->currentIndex());
    doLoad();
}

void CompilerSetOptionWidget::updateIcons()
{
    pIconsManager->setIcon(ui->btnFindCompilers, IconsManager::ACTION_EDIT_SEARCH);
    pIconsManager->setIcon(ui->btnAddCompilerSetByFolder, IconsManager::ACTION_FILE_OPEN_FOLDER);
    pIconsManager->setIcon(ui->btnAddBlankCompilerSet, IconsManager::ACTION_MISC_ADD);
    pIconsManager->setIcon(ui->btnRemoveCompilerSet, IconsManager::ACTION_MISC_REMOVE);
    pIconsManager->setIcon(ui->btnRenameCompilerSet, IconsManager::ACTION_MISC_RENAME);

    pIconsManager->setIcon(ui->btnChooseCCompiler, IconsManager::ACTION_FILE_OPEN_FOLDER);
    pIconsManager->setIcon(ui->btnChooseCppCompiler, IconsManager::ACTION_FILE_OPEN_FOLDER);
    pIconsManager->setIcon(ui->btnChooseGDB, IconsManager::ACTION_FILE_OPEN_FOLDER);
    pIconsManager->setIcon(ui->btnChooseGDBServer, IconsManager::ACTION_FILE_OPEN_FOLDER);
    pIconsManager->setIcon(ui->btnChooseMake, IconsManager::ACTION_FILE_OPEN_FOLDER);
    pIconsManager->setIcon(ui->btnChooseProfiler, IconsManager::ACTION_FILE_OPEN_FOLDER);
    pIconsManager->setIcon(ui->btnChooseResourceCompiler, IconsManager::ACTION_FILE_OPEN_FOLDER);
}

void CompilerSetOptionWidget::on_cbEncoding_currentTextChanged(const QString &arg1)
{
    QString userData = ui->cbEncoding->currentData().toString();
    if (userData == ENCODING_AUTO_DETECT
            || userData == ENCODING_SYSTEM_DEFAULT
            || userData == ENCODING_UTF8) {
        ui->cbEncodingDetails->setVisible(false);
        ui->cbEncodingDetails->clear();
    } else {
        ui->cbEncodingDetails->setVisible(true);
        ui->cbEncodingDetails->clear();
        QList<PCharsetInfo> infos = pCharsetInfoManager->findCharsetsByLanguageName(userData);
        foreach (const PCharsetInfo& info, infos) {
            ui->cbEncodingDetails->addItem(info->name);
        }
    }
}


void CompilerSetOptionWidget::on_cbEncodingDetails_currentTextChanged(const QString &arg1)
{

}

