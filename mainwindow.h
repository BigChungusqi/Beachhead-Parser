#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QTimer>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QGroupBox>
#include <QTextEdit>
#include <QPushButton>
#include <QAction>
#include <QMenu>
#include "texparser.h"
#include "localization.h"

class ZoomScrollArea;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void loadAndDisplayFile(const QString &filePath);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onOpenFile();
    void onOpenDirectory();
    void onFileItemClicked(QListWidgetItem *item);
    void onFileItemDoubleClicked(QListWidgetItem *item);
    void onZoomIn();
    void onZoomOut();
    void onFitToWindow();
    void onExportPng();
    void onPlayPause();
    void onStop();
    void onFrameChanged(int value);
    void onAnimationTick();
    void onThemeDark();
    void onThemeLight();
    void onLanguageEnglish();
    void onLanguageChinese();
    void onAbout();

private:
    void setupUI();
    void setupToolBar();
    void setupMenuBar();
    void setupConnections();
    void applyTheme(bool dark);
    void updateImageDisplay(const QImage &image);
    void updateInfoPanel(const TextureInfo &info);
    void updateAnimationControls();
    void populateFileList(const QString &directory);
    void retranslateUI();

    QToolBar *m_toolBar;
    QListWidget *m_fileList;
    ZoomScrollArea *m_scrollArea;
    QLabel *m_imageLabel;
    QTextEdit *m_infoPanel;

    QWidget *m_animWidget;
    QSlider *m_frameSlider;
    QLabel *m_frameLabel;
    QPushButton *m_playPauseBtn;
    QPushButton *m_stopBtn;
    QTimer *m_animTimer;

    QAction *m_openFileAct;
    QAction *m_openDirAct;
    QAction *m_zoomInAct;
    QAction *m_zoomOutAct;
    QAction *m_fitWindowAct;
    QAction *m_exportPngAct;
    QAction *m_themeDarkAct;
    QAction *m_themeLightAct;
    QAction *m_langEnAct;
    QAction *m_langZhAct;
    QAction *m_aboutAct;

    TexParser m_parser;
    QString m_currentDir;
    int m_currentFrame;
    double m_zoomLevel;
    bool m_isPlaying;
    bool m_isDarkTheme;
    static constexpr double ZOOM_FACTOR = 1.25;
    static constexpr int ANIM_INTERVAL_MS = 100;
    static constexpr const char* APP_VERSION = "1.0.1";
};

class ZoomScrollArea : public QScrollArea
{
    Q_OBJECT
public:
    explicit ZoomScrollArea(QWidget *parent = nullptr);
    void setOriginalImage(const QImage &image);
    void setZoomLevel(double level);
    double zoomLevel() const { return m_zoomLevel; }
    void fitToImage(const QImage &image);
    void fitToWindow();
    const QImage& originalImage() const { return m_originalImage; }
protected:
    void wheelEvent(QWheelEvent *event) override;
private:
    void applyZoom();
    double m_zoomLevel;
    QImage m_originalImage;
    QLabel *m_imageLabel;
};

#endif // MAINWINDOW_H
