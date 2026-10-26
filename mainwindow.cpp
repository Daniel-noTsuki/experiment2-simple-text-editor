#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      m_positionLabel(new QLabel(this)), m_countLabel(new QLabel(this))
{
    ui->setupUi(this);
    statusBar()->addPermanentWidget(m_countLabel);
    statusBar()->addPermanentWidget(m_positionLabel);

    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::newFile);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionUndo, &QAction::triggered, ui->editor, &QPlainTextEdit::undo);
    connect(ui->actionRedo, &QAction::triggered, ui->editor, &QPlainTextEdit::redo);
    connect(ui->actionCut, &QAction::triggered, ui->editor, &QPlainTextEdit::cut);
    connect(ui->actionCopy, &QAction::triggered, ui->editor, &QPlainTextEdit::copy);
    connect(ui->actionPaste, &QAction::triggered, ui->editor, &QPlainTextEdit::paste);
    connect(ui->actionSelectAll, &QAction::triggered, ui->editor, &QPlainTextEdit::selectAll);
    connect(ui->actionFind, &QAction::triggered, this, &MainWindow::showFindDialog);
    connect(ui->actionReplace, &QAction::triggered, this, &MainWindow::showReplaceDialog);
    connect(ui->actionFont, &QAction::triggered, this, &MainWindow::chooseFont);
    connect(ui->actionTextColor, &QAction::triggered, this, &MainWindow::chooseTextColor);
    connect(ui->actionBackground, &QAction::triggered, this, &MainWindow::chooseEditorColor);
    connect(ui->actionWrap, &QAction::toggled, this, [this](bool on) {
        ui->editor->setLineWrapMode(on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    });
    connect(ui->actionLineNumbers, &QAction::toggled, ui->editor, &CodeEditor::setLineNumbersVisible);
    connect(ui->actionToolbar, &QAction::toggled, ui->mainToolBar, &QWidget::setVisible);
    connect(ui->actionStatusbar, &QAction::toggled, ui->statusbar, &QWidget::setVisible);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);

    connect(ui->editor->document(), &QTextDocument::modificationChanged,
            this, &MainWindow::updateWindowTitle);
    connect(ui->editor, &QPlainTextEdit::selectionChanged, this, &MainWindow::updateActions);
    connect(ui->editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatus);
    connect(ui->editor, &QPlainTextEdit::textChanged, this, &MainWindow::updateStatus);
    connect(ui->editor->document(), &QTextDocument::undoAvailable,
            ui->actionUndo, &QAction::setEnabled);
    connect(ui->editor->document(), &QTextDocument::redoAvailable,
            ui->actionRedo, &QAction::setEnabled);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::updateActions);

    auto *timer = new QTimer(this);
    timer->setInterval(60000);
    connect(timer, &QTimer::timeout, this, &MainWindow::autoSave);
    timer->start();

    setCurrentFile(QString());
    updateActions();
    updateStatus();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) event->accept(); else event->ignore();
}

bool MainWindow::maybeSave()
{
    if (!ui->editor->document()->isModified()) return true;
    const auto answer = QMessageBox::warning(
        this, tr("未保存的修改"), tr("当前文档已修改，是否保存？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Save) return saveFile();
    return answer == QMessageBox::Discard;
}

void MainWindow::newFile()
{
    if (!maybeSave()) return;
    ui->editor->clear();
    ui->editor->document()->setModified(false);
    setCurrentFile(QString());
}

void MainWindow::openFile()
{
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("打开文本文件"), QString(),
                                                       tr("文本文件 (*.txt *.cpp *.h);;所有文件 (*)"));
    if (path.isEmpty()) return;
    QString error;
    if (!loadFromPath(path, &error))
        QMessageBox::critical(this, tr("打开失败"), error);
}

bool MainWindow::saveFile()
{
    if (m_currentFile.isEmpty()) return saveFileAs();
    QString error;
    if (!saveToPath(m_currentFile, &error)) {
        QMessageBox::critical(this, tr("保存失败"), error);
        return false;
    }
    statusBar()->showMessage(tr("已保存 %1").arg(QFileInfo(m_currentFile).fileName()), 3000);
    return true;
}

bool MainWindow::saveFileAs()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("另存为"),
        m_currentFile.isEmpty() ? QStringLiteral("未命名.txt") : m_currentFile,
        tr("文本文件 (*.txt);;所有文件 (*)"));
    if (path.isEmpty()) return false;
    QString error;
    if (!saveToPath(path, &error)) {
        QMessageBox::critical(this, tr("保存失败"), error);
        return false;
    }
    return true;
}

