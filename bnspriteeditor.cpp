#include "bnspriteeditor.h"
#include "ui_bnspriteeditor.h"

#define IMPORT_EXTENSIONS "BN Sprite (*.bnsa *.bnsprite *.dmp);;All files (*.*)"
#define IMPORT_EXTENSIONS_SF "SF Sprite (*.sfsa *.sfsprite *.bin);;All files (*.*)"
#define EXPORT_EXTENSIONS "Memory Dump (*.dmp);;BN Sprite (*.bnsa *.bnsprite);;All files (*.*)"
#define EXPORT_EXTENSIONS_SF "Memory Dump (*.bin);;SF Sprite (*.sfsa *.sfsprite);;All files (*.*)"
#define PALETTE_EXTENSIONS "Palette File (*.pal);;All files (*.*)"

static const QString c_programVersion = "v0.3.0";

//---------------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------------
BNSpriteEditor::BNSpriteEditor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::BNSpriteEditor)
{
    ui->setupUi(this);

    // Load previous path and window size
    this->setWindowTitle(this->windowTitle() + " " + c_programVersion);
    m_settings = new QSettings("brianuuu", "BNSpriteEditor", this);
    m_path = m_settings->value("DefaultDirectory", QString()).toString();
    m_simpleMode = false;
    if (m_settings->value("SimpleMode", false).toBool())
    {
        on_actionSimple_Mode_triggered();
    }

    m_copyAnim = -1;
    m_copyFrame = -1;

    m_tilesetImage = Q_NULLPTR;
    m_tilesetGraphic = new QGraphicsScene(this);
    ui->Tileset_GV->setScene(m_tilesetGraphic);

    m_oamImage = Q_NULLPTR;
    m_oamGraphic = new QGraphicsScene(this);
    ui->OAM_GV->setScene(m_oamGraphic);

    m_previewGraphic = new QGraphicsScene(this);
    m_previewGraphic->setSceneRect(0, 0, 256, 256);
    m_previewBG = m_previewGraphic->addPixmap(QPixmap(":/resources/SizeGrid.png"));
    m_previewBG->setZValue(-1);
    ui->Preview_GV->setScene(m_previewGraphic);
    ui->Preview_Color->installEventFilter(this);
    ui->Preview_Color->setHidden(true);

    m_previewHighlight = m_previewGraphic->addPixmap(QPixmap::fromImage(QImage()));
    m_previewHighlight->setZValue(1000);
    m_highlightUp = true;
    m_highlightAlpha = 0;
    m_highlightTimer = new QTimer(this);
    connect(m_highlightTimer, SIGNAL(timeout()), SLOT(on_OAM_Highlight_timeout()));

    ui->OAM_TW->setColumnWidth(0, 10);
    ui->OAM_TW->setColumnWidth(1, 10);
    ui->OAM_TW->setColumnWidth(2, 66);

    m_moveDir = -1;
    m_moveTimer = new QTimer(this);
    m_moveTimer->setSingleShot(true);
    connect(m_moveTimer, SIGNAL(timeout()), SLOT(on_Image_Move_timeout()));

    m_playingAnimation = false;
    m_animationTimer = new QTimer(this);
    m_animationTimer->setSingleShot(true);
    connect(m_animationTimer, SIGNAL(timeout()), SLOT(on_Frame_Play_timeout()));
    m_playingSubAnimation = false;
    m_playingSubFrame = -1;
    m_subAnimationTimer = new QTimer(this);
    m_subAnimationTimer->setSingleShot(true);
    connect(m_subAnimationTimer, SIGNAL(timeout()), SLOT(on_SubFrame_Play_timeout()));

    connect(ui->Preview_GV, SIGNAL(previewScreenPressed(QPoint)), this, SLOT(on_Preview_pressed(QPoint)));
    connect(ui->Palette_GV, SIGNAL(colorChanged(int,int,QRgb)), this, SLOT(on_Palette_Color_changed(int,int,QRgb)));
    ui->Palette_Warning->setHidden(true);
    m_paletteContextMenu = new PaletteContextMenu(this);
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->Palette_GV, &PaletteGraphicsView::paletteContextMenuRequested, this, &BNSpriteEditor::on_PaletteContexMenu_requested);
    connect(m_paletteContextMenu->copyColorAction, &QAction::triggered, this, &BNSpriteEditor::on_PaletteContexMenu_colorCopied);
    connect(m_paletteContextMenu->replaceColorAction, &QAction::triggered, this, &BNSpriteEditor::on_PaletteContexMenu_colorReplaced);
    connect(m_paletteContextMenu->copyPaletteAction, &QAction::triggered, this, &BNSpriteEditor::on_PaletteContexMenu_paletteCopied);
    connect(m_paletteContextMenu->replacePaletteAction, &QAction::triggered, this, &BNSpriteEditor::on_PaletteContexMenu_paletteReplaced);
    connect(m_paletteContextMenu->insertAboveAction, &QAction::triggered, this, &BNSpriteEditor::on_PaletteContexMenu_paletteInsertAbove);
    connect(m_paletteContextMenu->insertBelowAction, &QAction::triggered, this, &BNSpriteEditor::on_PaletteContexMenu_paletteInsertBelow);
    connect(m_paletteContextMenu->deletePaletteAction, &QAction::triggered, this, &BNSpriteEditor::on_PaletteContexMenu_paletteDeleted);
}

//---------------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------------
BNSpriteEditor::~BNSpriteEditor()
{
    m_settings->setValue("DefaultDirectory", m_path);
    m_settings->setValue("SimpleMode", m_simpleMode);

    if (m_tilesetImage != Q_NULLPTR)
    {
        delete m_tilesetImage;
    }
    if (m_oamImage != Q_NULLPTR)
    {
        delete m_oamImage;
    }

    for (QImage* image : m_animThumbnails)
    {
        delete image;
    }
    m_animThumbnails.clear();

    for (QImage* image : m_frameThumbnails)
    {
        delete image;
    }
    m_frameThumbnails.clear();

    delete ui;
}

//---------------------------------------------------------------------------
// Handling mouse events
//---------------------------------------------------------------------------
bool BNSpriteEditor::eventFilter(QObject *object, QEvent *event)
{
    // Changing a color of a color block
    if (event->type() == QEvent::FocusIn)
    {
        QWidget* widget = qobject_cast<QWidget*>(object);
        if (widget == ui->Preview_Color)
        {
            widget->clearFocus();
            QPalette pal = widget->palette();
            QColor color = pal.color(QPalette::Base);
            QColor newColor = QColorDialog::getColor(color, this, "Pick a Color");
            if (newColor.isValid())
            {
                // Update color
                pal.setColor(QPalette::Base, newColor);
                widget->setPalette(pal);

                on_Preview_BG_currentTextChanged("Fixed Color");
            }
            return true;
        }
    }

    return false;
}

void BNSpriteEditor::closeEvent(QCloseEvent *event)
{
    ui->Palette_GV->closeInfo();

    if (m_csm != Q_NULLPTR)
    {
        if (!m_csm->close())
        {
            event->ignore();
            return;
        }
    }
    event->accept();
}

//---------------------------------------------------------------------------
// Action slots
//---------------------------------------------------------------------------
void BNSpriteEditor::on_actionImport_Sprite_triggered()
{
    ImportSprite(false);
}

void BNSpriteEditor::on_actionImport_SF_Sprite_triggered()
{
    ImportSprite(true);
}

void BNSpriteEditor::ImportSprite(bool isSFSprite)
{
    if (m_csm != Q_NULLPTR && m_csm->isVisible())
    {
        QMessageBox::warning(this, "Import Sprite", "Sprite import is not allowed while Custom Sprite Manager editing is active.", QMessageBox::Ok);
        return;
    }

    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString file = QFileDialog::getOpenFileName(this, tr("Import Sprite"), path, isSFSprite ? IMPORT_EXTENSIONS_SF : IMPORT_EXTENSIONS);
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    int index = file.lastIndexOf('\\');
    if (index == -1) index = file.lastIndexOf('/');

    // Clear everything we are editing
    ResetProgram();
    ui->Sprite_Name->setText(file.mid(index + 1));
    ui->Sprite_Name->setCursorPosition(0);

    // Load BN Sprite file
    string errorMsg;
    bool success = false;
    if (isSFSprite)
    {
        success = m_sprite.LoadSF(file.toStdWString(), errorMsg);
    }
    else
    {
        success = m_sprite.LoadBN(file.toStdWString(), errorMsg);
    }

    if (!success)
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
    }
    else
    {
        LoadSpriteToUI();
        QMessageBox::information(this, "Open", "File load successful!", QMessageBox::Ok);
    }
}

void BNSpriteEditor::LoadSpriteToUI()
{
    Q_ASSERT(m_sprite.IsLoaded());

    // Get all palettes from sprite
    m_paletteGroups.clear();
    vector<BNSprite::PaletteGroup> paletteGroups;
    m_sprite.GetAllPaletteGroups(paletteGroups);
    for (BNSprite::PaletteGroup const& group : paletteGroups)
    {
        AddPaletteGroupFromSprite(group);
    }

    // Set palette group limit
    ui->Palette_SB_Group->setValue(0);
    ui->Palette_SB_Group->setMaximum(m_paletteGroups.size() - 1);

    // Generate thumbnails from the first frame of all animations
    int animationCount = m_sprite.GetAnimationCount();
    for (int i = 0; i < animationCount; i++)
    {
        AddAnimationThumbnail(i);
        qApp->processEvents();
    }

    // Enable buttons
    ui->Anim_PB_New->setEnabled(ui->Anim_LW->count() < 255);
}

void BNSpriteEditor::on_actionExport_Sprite_triggered()
{
    ExportSprite(false);
}

void BNSpriteEditor::on_actionExport_SF_Sprite_triggered()
{
    ExportSprite(true);
}

void BNSpriteEditor::ExportSprite(bool isSFSprite)
{
    if (!m_sprite.IsLoaded()) return;
    if (ui->Anim_LW->count() == 0) return;

    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString file = QFileDialog::getSaveFileName(this, tr("Export Sprite"), path + "/" + ui->Sprite_Name->text(), isSFSprite ? EXPORT_EXTENSIONS_SF : EXPORT_EXTENSIONS);
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    // Overwrite palette
    ReplacePaletteInSprite();

    // Save BN sprite file
    string errorMsg;
    bool success = false;
    if (isSFSprite)
    {
        success = m_sprite.SaveSF(file.toStdWString(), errorMsg);
    }
    else
    {
        success = m_sprite.SaveBN(file.toStdWString(), errorMsg);
    }

    if (!success)
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
    }
    else
    {
        QMessageBox::information(this, "Export", "Sprite has been exported!", QMessageBox::Ok);
    }
}

void BNSpriteEditor::ReplacePaletteInSprite()
{
    vector<BNSprite::PaletteGroup> paletteGroups;
    for (PaletteGroup const& group : m_paletteGroups)
    {
        BNSprite::PaletteGroup groupCopy;
        for (Palette const& pal : group)
        {
            BNSprite::Palette palCopy;
            for (uint32_t i = 0; i < pal.size(); i++)
            {
                uint32_t const& rgb = pal[i];
                uint16_t col = BNSprite::RGBtoGBA(rgb);
                palCopy.m_colors.push_back(col);
            }
            groupCopy.m_palettes.push_back(palCopy);
        }
        paletteGroups.push_back(groupCopy);
    }
    m_sprite.ReplaceAllPaletteGroups(paletteGroups);
}

void BNSpriteEditor::on_actionClose_triggered()
{
    QApplication::quit();
}

void BNSpriteEditor::on_actionSimple_Mode_triggered()
{
    ui->actionSimple_Mode->setChecked(true);
    if (m_simpleMode) return;

    m_simpleMode = true;
    ui->actionAdvanced_Mode->setChecked(false);

    ui->GB_Object_Sub1->setHidden(true);
    ui->GB_Image->setHidden(true);
    ui->GB_Tileset->setHidden(true);
    ui->GB_SubAnim->setHidden(true);
    ui->Object_Tabs->setHidden(true);
    ui->Layout_Mode->setDirection(QBoxLayout::RightToLeft);

    QSize newSize(1099,672);
    this->setMaximumSize(1099,672);
    this->setMinimumSize(0,672);
    this->resize(newSize);
}

void BNSpriteEditor::on_actionAdvanced_Mode_triggered()
{
    ui->actionAdvanced_Mode->setChecked(true);
    if (!m_simpleMode) return;

    m_simpleMode = false;
    ui->actionSimple_Mode->setChecked(false);

    ui->GB_Object_Sub1->setHidden(false);
    ui->GB_Image->setHidden(false);
    ui->GB_Tileset->setHidden(false);
    ui->GB_SubAnim->setHidden(false);
    ui->Object_Tabs->setHidden(false);
    ui->Layout_Mode->setDirection(QBoxLayout::TopToBottom);

    QSize newSize(1155,857);
    this->setMaximumSize(16777215,16777215);
    this->setMinimumSize(0,0);
    this->resize(newSize);
}

void BNSpriteEditor::on_actionMerge_Sprite_triggered()
{
    if (!m_sprite.IsLoaded()) return;

    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString file = QFileDialog::getOpenFileName(this, tr("Merga Sprite"), path, IMPORT_EXTENSIONS);
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    // Load BN Sprite file
    string errorMsg;
    if (!m_spriteMerge.LoadBN(file.toStdWString(), errorMsg))
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
    }
    else
    {
        // Loaded 2nd sprite, attempt to merge
        int const animationCount = m_sprite.GetAnimationCount();
        if (m_sprite.Merge(m_spriteMerge, errorMsg))
        {
            // Import additional palettes
            int const paletteGroupCount = m_paletteGroups.size();
            vector<BNSprite::PaletteGroup> paletteGroups;
            m_sprite.GetAllPaletteGroups(paletteGroups);
            for (int i = paletteGroupCount; i < paletteGroups.size(); i++)
            {
                AddPaletteGroupFromSprite(paletteGroups[i]);
            }

            // Set new palette group limit
            ui->Palette_SB_Group->setMaximum(m_paletteGroups.size() - 1);

            // Generate additional thumbnails
            int const animationCountNew = m_sprite.GetAnimationCount();
            for (int i = animationCount; i < animationCountNew; i++)
            {
                AddAnimationThumbnail(i);
                qApp->processEvents();
            }

            QMessageBox::information(this, "Merge", "Sprite merge completed!", QMessageBox::Ok);
        }
        else
        {
            QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
        }
    }

    m_spriteMerge.Clear();
}

