#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QImageWriter>
#include <QDir>
#include <QWheelEvent>
#include <QSplitter>
#include <QGroupBox>
#include <QAction>
#include <QMenuBar>
#include <QPushButton>
#include <QStyle>
#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QRegularExpression>
#include <QDebug>

// ============================================================
// ZoomScrollArea implementation
// ============================================================

ZoomScrollArea::ZoomScrollArea(QWidget *parent)
    : QScrollArea(parent)
    , m_zoomLevel(1.0)
    , m_imageLabel(nullptr)
{
    setWidgetResizable(false);
    setAlignment(Qt::AlignCenter);
    setBackgroundRole(QPalette::Dark);
}

void ZoomScrollArea::setOriginalImage(const QImage &image)
{
    m_originalImage = image.copy();
    applyZoom();
}

void ZoomScrollArea::setZoomLevel(double level)
{
    m_zoomLevel = qBound(0.05, level, 32.0);
    applyZoom();
}

void ZoomScrollArea::applyZoom()
{
    if (m_originalImage.isNull()) return;
    if (!widget()) return;

    QLabel *label = qobject_cast<QLabel*>(widget());
    if (!label) return;

    QSize scaledSize(
        qRound(m_originalImage.width() * m_zoomLevel),
        qRound(m_originalImage.height() * m_zoomLevel)
    );

    if (scaledSize.width() < 1) scaledSize.setWidth(1);
    if (scaledSize.height() < 1) scaledSize.setHeight(1);

    QImage scaled = m_originalImage.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    label->setPixmap(QPixmap::fromImage(scaled));
    label->resize(scaledSize);
}

void ZoomScrollArea::fitToImage(const QImage &image)
{
    if (image.isNull()) return;
    if (!widget()) return;

    m_originalImage = image.copy();

    QSize viewportSize = viewport()->size();
    double scaleX = (double)viewportSize.width() / image.width();
    double scaleY = (double)viewportSize.height() / image.height();
    m_zoomLevel = qMin(scaleX, scaleY) * 0.95;

    applyZoom();
}

void ZoomScrollArea::fitToWindow()
{
    if (m_originalImage.isNull()) return;
    fitToImage(m_originalImage);
}

void ZoomScrollArea::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
        setZoomLevel(m_zoomLevel * factor);
        event->accept();
    } else {
        QScrollArea::wheelEvent(event);
    }
}