bool MainWindow::loadFromPath(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = tr("无法打开 %1：%2").arg(path, file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFile::NoError) {
        if (error) *error = tr("读取 %1 失败：%2").arg(path, file.errorString());
        return false;
    }
    ui->editor->setPlainText(QString::fromUtf8(data));
    ui->editor->document()->setModified(false);
    setCurrentFile(QFileInfo(path).canonicalFilePath());
    return true;
}

bool MainWindow::saveToPath(const QString &path, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = tr("无法写入 %1：%2").arg(path, file.errorString());
        return false;
    }
    const QByteArray data = ui->editor->toPlainText().toUtf8();
    if (file.write(data) != data.size() || !file.commit()) {
        if (error) *error = tr("保存 %1 失败：%2").arg(path, file.errorString());
        return false;
    }
    setCurrentFile(QFileInfo(path).absoluteFilePath());
    ui->editor->document()->setModified(false);
    return true;
}

void MainWindow::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    const QString name = m_currentFile.isEmpty() ? tr("未命名.txt") : QFileInfo(m_currentFile).fileName();
    setWindowTitle(QStringLiteral("%1%2 - 简单文本编辑器")
        .arg(ui->editor->document()->isModified() ? QStringLiteral("*") : QString(), name));
}

void MainWindow::updateActions()
{
    const bool selected = ui->editor->textCursor().hasSelection();
    ui->actionCut->setEnabled(selected);
    ui->actionCopy->setEnabled(selected);
    ui->actionPaste->setEnabled(!QApplication::clipboard()->text().isEmpty());
}

void MainWindow::updateStatus()
{
    const QTextCursor cursor = ui->editor->textCursor();
    m_positionLabel->setText(tr("行 %1，列 %2").arg(cursor.blockNumber() + 1)
                             .arg(cursor.positionInBlock() + 1));
    m_countLabel->setText(tr("字符 %1  行数 %2").arg(ui->editor->toPlainText().size())
                          .arg(ui->editor->blockCount()));
}

void MainWindow::chooseFont()
{
    bool ok = false;
    const QFont font = QFontDialog::getFont(&ok, ui->editor->font(), this, tr("选择字体"));
    if (ok) ui->editor->setFont(font);
}

void MainWindow::chooseTextColor()
{
    const QColor color = QColorDialog::getColor(ui->editor->palette().color(QPalette::Text), this,
                                                 tr("选择字体颜色"));
    if (!color.isValid()) return;
    QPalette palette = ui->editor->palette();
    palette.setColor(QPalette::Text, color);
    ui->editor->setPalette(palette);
}

void MainWindow::chooseEditorColor()
{
    const QColor color = QColorDialog::getColor(ui->editor->palette().color(QPalette::Base), this,
                                                 tr("选择编辑器背景色"));
    if (!color.isValid()) return;
    QPalette palette = ui->editor->palette();
    palette.setColor(QPalette::Base, color);
    ui->editor->setPalette(palette);
}

bool MainWindow::findText(const QString &needle, bool backward, bool caseSensitive)
{
    if (needle.isEmpty()) return false;
    QTextDocument::FindFlags flags;
    if (backward) flags |= QTextDocument::FindBackward;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    if (ui->editor->find(needle, flags)) return true;

    QTextCursor cursor = ui->editor->textCursor();
    cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
    ui->editor->setTextCursor(cursor);
    return ui->editor->find(needle, flags);
}

int MainWindow::replaceAll(const QString &needle, const QString &replacement, bool caseSensitive)
{
    if (needle.isEmpty()) return 0;
    QTextDocument::FindFlags flags;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    QTextCursor cursor(ui->editor->document());
    cursor.beginEditBlock();
    int count = 0;
    while (true) {
        cursor = ui->editor->document()->find(needle, cursor, flags);
        if (cursor.isNull()) break;
        cursor.insertText(replacement);
        ++count;
    }
    cursor.endEditBlock();
    return count;
}