void BNSpriteEditor::on_actionExport_Sprite_as_Single_PNG_triggered()
{
    if (!m_sprite.IsLoaded()) return;

    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString name = ui->Sprite_Name->text();
    int index = name.lastIndexOf('.');
    name = name.mid(0, index);
    QString file = QFileDialog::getSaveFileName(this, tr("Export PNG"), path + "/" + name + ".png", "PNG (*.png)");
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString message = "Do you want to export with borders drawn?";
    resBtn = QMessageBox::question(this, "Export PNG", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    bool border = (resBtn == QMessageBox::Yes);

    int const borderSize = 3; // min 2
    QVector<QSize> animSizes;
    QVector<QPoint> animMins;
    QSize imageSize(0,0);
    for (int i = 0; i < ui->Anim_LW->count(); i++)
    {
        vector<BNSprite::Frame> frames;
        m_sprite.GetAnimationFrames(i, frames);

        int minX = -1;
        int maxX = 0;
        int minY = -1;
        int maxY = 0;

        // Get minimum size that fits all frames in each animation
        for (BNSprite::Frame const& frame : frames)
        {
            BNSprite::Object const& object = frame.m_objects[0];
            for (BNSprite::SubObject const& subObject : object.m_subObjects)
            {
                minX = qMin(minX, (int)subObject.m_posX);
                minY = qMin(minY, (int)subObject.m_posY);
                maxX = qMax(maxX, subObject.m_posX + subObject.m_sizeX - 1);
                maxY = qMax(maxY, subObject.m_posY + subObject.m_sizeY - 1);
            }
        }

        QSize animSize = QSize(maxX - minX + 1 + borderSize * 2, maxY - minY + 1 + borderSize * 2);

        // Add new animation height, get max width
        imageSize.rheight() += animSize.height();
        imageSize.setWidth(qMax(imageSize.width(), animSize.width() * (int)frames.size()));

        animSizes.push_back(animSize);
        animMins.push_back(QPoint(minX - borderSize, minY - borderSize));
    }

    QImage output(imageSize, QImage::Format_RGBA8888);
    output.fill(0);

    QPainter painter(&output);

    int currentY = 0;
    for (int i = 0; i < animSizes.size(); i++)
    {
        QSize const& animSize = animSizes[i];
        QPoint const& animMin = animMins[i];

        vector<BNSprite::Frame> frames;
        m_sprite.GetAnimationFrames(i, frames);

        for (int j = 0; j < frames.size(); j++)
        {
            BNSprite::Frame const& frame = frames[j];
            QImage image(animSize, QImage::Format_Indexed8);
            UpdateFrameImage(frame, &image, animMin.x(), animMin.y());
            painter.drawImage(animSize.width() * j, currentY, image);

            if (border)
            {
                painter.setPen(QColor(255,0,0));
                painter.drawRect(animSize.width() * j + 1, currentY + 1, animSize.width() - 3, animSize.height() - 3);

                painter.setPen(QColor(0,0,0));
                painter.drawLine(animSize.width() * j, currentY-1-animMin.y(), animSize.width() * j, currentY-animMin.y());
                painter.drawLine(animSize.width() * (j + 1) - 1, currentY-1-animMin.y(), animSize.width() * (j + 1) - 1, currentY-animMin.y());
                painter.drawLine(animSize.width() * j -1-animMin.x(), currentY, animSize.width() * j - animMin.x(), currentY);
                painter.drawLine(animSize.width() * j -1-animMin.x(), currentY + animSize.height() - 1, animSize.width() * j - animMin.x(), currentY + animSize.height() - 1);
            }
        }

        currentY += animSize.height();
    }

    if (output.save(file, "PNG"))
    {
        QMessageBox::information(this, "Export", "Export PNG successful!", QMessageBox::Ok);
    }
    else
    {
        QMessageBox::critical(this, "Error", "Export PNG failed!", QMessageBox::Ok);
    }
}

void BNSpriteEditor::on_actionExport_Sprite_as_Individual_PNG_triggered()
{
    if (!m_sprite.IsLoaded()) return;

    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), path, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir == Q_NULLPTR) return;
    m_path = dir;

    int frameCount = 0;
    for (int i = 0; i < ui->Anim_LW->count(); i++)
    {
        vector<BNSprite::Frame> frames;
        m_sprite.GetAnimationFrames(i, frames);

        int minX = 127;
        int maxX = -128;
        int minY = 127;
        int maxY = -128;

        // Get minimum size that fits all frames in each animation
        for (BNSprite::Frame const& frame : frames)
        {
            BNSprite::Object const& object = frame.m_objects[0];
            for (BNSprite::SubObject const& subObject : object.m_subObjects)
            {
                minX = qMin(minX, (int)subObject.m_posX);
                minY = qMin(minY, (int)subObject.m_posY);
                maxX = qMax(maxX, subObject.m_posX + subObject.m_sizeX - 1);
                maxY = qMax(maxY, subObject.m_posY + subObject.m_sizeY - 1);
            }
        }

        // Draw and export export each frame with the same size
        for (int j = 0; j < frames.size(); j++)
        {
            frameCount++;

            BNSprite::Frame const& frame = frames[j];
            QImage image(maxX - minX + 1, maxY - minY + 1, QImage::Format_Indexed8);
            UpdateFrameImage(frame, &image, minX, minY);

            if (!image.save(dir + "/" + QString::number(i) + "_" + QString::number(j) + ".png", "PNG"))
            {
                QMessageBox::critical(this, "Error", "Export PNG failed!", QMessageBox::Ok);
                return;
            }
        }
    }

    QMessageBox::information(this, "Export", QString::number(frameCount) + " PNG files exported!", QMessageBox::Ok);
}

void BNSpriteEditor::on_actionAbout_BNSpriteEditor_triggered()
{
    QString message = "BNSpriteEditor - Megaman Battle Network Sprite Creator/Editor " + c_programVersion;
    message += "\nCreated by brianuuu 2020";
    message += "\nYoutube: brianuuuSonic Reborn";
    message += "\n\nContributors:";
    message += "\nProf.9";
    QMessageBox::information(this, "About BNSpriteEditor", message, QMessageBox::Ok);
}

void BNSpriteEditor::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this, "About Qt");
}

void BNSpriteEditor::on_actionCustom_Sprite_Manager_triggered()
{
    if (m_sprite.IsLoaded())
    {
        QMessageBox::StandardButton resBtn = QMessageBox::Yes;
        QString message = "Using Custom Sprite Manager will close the current sprite.\nDo you wish to continue?";
        resBtn = QMessageBox::warning(this, "Custom Sprite Manager", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resBtn == QMessageBox::No)
        {
            return;
        }
    }

    // Clear everything we are editing
    ResetProgram();

    if (m_csm == Q_NULLPTR)
    {
        m_csm = new CustomSpriteManager();
        connect(m_csm, SIGNAL(BuildDataReady(int)), this, SLOT(on_CSM_BuildSprite_pressed(int)));
        connect(m_csm, SIGNAL(BuildCheckButton()), this, SLOT(on_CSM_BuildCheckButton_pressed()));
        connect(m_csm, SIGNAL(BuildPushFrame(int,bool)), this, SLOT(on_CSM_BuildPushFrame_pressed(int,bool)));
        connect(m_csm, SIGNAL(LoadProjectSignal(QString,uint32_t,qint64,int)), this, SLOT(on_CSM_LoadProject_pressed(QString,uint32_t,qint64,int)));
        connect(m_csm, SIGNAL(SaveProjectSignal(QString)), this, SLOT(on_CSM_SaveProject_pressed(QString)));
    }

    if (!m_csm->isVisible())
    {
        m_csm->SetDefaultPath(m_path);
    }

    m_csm->show();
    m_csm->raise();
}

void BNSpriteEditor::on_actionConvert_Sprite_to_be_Compatible_with_SF_triggered()
{
    if (!m_sprite.IsLoaded())
    {
        return;
    }

    if (IsCustomSpriteMakerActive())
    {
        QMessageBox::critical(this, "Error", "You should not use this while making custom sprites, set this in build option instead.", QMessageBox::Ok);
        return;
    }

    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString message = "Convert current sprite to be compatible to SF Sprite Format?";
    message += "\n*Combine all palette groups into one";
    message += "\n*Use only OAMs with even number tile start index";
    resBtn = QMessageBox::information(this, "Convert to SF", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    // Need to update palette in m_sprite first
    ReplacePaletteInSprite();

    bool modified = false;
    string errorMsg;
    if (m_sprite.ConvertBNtoSF(modified, errorMsg))
    {
        if (modified)
        {
            ResetProgram(false);
            LoadSpriteToUI();
            QMessageBox::information(this, "Convert to SF", "Conversion completed!", QMessageBox::Ok);
        }
        else
        {
            QMessageBox::information(this, "Convert to SF", "Sprite is already compatible with SF.", QMessageBox::Ok);
        }
    }
    else
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
    }
}

//---------------------------------------------------------------------------
// Resetting all buttons and data
//---------------------------------------------------------------------------
void BNSpriteEditor::ResetProgram(bool clearSprite)
{
    // Focus on something else otherwise default focus on Anim_LW/Frame_LW messes things up???
    ui->Preview_GV->setFocus();

    m_copyAnim = -1;
    m_copyFrame = -1;

    // Stop animation playing
    if (m_playingAnimation)
    {
        on_Frame_PB_Play_clicked();
    }
    if (m_playingSubAnimation)
    {
        on_SubFrame_PB_Play_clicked();
    }

    m_paletteContextMenu->reset();

    ResetOthers();
    ResetFrame();
    ResetAnim();

    if (clearSprite)
    {
        ui->Sprite_Name->setText("");
        m_sprite.Clear();
    }
}

void BNSpriteEditor::ResetAnim()
{
    for (QImage* image : m_animThumbnails)
    {
        delete image;
    }
    m_animThumbnails.clear();
    ui->Anim_LW->verticalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
    ui->Anim_LW->clear();

    ui->Anim_PB_Up->setEnabled(false);
    ui->Anim_PB_Down->setEnabled(false);
    ui->Anim_PB_New->setEnabled(false);
    ui->Anim_PB_Dup->setEnabled(false);
    ui->Anim_PB_Del->setEnabled(false);
}

void BNSpriteEditor::ResetFrame()
{
    ui->Palette_GV->clear();

    for (QImage* image : m_frameThumbnails)
    {
        delete image;
    }
    m_frameThumbnails.clear();
    ui->Frame_LW->horizontalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
    ui->Frame_LW->clear();

    ui->Frame_SB_Delay->setEnabled(false);
    ui->Frame_CB_Flag0->setEnabled(false);
    ui->Frame_CB_Flag1->setEnabled(false);
    ui->Frame_CB_Loop->setEnabled(false);
    ui->Frame_PB_Play->setEnabled(false);
    ui->Frame_PB_ShiftL->setEnabled(false);
    ui->Frame_PB_ShiftR->setEnabled(false);
    ui->Frame_PB_New->setEnabled(false);
    ui->Frame_PB_Copy->setEnabled(false);
    ui->Frame_PB_Paste->setEnabled(false);
    ui->Frame_PB_Del->setEnabled(false);

    m_frame = BNSprite::Frame();
}

void BNSpriteEditor::ResetOthers()
{
    ui->Image_PB_HFlip->setEnabled(false);
    ui->Image_PB_VFlip->setEnabled(false);
    ui->Image_PB_Up->setEnabled(false);
    ui->Image_PB_Down->setEnabled(false);
    ui->Image_PB_Left->setEnabled(false);
    ui->Image_PB_Right->setEnabled(false);

    ui->Palette_SB_Group->setEnabled(false);
    ui->Palette_SB_Index->setEnabled(false);
    ui->Palette_PB_Up->setEnabled(false);
    ui->Palette_PB_Down->setEnabled(false);
    ui->Palette_PB_Import->setEnabled(false);
    ui->Palette_PB_Export->setEnabled(false);
    ui->Palette_Warning->setHidden(true);

    ui->Object_Tabs->setEnabled(false);
    ui->Object_Tabs->blockSignals(true);
    while (ui->Object_Tabs->count() > 1)
    {
        delete ui->Object_Tabs->widget(ui->Object_Tabs->count() - 1);
    }
    ui->Object_Tabs->setCurrentIndex(0);
    ui->Object_Tabs->blockSignals(false);

    ui->Object_PB_New->setEnabled(false);
    ui->Object_PB_Dup->setEnabled(false);
    ui->Object_PB_Del->setEnabled(false);

    ResetOAM();

    m_tilesetGraphic->clear();
    m_tilesetGraphic->setSceneRect(0, 0, 0, 0);
    m_tilesetData.clear();
    ui->Tileset_Label->setText("Total No. of tiles: ---");
    ui->Tileset_SB_Index->setEnabled(false);
    ui->Tileset_PB_Import->setEnabled(false);
    ui->Tileset_PB_Export->setEnabled(false);

    ui->SubAnim_Tabs->setEnabled(false);
    ui->SubAnim_Tabs->blockSignals(true);
    while (ui->SubAnim_Tabs->count() > 1)
    {
        delete ui->SubAnim_Tabs->widget(ui->SubAnim_Tabs->count() - 1);
    }
    ui->SubAnim_Tabs->setCurrentIndex(0);
    ui->SubAnim_Tabs->blockSignals(false);

    ui->SubAnim_PB_New->setEnabled(false);
    ui->SubAnim_PB_Dup->setEnabled(false);
    ui->SubAnim_PB_Del->setEnabled(false);

    ResetSubFrame();
}

void BNSpriteEditor::ResetOAM()
{
    for(QGraphicsItem* item : m_previewOAMs)
    {
        m_previewGraphic->removeItem(item);
        delete item;
    }
    m_previewOAMs.clear();
    m_previewHighlight->setVisible(false);
    m_highlightTimer->stop();

    m_oamGraphic->clear();
    m_oamGraphic->setSceneRect(0, 0, 0, 0);
    ui->OAM_TW->clear();
    ui->OAM_PB_Up->setEnabled(false);
    ui->OAM_PB_Down->setEnabled(false);
    ui->OAM_SB_Tile->setEnabled(false);
    ui->OAM_CB_Size->setEnabled(false);
    ui->OAM_SB_XPos->setEnabled(false);
    ui->OAM_SB_YPos->setEnabled(false);
    ui->OAM_CB_HFlip->setEnabled(false);
    ui->OAM_CB_VFlip->setEnabled(false);
    ui->OAM_PB_New->setEnabled(false);
    ui->OAM_PB_Dup->setEnabled(false);
    ui->OAM_PB_Del->setEnabled(false);
}

void BNSpriteEditor::ResetSubFrame()
{
    ui->SubFrame_LW->horizontalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
    ui->SubFrame_LW->clear();
    ui->SubFrame_PB_Play->setEnabled(false);
    ui->SubFrame_CB_Loop->setEnabled(false);
    ui->SubFrame_PB_Add->setEnabled(false);
    ui->SubFrame_PB_Del->setEnabled(false);
    ui->SubFrame_PB_ShiftL->setEnabled(false);
    ui->SubFrame_PB_ShiftR->setEnabled(false);
    ui->SubFrame_SB_Index->setEnabled(false);
    ui->SubFrame_SB_Delay->setEnabled(false);
}

//---------------------------------------------------------------------------
// Get image from frame
//---------------------------------------------------------------------------
QImage *BNSpriteEditor::GetFrameImage(const BNSprite::Frame &_frame, bool _isThumbnail, int _subAnimID, int _subFrameID)
{
    int minX = -128;
    int maxX = 127;
    int minY = -128;
    int maxY = 127;

    int width = maxX - minX + 1;
    int height = maxY - minY + 1;

    if (_isThumbnail)
    {
        minX = 127;
        maxX = -128;
        minY = 127;
        maxY = -128;

        // Go through all objects to get minimum image size
        uint8_t objectIndex = _frame.m_subAnimations[_subAnimID].m_subFrames[_subFrameID].m_objectIndex;
        BNSprite::Object const& object = _frame.m_objects[objectIndex];
        for (BNSprite::SubObject const& subObject : object.m_subObjects)
        {
            minX = qMin(minX, (int)subObject.m_posX);
            minY = qMin(minY, (int)subObject.m_posY);
            maxX = qMax(maxX, subObject.m_posX + subObject.m_sizeX - 1);
            maxY = qMax(maxY, subObject.m_posY + subObject.m_sizeY - 1);
        }

        width = maxX - minX + 1;
        height = maxY - minY + 1;

        // Make this a square image, if larger than icon size, scale it down
        int longerSide = qMax(width, height);
        int iconSize = ui->Anim_LW->iconSize().width();
        if (longerSide <= iconSize)
        {
            minX = minX - (iconSize - width) / 2;
            minY = minY - (iconSize - height) / 2;
        }
        else
        {
            minX = minX - (longerSide - width) / 2;
            minY = minY - (longerSide - height) / 2;
        }

        width = qMax(longerSide, iconSize);
        height = qMax(longerSide, iconSize);
    }

    // Initialize image
    QImage* image = new QImage(width, height, QImage::Format_Indexed8);
    UpdateFrameImage(_frame, image, minX, minY, _subAnimID, _subFrameID);
    return image;
}

void BNSpriteEditor::UpdateFrameImage(const BNSprite::Frame &_frame, QImage *_image, int32_t _minX, int32_t _minY, int _subAnimID, int _subFrameID)
{
    // Frame MUST have at least one sub animation with one sub frame
    uint8_t objectIndex = _frame.m_subAnimations[_subAnimID].m_subFrames[_subFrameID].m_objectIndex;
    BNSprite::Object const& object = _frame.m_objects[objectIndex];
    int group = qMin((int)_frame.m_paletteGroupID, m_paletteGroups.size() - 1);
    PaletteGroup const& paletteGroup = m_paletteGroups[group];
    int index = qMin((int)object.m_paletteIndex, paletteGroup.size() - 1);
    Palette const& palette = paletteGroup[index];
    _image->setColorTable(palette);
    _image->fill(0);

    // Draw image
    vector<uint8_t> data;
    m_sprite.GetTilesetPixels(_frame.m_tilesetID, data);
    for (BNSprite::SubObject const& subObject : object.m_subObjects)
    {
        DrawOAMInImage(subObject, _image, _minX, _minY, data, false);
    }
}