// ============================================================
// MainWindow implementation
// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentFrame(0)
    , m_zoomLevel(1.0)
    , m_isPlaying(false)
    , m_isDarkTheme(true)
{
    // Initialize localization
    QString sysLang = QLocale::system().name();
    if (sysLang.startsWith("zh")) {
        Localization::instance().loadLanguage("zh");
    } else {
        Localization::instance().loadLanguage("en");
    }

    setWindowTitle(L("app_title"));
    resize(1200, 800);
    setAcceptDrops(true);

    setupUI();
    setupToolBar();
    setupMenuBar();
    setupConnections();
    applyTheme(m_isDarkTheme);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    // Central widget with splitter layout
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(mainSplitter);

    // === Left panel: File list ===
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(4, 4, 4, 4);

    QLabel *fileListLabel = new QLabel(L("file_list"), leftPanel);
    fileListLabel->setAlignment(Qt::AlignCenter);
    leftLayout->addWidget(fileListLabel);

    m_fileList = new QListWidget(leftPanel);
    m_fileList->setMinimumWidth(180);
    m_fileList->setMaximumWidth(300);
    m_fileList->setIconSize(QSize(16, 16));
    leftLayout->addWidget(m_fileList);

    mainSplitter->addWidget(leftPanel);

    // === Center panel: Image preview ===
    QWidget *centerPanel = new QWidget(this);
    QVBoxLayout *centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(4, 4, 4, 4);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setText(L("drag_hint"));
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(400, 300);

    m_scrollArea = new ZoomScrollArea(this);
    m_scrollArea->setWidget(m_imageLabel);
    centerLayout->addWidget(m_scrollArea, 1);

    // === Bottom animation controls ===
    m_animWidget = new QWidget(this);
    QHBoxLayout *animLayout = new QHBoxLayout(m_animWidget);
    animLayout->setContentsMargins(8, 4, 8, 4);

    QStyle *st = style();
    m_playPauseBtn = new QPushButton(L("play"), m_animWidget);
    m_playPauseBtn->setIcon(st->standardIcon(QStyle::SP_MediaPlay));
    m_playPauseBtn->setMaximumWidth(96);
    m_playPauseBtn->setEnabled(false);
    animLayout->addWidget(m_playPauseBtn);

    m_stopBtn = new QPushButton(L("stop"), m_animWidget);
    m_stopBtn->setIcon(st->standardIcon(QStyle::SP_MediaStop));
    m_stopBtn->setMaximumWidth(96);
    m_stopBtn->setEnabled(false);
    animLayout->addWidget(m_stopBtn);

    m_frameSlider = new QSlider(Qt::Horizontal, m_animWidget);
    m_frameSlider->setRange(0, 0);
    m_frameSlider->setEnabled(false);
    animLayout->addWidget(m_frameSlider, 1);

    m_frameLabel = new QLabel("0 / 0", m_animWidget);
    m_frameLabel->setMinimumWidth(100);
    animLayout->addWidget(m_frameLabel);

    centerLayout->addWidget(m_animWidget);

    mainSplitter->addWidget(centerPanel);

    // === Right panel: Info ===
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 4, 4, 4);

    QLabel *infoLabel = new QLabel(L("texture_info"), rightPanel);
    infoLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(infoLabel);

    m_infoPanel = new QTextEdit(rightPanel);
    m_infoPanel->setReadOnly(true);
    m_infoPanel->setMinimumWidth(200);
    m_infoPanel->setMaximumWidth(320);
    rightLayout->addWidget(m_infoPanel);

    mainSplitter->addWidget(rightPanel);

    // Set splitter sizes
    mainSplitter->setSizes(QList<int>{200, 700, 280});

    // Animation timer
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(ANIM_INTERVAL_MS);
}

void MainWindow::setupToolBar()
{
    QStyle *st = style();
    m_toolBar = addToolBar("Toolbar");
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(22, 22));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    m_openFileAct = m_toolBar->addAction(L("open_file"));
    m_openFileAct->setIcon(st->standardIcon(QStyle::SP_DialogOpenButton));
    m_openFileAct->setToolTip(L("open_file"));

    m_openDirAct  = m_toolBar->addAction(L("open_directory"));
    m_openDirAct->setIcon(st->standardIcon(QStyle::SP_DirOpenIcon));
    m_openDirAct->setToolTip(L("open_directory"));

    m_toolBar->addSeparator();

    // Qt6 removed SP_ZoomIn/SP_ZoomOut, so draw a theme-neutral magnifier icon.
    auto makeZoomIcon = [](bool zoomIn) -> QIcon {
        QPixmap pm(22, 22);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor(160, 168, 180), 2);
        p.setPen(pen);
        p.drawEllipse(QPoint(9, 9), 6, 6);
        p.drawLine(13, 13, 19, 19);
        p.drawLine(9, 6, 9, 12);
        if (zoomIn) p.drawLine(6, 9, 12, 9);
        p.end();
        return QIcon(pm);
    };

    m_zoomInAct   = m_toolBar->addAction(L("zoom_in"));
    m_zoomInAct->setIcon(makeZoomIcon(true));
    m_zoomInAct->setToolTip(L("zoom_in"));

    m_zoomOutAct  = m_toolBar->addAction(L("zoom_out"));
    m_zoomOutAct->setIcon(makeZoomIcon(false));
    m_zoomOutAct->setToolTip(L("zoom_out"));

    m_fitWindowAct = m_toolBar->addAction(L("fit_window"));
    m_fitWindowAct->setIcon(st->standardIcon(QStyle::SP_DesktopIcon));
    m_fitWindowAct->setToolTip(L("fit_window"));

    m_toolBar->addSeparator();

    m_exportPngAct = m_toolBar->addAction(L("export_png"));
    m_exportPngAct->setIcon(st->standardIcon(QStyle::SP_DialogSaveButton));
    m_exportPngAct->setToolTip(L("export_png"));
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // File menu
    QMenu *fileMenu = menuBar->addMenu("&" + L("file_list").left(1));
    fileMenu->addAction(m_openFileAct);
    fileMenu->addAction(m_openDirAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exportPngAct);
    fileMenu->addSeparator();
    QAction *exitAct = fileMenu->addAction(L("exit"));
    connect(exitAct, &QAction::triggered, this, &QMainWindow::close);

    // View menu
    QMenu *viewMenu = menuBar->addMenu("&" + L("view").left(1));
    viewMenu->addAction(m_zoomInAct);
    viewMenu->addAction(m_zoomOutAct);
    viewMenu->addAction(m_fitWindowAct);
    viewMenu->addSeparator();

    // Theme submenu
    QMenu *themeMenu = viewMenu->addMenu(L("theme"));
    m_themeDarkAct = themeMenu->addAction(L("theme_dark"));
    m_themeLightAct = themeMenu->addAction(L("theme_light"));
    m_themeDarkAct->setCheckable(true);
    m_themeLightAct->setCheckable(true);
    m_themeDarkAct->setChecked(m_isDarkTheme);
    m_themeLightAct->setChecked(!m_isDarkTheme);

    // Language submenu
    QMenu *langMenu = viewMenu->addMenu(L("language"));
    m_langEnAct = langMenu->addAction("English");
    m_langZhAct = langMenu->addAction("\u4e2d\u6587");
    m_langEnAct->setCheckable(true);
    m_langZhAct->setCheckable(true);
    if (Localization::instance().currentLanguage().startsWith("zh")) {
        m_langZhAct->setChecked(true);
    } else {
        m_langEnAct->setChecked(true);
    }

    // Help menu
    QMenu *helpMenu = menuBar->addMenu("&" + L("help").left(1));
    m_aboutAct = helpMenu->addAction(L("about"));
}