QDialog *MainWindow::createFindDialog(bool replaceMode)
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(replaceMode ? tr("替换") : tr("查找"));
    dialog->setObjectName(replaceMode ? QStringLiteral("replaceDialog") : QStringLiteral("findDialog"));
    auto *layout = new QGridLayout(dialog);
    auto *findEdit = new QLineEdit(dialog);
    findEdit->setObjectName(QStringLiteral("findEdit"));
    auto *replaceEdit = new QLineEdit(dialog);
    auto *caseBox = new QCheckBox(tr("区分大小写"), dialog);
    auto *backward = new QCheckBox(tr("向上查找"), dialog);
    auto *findButton = new QPushButton(tr("查找下一个"), dialog);
    auto *closeButton = new QPushButton(tr("关闭"), dialog);
    layout->addWidget(new QLabel(tr("查找内容："), dialog), 0, 0);
    layout->addWidget(findEdit, 0, 1, 1, 2);
    int row = 1;
    if (replaceMode) {
        layout->addWidget(new QLabel(tr("替换为："), dialog), row, 0);
        layout->addWidget(replaceEdit, row++, 1, 1, 2);
    }
    layout->addWidget(caseBox, row, 0);
    layout->addWidget(backward, row, 1);
    layout->addWidget(findButton, row++, 2);
    if (replaceMode) {
        auto *replaceButton = new QPushButton(tr("替换"), dialog);
        auto *allButton = new QPushButton(tr("全部替换"), dialog);
        layout->addWidget(replaceButton, row, 0);
        layout->addWidget(allButton, row++, 1);
        connect(replaceButton, &QPushButton::clicked, dialog, [=]() {
            QTextCursor cursor = ui->editor->textCursor();
            if (cursor.hasSelection() && cursor.selectedText().compare(findEdit->text(),
                    caseBox->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0)
                cursor.insertText(replaceEdit->text());
            if (!findText(findEdit->text(), backward->isChecked(), caseBox->isChecked()))
                QMessageBox::information(dialog, tr("查找"), tr("未找到指定内容。"));
        });
        connect(allButton, &QPushButton::clicked, dialog, [=]() {
            const int count = replaceAll(findEdit->text(), replaceEdit->text(), caseBox->isChecked());
            QMessageBox::information(dialog, tr("替换完成"), tr("共替换 %1 处。 ").arg(count));
        });
    }
    layout->addWidget(closeButton, row, 2);
    connect(findButton, &QPushButton::clicked, dialog, [=]() {
        if (!findText(findEdit->text(), backward->isChecked(), caseBox->isChecked()))
            QMessageBox::information(dialog, tr("查找"), tr("未找到指定内容。"));
    });
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);
    findEdit->setFocus();
    dialog->setMinimumWidth(430);
    return dialog;
}

QDialog *MainWindow::createAboutDialog()
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("关于简单文本编辑器"));
    auto *layout = new QVBoxLayout(dialog);
    auto *title = new QLabel(tr("简单文本编辑器"), dialog);
    QFont font = title->font(); font.setPointSize(20); font.setBold(true); title->setFont(font);
    title->setAlignment(Qt::AlignCenter);
    auto *body = new QLabel(tr("基于 Qt Widgets 的多窗口文本编辑实验\n"
                               "支持文件、编辑、查找替换、格式、行号和自动保存。\n\n版本 2.0"), dialog);
    body->setAlignment(Qt::AlignCenter);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dialog);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    layout->addWidget(title); layout->addWidget(body); layout->addWidget(buttons);
    dialog->setMinimumSize(460, 230);
    return dialog;
}

void MainWindow::showFindDialog() { auto *d = createFindDialog(false); d->setAttribute(Qt::WA_DeleteOnClose); d->show(); }
void MainWindow::showReplaceDialog() { auto *d = createFindDialog(true); d->setAttribute(Qt::WA_DeleteOnClose); d->show(); }
void MainWindow::showAboutDialog() { auto *d = createAboutDialog(); d->setAttribute(Qt::WA_DeleteOnClose); d->show(); }

void MainWindow::autoSave()
{
    if (m_currentFile.isEmpty() || !ui->editor->document()->isModified()) return;
    const QString backup = m_currentFile + QStringLiteral(".autosave");
    QSaveFile file(backup);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(ui->editor->toPlainText().toUtf8());
        if (file.commit()) statusBar()->showMessage(tr("已自动保存备份"), 2000);
    }
}