void BNSpriteEditor::DrawOAMInImage(const BNSprite::SubObject &_subObject, QImage *_image, int32_t _minX, int32_t _minY, const vector<uint8_t> &_data, bool _drawFirstColor)
{
    int startTile = _subObject.m_startTile;
    int tileXCount = _subObject.m_sizeX / 8;
    int tileYCount = _subObject.m_sizeY / 8;
    int xPos = _subObject.m_posX - _minX;
    int yPos = _subObject.m_posY - _minY;
    int pixelIndex = startTile * 64;

    for (int tileY = 0; tileY < tileYCount; tileY++)
    {
        for (int tileX = 0; tileX < tileXCount; tileX++)
        {
            for (int y = tileY * 8; y < tileY * 8 + 8; y++)
            {
                for (int x = tileX * 8; x < tileX * 8 + 8; x++)
                {
                    if (pixelIndex < _data.size())
                    {
                        uint8_t index = _data[pixelIndex];
                        if (index != 0 || _drawFirstColor)
                        {
                            int drawX = _subObject.m_hFlip ? xPos + _subObject.m_sizeX - 1 - x : xPos + x;
                            int drawY = _subObject.m_vFlip ? yPos + _subObject.m_sizeY - 1 - y : yPos + y;
                            if (drawX >= 0 && drawX < _image->width() && drawY >= 0 && drawY < _image->height())
                            {
                                _image->setPixel(drawX, drawY, index);
                            }
                        }
                        pixelIndex++;
                    }
                    else
                    {
                        return;
                    }
                }
            }
        }
    }
}



//---------------------------------------------------------------------------
// Animation signals
//---------------------------------------------------------------------------
void BNSpriteEditor::on_Anim_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    // Stop animation playing
    if (m_playingAnimation)
    {
        on_Frame_PB_Play_clicked();
    }
    if (m_playingSubAnimation)
    {
        on_SubFrame_PB_Play_clicked();
    }

    int animID = ui->Anim_LW->row(current);
    if (animID < 0) return;

    qDebug() << "Animation" << animID << "selected";

    ResetOthers();
    ResetFrame();

    int frameCount = m_sprite.GetAnimationFrameCount(animID);
    for (int i = 0; i < frameCount; i++)
    {
        AddFrameThumbnail(animID, i);
    }

    // Highlight
    if (previous)
    {
        previous->setForeground(QColor(0,0,0));
    }
    current->setSelected(true);
    current->setForeground(QColor(255,0,0));

    // Set animation buttons
    int count = ui->Anim_LW->count();
    ui->Anim_PB_Up->setEnabled(animID > 0);
    ui->Anim_PB_Down->setEnabled(animID < count - 1);
    ui->Anim_PB_New->setEnabled(count < 255);
    ui->Anim_PB_Dup->setEnabled(animID >= 0);
    ui->Anim_PB_Del->setEnabled(animID >= 0 && count > 1);

    // Set frame buttons
    ui->Frame_CB_Loop->setChecked(m_sprite.GetAnimationLoop(animID));
    ui->Frame_CB_Loop->setEnabled(true);
    ui->Frame_PB_Play->setEnabled(ui->Frame_LW->count() > 1);
    ui->Frame_PB_New->setEnabled(true);
    ui->Frame_PB_Paste->setEnabled(m_copyAnim != -1 || m_copyFrame != -1);

    on_CSM_BuildCheckButton_pressed();
}

void BNSpriteEditor::on_Anim_PB_Up_clicked()
{
    int animID = ui->Anim_LW->currentRow();
    SwapAnimation(animID, animID - 1);
}

void BNSpriteEditor::on_Anim_PB_Down_clicked()
{
    int animID = ui->Anim_LW->currentRow();
    SwapAnimation(animID, animID + 1);
}

void BNSpriteEditor::on_Anim_PB_New_clicked()
{
    int animID = m_sprite.NewAnimation();
    AddAnimationThumbnail(animID);
    ui->Anim_LW->setCurrentRow(animID);
}

void BNSpriteEditor::on_Anim_PB_Dup_clicked()
{
    int animID = m_sprite.NewAnimation(ui->Anim_LW->currentRow());
    AddAnimationThumbnail(animID);
    ui->Anim_LW->setCurrentRow(animID);
}

void BNSpriteEditor::on_Anim_PB_Del_clicked()
{
    // Stop animation playing
    if (m_playingAnimation)
    {
        on_Frame_PB_Play_clicked();
    }
    if (m_playingSubAnimation)
    {
        on_SubFrame_PB_Play_clicked();
    }

    int animID = ui->Anim_LW->currentRow();

    m_sprite.DeleteAnimation(animID);

    // Clear thumbnail
    delete m_animThumbnails[animID];
    m_animThumbnails.remove(animID);

    // We manually call item changed here, because takeItem() calls it but it's not deleted yet
    ui->Anim_LW->blockSignals(true);
    delete ui->Anim_LW->takeItem(animID);
    on_Anim_LW_currentItemChanged(ui->Anim_LW->item(animID == m_animThumbnails.size() ? animID - 1 : animID), Q_NULLPTR);
    ui->Anim_LW->blockSignals(false);

    // Fix the aniamtion numbers
    for (int i = 0; i < ui->Anim_LW->count(); i++)
    {
        QListWidgetItem* item = ui->Anim_LW->item(i);
        item->setText("Animation " + QString::number(i));
    }

    // If copied frame is/larger (they have shifted) than deleted animation, disable paste
    if (m_copyAnim >= animID)
    {
        m_copyAnim = -1;
        m_copyFrame = -1;
        ui->Frame_PB_Paste->setEnabled(false);
    }
}

//---------------------------------------------------------------------------
// Create thumbnail from animation to list view
//---------------------------------------------------------------------------
void BNSpriteEditor::AddAnimationThumbnail(int _animID)
{
    BNSprite::Frame frame = m_sprite.GetAnimationFrame(_animID, 0);
    QImage* image = GetFrameImage(frame, true);
    m_animThumbnails.push_back(image);
    QListWidgetItem* item = new QListWidgetItem(QIcon(QPixmap::fromImage(*image)), "Animation " + QString::number(_animID));
    ui->Anim_LW->addItem(item);
}

//---------------------------------------------------------------------------
// Update thumbnail for animation to list view
//---------------------------------------------------------------------------
void BNSpriteEditor::UpdateAnimationThumnail(int _animID)
{
    delete m_animThumbnails[_animID];
    BNSprite::Frame const frame = m_sprite.GetAnimationFrame(_animID, 0);
    QImage* image = GetFrameImage(frame, true);
    m_animThumbnails[_animID] = image;
    QListWidgetItem* item = ui->Anim_LW->item(_animID);
    item->setIcon(QIcon(QPixmap::fromImage(*image)));
}

//---------------------------------------------------------------------------
// Swap two animations
//---------------------------------------------------------------------------
void BNSpriteEditor::SwapAnimation(int _oldID, int _newID)
{
    // Swap in actual sprite
    m_sprite.SwapAnimations(_oldID, _newID);

    // Simply swap the two images
    qSwap(m_animThumbnails[_oldID], m_animThumbnails[_newID]);
    QListWidgetItem* item0 = ui->Anim_LW->item(_newID);
    item0->setIcon(QIcon(QPixmap::fromImage(*m_animThumbnails[_newID])));
    item0->setForeground(QColor(255,0,0));
    QListWidgetItem* item1 = ui->Anim_LW->item(_oldID);
    item1->setIcon(QIcon(QPixmap::fromImage(*m_animThumbnails[_oldID])));
    item1->setForeground(QColor(0,0,0));

    // Block signal to prevent loading animations again
    ui->Anim_LW->blockSignals(true);
    ui->Anim_LW->setCurrentRow(_newID);
    ui->Anim_LW->blockSignals(false);

    // Update button
    ui->Anim_PB_Up->setEnabled(_newID > 0);
    ui->Anim_PB_Down->setEnabled(_newID < ui->Anim_LW->count() - 1);

    // If copied frame is in one of these animation, disable it
    if (m_copyAnim == _oldID || m_copyAnim == _newID)
    {
        m_copyAnim = -1;
        m_copyFrame = -1;
        ui->Frame_PB_Paste->setEnabled(false);
    }
}

//---------------------------------------------------------------------------
// Frame signals
//---------------------------------------------------------------------------
void BNSpriteEditor::on_Frame_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    // Stop animation
    if (m_playingSubAnimation)
    {
        on_SubFrame_PB_Play_clicked();
    }

    int animID = ui->Anim_LW->currentRow();
    if (animID < 0) return;

    int frameID = ui->Frame_LW->row(current);
    if (frameID < 0) return;

    qDebug() << "Frame" << frameID << "selected";

    ResetOthers();

    // Highlight
    if (previous)
    {
        previous->setForeground(QColor(0,0,0));
    }
    current->setSelected(true);
    current->setForeground(QColor(255,0,0));

    // Enable editing
    m_frame = m_sprite.GetAnimationFrame(animID, frameID);

    // Palette
    int maximum = m_paletteGroups[m_frame.m_paletteGroupID].size() - 1;
    ui->Palette_SB_Group->setValue(m_frame.m_paletteGroupID);
    ui->Palette_SB_Group->setEnabled(true);
    ui->Palette_SB_Index->setMaximum(qMin(maximum, 255));
    ui->Palette_SB_Index->setEnabled(true);
    ui->Palette_PB_Import->setEnabled(!IsCustomSpriteMakerActive());
    ui->Palette_PB_Export->setEnabled(true);

    // Tileset
    ui->Tileset_SB_Index->setMaximum(m_sprite.GetTilesetCount() - 1);
    ui->Tileset_SB_Index->setValue(m_frame.m_tilesetID);
    ui->Tileset_SB_Index->setEnabled(true);
    ui->Tileset_PB_Import->setEnabled(!IsCustomSpriteMakerActive());
    ui->Tileset_PB_Export->setEnabled(true);
    CacheTileset();

    // Object (palette index is set when loading object)
    uint8_t objectIndex = m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex;
    int objectCount = m_frame.m_objects.size();
    ui->Object_Tabs->blockSignals(true);
    for (int i = 1; i < objectCount; i++)
    {
        QWidget* widget = new QWidget();
        ui->Object_Tabs->addTab(widget, "Object " + QString::number(i));
    }
    ui->Object_Tabs->setCurrentIndex(objectIndex);
    ui->Object_Tabs->setEnabled(true);
    ui->Object_Tabs->blockSignals(false);
    on_Object_Tabs_currentChanged(objectIndex);// <----Now we loaded palette, draw tileset
    ui->Object_PB_New->setEnabled(objectCount < 255);
    ui->Object_PB_Dup->setEnabled(objectCount < 255);
    ui->Object_PB_Del->setEnabled(objectCount > 1);

    // Frame
    ui->Frame_SB_Delay->setValue(m_frame.m_delay);
    ui->Frame_SB_Delay->setEnabled(true);
    ui->Frame_CB_Flag0->setChecked(m_frame.m_specialFlag0);
    ui->Frame_CB_Flag0->setEnabled(true);
    ui->Frame_CB_Flag1->setChecked(m_frame.m_specialFlag1);
    ui->Frame_CB_Flag1->setEnabled(true);
    int count = ui->Frame_LW->count();
    ui->Frame_PB_ShiftL->setEnabled(frameID > 0);
    ui->Frame_PB_ShiftR->setEnabled(frameID < count - 1);
    ui->Frame_PB_New->setEnabled(count < 255);
    ui->Frame_PB_Copy->setEnabled(frameID >= 0);
    // Paste is set when selecting animation
    ui->Frame_PB_Play->setEnabled(count > 1);
    ui->Frame_PB_Del->setEnabled(frameID >= 0 && count > 1);

    // Sub animation
    int subAnimCount = m_frame.m_subAnimations.size();
    ui->SubAnim_Tabs->blockSignals(true);
    for (int i = 1; i < subAnimCount; i++)
    {
        QWidget* widget = new QWidget();
        ui->SubAnim_Tabs->addTab(widget, "SubAnim " + QString::number(i));
    }
    ui->SubAnim_Tabs->setCurrentIndex(0);
    ui->SubAnim_Tabs->setEnabled(true);
    ui->SubAnim_Tabs->blockSignals(false);
    on_SubAnim_Tabs_currentChanged(0);
    ui->SubAnim_PB_New->setEnabled(subAnimCount < 255);
    ui->SubAnim_PB_Dup->setEnabled(subAnimCount < 255);
    ui->SubAnim_PB_Del->setEnabled(subAnimCount > 1);
}

void BNSpriteEditor::on_Frame_CB_Loop_toggled(bool checked)
{
    int animID = ui->Anim_LW->currentRow();
    if (m_sprite.GetAnimationLoop(animID) != checked)
    {
        m_sprite.SetAnimationLoop(animID, checked);
    }
}

void BNSpriteEditor::on_Frame_PB_Play_clicked()
{
    if (ui->Frame_LW->count() <= 1 && !m_playingAnimation) return;
    m_playingAnimation = !m_playingAnimation;

    ui->GB_Image->setEnabled(!m_playingAnimation);
    ui->GB_Object->setEnabled(!m_playingAnimation);
    ui->GB_Palette->setEnabled(!m_playingAnimation);
    ui->GB_SubAnim->setEnabled(!m_playingAnimation);
    ui->GB_Tileset->setEnabled(!m_playingAnimation);
    ui->GB_Frame_Sub1->setEnabled(!m_playingAnimation);
    ui->GB_Frame_Sub3->setEnabled(!m_playingAnimation);

    if (m_playingAnimation)
    {
        ui->Frame_LW->setCurrentRow(0);
        m_animationTimer->start(m_frame.m_delay * 16);
        ui->Frame_PB_Play->setText("Stop Playing");
    }
    else
    {
        m_animationTimer->stop();
        ui->Frame_PB_Play->setText("Play Animation");
    }
}

void BNSpriteEditor::on_Frame_Play_timeout()
{
    int currentRow = ui->Frame_LW->currentRow();
    int count = ui->Frame_LW->count();
    if (count <= 1)
    {
        on_Frame_PB_Play_clicked();
        return;
    }

    if (currentRow == count - 1)
    {
        ui->Frame_LW->setCurrentRow(0);
        m_animationTimer->start(m_frame.m_delay * 16);
    }
    else
    {
        ui->Frame_LW->setCurrentRow(currentRow + 1);
        if (currentRow + 1 == count - 1 && !ui->Frame_CB_Loop->isChecked())
        {
            // Non-loop animation, stop animation
            on_Frame_PB_Play_clicked();
        }
        else
        {
            m_animationTimer->start(m_frame.m_delay * 16);
        }
    }
}

void BNSpriteEditor::on_Frame_PB_ShiftL_clicked()
{
    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    SwapFrame(animID, frameID, frameID - 1, frameID - 1 == 0);
}

void BNSpriteEditor::on_Frame_PB_ShiftR_clicked()
{
    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    SwapFrame(animID, frameID, frameID + 1, frameID == 0);
}

void BNSpriteEditor::on_Frame_PB_New_clicked()
{
    int animID = ui->Anim_LW->currentRow();
    int frameID = m_sprite.NewFrame(animID);
    AddFrameThumbnail(animID, frameID);
    ui->Frame_LW->setCurrentRow(frameID);
}

void BNSpriteEditor::on_Frame_PB_Copy_clicked()
{
    m_copyAnim = ui->Anim_LW->currentRow();
    m_copyFrame = ui->Frame_LW->currentRow();

    if (m_copyAnim != -1 && m_copyFrame != -1)
    {
        ui->Frame_PB_Paste->setEnabled(true);
    }
}