void MainWindow::setupConnections()
{
    connect(m_openFileAct, &QAction::triggered, this, &MainWindow::onOpenFile);
    connect(m_openDirAct,  &QAction::triggered, this, &MainWindow::onOpenDirectory);
    connect(m_zoomInAct,   &QAction::triggered, this, &MainWindow::onZoomIn);
    connect(m_zoomOutAct,  &QAction::triggered, this, &MainWindow::onZoomOut);
    connect(m_fitWindowAct, &QAction::triggered, this, &MainWindow::onFitToWindow);
    connect(m_exportPngAct, &QAction::triggered, this, &MainWindow::onExportPng);

    connect(m_fileList, &QListWidget::itemClicked, this, &MainWindow::onFileItemClicked);
    connect(m_fileList, &QListWidget::itemDoubleClicked, this, &MainWindow::onFileItemDoubleClicked);

    connect(m_playPauseBtn, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_frameSlider, &QSlider::valueChanged, this, &MainWindow::onFrameChanged);
    connect(m_animTimer, &QTimer::timeout, this, &MainWindow::onAnimationTick);

    connect(m_themeDarkAct, &QAction::triggered, this, &MainWindow::onThemeDark);
    connect(m_themeLightAct, &QAction::triggered, this, &MainWindow::onThemeLight);
    connect(m_langEnAct, &QAction::triggered, this, &MainWindow::onLanguageEnglish);
    connect(m_langZhAct, &QAction::triggered, this, &MainWindow::onLanguageChinese);
    connect(m_aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::applyTheme(bool dark)
{
    m_isDarkTheme = dark;
    m_themeDarkAct->setChecked(dark);
    m_themeLightAct->setChecked(!dark);

    if (dark) {
        QString style = R"(
            QMainWindow, QWidget {
                background-color: #1f222b;
                color: #d7dbe5;
                font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
                font-size: 13px;
            }
            QMenuBar {
                background-color: #272b35;
                color: #d7dbe5;
                border-bottom: 1px solid #353a47;
                padding: 2px;
            }
            QMenuBar::item { padding: 4px 10px; border-radius: 4px; }
            QMenuBar::item:selected { background-color: #3a4150; }
            QMenu {
                background-color: #272b35;
                color: #d7dbe5;
                border: 1px solid #353a47;
                border-radius: 8px;
                padding: 4px;
            }
            QMenu::item { padding: 6px 22px 6px 12px; border-radius: 4px; }
            QMenu::item:selected { background-color: #3a4150; }
            QToolBar {
                background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2b2f3a,stop:1 #23262f);
                border-bottom: 1px solid #353a47;
                spacing: 6px;
                padding: 4px 6px;
            }
            QToolButton {
                background-color: #2f3440;
                color: #d7dbe5;
                border: 1px solid #3a4150;
                border-radius: 6px;
                padding: 6px 10px;
            }
            QToolButton:hover { background-color: #3a4150; border: 1px solid #4c8dff; }
            QToolButton:pressed { background-color: #4c8dff; color: #ffffff; }
            QListWidget {
                background-color: #1b1e25;
                color: #d7dbe5;
                border: 1px solid #353a47;
                border-radius: 8px;
                outline: none;
            }
            QListWidget::item { padding: 6px 8px; border-bottom: 1px solid #23262f; }
            QListWidget::item:selected { background-color: #4c8dff; color: #ffffff; }
            QListWidget::item:hover { background-color: #2a2f3a; }
            QScrollArea {
                background-color: #16181e;
                border: 1px solid #353a47;
                border-radius: 8px;
            }
            QLabel { color: #d7dbe5; background-color: transparent; }
            QTextEdit {
                background-color: #1b1e25;
                color: #c9d1e0;
                border: 1px solid #353a47;
                border-radius: 8px;
                padding: 8px;
                font-family: "Consolas", "Courier New", monospace;
                font-size: 12px;
            }
            QSlider::groove:horizontal { background: #3a4150; height: 6px; border-radius: 3px; }
            QSlider::handle:horizontal {
                background: #4c8dff; width: 16px; height: 16px; margin: -5px 0; border-radius: 8px;
            }
            QSlider::handle:horizontal:hover { background: #6aa0ff; }
            QPushButton {
                background-color: #2f3440; color: #d7dbe5;
                border: 1px solid #3a4150; border-radius: 6px; padding: 6px 16px;
            }
            QPushButton:hover { background-color: #3a4150; border: 1px solid #4c8dff; }
            QPushButton:pressed { background-color: #4c8dff; color: #ffffff; }
            QPushButton:disabled { background-color: #23262f; color: #5a6172; }
            QStatusBar {
                background-color: #23262f; color: #8a93a6;
                border-top: 1px solid #353a47;
            }
            QSplitter::handle { background-color: #353a47; width: 2px; }
            QGroupBox {
                border: 1px solid #353a47; border-radius: 8px;
                margin-top: 10px; padding-top: 14px; color: #d7dbe5;
            }
            QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }
        )";
        setStyleSheet(style);
        m_scrollArea->setBackgroundRole(QPalette::Dark);
    } else {
        QString style = R"(
            QMainWindow, QWidget {
                background-color: #f4f6fb;
                color: #2a2f3a;
                font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
                font-size: 13px;
            }
            QMenuBar {
                background-color: #ffffff;
                color: #2a2f3a;
                border-bottom: 1px solid #d8dee9;
                padding: 2px;
            }
            QMenuBar::item { padding: 4px 10px; border-radius: 4px; }
            QMenuBar::item:selected { background-color: #e8eefc; }
            QMenu {
                background-color: #ffffff;
                color: #2a2f3a;
                border: 1px solid #d8dee9;
                border-radius: 8px;
                padding: 4px;
            }
            QMenu::item { padding: 6px 22px 6px 12px; border-radius: 4px; }
            QMenu::item:selected { background-color: #e8eefc; }
            QToolBar {
                background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #ffffff,stop:1 #eef1f7);
                border-bottom: 1px solid #d8dee9;
                spacing: 6px;
                padding: 4px 6px;
            }
            QToolButton {
                background-color: #ffffff;
                color: #2a2f3a;
                border: 1px solid #d8dee9;
                border-radius: 6px;
                padding: 6px 10px;
            }
            QToolButton:hover { background-color: #eef1f7; border: 1px solid #2f6fed; }
            QToolButton:pressed { background-color: #2f6fed; color: #ffffff; }
            QListWidget {
                background-color: #ffffff;
                color: #2a2f3a;
                border: 1px solid #d8dee9;
                border-radius: 8px;
                outline: none;
            }
            QListWidget::item { padding: 6px 8px; border-bottom: 1px solid #eef1f7; }
            QListWidget::item:selected { background-color: #2f6fed; color: #ffffff; }
            QListWidget::item:hover { background-color: #f0f3f9; }
            QScrollArea {
                background-color: #ffffff;
                border: 1px solid #d8dee9;
                border-radius: 8px;
            }
            QLabel { color: #2a2f3a; background-color: transparent; }
            QTextEdit {
                background-color: #ffffff;
                color: #2a2f3a;
                border: 1px solid #d8dee9;
                border-radius: 8px;
                padding: 8px;
                font-family: "Consolas", "Courier New", monospace;
                font-size: 12px;
            }
            QSlider::groove:horizontal { background: #d8dee9; height: 6px; border-radius: 3px; }
            QSlider::handle:horizontal {
                background: #2f6fed; width: 16px; height: 16px; margin: -5px 0; border-radius: 8px;
            }
            QSlider::handle:horizontal:hover { background: #1f5fce; }
            QPushButton {
                background-color: #ffffff; color: #2a2f3a;
                border: 1px solid #d8dee9; border-radius: 6px; padding: 6px 16px;
            }
            QPushButton:hover { background-color: #eef1f7; border: 1px solid #2f6fed; }
            QPushButton:pressed { background-color: #2f6fed; color: #ffffff; }
            QPushButton:disabled { background-color: #f4f6fb; color: #9aa3b2; }
            QStatusBar {
                background-color: #ffffff; color: #6b7280;
                border-top: 1px solid #d8dee9;
            }
            QSplitter::handle { background-color: #d8dee9; width: 2px; }
            QGroupBox {
                border: 1px solid #d8dee9; border-radius: 8px;
                margin-top: 10px; padding-top: 14px; color: #2a2f3a;
            }
            QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }
        )";
        setStyleSheet(style);
        m_scrollArea->setBackgroundRole(QPalette::Light);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    for (const QUrl &url : urls) {
        QString filePath = url.toLocalFile();
        QFileInfo fi(filePath);

        if (fi.isDir()) {
            populateFileList(filePath);
        } else if (fi.isFile()) {
            loadAndDisplayFile(filePath);
        }
    }
}

void MainWindow::onOpenFile()
{
    QString filter = L("supported_formats") + " (*.clut *.rgb *.rmap8 *.tmap8 *.tmap32 *.hmap *.pal *.spr);;" + tr("All Files (*.*)");
    QString filePath = QFileDialog::getOpenFileName(this, L("open_file"), QString(), filter);
    if (!filePath.isEmpty()) {
        loadAndDisplayFile(filePath);
    }
}

void MainWindow::onOpenDirectory()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, L("open_directory"), QString());
    if (!dirPath.isEmpty()) {
        populateFileList(dirPath);
    }
}

void MainWindow::populateFileList(const QString &directory)
{
    m_currentDir = directory;
    m_fileList->clear();

    QDir dir(directory);
    QStringList filters;
    filters << "*.clut" << "*.rgb" << "*.rmap8" << "*.tmap8" << "*.tmap32" << "*.hmap" << "*.pal" << "*.spr"
            << "*.CLUT" << "*.RGB" << "*.RMAP8" << "*.TMAP8" << "*.TMAP32" << "*.HMAP" << "*.PAL" << "*.SPR";

    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);

    for (const QFileInfo &fi : fileList) {
        QListWidgetItem *item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        item->setToolTip(fi.absoluteFilePath() + "\n" +
                         QString::number(fi.size()) + " bytes");

        // Color-code by format
        QString ext = fi.suffix().toLower();
        QColor color;
        switch (TexParser::detectFormat(fi.fileName())) {
        case TextureFormat::CLUT:   color = QColor(100, 180, 255); break;
        case TextureFormat::RGB:    color = QColor(255, 180, 100); break;
        case TextureFormat::RMAP8:  color = QColor(180, 255, 100); break;
        case TextureFormat::TMAP8:  color = QColor(100, 255, 180); break;
        case TextureFormat::TMAP32: color = QColor(255, 100, 180); break;
        case TextureFormat::HMAP:   color = QColor(200, 200, 100); break;
        case TextureFormat::PAL:    color = QColor(255, 150, 50); break;
        case TextureFormat::SPR:    color = QColor(150, 100, 255); break;
        default: color = QColor(180, 180, 180);
        }
        item->setForeground(color);

        m_fileList->addItem(item);
    }

    statusBar()->showMessage(QString("Loaded %1 texture files - %2").arg(fileList.size()).arg(directory));

    // Auto-select first file
    if (m_fileList->count() > 0) {
        m_fileList->setCurrentRow(0);
        onFileItemClicked(m_fileList->item(0));
    }
}

void MainWindow::onFileItemClicked(QListWidgetItem *item)
{
    QString filePath = item->data(Qt::UserRole).toString();
    loadAndDisplayFile(filePath);
}

void MainWindow::onFileItemDoubleClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
}

void MainWindow::loadAndDisplayFile(const QString &filePath)
{
    // Stop any playing animation
    if (m_isPlaying) {
        onPlayPause();
    }

    if (!m_parser.parseFile(filePath)) {
        m_imageLabel->setText(L("cannot_parse") + ": " + QFileInfo(filePath).fileName());
        m_imageLabel->setPixmap(QPixmap());
        m_scrollArea->setOriginalImage(QImage());
        m_infoPanel->clear();
        updateAnimationControls();
        return;
    }

    // SPR auto-loads its .PAL inside the parser; only warn (no blocking dialog)
    // if no palette could be located in the same directory.
    if (m_parser.getInfo().format == TextureFormat::SPR && m_parser.getPalPalettes().isEmpty()) {
        statusBar()->showMessage(tr("Warning: SPR loaded without PAL - colors may be incorrect"));
    }

    m_currentFrame = 0;
    QImage img = m_parser.getImage(0);
    updateImageDisplay(img);
    updateInfoPanel(m_parser.getInfo());
    updateAnimationControls();

    // Auto-fit
    m_scrollArea->fitToImage(img);
    m_zoomLevel = m_scrollArea->zoomLevel();

    statusBar()->showMessage(QString("Loaded: %1 [%2x%3]")
        .arg(m_parser.getInfo().fileName)
        .arg(m_parser.getInfo().width)
        .arg(m_parser.getInfo().height));
}

void MainWindow::updateImageDisplay(const QImage &image)
{
    if (image.isNull()) {
        m_imageLabel->setText(L("no_image_data"));
        m_imageLabel->setPixmap(QPixmap());
        return;
    }

    m_imageLabel->setPixmap(QPixmap::fromImage(image));
    m_imageLabel->adjustSize();
}

void MainWindow::updateInfoPanel(const TextureInfo &info)
{
    auto head = [&](const QString &t) {
        return QString("<div style='font-weight:600;font-size:12px;color:#5b9bff;"
                       "margin:12px 0 5px;padding-bottom:4px;"
                       "border-bottom:1px solid rgba(120,130,150,0.35);'>") + t + "</div>";
    };
    // 固定标签列宽并右对齐，保证所有取值从同一竖线起排 -> 文字精确对齐
    auto row = [&](const QString &k, const QString &v) {
        return QString("<tr><td width='118' align='right' valign='top' "
                       "style='padding:3px 12px 3px 0;color:#8a93a6;white-space:nowrap;'>")
               + k + "</td><td style='padding:3px 0;color:#d8dce6;'>"
               + v + "</td></tr>";
    };

    QString html;
    html += head(L("basic_info"));
    html += "<table style='width:100%;border-collapse:collapse;font-size:12px;'>";
    html += row(L("file_name"), info.fileName);
    html += row(L("format"), info.formatName);
    html += row(L("dimensions"), QString("%1 × %2").arg(info.width).arg(info.height));
    html += row(L("color_depth"), QString::number(info.colorDepth) + " bit");
    html += row(L("file_size"), QString::number(info.fileSize) + " bytes");
    html += row(L("header_size"), QString::number(info.headerSize) + " bytes");
    html += row(L("alpha_channel"), info.hasAlpha ? L("yes") : L("no"));
    html += "</table>";

    if (info.isAnimated) {
        html += head(L("animation_info"));
        html += "<table style='width:100%;border-collapse:collapse;font-size:12px;'>";
        html += row(L("frame_count"), QString::number(info.frameCount));
        html += row(L("frame_height"), QString::number(info.frameHeight));
        html += "</table>";
    }

    if (info.usesClut || info.palCount > 0) {
        html += head(L("palette_info"));
        html += "<table style='width:100%;border-collapse:collapse;font-size:12px;'>";
        if (info.usesClut)
            html += row(L("palette_file"), QFileInfo(info.clutFile).fileName());
        if (info.palCount > 0)
            html += row(L("pal_count"), QString::number(info.palCount));
        html += "</table>";
    }

    if (info.format == TextureFormat::SPR) {
        html += head(L("spr_label"));
        html += "<table style='width:100%;border-collapse:collapse;font-size:12px;'>";
        html += row(L("frame_count"), QString::number(info.frameCount));
        html += row(L("pal_status"), m_parser.getPalPalettes().isEmpty() ? L("not_loaded") : L("loaded"));
        if (!m_parser.getPalPalettes().isEmpty())
            html += row(L("palettes"), QString::number(m_parser.getPalPalettes().size()));
        html += "</table>";
    }

    html += head(L("statistics"));
    html += "<table style='width:100%;border-collapse:collapse;font-size:12px;'>";
    html += row(L("total_pixels"),
                QString::number(static_cast<qulonglong>(info.width) * info.height));
    if (info.fileSize > 0) {
        double bpp = (double)info.fileSize * 8.0 / (info.width * info.height);
        html += row(L("actual_bpp"), QString::number(bpp, 'f', 2) + " bpp");
    }
    html += "</table>";

    m_infoPanel->setHtml(html);
}

void MainWindow::updateAnimationControls()
{
    const TextureInfo &info = m_parser.getInfo();
    bool hasAnimation = info.isAnimated && info.frameCount > 1;

    m_playPauseBtn->setEnabled(hasAnimation);
    m_stopBtn->setEnabled(hasAnimation);
    m_frameSlider->setEnabled(hasAnimation);

    if (hasAnimation) {
        m_frameSlider->setRange(0, info.frameCount - 1);
        m_frameSlider->setValue(0);
        m_frameLabel->setText(QString("%1: %2 / %3").arg(L("frame")).arg(1).arg(info.frameCount));
    } else {
        m_frameSlider->setRange(0, 0);
        m_frameSlider->setValue(0);
        m_frameLabel->setText("0 / 0");
    }
}

void MainWindow::onZoomIn()
{
    m_zoomLevel *= ZOOM_FACTOR;
    m_scrollArea->setZoomLevel(m_zoomLevel);
}

void MainWindow::onZoomOut()
{
    m_zoomLevel /= ZOOM_FACTOR;
    m_scrollArea->setZoomLevel(m_zoomLevel);
}

void MainWindow::onFitToWindow()
{
    m_scrollArea->fitToWindow();
    m_zoomLevel = m_scrollArea->zoomLevel();
}

void MainWindow::onExportPng()
{
    QImage img = m_parser.getImage(m_currentFrame);
    if (img.isNull()) {
        QMessageBox::warning(this, L("export_failed"), L("no_image_to_export"));
        return;
    }

    QString defaultName = QFileInfo(m_parser.getInfo().fileName).completeBaseName() + ".png";
    QString filePath = QFileDialog::getSaveFileName(this, L("export_png"), defaultName, "PNG (*.png)");
    if (!filePath.isEmpty()) {
        if (img.save(filePath, "PNG")) {
            statusBar()->showMessage("Exported: " + filePath);
        } else {
            QMessageBox::warning(this, L("export_failed"), "Cannot save: " + filePath);
        }
    }
}

void MainWindow::onPlayPause()
{
    QStyle *st = style();
    if (m_isPlaying) {
        m_animTimer->stop();
        m_isPlaying = false;
        m_playPauseBtn->setText(L("play"));
        m_playPauseBtn->setIcon(st->standardIcon(QStyle::SP_MediaPlay));
    } else {
        if (m_parser.getFrameCount() <= 1) return;
        m_isPlaying = true;
        m_playPauseBtn->setText(L("pause"));
        m_playPauseBtn->setIcon(st->standardIcon(QStyle::SP_MediaPause));
        m_animTimer->start();
    }
}

void MainWindow::onStop()
{
    QStyle *st = style();
    m_animTimer->stop();
    m_isPlaying = false;
    m_playPauseBtn->setText(L("play"));
    m_playPauseBtn->setIcon(st->standardIcon(QStyle::SP_MediaPlay));
    m_currentFrame = 0;
    m_frameSlider->setValue(0);

    QImage img = m_parser.getImage(0);
    updateImageDisplay(img);
    m_scrollArea->setOriginalImage(img);
    m_frameLabel->setText(QString("%1: %2 / %3").arg(L("frame")).arg(1).arg(m_parser.getFrameCount()));
}

void MainWindow::onFrameChanged(int value)
{
    m_currentFrame = value;
    QImage img = m_parser.getFrame(value);
    if (!img.isNull()) {
        updateImageDisplay(img);
        m_scrollArea->setOriginalImage(img);
    }
    m_frameLabel->setText(QString("%1: %2 / %3").arg(L("frame")).arg(value + 1).arg(m_parser.getFrameCount()));
}

void MainWindow::onAnimationTick()
{
    int totalFrames = m_parser.getFrameCount();
    if (totalFrames <= 1) return;

    m_currentFrame = (m_currentFrame + 1) % totalFrames;
    m_frameSlider->blockSignals(true);
    m_frameSlider->setValue(m_currentFrame);
    m_frameSlider->blockSignals(false);

    QImage img = m_parser.getFrame(m_currentFrame);
    if (!img.isNull()) {
        updateImageDisplay(img);
        m_scrollArea->setOriginalImage(img);
    }
    m_frameLabel->setText(QString("%1: %2 / %3").arg(L("frame")).arg(m_currentFrame + 1).arg(totalFrames));
}

void MainWindow::onThemeDark()
{
    applyTheme(true);
}

void MainWindow::onThemeLight()
{
    applyTheme(false);
}

void MainWindow::onLanguageEnglish()
{
    Localization::instance().loadLanguage("en");
    m_langEnAct->setChecked(true);
    m_langZhAct->setChecked(false);
    retranslateUI();
}

void MainWindow::onLanguageChinese()
{
    Localization::instance().loadLanguage("zh");
    m_langEnAct->setChecked(false);
    m_langZhAct->setChecked(true);
    retranslateUI();
}

void MainWindow::onAbout()
{
    QString aboutText = QString(
        "<h2>Beachhead 2000 Parser Viewer</h2>"
        "<p><b>%1:</b> %2</p>"
        "<p><b>%3:</b> CLUT, RGB, RMAP8, TMAP8, TMAP32, HMAP</p>"
        "<p>%4</p>"
    ).arg(L("version")).arg(APP_VERSION)
     .arg(L("supported_formats"))
     .arg(L("drag_files_to_view"));

    QMessageBox::about(this, L("about"), aboutText);
}

void MainWindow::retranslateUI()
{
    setWindowTitle(L("app_title"));
    m_openFileAct->setText(L("open_file"));
    m_openDirAct->setText(L("open_directory"));
    m_zoomInAct->setText(L("zoom_in"));
    m_zoomOutAct->setText(L("zoom_out"));
    m_fitWindowAct->setText(L("fit_window"));
    m_exportPngAct->setText(L("export_png"));
    m_themeDarkAct->setText(L("theme_dark"));
    m_themeLightAct->setText(L("theme_light"));
    m_aboutAct->setText(L("about"));

    if (!m_isPlaying) {
        m_playPauseBtn->setText(L("play"));
    } else {
        m_playPauseBtn->setText(L("pause"));
    }
    m_stopBtn->setText(L("stop"));

    // Refresh info panel if we have a loaded file
    if (!m_parser.getInfo().fileName.isEmpty()) {
        updateInfoPanel(m_parser.getInfo());
    }
}

#include "mainwindow.moc"