bool MainWindow::runSelfTests(QTextStream &out)
{
    int passed = 0, failed = 0;
    auto check = [&](bool condition, const QString &name) {
        out << (condition ? "PASS " : "FAIL ") << name << Qt::endl;
        condition ? ++passed : ++failed;
    };

    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("临时测试目录创建"));
    const QString path = dir.filePath(QStringLiteral("中文 文件.txt"));
    ui->editor->setPlainText(QStringLiteral("第一行\nQt 文本编辑器\n最后一行"));
    QString error;
    check(saveToPath(path, &error), QStringLiteral("UTF-8 保存与含空格路径"));
    ui->editor->clear();
    check(loadFromPath(path, &error), QStringLiteral("重新打开已保存文件"));
    check(ui->editor->toPlainText().contains(QStringLiteral("Qt 文本编辑器")), QStringLiteral("保存内容一致"));

    ui->editor->moveCursor(QTextCursor::Start);
    check(findText(QStringLiteral("文本"), false, false), QStringLiteral("连续查找命中"));
    check(!findText(QStringLiteral("不存在"), false, false), QStringLiteral("未找到分支"));
    ui->editor->setPlainText(QStringLiteral("cat CAT cat"));
    check(replaceAll(QStringLiteral("cat"), QStringLiteral("dog"), false) == 3,
          QStringLiteral("不区分大小写全部替换"));
    check(ui->editor->toPlainText() == QStringLiteral("dog dog dog"), QStringLiteral("替换结果正确"));

    ui->editor->setPlainText(QStringLiteral("abcdef"));
    QTextCursor cursor = ui->editor->textCursor();
    cursor.setPosition(1); cursor.setPosition(4, QTextCursor::KeepAnchor); ui->editor->setTextCursor(cursor);
    updateActions();
    check(ui->actionCut->isEnabled() && ui->actionCopy->isEnabled(), QStringLiteral("选区驱动剪切复制使能"));
    cursor.clearSelection(); ui->editor->setTextCursor(cursor); updateActions();
    check(!ui->actionCut->isEnabled() && !ui->actionCopy->isEnabled(), QStringLiteral("无选区禁用剪切复制"));

    out << "SUMMARY passed=" << passed << " failed=" << failed << Qt::endl;
    return failed == 0;
}