void BNSpriteEditor::on_Frame_PB_Paste_clicked()
{
    int animID = ui->Anim_LW->currentRow();
    int frameID = m_sprite.NewFrame(animID, m_copyAnim, m_copyFrame);

    if (frameID == -1)
    {
        qDebug() << "Paste frame failed!";
        return;
    }

    qDebug() << "Paste frame from Anim" << m_copyAnim << "Frame" << m_copyFrame;
    AddFrameThumbnail(animID, frameID);
    ui->Frame_LW->setCurrentRow(frameID);
}

void BNSpriteEditor::on_Frame_PB_Del_clicked()
{
    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.DeleteFrame(animID, frameID);

    // Clear thumbnail
    delete m_frameThumbnails[frameID];
    m_frameThumbnails.remove(frameID);

    // We manually call item changed here, because takeItem() calls it but it's not deleted yet
    ui->Frame_LW->blockSignals(true);
    delete ui->Frame_LW->takeItem(frameID);
    on_Frame_LW_currentItemChanged(ui->Frame_LW->item(frameID == m_frameThumbnails.size() ? frameID - 1 : frameID), Q_NULLPTR);
    ui->Frame_LW->blockSignals(false);

    // Fix the frame numbers
    for (int i = 0; i < ui->Frame_LW->count(); i++)
    {
        QListWidgetItem* item = ui->Frame_LW->item(i);
        item->setText("Frame " + QString::number(i));
    }

    // Update anim thumbnail if frameID is 0
    if (frameID == 0)
    {
        UpdateAnimationThumnail(animID);
    }

    // If deleted the copied frame or frames before it, disable paste
    if (m_copyAnim == animID && m_copyFrame >= frameID)
    {
        m_copyAnim = -1;
        m_copyFrame = -1;
        ui->Frame_PB_Paste->setEnabled(false);
    }
}

void BNSpriteEditor::on_Frame_SB_Delay_valueChanged(int arg1)
{
    if (m_frame.m_delay == arg1 || !ui->Frame_SB_Delay->isEnabled()) return;

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();

    m_frame.m_delay = arg1;
    m_sprite.ReplaceFrame(animID, frameID, m_frame);
}

void BNSpriteEditor::on_Frame_CB_Flag0_toggled(bool checked)
{
    if (m_frame.m_specialFlag0 == checked || !ui->Frame_CB_Flag0->isEnabled()) return;

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();

    m_frame.m_specialFlag0 = checked;
    m_sprite.ReplaceFrame(animID, frameID, m_frame);
}

void BNSpriteEditor::on_Frame_CB_Flag1_toggled(bool checked)
{
    if (m_frame.m_specialFlag1 == checked || !ui->Frame_CB_Flag1->isEnabled()) return;

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();

    m_frame.m_specialFlag1 = checked;
    m_sprite.ReplaceFrame(animID, frameID, m_frame);
}

//---------------------------------------------------------------------------
// Create thumbnail from frame to list view
//---------------------------------------------------------------------------
void BNSpriteEditor::AddFrameThumbnail(int _animID, int _frameID)
{
    BNSprite::Frame const& frame = m_sprite.GetAnimationFrame(_animID, _frameID);
    QImage* image = GetFrameImage(frame, true);
    m_frameThumbnails.push_back(image);
    QListWidgetItem* item = new QListWidgetItem(QIcon(QPixmap::fromImage(*image)), "Frame " + QString::number(_frameID));
    ui->Frame_LW->addItem(item);
}

//---------------------------------------------------------------------------
// Update thumbnail for frame to list view
//---------------------------------------------------------------------------
void BNSpriteEditor::UpdateFrameThumbnail(int _frameID)
{
    int animID = ui->Anim_LW->currentRow();
    delete m_frameThumbnails[_frameID];
    QImage* image = GetFrameImage(m_sprite.GetAnimationFrame(animID, _frameID), true);
    m_frameThumbnails[_frameID] = image;
    QListWidgetItem* item = ui->Frame_LW->item(_frameID);
    item->setIcon(QIcon(QPixmap::fromImage(*image)));

    if (_frameID == 0)
    {
        UpdateAnimationThumnail(ui->Anim_LW->currentRow());
    }
}

//---------------------------------------------------------------------------
// Swap two frames
//---------------------------------------------------------------------------
void BNSpriteEditor::SwapFrame(int _animID, int _oldID, int _newID, bool _updateAnimThumbnail)
{
    // Swap in actual frame
    m_sprite.SwapFrames(_animID, _oldID, _newID);

    // Simply swap the two images
    qSwap(m_frameThumbnails[_oldID], m_frameThumbnails[_newID]);
    QListWidgetItem* item0 = ui->Frame_LW->item(_newID);
    item0->setIcon(QIcon(QPixmap::fromImage(*m_frameThumbnails[_newID])));
    item0->setForeground(QColor(255,0,0));
    QListWidgetItem* item1 = ui->Frame_LW->item(_oldID);
    item1->setIcon(QIcon(QPixmap::fromImage(*m_frameThumbnails[_oldID])));
    item1->setForeground(QColor(0,0,0));

    // Block signal to prevent loading frame again
    ui->Frame_LW->blockSignals(true);
    ui->Frame_LW->setCurrentRow(_newID);
    ui->Frame_LW->blockSignals(false);

    // Update button
    ui->Frame_PB_ShiftL->setEnabled(_newID > 0);
    ui->Frame_PB_ShiftR->setEnabled(_newID < ui->Frame_LW->count() - 1);

    // Update animation thumbnail if first frame is changed
    if (_updateAnimThumbnail)
    {
        UpdateAnimationThumnail(_animID);
    }

    // If swapped the copied frame, disable paste
    if (m_copyAnim == _animID && (m_copyFrame == _oldID || m_copyFrame == _newID))
    {
        m_copyAnim = -1;
        m_copyFrame = -1;
        ui->Frame_PB_Paste->setEnabled(false);
    }
}

//---------------------------------------------------------------------------
// Tileset signals
//---------------------------------------------------------------------------
void BNSpriteEditor::on_Tileset_SB_Index_valueChanged(int arg1)
{
    if (m_frame.m_tilesetID == arg1 || !ui->Tileset_SB_Index->isEnabled()) return;

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();

    m_frame.m_tilesetID = arg1;
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    CacheTileset();
    UpdateDrawTileset();
    UpdateFrameThumbnail(frameID);

    // Update preview
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object const& object = m_frame.m_objects[objectID];
    for (int i = 0; i < m_previewOAMs.size(); i++)
    {
        UpdatePreviewOAM(i, object.m_subObjects[i], true);
    }
    for (int i = 0; i < ui->SubFrame_LW->count(); i++)
    {
        UpdateSubFrameThumbnail(i);
    }
}

void BNSpriteEditor::on_Tileset_PB_Import_clicked()
{
    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString message = "Do you wish to import tileset? All frame that uses this tileset will be affected.";
    resBtn = QMessageBox::warning(this, "Import Tileset", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString file = QFileDialog::getOpenFileName(this, tr("Import Tileset"), path, "Tileset Dump (*.bin *.dmp);;All files (*.*)");
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    // Load BN Sprite file
    string errorMsg;
    if (!m_sprite.ImportTileset(ui->Tileset_SB_Index->value(), file.toStdWString(), errorMsg))
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
    }
    else
    {
        CacheTileset();
        UpdateDrawTileset();

        // Update all anim and frame thumbnail
        int animID = ui->Anim_LW->currentRow();
        int frameCount = m_sprite.GetAnimationFrameCount(animID);
        for (int i = 0; i < frameCount; i++)
        {
            UpdateFrameThumbnail(i);
        }

        // We don't need to update current anim thumbnail, it is already updated above
        int animCount = m_sprite.GetAnimationCount();
        for (int i = 0; i < animCount; i++)
        {
            if (i == animID) continue;
            UpdateAnimationThumnail(i);
        }

        for (int i = 0; i < ui->SubFrame_LW->count(); i++)
        {
            UpdateSubFrameThumbnail(i);
        }

        // Reload Object
        on_Object_Tabs_currentChanged(ui->Object_Tabs->currentIndex());

        QMessageBox::information(this, "Import", "Tileset imported!", QMessageBox::Ok);
    }
}

void BNSpriteEditor::on_Tileset_PB_Export_clicked()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString file = QFileDialog::getSaveFileName(this, tr("Export Tileset"), path, "Tileset Dump (*.bin *.dmp);;All files (*.*)");
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    // Load BN Sprite file
    string errorMsg;
    if (!m_sprite.ExportTileset(ui->Tileset_SB_Index->value(), file.toStdWString(), errorMsg))
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
    }
    else
    {
        QMessageBox::information(this, "Export", "Tileset exported!", QMessageBox::Ok);
    }
}

//---------------------------------------------------------------------------
// Cache current tileset
//---------------------------------------------------------------------------
void BNSpriteEditor::CacheTileset()
{
    if (m_tilesetImage != Q_NULLPTR)
    {
        delete m_tilesetImage;
        m_tilesetImage = Q_NULLPTR;
    }

    m_tilesetData.clear();
    m_sprite.GetTilesetPixels(ui->Tileset_SB_Index->value(), m_tilesetData);

    Q_ASSERT(!m_tilesetData.empty());

    int tileCount = m_tilesetData.size() / 64;
    ui->Tileset_Label->setText("Total No. of tiles: " + QString::number(tileCount));
}

//---------------------------------------------------------------------------
// Draw current tileset
//---------------------------------------------------------------------------
void BNSpriteEditor::UpdateDrawTileset()
{
    // We have to cache tileset first!
    Q_ASSERT(!m_tilesetData.empty());

    int tileCount = m_tilesetData.size() / 64;
    ui->Tileset_Label->setText("Total No. of tiles: " + QString::number(tileCount));

    int tileXCount = qMin(tileCount, 8);
    int tileYCount = tileCount / 8 + (tileCount % 8 > 0 ? 1 : 0);

    m_tilesetImage = new QImage(tileXCount * 8, tileYCount * 8, QImage::Format_Indexed8);
    int group = qMin(ui->Palette_SB_Group->value(), m_paletteGroups.size() - 1);
    PaletteGroup const& paletteGroup = m_paletteGroups[group];
    int index = qMin(ui->Palette_SB_Index->value(), paletteGroup.size() - 1);
    Palette palette = paletteGroup[index];
    palette[0] |= 0xFF000000; // undo transparency on first color
    m_tilesetImage->setColorTable(palette);
    m_tilesetImage->fill(0);

    BNSprite::SubObject dummyOAM;
    dummyOAM.m_sizeX = tileXCount * 8;
    dummyOAM.m_sizeY = tileYCount * 8;
    dummyOAM.m_posX = 0;
    dummyOAM.m_posY = 0;
    DrawOAMInImage(dummyOAM, m_tilesetImage, 0, 0, m_tilesetData, true);

    m_tilesetGraphic->clear();
    m_tilesetGraphic->setSceneRect(0, 0, m_tilesetImage->width(), m_tilesetImage->height());
    m_tilesetGraphic->addPixmap(QPixmap::fromImage(*m_tilesetImage));
}

//---------------------------------------------------------------------------
// Sub animation signals
//---------------------------------------------------------------------------
void BNSpriteEditor::on_SubAnim_Tabs_currentChanged(int index)
{
    ResetSubFrame();

    BNSprite::SubAnimation const& subAnim = m_frame.m_subAnimations[index];
    for (int i = 0; i < subAnim.m_subFrames.size(); i++)
    {
        AddSubFrame(i);
    }

    ui->SubFrame_CB_Loop->setChecked(subAnim.m_loop);
    ui->SubFrame_CB_Loop->setEnabled(true);
    ui->SubFrame_PB_Play->setEnabled(subAnim.m_subFrames.size() > 1);
    ui->SubFrame_PB_Add->setEnabled(true);
    ui->SubFrame_SB_Index->setMaximum(m_frame.m_objects.size() - 1);
}

void BNSpriteEditor::on_SubAnim_PB_New_clicked()
{
    BNSprite::SubAnimation subAnim;
    subAnim.m_subFrames.push_back(BNSprite::SubFrame());
    m_frame.m_subAnimations.push_back(subAnim);

    int subAnimCount = m_frame.m_subAnimations.size();
    QWidget* widget = new QWidget();
    ui->SubAnim_Tabs->addTab(widget, "SubAnim " + QString::number(subAnimCount - 1));
    ui->SubAnim_Tabs->setCurrentIndex(subAnimCount - 1);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    ui->SubAnim_PB_New->setEnabled(subAnimCount < 255);
    ui->SubAnim_PB_Dup->setEnabled(subAnimCount < 255);
    ui->SubAnim_PB_Del->setEnabled(subAnimCount > 1);
}

void BNSpriteEditor::on_SubAnim_PB_Dup_clicked()
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    m_frame.m_subAnimations.push_back(m_frame.m_subAnimations[subAnimID]);

    int subAnimCount = m_frame.m_subAnimations.size();
    QWidget* widget = new QWidget();
    ui->SubAnim_Tabs->addTab(widget, "SubAnim " + QString::number(subAnimCount - 1));
    ui->SubAnim_Tabs->setCurrentIndex(subAnimCount - 1);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    ui->SubAnim_PB_New->setEnabled(subAnimCount < 255);
    ui->SubAnim_PB_Dup->setEnabled(subAnimCount < 255);
    ui->SubAnim_PB_Del->setEnabled(subAnimCount > 1);
}

void BNSpriteEditor::on_SubAnim_PB_Del_clicked()
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    m_frame.m_subAnimations.erase(m_frame.m_subAnimations.begin() + subAnimID);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Manually call valueChanged here, because it is not changed if not deleting max
    int subAnimCount = m_frame.m_subAnimations.size();
    delete ui->SubAnim_Tabs->widget(subAnimID);
    ui->SubAnim_PB_Del->setEnabled(subAnimCount > 1);

    // Rename all tabs
    for (int i = 0; i < ui->SubAnim_Tabs->count(); i++)
    {
        ui->SubAnim_Tabs->setTabText(i, "SubAnim " + QString::number(i));
    }

    if (subAnimID == 0)
    {
        UpdateFrameThumbnail(frameID);
    }
}

void BNSpriteEditor::on_SubFrame_PB_Play_clicked()
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    BNSprite::SubAnimation const& subAnim = m_frame.m_subAnimations[subAnimID];

    int count = subAnim.m_subFrames.size();
    if (count <= 1 && !m_playingSubAnimation) return;
    m_playingSubAnimation = !m_playingSubAnimation;

    ui->GB_Image->setEnabled(!m_playingSubAnimation);
    ui->GB_Object->setEnabled(!m_playingSubAnimation);
    ui->GB_Palette->setEnabled(!m_playingSubAnimation);
    ui->GB_Tileset->setEnabled(!m_playingSubAnimation);
    ui->GB_Frame_Sub1->setEnabled(!m_playingSubAnimation);
    ui->GB_Frame_Sub2->setEnabled(!m_playingSubAnimation);

    ui->SubAnim_Tabs->setEnabled(!m_playingSubAnimation);
    ui->GB_SubAnim_Sub1->setEnabled(!m_playingSubAnimation);
    ui->GB_SubFrame_Sub1->setEnabled(!m_playingSubAnimation);
    ui->GB_SubFrame_Sub2->setEnabled(!m_playingSubAnimation);
    ui->GB_SubFrame_Sub3->setEnabled(!m_playingSubAnimation);

    if (m_playingSubAnimation)
    {
        m_playingSubFrame = 0;

        BNSprite::SubFrame const& subFrame = subAnim.m_subFrames[m_playingSubFrame];
        int objectIndex = subFrame.m_objectIndex;

        ui->SubFrame_LW->setCurrentRow(m_playingSubFrame);
        m_subAnimationTimer->start(subFrame.m_delay * 16);
        ui->SubFrame_PB_Play->setText("Stop Playing");
    }
    else
    {
        m_playingSubFrame = -1;

        m_subAnimationTimer->stop();
        ui->SubFrame_PB_Play->setText("Play Animation");
    }
}

