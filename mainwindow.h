#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextStream>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QLabel;
class QCloseEvent;
class QDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool runSelfTests(QTextStream &out);
    void captureEvidence(const QString &outputDir);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void chooseFont();
    void chooseTextColor();
    void chooseEditorColor();
    void showFindDialog();
    void showReplaceDialog();
    void showAboutDialog();
    void updateActions();
    void updateStatus();
    void updateWindowTitle();
    void autoSave();

private:
    bool maybeSave();
    bool loadFromPath(const QString &path, QString *error = nullptr);
    bool saveToPath(const QString &path, QString *error = nullptr);
    bool findText(const QString &needle, bool backward, bool caseSensitive);
    int replaceAll(const QString &needle, const QString &replacement, bool caseSensitive);
    QDialog *createFindDialog(bool replaceMode);
    QDialog *createAboutDialog();
    void setCurrentFile(const QString &path);

    Ui::MainWindow *ui;
    QString m_currentFile;
    QLabel *m_positionLabel;
    QLabel *m_countLabel;
};

#endif