void MainWindow::captureEvidence(const QString &outputDir)
{
    QDir().mkpath(outputDir);
    resize(980, 640);
    auto saveMain = [&](const QString &name) {
        repaint();
        QApplication::processEvents();
        grab().save(QDir(outputDir).filePath(name));
    };

    // 1 主界面、菜单工具栏与整体布局。
    ui->editor->setPlainText(QStringLiteral(
        "简单文本编辑器功能验证\n\n"
        "1. 文件新建、打开、保存和另存为\n"
        "2. 撤销、重做、剪切、复制、粘贴和全选\n"
        "3. 查找、替换与全部替换\n"
        "4. 字体、颜色、背景色、自动换行和行号\n"
        "5. 未保存提示、异常文件处理和定时备份"));
    ui->editor->document()->setModified(true);
    ui->editor->setFocus();
    saveMain(QStringLiteral("01-main-window.png"));

    auto captureDialog = [&](QDialog *dialog, const QString &name) {
        dialog->show(); dialog->raise(); dialog->activateWindow();
        QApplication::processEvents();
        dialog->grab().save(QDir(outputDir).filePath(name));
        dialog->close(); dialog->deleteLater(); QApplication::processEvents();
    };
    captureDialog(createFindDialog(false), QStringLiteral("02-find-dialog.png"));
    captureDialog(createFindDialog(true), QStringLiteral("03-replace-dialog.png"));
    captureDialog(createAboutDialog(), QStringLiteral("04-about-dialog.png"));

    // 5 实际保存一个 UTF-8 文件，并显示保存结果与修改标记。
    ui->editor->setPlainText(QStringLiteral("macOS 文件保存验证\n第一行\n第二行"));
    QString error;
    const QString savedPath = QDir(outputDir).filePath(QStringLiteral("实验2保存验证.txt"));
    saveToPath(savedPath, &error);
    statusBar()->showMessage(tr("已保存：%1").arg(savedPath));
    saveMain(QStringLiteral("05-file-save-result.png"));

    // 6 撤销、重做、剪切、复制、粘贴、全选与选区状态。
    ui->editor->setPlainText(QStringLiteral("撤销  重做  剪切  复制  粘贴  全选\n"
                                             "当前选区会同步启用剪切和复制操作。"));
    QTextCursor editCursor = ui->editor->textCursor();
    editCursor.setPosition(8);
    editCursor.setPosition(14, QTextCursor::KeepAnchor);
    ui->editor->setTextCursor(editCursor);
    QApplication::clipboard()->setText(QStringLiteral("来自剪贴板的文本"));
    updateActions();
    statusBar()->showMessage(tr("已选中 %1 个字符；剪切、复制、粘贴均可用")
                             .arg(editCursor.selectedText().size()));
    saveMain(QStringLiteral("06-edit-actions.png"));

    // 7 字体、文字颜色、背景色与自动换行结果。
    ui->editor->setPlainText(QStringLiteral(
        "字体与颜色设置结果\n"
        "当前编辑区使用 15 pt Menlo、蓝色文字和浅黄色背景。\n"
        "关闭自动换行后，较长的一行会保持在同一行中，用户可以通过水平滚动查看完整内容。"));
    QFont evidenceFont(QStringLiteral("Menlo"), 15);
    ui->editor->setFont(evidenceFont);
    QPalette formatPalette = ui->editor->palette();
    formatPalette.setColor(QPalette::Text, QColor(27, 86, 170));
    formatPalette.setColor(QPalette::Base, QColor(255, 252, 224));
    ui->editor->setPalette(formatPalette);
    ui->actionWrap->setChecked(false);
    statusBar()->showMessage(tr("字体：Menlo 15 pt；文字：蓝色；背景：浅黄色；自动换行：关闭"));
    saveMain(QStringLiteral("07-formatting-result.png"));

    // 8 工具栏、状态栏和状态信息。
    ui->editor->setPalette(QPalette());
    ui->editor->setFont(QFont(QStringLiteral("Menlo"), 12));
    ui->actionWrap->setChecked(true);
    ui->mainToolBar->show();
    ui->statusbar->show();
    ui->editor->setPlainText(QStringLiteral("状态栏验证\n第二行\n光标位于第三行第六列"));
    QTextCursor statusCursor = ui->editor->textCursor();
    statusCursor.movePosition(QTextCursor::End);
    ui->editor->setTextCursor(statusCursor);
    updateStatus();
    saveMain(QStringLiteral("08-toolbar-statusbar.png"));

    // 9 cut/copy/paste 的使能状态（有选区 + 剪贴板有文本）。
    ui->editor->setPlainText(QStringLiteral("选择这段文字后，剪切和复制按钮应立即启用。"));
    QTextCursor actionCursor = ui->editor->textCursor();
    actionCursor.setPosition(0);
    actionCursor.setPosition(6, QTextCursor::KeepAnchor);
    ui->editor->setTextCursor(actionCursor);
    QApplication::clipboard()->setText(QStringLiteral("可粘贴内容"));
    updateActions();
    statusBar()->showMessage(tr("Action状态：cut=启用  copy=启用  paste=启用"));
    saveMain(QStringLiteral("09-action-enabled.png"));

    // 10 连续查找后的实际选中结果。
    ui->editor->setPlainText(QStringLiteral("Qt 是跨平台框架。Qt Widgets 用于构建桌面界面。再次出现 Qt。"));
    ui->editor->moveCursor(QTextCursor::Start);
    findText(QStringLiteral("Qt"), false, true);
    findText(QStringLiteral("Qt"), false, true);
    statusBar()->showMessage(tr("连续查找：已定位到第二个 Qt"));
    saveMain(QStringLiteral("10-find-result.png"));

    // 11 全部替换的实际结果。
    ui->editor->setPlainText(QStringLiteral("cat CAT cat"));
    const int replaced = replaceAll(QStringLiteral("cat"), QStringLiteral("dog"), false);
    statusBar()->showMessage(tr("全部替换完成：共替换 %1 处，可一次撤销").arg(replaced));
    saveMain(QStringLiteral("11-replace-all-result.png"));

    // 12 行号区滚动与多行显示结果。
    QStringList numberedLines;
    for (int i = 1; i <= 18; ++i)
        numberedLines << tr("第 %1 行：行号区域随文本块同步绘制").arg(i);
    ui->editor->setPlainText(numberedLines.join(QLatin1Char('\n')));
    ui->actionLineNumbers->setChecked(true);
    ui->editor->moveCursor(QTextCursor::End);
    updateStatus();
    saveMain(QStringLiteral("12-line-numbers.png"));

    // 退出取证模式时不触发未保存询问。
    ui->editor->document()->setModified(false);
}