void BNSpriteEditor::on_SubFrame_Play_timeout()
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    BNSprite::SubAnimation const& subAnim = m_frame.m_subAnimations[subAnimID];

    int count = subAnim.m_subFrames.size();
    if (count <= 1)
    {
        on_SubFrame_PB_Play_clicked();
        return;
    }

    if (m_playingSubFrame == count - 1)
    {
        m_playingSubFrame = 0;
        BNSprite::SubFrame const& subFrame = subAnim.m_subFrames[m_playingSubFrame];

        ui->SubFrame_LW->setCurrentRow(m_playingSubFrame);
        m_subAnimationTimer->start(subFrame.m_delay * 16);
    }
    else
    {
        m_playingSubFrame++;
        BNSprite::SubFrame const& subFrame = subAnim.m_subFrames[m_playingSubFrame];

        ui->SubFrame_LW->setCurrentRow(m_playingSubFrame);

        if (m_playingSubFrame == count - 1 && !ui->SubFrame_CB_Loop->isChecked())
        {
            // Non-loop animation, stop animation
            on_SubFrame_PB_Play_clicked();
        }
        else
        {
            m_subAnimationTimer->start(subFrame.m_delay * 16);
        }
    }
}

void BNSpriteEditor::on_SubFrame_CB_Loop_toggled(bool checked)
{
    BNSprite::SubAnimation& subAnim = m_frame.m_subAnimations[ui->SubAnim_Tabs->currentIndex()];
    if (subAnim.m_loop == checked || !ui->SubFrame_CB_Loop->isEnabled()) return;
    subAnim.m_loop = checked;

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);
}

void BNSpriteEditor::on_SubFrame_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    BNSprite::SubAnimation const& subAnim = m_frame.m_subAnimations[subAnimID];
    int subFrameID = ui->SubFrame_LW->row(current);
    if (subFrameID < 0) return;
    BNSprite::SubFrame const& subFrame = subAnim.m_subFrames[subFrameID];

    qDebug() << "SubFrame" << subFrameID << "selected";

    // Jump to object
    ui->Object_Tabs->setCurrentIndex(subFrame.m_objectIndex);

    int count = subAnim.m_subFrames.size();
    ui->SubFrame_SB_Index->setValue(subFrame.m_objectIndex);
    ui->SubFrame_SB_Index->setEnabled(true);
    ui->SubFrame_SB_Delay->setValue(subFrame.m_delay);
    ui->SubFrame_SB_Delay->setEnabled(true);
    ui->SubFrame_PB_Del->setEnabled(count > 1);
    ui->SubFrame_PB_ShiftL->setEnabled(subFrameID > 0);
    ui->SubFrame_PB_ShiftR->setEnabled(subFrameID < count - 1);
}

void BNSpriteEditor::on_SubFrame_PB_Add_clicked()
{
    BNSprite::SubAnimation& subAnim = m_frame.m_subAnimations[ui->SubAnim_Tabs->currentIndex()];
    subAnim.m_subFrames.push_back(BNSprite::SubFrame());
    AddSubFrame(subAnim.m_subFrames.size() - 1);
    ui->SubFrame_LW->setCurrentRow(subAnim.m_subFrames.size() - 1);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    ui->SubFrame_PB_Play->setEnabled(true);
}

void BNSpriteEditor::on_SubFrame_PB_Del_clicked()
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    BNSprite::SubAnimation& subAnim = m_frame.m_subAnimations[subAnimID];
    int subFrameID = ui->SubFrame_LW->currentRow();

    int oldObjectIndex = -1;
    if (subAnimID == 0)
    {
        oldObjectIndex = subAnim.m_subFrames[0].m_objectIndex;
    }

    subAnim.m_subFrames.erase(subAnim.m_subFrames.begin() + subFrameID);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // We manually call item changed here, because takeItem() calls it but it's not deleted yet
    ui->SubFrame_LW->blockSignals(true);
    delete ui->SubFrame_LW->takeItem(subFrameID);
    on_SubFrame_LW_currentItemChanged(ui->SubFrame_LW->item(subFrameID == subAnim.m_subFrames.size() ? subFrameID - 1 : subFrameID), Q_NULLPTR);
    ui->SubFrame_LW->blockSignals(false);

    // Update thumbnail if sub anim 0 sub frame 0 object index is changed
    if (subAnimID == 0)
    {
        int newObjectIndex = subAnim.m_subFrames[0].m_objectIndex;
        if (oldObjectIndex != newObjectIndex)
        {
            UpdateFrameThumbnail(frameID);
        }
    }

    ui->SubFrame_PB_Play->setEnabled(subAnim.m_subFrames.size() > 1);
}

void BNSpriteEditor::on_SubFrame_PB_ShiftL_clicked()
{
    int subFrameID = ui->SubFrame_LW->currentRow();
    SwapSubFrame(subFrameID, subFrameID - 1);
}

void BNSpriteEditor::on_SubFrame_PB_ShiftR_clicked()
{
    int subFrameID = ui->SubFrame_LW->currentRow();
    SwapSubFrame(subFrameID, subFrameID + 1);
}

void BNSpriteEditor::on_SubFrame_SB_Index_valueChanged(int arg1)
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    BNSprite::SubAnimation& subAnim = m_frame.m_subAnimations[subAnimID];
    int subFrameID = ui->SubFrame_LW->currentRow();
    BNSprite::SubFrame& subFrame = subAnim.m_subFrames[subFrameID];

    if (subFrame.m_objectIndex == arg1 || !ui->SubFrame_SB_Index->isEnabled()) return;

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();

    subFrame.m_objectIndex = arg1;
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Jump to object
    ui->Object_Tabs->setCurrentIndex(arg1);

    // Update subframe thumbnail
    UpdateSubFrameThumbnail(subFrameID);

    // Update thumbnail if sub anim 0 sub frame 0 object index is changed
    if (subAnimID == 0 && subFrameID == 0)
    {
        UpdateFrameThumbnail(frameID);
    }
}

void BNSpriteEditor::on_SubFrame_SB_Delay_valueChanged(int arg1)
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    BNSprite::SubAnimation& subAnim = m_frame.m_subAnimations[subAnimID];
    int subFrameID = ui->SubFrame_LW->currentRow();
    BNSprite::SubFrame& subFrame = subAnim.m_subFrames[subFrameID];

    if (subFrame.m_delay == arg1 || !ui->SubFrame_SB_Delay->isEnabled()) return;

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();

    subFrame.m_delay = arg1;
    m_sprite.ReplaceFrame(animID, frameID, m_frame);
}

//---------------------------------------------------------------------------
// Add sub frame
//---------------------------------------------------------------------------
void BNSpriteEditor::AddSubFrame(int _subFrameID)
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    QImage* image = GetFrameImage(m_frame, true, subAnimID, _subFrameID);
    QListWidgetItem* item = new QListWidgetItem(QIcon(QPixmap::fromImage(*image)), "");
    ui->SubFrame_LW->addItem(item);
    delete image;
}

//---------------------------------------------------------------------------
// Swap two sub frames
//---------------------------------------------------------------------------
void BNSpriteEditor::SwapSubFrame(int _oldID, int _newID)
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    BNSprite::SubAnimation& subAnim = m_frame.m_subAnimations[subAnimID];

    int oldObjectIndex = -1;
    if (subAnimID == 0)
    {
        oldObjectIndex = subAnim.m_subFrames[0].m_objectIndex;
    }

    // Swap in actual sub frame
    iter_swap(subAnim.m_subFrames.begin() + _oldID, subAnim.m_subFrames.begin() + _newID);

    // Update sprite
    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Simply swap the two icons
    QListWidgetItem* item0 = ui->SubFrame_LW->item(_newID);
    QIcon const iconTemp = item0->icon();
    QListWidgetItem* item1 = ui->SubFrame_LW->item(_oldID);
    item0->setIcon(item1->icon());
    item1->setIcon(iconTemp);

    // Block signal to prevent loading frame again
    ui->SubFrame_LW->blockSignals(true);
    ui->SubFrame_LW->setCurrentRow(_newID);
    ui->SubFrame_LW->blockSignals(false);

    // Update button
    ui->SubFrame_PB_ShiftL->setEnabled(_newID > 0);
    ui->SubFrame_PB_ShiftR->setEnabled(_newID < ui->SubFrame_LW->count() - 1);

    // Update thumbnail if sub anim 0 sub frame 0 object index is changed
    if (subAnimID == 0)
    {
        int newObjectIndex = subAnim.m_subFrames[0].m_objectIndex;
        if (oldObjectIndex != newObjectIndex)
        {
            UpdateFrameThumbnail(frameID);
        }
    }
}

//---------------------------------------------------------------------------
// Update sub frame thumbnail
//---------------------------------------------------------------------------
void BNSpriteEditor::UpdateSubFrameThumbnail(int _subFrameID)
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    QImage* image = GetFrameImage(m_frame, true, subAnimID, _subFrameID);
    QListWidgetItem* item = ui->SubFrame_LW->item(_subFrameID);
    item->setIcon(QIcon(QPixmap::fromImage(*image)));
    delete image;
}

//---------------------------------------------------------------------------
// An object has changed, check all sub frames of current sub animation
//---------------------------------------------------------------------------
void BNSpriteEditor::UpdateSubFrameThumbnailFromObject(int _objectID)
{
    int subAnimID = ui->SubAnim_Tabs->currentIndex();
    BNSprite::SubAnimation const& subAnim = m_frame.m_subAnimations[subAnimID];

    for (int i = 0; i < subAnim.m_subFrames.size(); i++)
    {
        BNSprite::SubFrame const& subFrame = subAnim.m_subFrames[i];
        if (subFrame.m_objectIndex == _objectID)
        {
            UpdateSubFrameThumbnail(i);
        }
    }
}

//---------------------------------------------------------------------------
// Palette signals
//---------------------------------------------------------------------------
void BNSpriteEditor::on_Palette_SB_Group_valueChanged(int arg1)
{
    if (arg1 == m_frame.m_paletteGroupID || !ui->Palette_SB_Group->isEnabled()) return;
    m_frame.m_paletteGroupID = arg1;

    // Force palette index to be 0
    int maximum = m_paletteGroups[m_frame.m_paletteGroupID].size() - 1;
    ui->Palette_SB_Index->blockSignals(true);
    ui->Palette_SB_Index->setMaximum(qMin(maximum, 255));
    ui->Palette_SB_Index->setValue(0);
    ui->Palette_SB_Index->blockSignals(false);
    ui->Palette_PB_Up->setEnabled(false);
    ui->Palette_PB_Down->setEnabled(maximum != 0 && !IsCustomSpriteMakerActive());
    for (BNSprite::Object& object : m_frame.m_objects)
    {
        object.m_paletteIndex = 0;
    }

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    int objectID = ui->Object_Tabs->currentIndex();
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update sub frame thumbnail
    for (int i = 0; i < m_frame.m_objects.size(); i++)
    {
        UpdateSubFrameThumbnailFromObject(i);
    }

    // Update OAM
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    BNSprite::Object const& object = m_frame.m_objects[objectID];
    for (int i = 0; i < object.m_subObjects.size(); i++)
    {
        BNSprite::SubObject const& subObject = object.m_subObjects[i];
        UpdatePreviewOAM(i, subObject, true);

        if (i == OAMIndex)
        {
            UpdateOAMThumbnail(subObject);
        }
    }

    UpdateDrawTileset();
    UpdatePalettePreview();
}

void BNSpriteEditor::on_Palette_SB_Index_valueChanged(int arg1)
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];

    // Show warning for BN sprites if index > 15
    if (arg1 > 15)
    {
        ui->Palette_Warning->setHidden(false);
    }
    else
    {
        ui->Palette_Warning->setHidden(true);
    }

    if (object.m_paletteIndex == arg1 || !ui->Palette_SB_Index->isEnabled()) return;
    object.m_paletteIndex = arg1;

    int maximum = m_paletteGroups[m_frame.m_paletteGroupID].size() - 1;
    ui->Palette_PB_Up->setEnabled(object.m_paletteIndex > 0 && !IsCustomSpriteMakerActive());
    ui->Palette_PB_Down->setEnabled(object.m_paletteIndex < maximum && !IsCustomSpriteMakerActive());

    // Only update if index <= 255
    if (arg1 <= 255)
    {
        int animID = ui->Anim_LW->currentRow();
        int frameID = ui->Frame_LW->currentRow();
        m_sprite.ReplaceFrame(animID, frameID, m_frame);

        // Update thumbnail
        if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
        {
            UpdateFrameThumbnail(frameID);
        }

        // Update sub frame thumbnail
        UpdateSubFrameThumbnailFromObject(objectID);

        // Update tileset color
        UpdateDrawTileset();
    }

    // Update preview
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    for (int i = 0; i < object.m_subObjects.size(); i++)
    {
        BNSprite::SubObject const& subObject = object.m_subObjects[i];
        UpdatePreviewOAM(i, subObject, true);

        if (i == OAMIndex)
        {
            UpdateOAMThumbnail(subObject);
        }
    }

    SetPaletteSelected(arg1);
}

void BNSpriteEditor::on_Palette_PB_Up_pressed()
{
    int index = ui->Palette_SB_Index->value();
    SwapPalette(index, index - 1);
    ui->Palette_SB_Index->setValue(index - 1);

    // swapping doesn't change current frame palette
    UpdateAllThumbnails(-1);
}

void BNSpriteEditor::on_Palette_PB_Down_pressed()
{
    int index = ui->Palette_SB_Index->value();
    SwapPalette(index, index + 1);
    ui->Palette_SB_Index->setValue(index + 1);

    // swapping doesn't change current frame palette
    UpdateAllThumbnails(-1);
}

void BNSpriteEditor::on_Palette_PB_Import_pressed()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString file = QFileDialog::getOpenFileName(this, tr("Import Palette"), path, PALETTE_EXTENSIONS);
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    FILE* f;
    _wfopen_s(&f, file.toStdWString().c_str(), L"rb");
    if (!f)
    {
        QMessageBox::critical(this, "Error", "Unable to open file!");
        return;
    }

    // File size
    fseek(f, 0, SEEK_END);
    uint32_t fileSize = ftell(f);
    if (fileSize % 0x20 != 0)
    {
        QMessageBox::critical(this, "Error", "Invalid palette file, it must be a multiple of 0x20 bytes!");
        fclose(f);
        return;
    }

    if (fileSize > (0x20 * 256))
    {
        QMessageBox::critical(this, "Error", "Cannot import more than 256 palettes!");
        fclose(f);
        return;
    }

    fseek(f, 0x00, SEEK_SET);
    PaletteGroup tempGroup;
    while((uint32_t)ftell(f) < fileSize)
    {
        // TODO: 256 color support?
        Palette tempPal;
        for (uint8_t i = 0; i < 16; i++)
        {
            uint8_t byte, byte2;
            fread(&byte, 1, 1, f);
            fread(&byte2, 1, 1, f);
            QRgb rgb = BNSprite::GBAtoRGB((byte2 << 8) + byte);
            tempPal.push_back(rgb);
        }
        tempPal[0] &= 0x00FFFFFF; // set first palette to alpha 0 for transparency
        tempGroup.push_back(tempPal);
    }
    fclose(f);

    QMessageBox msgBox;
    msgBox.setWindowTitle("Import Palette");
    msgBox.setIcon(QMessageBox::Question);
    QString message = "Do you want to append palettes, replace current group or create new group?";
    message += "\n(WARNING: If the number of palette imported to replace is fewer than what it currently has, palette indices of frames will be clamped!)";
    msgBox.setText(message);
    QAbstractButton* pButtonAppend = msgBox.addButton("Append", QMessageBox::YesRole);
    QAbstractButton* pButtonReplace = msgBox.addButton("Replace", QMessageBox::YesRole);
    QAbstractButton* pButtonNewGroup = msgBox.addButton("New Group", QMessageBox::YesRole);
    QAbstractButton* pButtonCancel = msgBox.addButton("Cancel", QMessageBox::NoRole);
    msgBox.exec();

    int group = ui->Palette_SB_Group->value();
    PaletteGroup& palGroup = m_paletteGroups[group];

    QAbstractButton* clickedButton = msgBox.clickedButton();
    if (clickedButton == pButtonCancel)
    {
        return;
    }
    else if (clickedButton == pButtonNewGroup)
    {
        m_paletteGroups.push_back(tempGroup);
        ui->Palette_SB_Group->setMaximum(m_paletteGroups.size() - 1);
    }
    else // if (clickedButton == pButtonReplace || clickedButton == pButtonAppend)
    {
        if (clickedButton == pButtonReplace)
        {
            palGroup.clear();
        }
        else if (palGroup.size() + tempGroup.size() > 256)
        {
            QMessageBox::critical(this, "Error", "Maximum no. of palette is reached (256)!");
            return;
        }

        for (Palette const& palette : tempGroup)
        {
            palGroup.push_back(palette);
        }
    }

    // Only need to update thumbnail if we replace the group
    if (msgBox.clickedButton() == pButtonReplace)
    {
        UpdateAllThumbnails(-1, true);
    }

    // Reload current frame
    on_Frame_LW_currentItemChanged(ui->Frame_LW->currentItem(), Q_NULLPTR);
}

void BNSpriteEditor::on_Palette_PB_Export_pressed()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString file = QFileDialog::getSaveFileName(this, tr("Export Palette"), path, PALETTE_EXTENSIONS);
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    FILE* f;
    _wfopen_s(&f, file.toStdWString().c_str(), L"wb");
    if (!f)
    {
        QMessageBox::critical(this, "Error", "Unable to open file!");
        return;
    }

    int group = ui->Palette_SB_Group->value();
    PaletteGroup const& palGroup = m_paletteGroups[group];
    for (Palette const& pal : palGroup)
    {
        // TODO: 256 color support?
        for (uint8_t i = 0; i < 16; i++)
        {
            uint16_t const gbaColor = BNSprite::RGBtoGBA(pal[i]);
            fwrite(&gbaColor, 2, 1, f);
        }
    }

    fclose(f);
    QMessageBox::information(this, "Export Palette", "Palettes of this group is exported!");
}

void BNSpriteEditor::on_Palette_Color_changed(int paletteIndex, int colorIndex, QRgb color)
{
    int group = ui->Palette_SB_Group->value();
    Palette& palette = m_paletteGroups[group][paletteIndex];
    palette[colorIndex] = color;
    palette[0] &= 0x00FFFFFF; // transparency for first color

    UpdateAllThumbnails(paletteIndex);
}

void BNSpriteEditor::on_PaletteContexMenu_requested(int paletteIndex, int colorIndex, QPoint pos)
{
    int group = ui->Palette_SB_Group->value();
    PaletteGroup const& palGroup = m_paletteGroups[group];
    if (paletteIndex >= palGroup.size() || colorIndex > 15) return;

    // cannot delete if there's only one palette or using custom sprite maker
    m_paletteContextMenu->deletePaletteAction->setEnabled(palGroup.size() > 1 && !IsCustomSpriteMakerActive());

    // cannot insert if maximum is reached
    bool enableInsert = palGroup.size() - 1 < 255 && !IsCustomSpriteMakerActive() && !m_paletteContextMenu->getPalettecopied().isEmpty();
    m_paletteContextMenu->insertAboveAction->setEnabled(enableInsert);
    m_paletteContextMenu->insertBelowAction->setEnabled(enableInsert);

    // cannot copy palette for 256 color
    m_paletteContextMenu->copyPaletteAction->setEnabled(!m_sprite.Is256Color());

    Palette const& palette = palGroup[paletteIndex];
    m_paletteContextMenu->setPaletteIndex(paletteIndex);
    m_paletteContextMenu->setColorIndex(colorIndex);
    m_paletteContextMenu->setPalettePressed(palette);
    m_paletteContextMenu->setColorPressed(palette[colorIndex]);
    m_paletteContextMenu->popup(pos);
}

void BNSpriteEditor::on_PaletteContexMenu_colorCopied()
{
    m_paletteContextMenu->setColorCopied();
}

void BNSpriteEditor::on_PaletteContexMenu_colorReplaced()
{
    int group = ui->Palette_SB_Group->value();
    int paletteIndex = m_paletteContextMenu->getPaletteIndex();
    int colorIndex = m_paletteContextMenu->getColorIndex();

    PaletteGroup const& palGroup = m_paletteGroups[group];
    if (paletteIndex >= palGroup.size() || colorIndex > 15) return;

    QRgb const color = m_paletteContextMenu->getColorcopied();
    if ((color >> 24) == 0xFF)
    {
        on_Palette_Color_changed(paletteIndex, colorIndex, color);
        ui->Palette_GV->replaceColor(paletteIndex, colorIndex, color);
    }
}

void BNSpriteEditor::on_PaletteContexMenu_paletteCopied()
{
    m_paletteContextMenu->setPaletteCopied();
}

void BNSpriteEditor::on_PaletteContexMenu_paletteReplaced()
{
    int group = ui->Palette_SB_Group->value();
    int paletteIndex = m_paletteContextMenu->getPaletteIndex();

    Palette& palette = m_paletteGroups[group][paletteIndex];
    Palette const paletteNew = m_paletteContextMenu->getPalettecopied();

    if (palette.size() != paletteNew.size()) return;
    palette = paletteNew;
    ui->Palette_GV->replacePalette(paletteIndex, paletteNew);

    UpdateAllThumbnails(paletteIndex);
}

void BNSpriteEditor::on_PaletteContexMenu_paletteInsertAbove()
{
    int insertAt = m_paletteContextMenu->getPaletteIndex();
    Palette const palette = m_paletteContextMenu->getPalettecopied();
    InsertPalette(insertAt, palette);

    // increment index by one
    ui->Palette_SB_Index->setValue(ui->Palette_SB_Index->value() + 1);
}

void BNSpriteEditor::on_PaletteContexMenu_paletteInsertBelow()
{
    int insertAt = m_paletteContextMenu->getPaletteIndex() + 1;
    Palette const palette = m_paletteContextMenu->getPalettecopied();
    InsertPalette(insertAt, palette);
}

void BNSpriteEditor::on_PaletteContexMenu_paletteDeleted()
{
    int paletteIndex = m_paletteContextMenu->getPaletteIndex();
    DeletePalette(paletteIndex);
}

//---------------------------------------------------------------------------
// Cache the palette group from sprite
//---------------------------------------------------------------------------
void BNSpriteEditor::AddPaletteGroupFromSprite(const BNSprite::PaletteGroup &paletteGroup)
{
    PaletteGroup groupCopy;
    for (BNSprite::Palette const& pal : paletteGroup.m_palettes)
    {
        uint32_t const size = pal.m_colors.size();
        Q_ASSERT(size == 16 || size == 256);

        Palette palCopy;
        for (uint32_t i = 0; i < size; i++)
        {
            uint16_t const& col = pal.m_colors[i];
            uint32_t rgb = BNSprite::GBAtoRGB(col);
            palCopy.push_back(rgb);
        }
        palCopy[0] &= 0x00FFFFFF; // set first palette to alpha 0 for transparency
        groupCopy.push_back(palCopy);
    }
    m_paletteGroups.push_back(groupCopy);
}

//---------------------------------------------------------------------------
// Highlight selected palette
//---------------------------------------------------------------------------
void BNSpriteEditor::SetPaletteSelected(int index)
{
    ui->Palette_GV->setPaletteSelected(index);
}

//---------------------------------------------------------------------------
// Draw preview images
//---------------------------------------------------------------------------
void BNSpriteEditor::UpdatePalettePreview()
{
    ui->Palette_GV->clear();

    int group = ui->Palette_SB_Group->value();
    for (int i = 0; i < m_paletteGroups[group].size(); i++)
    {
        ui->Palette_GV->addPalette(m_paletteGroups[group][i], m_sprite.Is256Color());
    }

    int index = ui->Palette_SB_Index->value();
    SetPaletteSelected(index);
}

//---------------------------------------------------------------------------
// Swap two palettes
//---------------------------------------------------------------------------
void BNSpriteEditor::SwapPalette(int id1, int id2)
{
    int group = ui->Palette_SB_Group->value();

    PaletteGroup& palGroup = m_paletteGroups[group];
    qSwap(palGroup[id1], palGroup[id2]);

    ui->Palette_GV->swapPalette(id1, id2);
}

//---------------------------------------------------------------------------
// Insert a new palette at
//---------------------------------------------------------------------------
void BNSpriteEditor::InsertPalette(int insertAt, const Palette palette)
{
    int group = ui->Palette_SB_Group->value();
    int index = ui->Palette_SB_Index->value();

    PaletteGroup& palGroup = m_paletteGroups[group];
    if (insertAt < 0 || insertAt > palGroup.size()) return;

    palGroup.insert(insertAt, palette);
    ui->Palette_GV->addPalette(palette, false, insertAt);

    int maximum = palGroup.size() - 1;
    ui->Palette_SB_Index->setMaximum(qMin(maximum, 255));
    ui->Palette_PB_Up->setEnabled(index > 0 && !IsCustomSpriteMakerActive());
    ui->Palette_PB_Down->setEnabled(index < maximum && !IsCustomSpriteMakerActive());

    UpdateAllThumbnails(-1, true);
}

//---------------------------------------------------------------------------
// Delete a palette in a group
//---------------------------------------------------------------------------
void BNSpriteEditor::DeletePalette(int paletteIndex)
{
    int group = ui->Palette_SB_Group->value();
    int index = ui->Palette_SB_Index->value();

    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString message = "Do you wish to delete palette index " + QString::number(paletteIndex) + "? All frame that uses this palette will be affected.";
    resBtn = QMessageBox::warning(this, "Delete Palette", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    PaletteGroup& paletteGroup = m_paletteGroups[group];
    if (paletteGroup.size() <= 1) return;
    paletteGroup.remove(paletteIndex);
    ui->Palette_GV->deletePalette(paletteIndex);

    int maximum = paletteGroup.size() - 1;
    ui->Palette_SB_Index->blockSignals(true);
    ui->Palette_SB_Index->setMaximum(qMin(maximum, 255)); // note that the frame is not saved
    ui->Palette_SB_Index->blockSignals(false);
    ui->Palette_PB_Up->setEnabled(index > 0 && !IsCustomSpriteMakerActive());
    ui->Palette_PB_Down->setEnabled(index < maximum && !IsCustomSpriteMakerActive());

    // Redraw preview, OAM, tileset
    UpdateAllThumbnails(-1, true);
}

//---------------------------------------------------------------------------
// Palette has been updated, redrawn everything that is affected by this
//---------------------------------------------------------------------------
void BNSpriteEditor::UpdateAllThumbnails(int paletteIndex, bool redrawAll)
{
    // Check current selected frame
    if (ui->Palette_SB_Index->value() == paletteIndex || redrawAll)
    {
        // Redraw preview, OAM, tileset
        int objectID = ui->Object_Tabs->currentIndex();
        BNSprite::Object const& object = m_frame.m_objects[objectID];
        int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
        for (int i = 0; i < m_previewOAMs.size(); i++)
        {
            UpdatePreviewOAM(i, object.m_subObjects[i], true);

            if (i == OAMIndex)
            {
                UpdateOAMThumbnail(object.m_subObjects[i]);
            }
        }

        UpdateDrawTileset();
    }

    // Update frame/animation thumbnails
    int animID = ui->Anim_LW->currentRow();
    for (int i = 0; i < ui->Frame_LW->count(); i++)
    {
        UpdateFrameThumbnail(i);
    }
    for (int i = 0; i < ui->Anim_LW->count(); i++)
    {
        if (animID == i) continue;
        UpdateAnimationThumnail(i);
    }
    for (int i = 0; i < ui->SubFrame_LW->count(); i++)
    {
        UpdateSubFrameThumbnail(i);
    }
}

//---------------------------------------------------------------------------
// Object signals
//---------------------------------------------------------------------------
void BNSpriteEditor::on_Object_Tabs_currentChanged(int index)
{
    BNSprite::Object& object = m_frame.m_objects[index];

    // Clamp the palette index to valid
    int maximum = ui->Palette_SB_Index->maximum();
    if (object.m_paletteIndex > maximum)
    {
        object.m_paletteIndex = maximum;
    }
    ui->Palette_SB_Index->setValue(object.m_paletteIndex);
    ui->Palette_PB_Up->setEnabled(object.m_paletteIndex > 0 && !IsCustomSpriteMakerActive());
    ui->Palette_PB_Down->setEnabled(object.m_paletteIndex < maximum && !IsCustomSpriteMakerActive());

    // Load OAMs
    ResetOAM();
    ui->OAM_PB_New->setEnabled(true);
    ui->OAM_TW->scrollToTop();
    for (BNSprite::SubObject const& subObject : object.m_subObjects)
    {
        AddOAM(subObject);
    }

    UpdateDrawTileset();
    UpdateImageControlButtons();
    UpdatePalettePreview();
}

void BNSpriteEditor::on_Object_PB_New_clicked()
{
    BNSprite::Object object;
    object.m_subObjects.push_back(BNSprite::SubObject());
    m_frame.m_objects.push_back(object);

    int objectCount = m_frame.m_objects.size();
    QWidget* widget = new QWidget();
    ui->Object_Tabs->addTab(widget, "Object " + QString::number(objectCount - 1));
    ui->Object_Tabs->setCurrentIndex(objectCount - 1);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    ui->Object_PB_New->setEnabled(objectCount < 255);
    ui->Object_PB_Dup->setEnabled(objectCount < 255);
    ui->Object_PB_Del->setEnabled(objectCount > 1);

    // Reload sub animation
    on_SubAnim_Tabs_currentChanged(ui->SubAnim_Tabs->currentIndex());
}

void BNSpriteEditor::on_Object_PB_Dup_clicked()
{
    int objectID = ui->Object_Tabs->currentIndex();
    m_frame.m_objects.push_back(m_frame.m_objects[objectID]);

    int objectCount = m_frame.m_objects.size();
    QWidget* widget = new QWidget();
    ui->Object_Tabs->addTab(widget, "Object " + QString::number(objectCount - 1));
    ui->Object_Tabs->setCurrentIndex(objectCount - 1);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    ui->Object_PB_New->setEnabled(objectCount < 255);
    ui->Object_PB_Dup->setEnabled(objectCount < 255);
    ui->Object_PB_Del->setEnabled(objectCount > 1);

    // Reload sub animation
    on_SubAnim_Tabs_currentChanged(ui->SubAnim_Tabs->currentIndex());
}

void BNSpriteEditor::on_Object_PB_Del_clicked()
{
    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString message = "Do you wish to delete object? This will affect all sub animations in this frame.";
    resBtn = QMessageBox::warning(this, "Delete Object", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    int objectID = ui->Object_Tabs->currentIndex();
    m_frame.m_objects.erase(m_frame.m_objects.begin() + objectID);

    // Manually call valueChanged here, because it is not changed if not deleting max
    int objectCount = m_frame.m_objects.size();
    delete ui->Object_Tabs->widget(objectID);
    ui->Object_PB_Del->setEnabled(objectCount > 1);

    // Go through all the sub animations and check for invalid index
    for (BNSprite::SubAnimation& subAnim : m_frame.m_subAnimations)
    {
        for (BNSprite::SubFrame& subframe : subAnim.m_subFrames)
        {
            if (subframe.m_objectIndex >= objectCount)
            {
                subframe.m_objectIndex = objectCount - 1;
            }
        }
    }

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Rename all tabs
    for (int i = 0; i < ui->Object_Tabs->count(); i++)
    {
        ui->Object_Tabs->setTabText(i, "Object " + QString::number(i));
    }

    // Reload sub animation
    on_SubAnim_Tabs_currentChanged(ui->SubAnim_Tabs->currentIndex());
    UpdateFrameThumbnail(frameID);
}

void BNSpriteEditor::on_OAM_TW_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    int index = ui->OAM_TW->indexOfTopLevelItem(current);
    if (index < 0) return;

    qDebug() << "OAM" << index << "selected";

    // Highlight
    current->setSelected(true);
    current->setForeground(0, QColor(255,0,0));
    current->setForeground(1, QColor(255,0,0));
    current->setForeground(2, QColor(255,0,0));
    if (previous)
    {
        previous->setForeground(0, QColor(0,0,0));
        previous->setForeground(1, QColor(0,0,0));
        previous->setForeground(2, QColor(0,0,0));
    }

    // Set OAM buttons
    int count = ui->OAM_TW->topLevelItemCount();
    ui->OAM_PB_Up->setEnabled(index > 0);
    ui->OAM_PB_Down->setEnabled(index < count - 1);
    ui->OAM_PB_New->setEnabled(count < 255);
    ui->OAM_PB_Dup->setEnabled(index >= 0);
    ui->OAM_PB_Del->setEnabled(index >= 0 && count > 1);

    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object const& object = m_frame.m_objects[objectID];
    BNSprite::SubObject const& subObject = object.m_subObjects[index];

    ui->OAM_SB_Tile->setMaximum(m_tilesetData.size() / 64 - 1);
    ui->OAM_SB_Tile->setValue(subObject.m_startTile);
    ui->OAM_SB_Tile->setEnabled(true);
    ui->OAM_CB_Size->setCurrentText(QString::number(subObject.m_sizeX) + "x" + QString::number(subObject.m_sizeY));
    ui->OAM_CB_Size->setEnabled(true);
    ui->OAM_SB_XPos->setValue(subObject.m_posX);
    ui->OAM_SB_XPos->setEnabled(true);
    ui->OAM_SB_YPos->setValue(subObject.m_posY);
    ui->OAM_SB_YPos->setEnabled(true);
    ui->OAM_CB_HFlip->setChecked(subObject.m_hFlip);
    ui->OAM_CB_HFlip->setEnabled(true);
    ui->OAM_CB_VFlip->setChecked(subObject.m_vFlip);
    ui->OAM_CB_VFlip->setEnabled(true);

    UpdateOAMThumbnail(subObject);

    // Preview selection
    if (ui->Preview_CB_Highlight->isChecked())
    {
        m_highlightUp = false;
        m_highlightAlpha = 255;
        m_highlightTimer->stop();
        m_highlightTimer->start(2);
        m_previewHighlight->setVisible(true);
        m_previewHighlight->setPos(subObject.m_posX + 128, subObject.m_posY + 128);
    }
}

void BNSpriteEditor::on_OAM_PB_Up_clicked()
{
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    SwapOAM(OAMIndex, OAMIndex - 1);
}

void BNSpriteEditor::on_OAM_PB_Down_clicked()
{
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    SwapOAM(OAMIndex, OAMIndex + 1);
}

void BNSpriteEditor::on_OAM_PB_New_clicked()
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    object.m_subObjects.push_back(BNSprite::SubObject());

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    AddOAM(BNSprite::SubObject());
    ui->OAM_TW->setCurrentItem(ui->OAM_TW->topLevelItem(object.m_subObjects.size() - 1));

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }
}

void BNSpriteEditor::on_OAM_PB_Dup_clicked()
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];

    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    object.m_subObjects.push_back(object.m_subObjects[OAMIndex]);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    AddOAM(object.m_subObjects[OAMIndex]);
    ui->OAM_TW->setCurrentItem(ui->OAM_TW->topLevelItem(object.m_subObjects.size() - 1));

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }
}

void BNSpriteEditor::on_OAM_PB_Del_clicked()
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];

    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    object.m_subObjects.erase(object.m_subObjects.begin() + OAMIndex);

    // We manually call item changed here, because takeItem() calls it but it's not deleted yet
    ui->OAM_TW->blockSignals(true);
    delete ui->OAM_TW->takeTopLevelItem(OAMIndex);
    on_OAM_TW_currentItemChanged(ui->OAM_TW->topLevelItem(OAMIndex == object.m_subObjects.size() ? OAMIndex - 1 : OAMIndex), Q_NULLPTR);
    ui->OAM_TW->blockSignals(false);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update preview
    QGraphicsItem* item = m_previewOAMs[OAMIndex];
    m_previewGraphic->removeItem(item);
    delete item;
    m_previewOAMs.remove(OAMIndex);
    UpdateImageControlButtons();
}

void BNSpriteEditor::on_OAM_SB_Tile_valueChanged(int arg1)
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    BNSprite::SubObject& subObject = object.m_subObjects[OAMIndex];

    if (subObject.m_startTile == arg1 || !ui->OAM_SB_Tile->isEnabled()) return;
    subObject.m_startTile = arg1;

    QTreeWidgetItem *item = ui->OAM_TW->topLevelItem(OAMIndex);
    item->setText(0, QString::number(arg1));

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update OAM
    UpdateOAMThumbnail(subObject);

    // Update preview
    UpdatePreviewOAM(OAMIndex, subObject, true);

    // Update sub frame thumbnail
    UpdateSubFrameThumbnailFromObject(objectID);
}

void BNSpriteEditor::on_OAM_CB_Size_currentTextChanged(const QString &arg1)
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    BNSprite::SubObject& subObject = object.m_subObjects[OAMIndex];

    QStringList size = arg1.split('x');
    int sizeX = size[0].toInt();
    int sizeY = size[1].toInt();
    if ((subObject.m_sizeX == sizeX && subObject.m_sizeY == sizeY) || !ui->OAM_CB_Size->isEnabled()) return;
    subObject.m_sizeX = sizeX;
    subObject.m_sizeY = sizeY;

    QTreeWidgetItem *item = ui->OAM_TW->topLevelItem(OAMIndex);
    QString sizeString = QString::number(subObject.m_sizeX) + "x" + QString::number(subObject.m_sizeY);
    item->setText(1, sizeString);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update OAM preview
    UpdateOAMThumbnail(subObject);

    // Update preview
    UpdatePreviewOAM(OAMIndex, subObject, true);
    UpdateImageControlButtons();

    // Update sub frame thumbnail
    UpdateSubFrameThumbnailFromObject(objectID);
}

void BNSpriteEditor::on_OAM_SB_XPos_valueChanged(int arg1)
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    BNSprite::SubObject& subObject = object.m_subObjects[OAMIndex];

    if (subObject.m_posX == arg1 || !ui->OAM_SB_XPos->isEnabled()) return;
    subObject.m_posX = arg1;

    QTreeWidgetItem *item = ui->OAM_TW->topLevelItem(OAMIndex);
    QString posString = QString::number(subObject.m_posX) + "," + QString::number(subObject.m_posY);
    item->setText(2, posString);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update preview
    UpdatePreviewOAM(OAMIndex, subObject, false);
    UpdateImageControlButtons();

    // Update sub frame thumbnail
    UpdateSubFrameThumbnailFromObject(objectID);

    m_previewHighlight->setPos(subObject.m_posX + 128, subObject.m_posY + 128);
}

void BNSpriteEditor::on_OAM_SB_YPos_valueChanged(int arg1)
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    BNSprite::SubObject& subObject = object.m_subObjects[OAMIndex];

    if (subObject.m_posY == arg1 || !ui->OAM_SB_YPos->isEnabled()) return;
    subObject.m_posY = arg1;

    QTreeWidgetItem *item = ui->OAM_TW->topLevelItem(OAMIndex);
    QString posString = QString::number(subObject.m_posX) + "," + QString::number(subObject.m_posY);
    item->setText(2, posString);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update preview
    UpdatePreviewOAM(OAMIndex, subObject, false);
    UpdateImageControlButtons();

    // Update sub frame thumbnail
    UpdateSubFrameThumbnailFromObject(objectID);

    m_previewHighlight->setPos(subObject.m_posX + 128, subObject.m_posY + 128);
}

void BNSpriteEditor::on_OAM_CB_HFlip_toggled(bool checked)
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    BNSprite::SubObject& subObject = object.m_subObjects[OAMIndex];

    if (subObject.m_hFlip == checked || !ui->OAM_CB_HFlip->isEnabled()) return;
    subObject.m_hFlip = checked;

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update OAM
    UpdateOAMThumbnail(subObject);

    // Update preview
    UpdatePreviewOAM(OAMIndex, subObject, true);

    // Update sub frame thumbnail
    UpdateSubFrameThumbnailFromObject(objectID);
}

void BNSpriteEditor::on_OAM_CB_VFlip_toggled(bool checked)
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    BNSprite::SubObject& subObject = object.m_subObjects[OAMIndex];

    if (subObject.m_vFlip == checked || !ui->OAM_CB_VFlip->isEnabled()) return;
    subObject.m_vFlip = checked;

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update OAM
    UpdateOAMThumbnail(subObject);

    // Update preview
    UpdatePreviewOAM(OAMIndex, subObject, true);

    // Update sub frame thumbnail
    UpdateSubFrameThumbnailFromObject(objectID);
}

void BNSpriteEditor::on_OAM_Highlight_timeout()
{
    HighlightPreviewOAM();
}

//---------------------------------------------------------------------------
// Add new OAM
//---------------------------------------------------------------------------
void BNSpriteEditor::AddOAM(const BNSprite::SubObject &_subObject)
{
    // Add to list view
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->OAM_TW);
    item->setText(0, QString::number(_subObject.m_startTile));
    item->setText(1, QString::number(_subObject.m_sizeX) + "x" + QString::number(_subObject.m_sizeY));
    item->setText(2, QString::number(_subObject.m_posX) + "," + QString::number(_subObject.m_posY));

    // Add to preview
    AddPreviewOAM(_subObject);
}

//---------------------------------------------------------------------------
// Swap two OAMs
//---------------------------------------------------------------------------
void BNSpriteEditor::SwapOAM(int _oldID, int _newID)
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    iter_swap(object.m_subObjects.begin() + _oldID, object.m_subObjects.begin() + _newID);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Swap OAM in tree view
    QTreeWidgetItem* item0 = ui->OAM_TW->topLevelItem(_oldID);
    item0->setForeground(0, QColor(0,0,0));
    item0->setForeground(1, QColor(0,0,0));
    item0->setForeground(2, QColor(0,0,0));
    QTreeWidgetItem* item1 = ui->OAM_TW->topLevelItem(_newID);
    item1->setForeground(0, QColor(255,0,0));
    item1->setForeground(1, QColor(255,0,0));
    item1->setForeground(2, QColor(255,0,0));
    QString tempTile = item0->text(0);
    QString tempSize = item0->text(1);
    QString tempPos = item0->text(2);
    item0->setText(0, item1->text(0));
    item0->setText(1, item1->text(1));
    item0->setText(2, item1->text(2));
    item1->setText(0, tempTile);
    item1->setText(1, tempSize);
    item1->setText(2, tempPos);

    // Block signal to prevent loading animations again
    ui->OAM_TW->blockSignals(true);
    ui->OAM_TW->setCurrentItem(item1);
    ui->OAM_TW->blockSignals(false);

    // Update button
    ui->OAM_PB_Up->setEnabled(_newID > 0);
    ui->OAM_PB_Down->setEnabled(_newID < ui->OAM_TW->topLevelItemCount() - 1);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update preview (layer change)
    qSwap(m_previewOAMs[_oldID], m_previewOAMs[_newID]);
    m_previewOAMs[_oldID]->setZValue(_oldID);
    m_previewOAMs[_newID]->setZValue(_newID);
}

//---------------------------------------------------------------------------
// Update preview for current selected OAM
//---------------------------------------------------------------------------
void BNSpriteEditor::UpdateOAMThumbnail(const BNSprite::SubObject &_subObject)
{
    if (m_oamImage != Q_NULLPTR)
    {
        delete m_oamImage;
    }

    if (m_tilesetData.empty()) return;

    m_oamImage = new QImage(_subObject.m_sizeX, _subObject.m_sizeY, QImage::Format_Indexed8);
    int group = qMin(ui->Palette_SB_Group->value(), m_paletteGroups.size() - 1);
    PaletteGroup const& paletteGroup = m_paletteGroups[group];
    int index = qMin(ui->Palette_SB_Index->value(), paletteGroup.size() - 1);
    Palette palette = paletteGroup[index];
    palette[0] |= 0xFF000000; // undo transparency on first color
    m_oamImage->setColorTable(palette);
    m_oamImage->fill(0);

    DrawOAMInImage(_subObject, m_oamImage, _subObject.m_posX, _subObject.m_posY, m_tilesetData, true);

    m_oamGraphic->clear();
    m_oamGraphic->setSceneRect(0, 0, m_oamImage->width(), m_oamImage->height());
    m_oamGraphic->addPixmap(QPixmap::fromImage(*m_oamImage));
}

//---------------------------------------------------------------------------
// Preview signals
//---------------------------------------------------------------------------
void BNSpriteEditor::on_Preview_BG_currentTextChanged(const QString &arg1)
{
    if (arg1 != "Fixed Color")
    {
        m_previewBG->setPixmap(QPixmap(":/resources/" + arg1 + ".png"));
        ui->Preview_Color->setHidden(true);
    }
    else
    {
        QPalette pal = ui->Preview_Color->palette();
        QColor color = pal.color(QPalette::Base);
        QImage image = QImage(256, 256,QImage::Format_RGB888);
        image.fill(color.rgb());

        m_previewBG->setPixmap(QPixmap::fromImage(image));
        ui->Preview_Color->setHidden(false);
    }
}

void BNSpriteEditor::on_Preview_CB_Highlight_clicked(bool checked)
{
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    if (!checked)
    {
        m_previewHighlight->setVisible(false);
        m_highlightTimer->stop();
    }
    else if (OAMIndex != -1)
    {
        m_highlightUp = false;
        m_highlightAlpha = 255;
        m_highlightTimer->stop();
        m_highlightTimer->start(2);
        m_previewHighlight->setVisible(true);

        int objectID = ui->Object_Tabs->currentIndex();
        BNSprite::Object const& object = m_frame.m_objects[objectID];
        BNSprite::SubObject const& subObject = object.m_subObjects[OAMIndex];
        m_previewHighlight->setPos(subObject.m_posX + 128, subObject.m_posY + 128);
    }
}

void BNSpriteEditor::on_Preview_pressed(QPoint pos)
{
    // Preview screen is pressed, check if any OAM is selected
    int objectID = ui->Object_Tabs->currentIndex();
    if (objectID >= m_frame.m_objects.size()) return;

    BNSprite::Object& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    if (OAMIndex == -1 || OAMIndex >= m_previewOAMs.size()) return;

    // Clamp the pos
    pos.rx() = qBound(0, pos.x(), 255);
    pos.ry() = qBound(0, pos.y(), 255);

    BNSprite::SubObject& subObject = object.m_subObjects[OAMIndex];
    subObject.m_posX = pos.x() - 128;
    subObject.m_posY = pos.y() - 128;

    QTreeWidgetItem *item = ui->OAM_TW->topLevelItem(OAMIndex);
    QString posString = QString::number(subObject.m_posX) + "," + QString::number(subObject.m_posY);
    item->setText(2, posString);

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update preview
    UpdatePreviewOAM(OAMIndex, subObject, false);
    UpdateImageControlButtons();

    // Update sub frame thumbnail
    UpdateSubFrameThumbnailFromObject(objectID);

    m_previewHighlight->setPos(pos);
}

//---------------------------------------------------------------------------
// Add new OAM in the preview window
//---------------------------------------------------------------------------
void BNSpriteEditor::AddPreviewOAM(const BNSprite::SubObject &_subObject)
{
    QGraphicsPixmapItem* graphicsItem = m_previewGraphic->addPixmap(QPixmap::fromImage(QImage()));
    m_previewOAMs.push_back(graphicsItem);
    graphicsItem->setPos(_subObject.m_posX + 128, _subObject.m_posY + 128);
    graphicsItem->setZValue(m_previewOAMs.size() - 1);

    DrawPreviewOAM(graphicsItem, _subObject);
}

//---------------------------------------------------------------------------
// Draw OAM in the preview window
//---------------------------------------------------------------------------
void BNSpriteEditor::DrawPreviewOAM(QGraphicsPixmapItem *_graphicsItem, const BNSprite::SubObject &_subObject)
{
    QImage image = QImage(_subObject.m_sizeX, _subObject.m_sizeY, QImage::Format_Indexed8);
    int group = qMin(ui->Palette_SB_Group->value(), m_paletteGroups.size() - 1);
    PaletteGroup const& paletteGroup = m_paletteGroups[group];
    int index = qMin(ui->Palette_SB_Index->value(), paletteGroup.size() - 1);
    Palette const& palette = paletteGroup[index];
    image.setColorTable(palette);
    image.fill(0);

    DrawOAMInImage(_subObject, &image, _subObject.m_posX, _subObject.m_posY, m_tilesetData, false);
    _graphicsItem->setPixmap(QPixmap::fromImage(image));
}

//---------------------------------------------------------------------------
// Update one OAM in the preview window
//---------------------------------------------------------------------------
void BNSpriteEditor::UpdatePreviewOAM(int _oamID, BNSprite::SubObject const& _subObject, bool _redraw)
{
    if (_oamID >= m_previewOAMs.size()) return;

    QGraphicsPixmapItem* item = m_previewOAMs[_oamID];
    if (_redraw)
    {
        DrawPreviewOAM(item, _subObject);
    }

    // Reposition
    item->setPos(_subObject.m_posX + 128, _subObject.m_posY + 128);
}

//---------------------------------------------------------------------------
// Highlight one OAM in the preview window
//---------------------------------------------------------------------------
void BNSpriteEditor::HighlightPreviewOAM()
{
    int objectID = ui->Object_Tabs->currentIndex();
    if (objectID >= m_frame.m_objects.size())
    {
        m_previewHighlight->setVisible(false);
        m_highlightTimer->stop();
        return;
    }
    BNSprite::Object const& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());
    if (OAMIndex == -1 || OAMIndex >= m_previewOAMs.size())
    {
        m_previewHighlight->setVisible(false);
        m_highlightTimer->stop();
        return;
    }

    if (m_highlightUp)
    {
        if (++m_highlightAlpha == 255)
        {
            m_highlightUp = false;
        }
    }
    else
    {
        if (--m_highlightAlpha <= 20)
        {
            m_highlightUp = true;
        }
    }

    BNSprite::SubObject const& subObject = object.m_subObjects[OAMIndex];
    QImage image = QImage(subObject.m_sizeX, subObject.m_sizeY, QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); y++)
    {
        for (int x = 0; x < image.width(); x++)
        {
            if (x == 0 || x == image.width() - 1 || y == 0 || y == image.height() - 1)
            {
                image.setPixel(x, y, qRgba(255, 0, 0, m_highlightAlpha));
            }
            else
            {
                image.setPixel(x, y, 0);
            }
        }
    }
    m_previewHighlight->setPixmap(QPixmap::fromImage(image));
}

//---------------------------------------------------------------------------
// Image control signals
//---------------------------------------------------------------------------
void BNSpriteEditor::on_Image_PB_Up_pressed()
{
    m_moveDir = 0;
    MoveImage();
    m_moveTimer->start(500);
}

void BNSpriteEditor::on_Image_PB_Down_pressed()
{
    m_moveDir = 1;
    MoveImage();
    m_moveTimer->start(500);
}

void BNSpriteEditor::on_Image_PB_Left_pressed()
{
    m_moveDir = 2;
    MoveImage();
    m_moveTimer->start(500);
}

void BNSpriteEditor::on_Image_PB_Right_pressed()
{
    m_moveDir = 3;
    MoveImage();
    m_moveTimer->start(500);
}

void BNSpriteEditor::on_Image_PB_Up_released()
{
    m_moveDir = -1;
    m_moveTimer->stop();
}

void BNSpriteEditor::on_Image_PB_Down_released()
{
    m_moveDir = -1;
    m_moveTimer->stop();
}

void BNSpriteEditor::on_Image_PB_Left_released()
{
    m_moveDir = -1;
    m_moveTimer->stop();
}

void BNSpriteEditor::on_Image_PB_Right_released()
{
    m_moveDir = -1;
    m_moveTimer->stop();
}

void BNSpriteEditor::on_Image_PB_HFlip_clicked()
{
    FlipImage(false);
}

void BNSpriteEditor::on_Image_PB_VFlip_clicked()
{
    FlipImage(true);
}

void BNSpriteEditor::on_Image_Move_timeout()
{
    if (m_moveDir != -1)
    {
        MoveImage();
        m_moveTimer->start(30);
    }
}

void BNSpriteEditor::MoveImage()
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());

    for (int i = 0; i < object.m_subObjects.size(); i++)
    {
        BNSprite::SubObject& subObject = object.m_subObjects[i];
        switch(m_moveDir)
        {
            case 0: subObject.m_posY--; break;
            case 1: subObject.m_posY++; break;
            case 2: subObject.m_posX--; break;
            case 3: subObject.m_posX++; break;
            default: break;
        }

        QTreeWidgetItem *item = ui->OAM_TW->topLevelItem(i);
        QString posString = QString::number(subObject.m_posX) + "," + QString::number(subObject.m_posY);
        item->setText(2, posString);

        if (OAMIndex == i)
        {
            ui->OAM_SB_XPos->blockSignals(true);
            ui->OAM_SB_XPos->setValue(subObject.m_posX);
            ui->OAM_SB_XPos->blockSignals(false);
            ui->OAM_SB_YPos->blockSignals(true);
            ui->OAM_SB_YPos->setValue(subObject.m_posY);
            ui->OAM_SB_YPos->blockSignals(false);

            m_previewHighlight->setPos(subObject.m_posX + 128, subObject.m_posY + 128);
        }

        UpdatePreviewOAM(i, subObject, false);
    }

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update sub frame thumbnail
    UpdateSubFrameThumbnailFromObject(objectID);

    UpdateImageControlButtons();
}

void BNSpriteEditor::FlipImage(bool _vertical)
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object& object = m_frame.m_objects[objectID];
    int OAMIndex = ui->OAM_TW->indexOfTopLevelItem(ui->OAM_TW->currentItem());

    for (int i = 0; i < object.m_subObjects.size(); i++)
    {
        BNSprite::SubObject& subObject = object.m_subObjects[i];
        if (_vertical)
        {
            subObject.m_vFlip = !subObject.m_vFlip;
            subObject.m_posY = 0 - subObject.m_posY - subObject.m_sizeY;
        }
        else
        {
            subObject.m_hFlip = !subObject.m_hFlip;
            subObject.m_posX = 0 - subObject.m_posX - subObject.m_sizeX;
        }

        QTreeWidgetItem *item = ui->OAM_TW->topLevelItem(i);
        QString posString = QString::number(subObject.m_posX) + "," + QString::number(subObject.m_posY);
        item->setText(2, posString);

        if (OAMIndex == i)
        {
            if (_vertical)
            {
                ui->OAM_CB_VFlip->blockSignals(true);
                ui->OAM_CB_VFlip->toggle();
                ui->OAM_CB_VFlip->blockSignals(false);
            }
            else
            {
                ui->OAM_CB_HFlip->blockSignals(true);
                ui->OAM_CB_HFlip->toggle();
                ui->OAM_CB_HFlip->blockSignals(false);
            }

            m_previewHighlight->setPos(subObject.m_posX + 128, subObject.m_posY + 128);
        }

        UpdatePreviewOAM(i, subObject, true);
    }

    int animID = ui->Anim_LW->currentRow();
    int frameID = ui->Frame_LW->currentRow();
    m_sprite.ReplaceFrame(animID, frameID, m_frame);

    // Update thumbnail
    if (objectID == m_frame.m_subAnimations[0].m_subFrames[0].m_objectIndex)
    {
        UpdateFrameThumbnail(frameID);
    }

    // Update sub frame thumbnail
    UpdateSubFrameThumbnailFromObject(objectID);

    UpdateImageControlButtons();
}

void BNSpriteEditor::UpdateImageControlButtons()
{
    int objectID = ui->Object_Tabs->currentIndex();
    BNSprite::Object const& object = m_frame.m_objects[objectID];

    int minX = 127;
    int maxX = -128;
    int minY = 127;
    int maxY = -128;
    int maxXwidth = -128;
    int maxYheight = -128;

    for (BNSprite::SubObject const& subObject : object.m_subObjects)
    {
        minX = qMin(minX, (int)subObject.m_posX);
        minY = qMin(minY, (int)subObject.m_posY);
        maxX = qMax(maxX, (int)subObject.m_posX);
        maxY = qMax(maxY, (int)subObject.m_posY);
        maxXwidth = qMax(maxXwidth, subObject.m_posX + subObject.m_sizeX - 1);
        maxYheight = qMax(maxYheight, subObject.m_posY + subObject.m_sizeY - 1);
    }

    // Image control
    ui->Image_PB_HFlip->setEnabled(maxXwidth <= 127);
    ui->Image_PB_VFlip->setEnabled(maxYheight <= 127);
    ui->Image_PB_Up->setEnabled(minY > -128);
    ui->Image_PB_Down->setEnabled(maxY < 127);
    ui->Image_PB_Left->setEnabled(minX > -128);
    ui->Image_PB_Right->setEnabled(maxX < 127);
}

//---------------------------------------------------------------------------
// Load all custom sprite tileset and palette data
//---------------------------------------------------------------------------
void BNSpriteEditor::on_CSM_BuildSprite_pressed(int tilesetCount)
{
    if (m_csm == Q_NULLPTR) return;

    ResetProgram();

    // Create tileset data
    for (int i = 0; i < tilesetCount; i++)
    {
        std::vector<uint8_t> data;
        m_csm->GetRawTilesetData(i, data);
        m_sprite.ImportCustomTileset(data);
    }

    // Create palette data
    m_paletteGroups = m_csm->GetPaletteData();
    for (PaletteGroup& group : m_paletteGroups)
    {
        for (Palette& palette : group)
        {
            // Add transparency back
            palette[0] &= 0x00FFFFFF;
        }
    }
    ui->Palette_SB_Group->setValue(0);
    ui->Palette_SB_Group->setMaximum(m_paletteGroups.size() - 1);

    ui->Anim_PB_New->setEnabled(ui->Anim_LW->count() < 255);
    on_CSM_BuildCheckButton_pressed();
}

void BNSpriteEditor::on_CSM_BuildCheckButton_pressed()
{
    if (m_csm != Q_NULLPTR)
    {
        m_csm->SetPushAnimEnabled(ui->Anim_LW->count() < 255);
        m_csm->SetPushFrameEnabled(ui->Frame_LW->count() > 0);
    }
}

void BNSpriteEditor::on_CSM_BuildPushFrame_pressed(int frameID, bool newAnim)
{
    // We can't push frame is no animation is selected
    if (!newAnim && ui->Frame_LW->count() == 0)
    {
        return;
    }

    // Get frame data
    BNSprite::Frame frame;
    m_csm->GetFrameData(frameID, frame);

    if (!newAnim)
    {
        // New frame for current animation
        int editorAnimID = ui->Anim_LW->currentRow();
        int editorFrameID = m_sprite.NewFrame(editorAnimID);
        m_sprite.ReplaceFrame(editorAnimID, editorFrameID, frame);
        AddFrameThumbnail(editorAnimID, editorFrameID);
        ui->Frame_LW->setCurrentRow(editorFrameID);
    }
    else
    {
        int editorAnimID = m_sprite.NewAnimation();
        m_sprite.ReplaceFrame(editorAnimID, 0, frame);
        AddAnimationThumbnail(editorAnimID);
        ui->Anim_LW->setCurrentRow(editorAnimID);
        ui->Frame_LW->setCurrentRow(0);
    }
}

void BNSpriteEditor::on_CSM_LoadProject_pressed(QString file, uint32_t saveVersion, qint64 skipByte, int tilesetCount)
{
    ResetProgram();

    // Create tileset data
    for (int i = 0; i < tilesetCount; i++)
    {
        std::vector<uint8_t> data;
        m_csm->GetRawTilesetData(i, data);
        m_sprite.ImportCustomTileset(data);
    }

    QFile input(file);
    input.open(QIODevice::ReadOnly);
    QDataStream in(&input);
    in.setVersion(QDataStream::Qt_5_13);

    input.seek(skipByte);

    // Load palette
    in >> m_paletteGroups;
    ui->Palette_SB_Group->setValue(0);
    ui->Palette_SB_Group->setMaximum(m_paletteGroups.size() - 1);

    int animationCount = 0;
    in >> animationCount;

    // Animation
    for (int animID = 0; animID < animationCount; animID++)
    {
        bool loop = false;
        in >> loop;

        // Frame
        int frameCount = 0;
        in >> frameCount;

        for (int frameID = 0; frameID < frameCount; frameID++)
        {
            BNSprite::Frame frame;
            in >> frame.m_specialFlag0;
            in >> frame.m_specialFlag1;
            in >> frame.m_tilesetID;
            in >> frame.m_paletteGroupID;
            in >> frame.m_delay;

            // Sub animation
            int subAnimCount = 0;
            in >> subAnimCount;

            for (int subAnimID = 0; subAnimID < subAnimCount; subAnimID++)
            {
                BNSprite::SubAnimation subAnim;
                in >> subAnim.m_loop;

                // Sub frame
                int subFrameCount = 0;
                in >> subFrameCount;

                for (int subFrameID = 0; subFrameID < subFrameCount; subFrameID++)
                {
                    BNSprite::SubFrame subFrame;
                    in >> subFrame.m_objectIndex;
                    in >> subFrame.m_delay;
                    subAnim.m_subFrames.push_back(subFrame);
                }
                frame.m_subAnimations.push_back(subAnim);
            }

            // Objects
            int objectCount = 0;
            in >> objectCount;

            for (int objectID = 0; objectID < objectCount; objectID++)
            {
                BNSprite::Object object;
                in >> object.m_paletteIndex;

                // Sub object
                int subObjectCount = 0;
                in >> subObjectCount;

                for (int subObjectID = 0; subObjectID < subObjectCount; subObjectID++)
                {
                    BNSprite::SubObject subObject;

                    if (saveVersion >= 2)
                    {
                        // ver.2 afterwards uses uint16_t
                        in >> subObject.m_startTile;
                    }
                    else
                    {
                        // ver.1 uses uint8_t
                        uint8_t startTile;
                        in >> startTile;
                        subObject.m_startTile = startTile;
                    }

                    in >> subObject.m_posX;
                    in >> subObject.m_posY;
                    in >> subObject.m_hFlip;
                    in >> subObject.m_vFlip;
                    in >> subObject.m_sizeX;
                    in >> subObject.m_sizeY;
                    object.m_subObjects.push_back(subObject);
                }
                frame.m_objects.push_back(object);
            }

            if (frameID == 0)
            {
                m_sprite.NewAnimation();
                m_sprite.ReplaceFrame(animID, 0, frame);
                AddAnimationThumbnail(animID);
            }
            else
            {
                // New frame for current animation
                m_sprite.NewFrame(animID);
                m_sprite.ReplaceFrame(animID, frameID, frame);
            }
        }

        m_sprite.SetAnimationLoop(animID, loop);
        qApp->processEvents();
    }

    ui->Anim_PB_New->setEnabled(ui->Anim_LW->count() < 255);
    on_CSM_BuildCheckButton_pressed();
}

void BNSpriteEditor::on_CSM_SaveProject_pressed(QString file)
{
    QFile output(file);
    output.open(QIODevice::Append);
    QDataStream out(&output);
    out.setVersion(QDataStream::Qt_5_13);

    // Save palette
    out << m_paletteGroups;

    // Animation
    int animationCount = m_sprite.GetAnimationCount();
    out << animationCount;

    for (int animID = 0; animID < animationCount; animID++)
    {
        out << m_sprite.GetAnimationLoop(animID);

        // Frame
        vector<BNSprite::Frame> frames;
        m_sprite.GetAnimationFrames(animID, frames);
        out << frames.size();

        for (int frameID = 0; frameID < frames.size(); frameID++)
        {
            BNSprite::Frame const& frame = frames[frameID];
            out << frame.m_specialFlag0;
            out << frame.m_specialFlag1;
            out << frame.m_tilesetID;
            out << frame.m_paletteGroupID;
            out << frame.m_delay;

            // Sub animation
            out << frame.m_subAnimations.size();
            for (BNSprite::SubAnimation const& subAnim : frame.m_subAnimations)
            {
                out << subAnim.m_loop;

                // Sub frame
                out << subAnim.m_subFrames.size();
                for (BNSprite::SubFrame const& subFrame : subAnim.m_subFrames)
                {
                    out << subFrame.m_objectIndex;
                    out << subFrame.m_delay;
                }
            }

            // Objects
            out << frame.m_objects.size();
            for (BNSprite::Object const& object : frame.m_objects)
            {
                out << object.m_paletteIndex;

                // Sub object
                out << object.m_subObjects.size();
                for (BNSprite::SubObject const& subObject : object.m_subObjects)
                {
                    out << subObject.m_startTile;
                    out << subObject.m_posX;
                    out << subObject.m_posY;
                    out << subObject.m_hFlip;
                    out << subObject.m_vFlip;
                    out << subObject.m_sizeX;
                    out << subObject.m_sizeY;
                }
            }
        }
    }
}
