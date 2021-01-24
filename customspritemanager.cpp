#include "customspritemanager.h"
#include "ui_customspritemanager.h"

#define DEBUG_TILE_SAMPLING 1
#define USE_FIRST_FIT_OAM 0

// Save version
static const uint32_t c_saveVersion = 3;
// 1: initial save version
// 2: fix for m_startTile changed from uint8_t to uint16_t
// 3: added m_evenOAM

//---------------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------------
CustomSpriteManager::CustomSpriteManager(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CustomSpriteManager)
{
    ui->setupUi(this);
    this->resize(this->size() - QSize(0, 205));
    //layout()->setSizeConstraint(QLayout::SetFixedSize);

    // Insert all possible tile size, from largest to lowest
    m_oamSizesMap[SIZE_64x64]   = QSize(8,8);    // 64x64  (64 tiles)
    m_oamSizesMap[SIZE_64x32]   = QSize(8,4);    // 64x32  (32 tiles)
    m_oamSizesMap[SIZE_32x64]   = QSize(4,8);    // 32x64  (32 tiles)
    m_oamSizesMap[SIZE_32x32]   = QSize(4,4);    // 32x32  (16 tiles)
    m_oamSizesMap[SIZE_32x16]   = QSize(4,2);    // 32x16  (8 tiles)
    m_oamSizesMap[SIZE_16x32]   = QSize(2,4);    // 16x32  (8 tiles)
    m_oamSizesMap[SIZE_16x16]   = QSize(2,2);    // 16x16  (4 tiles)
    m_oamSizesMap[SIZE_32x8]    = QSize(4,1);    // 32x8   (4 tiles)
    m_oamSizesMap[SIZE_8x32]    = QSize(1,4);    // 8x32   (4 tiles)
    m_oamSizesMap[SIZE_16x8]    = QSize(2,1);    // 16x8   (2 tiles)
    m_oamSizesMap[SIZE_8x16]    = QSize(1,2);    // 8x16   (2 tiles)
    m_oamSizesMap[SIZE_8x8]     = QSize(1,1);    // 8x8    (1 tile)

    ui->Transparency_Color->installEventFilter(this);
    m_resourceGraphic = new QGraphicsScene(this);
    m_resourceGraphic->setSceneRect(0, 0, 256, 256);
    ui->Resources_GV->setScene(m_resourceGraphic);
    on_Resources_PB_Sample_clicked();

    m_previewGraphic = new QGraphicsScene(this);
    m_previewGraphic->setSceneRect(0, 0, 256, 256);
    m_previewBG = m_previewGraphic->addPixmap(QPixmap(":/resources/SizeGrid.png"));
    m_previewBG->setZValue(-1);
    ui->Preview_GV->setScene(m_previewGraphic);

    ui->Palette_GV->setMouseEventEnabled(false);
    ui->Palette_Warning->setHidden(true);
    connect(ui->Palette_GV, SIGNAL(colorChanged(int,int,QRgb)), this, SLOT(on_Palette_Color_changed(int,int,QRgb)));
    connect(ui->Preview_GV, SIGNAL(previewScreenPressed(QPoint)), this, SLOT(on_Preview_pressed(QPoint)));

    m_previewHighlight = m_previewGraphic->addPixmap(QPixmap::fromImage(QImage()));
    m_previewHighlight->setZValue(1000);
    m_highlightUp = true;
    m_highlightAlpha = 0;
    m_highlightTimer = new QTimer(this);
    connect(m_highlightTimer, SIGNAL(timeout()), SLOT(on_Preview_Highlight_timeout()));

    ui->Layer_LW->installEventFilter(this);
    ui->Layer_LW->SetAcceptFromSource(ui->Resources_LW);
    connect(ui->Layer_LW, SIGNAL(dropEventSignal(bool)), this, SLOT(on_Layer_LW_layerUpdated(bool)));

    connect(ui->Frame_LW, SIGNAL(keyUpDownSignal(QListWidgetItem*)), this, SLOT(on_Frame_LW_itemPressed(QListWidgetItem*)));

    ResetProgram();
}

//---------------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------------
CustomSpriteManager::~CustomSpriteManager()
{
    delete ui;
}

//---------------------------------------------------------------------------
// Handling mouse events
//---------------------------------------------------------------------------
bool CustomSpriteManager::eventFilter(QObject *object, QEvent *event)
{
    // Changing a color of a color block
    QWidget* widget = qobject_cast<QWidget*>(object);
    if (event->type() == QEvent::FocusIn && widget == ui->Transparency_Color)
    {
        widget->clearFocus();
        QPalette pal = widget->palette();
        QColor color = pal.color(QPalette::Base);
        QColor newColor = QColorDialog::getColor(color, this, "Pick a Color");
        if (newColor.isValid())
        {
            // Update color
            pal.setColor(QPalette::Base, newColor.rgb());
            widget->setPalette(pal);
        }
        return true;
    }
    else if (event->type() == QEvent::ChildRemoved && widget == ui->Layer_LW)
    {
        // 1,2,3,4,5   left shifted = take 1st, insert last
        // 1,3,4,2,5   right shifted = take last, insert first
        //   ^ ^ ^

        int diffBegin = -1;
        int diffEnd = m_layerItems.count() - 1;

        for (int i = 0; i < m_layerItems.count(); i++)
        {
            if (diffBegin == -1)
            {
                // Find the first item that is different
                if (ui->Layer_LW->item(i) != m_layerItems[i])
                {
                    diffBegin = i;
                }
            }
            else
            {
                // Find the last item that is different
                if (ui->Layer_LW->item(i) == m_layerItems[i])
                {
                    diffEnd = i - 1;
                    break;
                }
            }
        }

        if (diffBegin == -1)
        {
            // No change
            return false;
        }

        if (diffBegin == diffEnd)
        {
            // Item dragged and dropped at the same place, pointer updated
            m_layerItems[diffBegin] = ui->Layer_LW->item(diffBegin);
            on_Layer_LW_currentItemChanged(ui->Layer_LW->item(diffBegin), Q_NULLPTR);
            return true;
        }

        // Find out if the area is left or right shifted
        bool insertBegin = (m_layerItems[diffBegin] == ui->Layer_LW->item(diffBegin + 1));
        int take = insertBegin ? diffEnd : diffBegin;
        int insert = insertBegin ? diffBegin : diffEnd;

        // Item pointer is new, not retained
        m_layerItems.remove(take);
        m_layerItems.insert(insert, ui->Layer_LW->item(insert));

        Layer layer = m_editingLayers[take];
        m_editingLayers.remove(take);
        m_editingLayers.insert(insert, layer);

        QGraphicsPixmapItem* graphicsItem = m_previewPixmaps[take];
        m_previewPixmaps.remove(take);
        m_previewPixmaps.insert(insert, graphicsItem);

        // Update preview z-layer
        UpdatePreviewZValues();

        qDebug() << "take" << take << "insert" << insert;
        on_Layer_LW_currentItemChanged(ui->Layer_LW->item(insert), ui->Layer_LW->item(take));

        m_layerEdited = true;
        UpdateStatus();
        return true;
    }

    return false;
}

void CustomSpriteManager::closeEvent(QCloseEvent *event)
{
    if (!m_paletteGroups[0].empty())
    {
        QMessageBox::StandardButton resBtn = QMessageBox::Yes;
        QString message = "Close Custom Sprite Manager?\nAll unsaved changes will be lost, including built sprite data!";
        resBtn = QMessageBox::warning(this, "Close", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resBtn == QMessageBox::No)
        {
            event->ignore();
            return;
        }
    }

    ResetProgram();
    ui->Palette_GV->closeInfo();
    event->accept();
}

//---------------------------------------------------------------------------
// Action slots
//---------------------------------------------------------------------------
void CustomSpriteManager:: on_actionLoad_Project_triggered()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString file = QFileDialog::getOpenFileName(this, tr("Load Project"), path, "Custom Sprite Project (*.csp)");
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    QFile input(file);
    input.open(QIODevice::ReadOnly);
    QDataStream in(&input);
    in.setVersion(QDataStream::Qt_5_13);

    ResetProgram();

    // Load save version
    uint32_t saveVersion = 0;
    in >> saveVersion;

    // Load palette
    in >> m_paletteGroups;
    ui->Palette_SB_Group->blockSignals(true);
    ui->Palette_SB_Group->setMaximum(m_paletteGroups.size() - 1);
    ui->Palette_SB_Group->blockSignals(false);
    ui->Palette_SB_Index->setEnabled(true);
    ui->Resources_PB_Add->setEnabled(true);
    on_Palette_SB_Group_valueChanged(0);

    // Load resources
    int resourceSize = 0;
    in >> resourceSize;
    m_resources.reserve(resourceSize);

    for (int i = 0; i < resourceSize; i++)
    {
        Resource resource;
        in >> resource.m_name;
        resource.m_image = new QImage();
        in >> *resource.m_image;
        in >> resource.m_palette;
        in >> resource.m_paletteOwnerships;

        resource.m_thumbnail = new QImage();
        in >> *resource.m_thumbnail;
        // skipped: m_selectable

        in >> resource.m_croppedStartPos;
        resource.m_croppedImage = new QImage();
        in >> *resource.m_croppedImage;
        in >> resource.m_tileStartPos;
        in >> resource.m_usedTiles;
        in >> resource.m_tileAllowance;

        int oamListSize = 0;
        in >> oamListSize;
        for (int j = 0; j < oamListSize; j++)
        {
            OAMInfo info;
            int oamSize = 0;
            in >> oamSize;
            info.m_oamSize = (OAMSize)oamSize;
            in >> info.m_topLeft;
            resource.m_oamInfoList.push_back(info);
        }

        in >> resource.m_tileCount;
        in >> resource.m_forceSingleOAM;

        m_resources.push_back(resource);
        m_resourceNamesIDMap.insert(resource.m_name, i);

        QListWidgetItem* item = new QListWidgetItem(QIcon(QPixmap::fromImage(*resource.m_thumbnail)), resource.m_name);
        ui->Resources_LW->addItem(item);

        ui->Resources_PB_GenAll->setEnabled(true);
        qApp->processEvents();
    }
    SetResourcesSelectable();

    // Load frame
    int frameCount = 0;
    in >> frameCount;
    for (int i = 0; i < frameCount; i++)
    {
        Frame frame;
        in >> frame.m_palGroup;
        in >> frame.m_palIndex;

        int shadowType = 0;
        in >> shadowType;
        frame.m_shadowType = (ShadowType)shadowType;

        int layerCount = 0;
        in >> layerCount;
        for (int j = 0; j < layerCount; j++)
        {
            Layer layer;
            in >> layer.m_resourceName;
            in >> layer.m_pos;
            in >> layer.m_hFlip;
            in >> layer.m_vFlip;
            frame.m_layers.push_back(layer);
        }

        m_frames.push_back(frame);

        // Thumbnail
        QPixmap pixmap;
        in >> pixmap;
        QListWidgetItem* item = new QListWidgetItem(QIcon(pixmap), "Frame " + QString::number(i));
        ui->Frame_LW->addItem(item);

        ui->Build_PB_Sprite->setEnabled(true);
        qApp->processEvents();
    }

    in >> m_emptyFrame;
    if (m_emptyFrame)
    {
        ui->Frame_LW->item(ui->Frame_LW->count() - 1)->setText("Empty Frame");
    }

    if (saveVersion >= 3)
    {
        in >> m_evenOAM;
    }

    // Load tileset
    int tilesetCount = 0;
    in >> tilesetCount;
    for (int i = 0; i < tilesetCount; i++)
    {
        Tileset tileset;
        in >> tileset.m_palGroup;
        in >> tileset.m_palIndex;
        in >> tileset.m_reserveShadow;
        in >> tileset.m_resourceIDs;
        in >> tileset.m_tileData;
        m_tilesets.push_back(tileset);
    }
    in >> m_frameToTilesetMap;

    qint64 pos = input.pos();
    input.close();

    if (tilesetCount > 0)
    {
        emit LoadProjectSignal(file, saveVersion, pos, m_tilesets.size());

        ui->Frame_LW->setCurrentRow(-1);
        BuildLayoutChange(true);
    }

    UpdateStatus();
}

void CustomSpriteManager::on_actionSave_Project_triggered()
{
    if (m_paletteGroups[0].empty()) return;

    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString file = QFileDialog::getSaveFileName(this, tr("Save Project"), path, "Custom Sprite Project (*.csp)");
    if (file == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(file);
    m_path = info.dir().absolutePath();

    QFile output(file);
    output.open(QIODevice::WriteOnly);
    QDataStream out(&output);
    out.setVersion(QDataStream::Qt_5_13);

    // Save version
    out << c_saveVersion;

    // Save palette
    out << m_paletteGroups;

    // Save resource
    out << m_resources.size();
    for (Resource const& resource : m_resources)
    {
        out << resource.m_name;
        out << *resource.m_image;
        out << resource.m_palette;
        out << resource.m_paletteOwnerships;

        out << *resource.m_thumbnail;
        // skipped: m_selectable

        out << resource.m_croppedStartPos;
        out << *resource.m_croppedImage;
        out << resource.m_tileStartPos;
        out << resource.m_usedTiles;
        out << resource.m_tileAllowance;

        out << resource.m_oamInfoList.size();
        for (OAMInfo const& info : resource.m_oamInfoList)
        {
            out << (int)info.m_oamSize;
            out << info.m_topLeft;
        }

        out << resource.m_tileCount;
        out << resource.m_forceSingleOAM;
    }

    // Save frame
    out << m_frames.size();
    for (int i = 0; i < m_frames.size(); i++)
    {
        Frame const& frame = m_frames[i];
        out << frame.m_palGroup;
        out << frame.m_palIndex;
        out << frame.m_shadowType;

        out << frame.m_layers.size();
        for (Layer const& layer : frame.m_layers)
        {
            out << layer.m_resourceName;
            out << layer.m_pos;
            out << layer.m_hFlip;
            out << layer.m_vFlip;
        }

        // thumbnail
        out << ui->Frame_LW->item(i)->icon().pixmap(84,84);
    }

    out << m_emptyFrame;
    out << m_evenOAM;

    // Save tileset
    out << m_tilesets.size();
    for (Tileset const& tileset : m_tilesets)
    {
        out << tileset.m_palGroup;
        out << tileset.m_palIndex;
        out << tileset.m_reserveShadow;
        out << tileset.m_resourceIDs;
        out << tileset.m_tileData;
    }
    out << m_frameToTilesetMap;

    output.close();
    emit SaveProjectSignal(file);
}

//---------------------------------------------------------------------------
// Resetting all buttons and data
//---------------------------------------------------------------------------
void CustomSpriteManager::ResetProgram()
{
    ResetBuild();
    BuildLayoutChange(false);
    ui->Build_PB_Sprite->setEnabled(false);
    m_emptyFrame = false;
    m_evenOAM = false;

    // Reset frames
    m_currentFrame = -1;
    m_frames.clear();
    ui->Frame_LW->clear();
    ui->Frame_PB_Delete->setEnabled(false);

    // Init palette with at least one group
    m_paletteGroups.clear();
    m_paletteGroups.push_back(PaletteGroup());

    ResetLayer();

    // Reset palette
    ui->Palette_GV->clear();
    ui->Palette_PB_NewGroup->setEnabled(false);
    ui->Palette_PB_DelGroup->setEnabled(false);
    ui->Palette_PB_NewPal->setEnabled(true);
    ui->Palette_PB_DelPal->setEnabled(false);
    ui->Palette_SB_Index->setEnabled(false);

    ui->Palette_SB_Group->blockSignals(true);
    ui->Palette_SB_Group->setMaximum(0);
    ui->Palette_SB_Group->blockSignals(false);
    ui->Palette_SB_Index->blockSignals(true);
    ui->Palette_SB_Index->setMaximum(0);
    ui->Palette_SB_Index->blockSignals(false);

    // Reset resources
    m_resources.clear();
    m_resourceNamesIDMap.clear();
    ui->Resources_LW->clear();
    ui->Resources_PB_Add->setEnabled(false);
    ui->Resources_PB_Delete->setEnabled(false);
    ui->Resources_PB_GenAll->setEnabled(false);
    ResetOAM();

    UpdateStatus();
}

void CustomSpriteManager::ResetOAM()
{
    m_resourceGraphic->clear();
    ui->Resources_SB_64->setEnabled(false);
    ui->Resources_SB_32->setEnabled(false);
    ui->Resources_SB_16->setEnabled(false);
    ui->Resources_SB_8->setEnabled(false);
    ui->Resources_SB_4->setEnabled(false);
    ui->Resources_CB_SingleOAM->setEnabled(false);
}

void CustomSpriteManager::ResetLayer()
{
    m_layerEdited = false;
    m_layerItems.clear();
    m_editingLayers.clear();
    ui->Layer_LW->clear();
    ui->Layer_SB_X->setEnabled(false);
    ui->Layer_SB_Y->setEnabled(false);
    ui->Layer_CB_HFlip->setEnabled(false);
    ui->Layer_CB_VFlip->setEnabled(false);
    ui->Layer_PB_Delete->setEnabled(false);
    ui->Layer_PB_Save->setEnabled(false);
    ui->Layer_PB_Update->setEnabled(false);

    // Preview
    for(QGraphicsItem* item : m_previewPixmaps)
    {
        m_previewGraphic->removeItem(item);
        delete item;
    }
    m_previewPixmaps.clear();
    m_previewHighlight->setVisible(false);
    m_highlightTimer->stop();

    // Re-enable palette widgets
    EnablePaletteWidgets();

    UpdateStatus();
}

void CustomSpriteManager::ResetBuild()
{
    // Clear build related variables
    m_tilesets.clear();
    m_frameToTilesetMap.clear();
    m_cachedBNFrames.clear();

    ui->Build_PB_Anim->setEnabled(false);
    ui->Build_PB_Frame->setEnabled(false);
}

void CustomSpriteManager::BuildLayoutChange(bool buildComplete)
{
    ui->Build_Control_Group->setHidden(!buildComplete);
    ui->Build_PB_Sprite->setHidden(buildComplete);

    ui->Frame_PB_Delete->setHidden(buildComplete);

    ui->Preview_X->setHidden(buildComplete);
    ui->Preview_Y->setHidden(buildComplete);
    ui->Label_XY->setHidden(buildComplete);

    ui->Layer_RB_Reserved->setEnabled(!buildComplete);
    ui->Layer_RB_First->setEnabled(!buildComplete);
    ui->Layer_RB_None->setEnabled(!buildComplete);
    ui->Layer_PB_Save->setHidden(buildComplete);
    ui->Layer_PB_Delete->setHidden(buildComplete);

    ui->GB_Palette->setHidden(buildComplete);
    ui->GB_Transparency->setHidden(buildComplete);
    ui->GB_Resources->setHidden(buildComplete);

    if (!ui->Resource_Sample_Group->isHidden())
    {
        on_Resources_PB_Sample_clicked();
    }

    this->setMinimumWidth(buildComplete ? 460 : 751);
    this->setMaximumWidth(buildComplete ? 460 : 751);

    if (buildComplete)
    {
        qApp->processEvents();
        QSize size = this->size();
        size.setHeight(640);
        this->resize(size);
    }
}


//---------------------------------------------------------------------------
// Show some info for user to guide them
//---------------------------------------------------------------------------
void CustomSpriteManager::UpdateStatus(QString const& status, QColor color)
{
    // Text override
    if (status.size() != 0)
    {
        ui->Status->setText(status);
    }
    else
    {
        // Auto text
        QString text;
        if (m_layerEdited)
        {
            QString errorLayer;
            if (!CheckLayerValidOAM(m_editingLayers, errorLayer))
            {
                color = QColor(255,0,0);
                text = "Layer \"" + errorLayer + "\" has OAM outside allowed area!";

                ui->Layer_PB_Update->setEnabled(false);
                ui->Layer_PB_Save->setEnabled(false);
            }
            else
            {
                color = QColor(255,0,0);
                text = "You have unsaved Frame Layer changes!";

                ui->Layer_PB_Update->setEnabled(m_currentFrame >= 0);
                ui->Layer_PB_Save->setEnabled(true);
            }
        }
        else if (!m_frameToTilesetMap.empty())
        {
            text = "Select a frame to push to Sprite Editor! You can also double-click!";
        }
        else if (ui->Frame_LW->count() > 0)
        {
            color = QColor(0,170,0);
            text = "Ready to build sprite!";
        }
        else if (ui->Resources_LW->count() > 0)
        {
            text = "Select compatible palette, add resources to Frame Layer by double-click or drag-and-drop!";
        }
        else if (!m_paletteGroups[0].empty())
        {
            text = "Add resources that have an existing palette.";
        }
        else
        {
            text = "Start by adding palettes from .PNG files!";
        }
        ui->Status->setText(text);
    }

    // Set color
    QString styleSheet = "color: rgb("
            + QString::number(color.red()) + " ,"
            + QString::number(color.green()) + " ,"
            + QString::number(color.blue()) + ");";
    ui->Status->setStyleSheet(styleSheet);
}

//---------------------------------------------------------------------------
// Palette slots
//---------------------------------------------------------------------------
void CustomSpriteManager::on_Palette_PB_NewPal_clicked()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QStringList files = QFileDialog::getOpenFileNames(this, tr("Add Palettes"), path, "PNG Images (*.png)");
    if (files.empty()) return;

    // Save directory
    QFileInfo info(files[0]);
    m_path = info.dir().absolutePath();

    if (files.size() > 256)
    {
        QMessageBox::critical(this, "Error", "You cannot add more than 256 palettes in the same palette group!");
        return;
    }

    int group = ui->Palette_SB_Group->value();
    if (m_paletteGroups[group].size() + files.size() > 256)
    {
        QMessageBox::critical(this, "Error", "Maximum no. of palette is reached (256), please create new group instead!");
        return;
    }

    bool success = GeneratePaletteFromFiles(files, group);
    if (success)
    {
        on_Palette_SB_Group_valueChanged(group);
    }

    UpdateStatus();
}

void CustomSpriteManager::on_Palette_PB_DelPal_clicked()
{
    int group = ui->Palette_SB_Group->value();
    int index = ui->Palette_SB_Index->value();

    // Delete group if only one palette left, but still have other groups
    PaletteGroup const& paletteGroup = m_paletteGroups[group];
    if (paletteGroup.size() == 1 && m_paletteGroups.size() > 1)
    {
        on_Palette_PB_DelGroup_clicked();
        return;
    }

    QVector<int> resourceIDs;
    if (GetResourceIDsForPalette(resourceIDs, group, index, true))
    {
        QMessageBox::StandardButton resBtn = QMessageBox::Yes;
        QString message = "Do you wish to delete palette index " + QString::number(index) + " and resources\nthat can only be used by this palette?";
        resBtn = QMessageBox::warning(this, "Delete Palette", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resBtn == QMessageBox::No)
        {
            return;
        }
    }

    // This is the very last palette,  just reset the program
    if (m_paletteGroups.size() == 1 && paletteGroup.size() == 1)
    {
        ResetProgram();
        return;
    }

    DeletePalette(group, index);

    // Reload next palette if this is not the last palette
    ui->Palette_SB_Index->blockSignals(true);
    ui->Palette_SB_Index->setMaximum(qMin(paletteGroup.size() - 1, 255));
    ui->Palette_SB_Index->blockSignals(false);
    ui->Palette_PB_NewPal->setEnabled(paletteGroup.size() < 256);
    if (index > paletteGroup.size() - 1)
    {
        index--;
        ui->Palette_GV->setPaletteSelected(index);
    }
    on_Palette_SB_Index_valueChanged(index);

    EnablePaletteWidgets();
    UpdateResourceNameIDMap();

    UpdateStatus();
}

void CustomSpriteManager::on_Palette_PB_NewGroup_clicked()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QStringList files = QFileDialog::getOpenFileNames(this, tr("Add Palettes"), path, "PNG Images (*.png)");
    if (files.empty()) return;

    // Save directory
    QFileInfo info(files[0]);
    m_path = info.dir().absolutePath();

    if (files.size() > 256)
    {
        QMessageBox::critical(this, "Error", "You cannot add more than 256 palettes in the same palette group!");
        return;
    }

    // Create a group to push palette in
    m_paletteGroups.push_back(PaletteGroup());
    int group = m_paletteGroups.size() - 1;
    bool success = GeneratePaletteFromFiles(files, group);

    if (success)
    {
        ui->Palette_SB_Group->setMaximum(group);
        ui->Palette_SB_Group->setValue(group); // this will call valueChanged
    }
    else
    {
        // Failed, delete the empty group
        m_paletteGroups.pop_back();
    }

    UpdateStatus();
}

void CustomSpriteManager::on_Palette_PB_DelGroup_clicked()
{
    int group = ui->Palette_SB_Group->value();

    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString message = "Do you wish to delete palette group " + QString::number(group) + " and resources\nthat can only be used by palettes in thie group?";
    resBtn = QMessageBox::warning(this, "Delete Palette", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    while (m_paletteGroups[group].size() > 0)
    {
        DeletePalette(group, 0);
    }

    // Fix resource group ID that behind this
    for (int i = 0; i < m_resources.size(); i++)
    {
        Resource& resource = m_resources[i];
        for (Ownership& ownership : resource.m_paletteOwnerships)
        {
            if (ownership.first > group)
            {
                ownership.first--;
            }
        }
    }
    for (int i = 0; i < m_frames.size(); i++)
    {
        Frame& frame = m_frames[i];
        if (frame.m_palGroup > group)
        {
            frame.m_palGroup--;
        }
    }

    // Group is empty, remove it
    m_paletteGroups.remove(group);
    ui->Palette_SB_Group->blockSignals(true);
    ui->Palette_SB_Group->setMaximum(m_paletteGroups.size() - 1);
    ui->Palette_SB_Group->blockSignals(false);
    if (group > m_paletteGroups.size() - 1)
    {
        group--;
    }
    on_Palette_SB_Group_valueChanged(group);

    UpdateStatus();
}

void CustomSpriteManager::on_Palette_SB_Group_valueChanged(int arg1)
{
    PaletteGroup const& paletteGroup = m_paletteGroups[arg1];
    ui->Palette_SB_Index->blockSignals(true);
    ui->Palette_SB_Index->setMaximum(qMin(paletteGroup.size() - 1, 255));
    ui->Palette_SB_Index->blockSignals(false);
    ui->Palette_PB_NewPal->setEnabled(paletteGroup.size() < 256);

    UpdatePalettePreview();
    EnablePaletteWidgets();
}

void CustomSpriteManager::on_Palette_SB_Index_valueChanged(int arg1)
{
    // Show warning for BN sprites if index > 15
    if (arg1 > 15)
    {
        ui->Palette_Warning->setHidden(false);
    }
    else
    {
        ui->Palette_Warning->setHidden(true);
    }
    ui->Palette_GV->setPaletteSelected(arg1);

    SetResourcesSelectable();
}

void CustomSpriteManager::on_Palette_Color_changed(int paletteIndex, int colorIndex, QRgb color)
{
    // Color change is not allowed here, this won't be called
    int group = ui->Palette_SB_Group->value();
    m_paletteGroups[group][paletteIndex][colorIndex] = color;
}

//---------------------------------------------------------------------------
// Get palette from png files, return true if at least one successful read
//---------------------------------------------------------------------------
bool CustomSpriteManager::GeneratePaletteFromFiles(const QStringList &files, int group)
{
    // This assume the group is already created
    PaletteGroup& paletteGroup = m_paletteGroups[group];

    bool success = false;
    for (QString const& file : files)
    {
        // File name
        int index = file.lastIndexOf('\\');
        if (index == -1) index = file.lastIndexOf('/');
        QString name = file.mid(index + 1);
        name = name.mid(0, name.size() - 4);

        QImage image(file);
        Palette palette;
        GetImagePalette(palette, &image);
        if (palette.size() > 16)
        {
            QMessageBox::critical(this, "Error", name + ".png has more than 16 colors!", QMessageBox::Ok);
            continue;
        }

        // Fill the remaining color as black
        while (palette.size() < 16)
        {
            palette.push_back(0xFF000000);
        }

        // Save palette to current group
        paletteGroup.push_back(palette);
        success = true;

        // This is the first palette of the group
        int const paletteCount = paletteGroup.size();
        if (paletteCount == 1)
        {
            ui->Palette_SB_Index->setEnabled(true);
            ui->Resources_PB_Add->setEnabled(true);
        }

        // Check if any existing resource can use this palette
        int paletteIndex = paletteGroup.size() - 1;
        for (int i = 0; i < m_resources.size(); i++)
        {
            Resource& resource = m_resources[i];
            if (ComparePalette(palette, resource.m_palette))
            {
                resource.m_paletteOwnerships.push_back(Ownership(group,paletteIndex));
                success = true;

                qDebug() << "Palette ownership added to" << resource.m_name;
            }
        }
    }

    return success;
}

//---------------------------------------------------------------------------
// Delete palette in this group and remove resouces of this palette
//---------------------------------------------------------------------------
void CustomSpriteManager::GetImagePalette(Palette &palette, const QImage *image)
{
    QRgb transColor = BNSprite::ClampRGB(ui->Transparency_Color->palette().color(QPalette::Base).rgb());
    for (int y = 0; y < image->height(); y++)
    {
        for (int x = 0; x < image->width(); x++)
        {
            QRgb rgb = image->pixel(x,y);

            // Push transparent color
            if (x == 0 && y == 0)
            {
                if (ui->Transparency_Top->isChecked())
                {
                    transColor = BNSprite::ClampRGB(rgb);
                }
                palette.push_back(transColor);
            }

            // Find pixel with alpha > 0 and not transparent color
            if ((rgb >> 24) != 0)
            {
                rgb = BNSprite::ClampRGB(rgb);
                if (rgb != transColor && !palette.contains(rgb))
                {
                    palette.push_back(rgb);
                }
            }
        }
    }
}

//---------------------------------------------------------------------------
// Return true if "source" contains all the color needed from "palette"
//---------------------------------------------------------------------------
bool CustomSpriteManager::ComparePalette(const Palette &source, const Palette &palette)
{
    Q_ASSERT(!source.empty() && !palette.empty());

    for (int i = 1; i < palette.size(); i++)
    {
        bool found = false;
        for (int j = 1; j < source.size(); j++)
        {
            if (palette[i] == source[j])
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------------------------
// Return true at least one imported palette exist for "palette"
//---------------------------------------------------------------------------
bool CustomSpriteManager::FindExistingPalette(Resource &resource)
{
    resource.m_paletteOwnerships.clear();
    bool success = false;

    for (int i = 0; i < m_paletteGroups.size(); i++)
    {
        PaletteGroup const& group = m_paletteGroups[i];
        for (int j = 0; j < group.size(); j++)
        {
            Palette const& source = group[j];
            if (ComparePalette(source, resource.m_palette))
            {
                resource.m_paletteOwnerships.push_back(Ownership(i,j));
                success = true;

                qDebug() << "Find palette at [" << i << "," << j << "]";
            }
        }
    }

    return success;
}

//---------------------------------------------------------------------------
// Delete palette in this group and remove resouces of this palette
//---------------------------------------------------------------------------
void CustomSpriteManager::DeletePalette(int group, int index)
{
    PaletteGroup& paletteGroup = m_paletteGroups[group];
    paletteGroup.remove(index);
    ui->Palette_GV->deletePalette(index);

    // Delete resources that can only used by this palette
    QVector<int> resourceIDs;
    GetResourceIDsForPalette(resourceIDs, group, index, false);
    for (int i = resourceIDs.size() - 1; i >= 0; i--)
    {
        int const& id = resourceIDs[i];
        Resource& resource = m_resources[id];

        if (resource.m_paletteOwnerships.size() > 1)
        {
            // This resource can be use by other palette, just delete current one
            int removeIndex = resource.m_paletteOwnerships.indexOf(Ownership(group,index));
            resource.m_paletteOwnerships.remove(removeIndex);
        }
        else
        {
            DeleteResource(id);
        }
    }

    // Delete frame that uses this palette
    for (int i = m_frames.size() - 1; i >= 0; i--)
    {
        Frame const& frame = m_frames[i];
        if (frame.m_palGroup == group && frame.m_palIndex == index)
        {
            DeleteFrame(i);
        }
    }

    // Fix ID of any index behind it
    for (int i = 0; i < m_resources.size(); i++)
    {
        Resource& resource = m_resources[i];
        for (Ownership& ownership : resource.m_paletteOwnerships)
        {
            if (ownership.first == group && ownership.second > index)
            {
                ownership.second--;
            }
        }
    }
    for (int i = 0; i < m_frames.size(); i++)
    {
        Frame& frame = m_frames[i];
        if (frame.m_palGroup == group && frame.m_palIndex > index)
        {
            frame.m_palIndex--;
        }
    }

    UpdateStatus();
}

//---------------------------------------------------------------------------
// Display current palette group
//---------------------------------------------------------------------------
void CustomSpriteManager::UpdatePalettePreview()
{
    ui->Palette_GV->clear();

    int group = ui->Palette_SB_Group->value();
    for (int i = 0; i < m_paletteGroups[group].size(); i++)
    {
        ui->Palette_GV->addPalette(m_paletteGroups[group][i], false);
    }

    // Call valueChanged to filter resources
    int index = ui->Palette_SB_Index->value();
    on_Palette_SB_Index_valueChanged(index);
}

//---------------------------------------------------------------------------
// Palette Widgets
//---------------------------------------------------------------------------
void CustomSpriteManager::EnablePaletteWidgets()
{
    int group = ui->Palette_SB_Group->value();
    bool editingLayer = !m_editingLayers.empty();
    ui->Palette_PB_NewGroup->setEnabled(!m_paletteGroups[0].empty() && !editingLayer);
    ui->Palette_PB_DelGroup->setEnabled(m_paletteGroups.size() > 1 && !editingLayer);
    ui->Palette_PB_NewPal->setEnabled(m_paletteGroups[group].size() < 256);
    ui->Palette_PB_DelPal->setEnabled(m_paletteGroups[group].size() > 0 && !editingLayer);
    ui->Palette_SB_Group->setEnabled(!editingLayer);
    ui->Palette_SB_Index->setEnabled(m_paletteGroups[group].size() > 0 && !editingLayer);
}

//---------------------------------------------------------------------------
// Resources slots
//---------------------------------------------------------------------------
void CustomSpriteManager::on_Resources_PB_Add_clicked()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QStringList files = QFileDialog::getOpenFileNames(this, tr("Import Sprites"), path, "PNG Images (*.png)");
    if (files.empty()) return;

    // Save directory
    QFileInfo info(files[0]);
    m_path = info.dir().absolutePath();

    for (QString const& file : files)
    {
        // File name
        int index = file.lastIndexOf('\\');
        if (index == -1) index = file.lastIndexOf('/');
        QString name = file.mid(index + 1);
        name = name.mid(0, name.size() - 4);

        QImage tempImage(file);
        QImage* image = new QImage(tempImage.convertToFormat(QImage::Format_ARGB32));
        if (image->height() > 256 || image->width() > 256)
        {
            QMessageBox::critical(this, "Error", name + ".png size is larger than 256x256!", QMessageBox::Ok);
            delete image;
            continue;
        }

        Resource resource;
        GetImagePalette(resource.m_palette, image);
        if (resource.m_palette.size() <= 1)
        {
            QMessageBox::critical(this, "Error", name + ".png is completely transparent!", QMessageBox::Ok);
            delete image;
            continue;
        }
        else if (resource.m_palette.size() > 16)
        {
            QMessageBox::critical(this, "Error", name + ".png has more than 16 colors!", QMessageBox::Ok);
            delete image;
            continue;
        }

        // Go through all palettes to check if it exist for this image
        if (!FindExistingPalette(resource))
        {
            QMessageBox::critical(this, "Error", "No existing palette found for " + name + ".png, please import the palette first!", QMessageBox::Ok);
            continue;
        }

        // Transparency
        QRgb transColor = ui->Transparency_Color->palette().color(QPalette::Base).rgb();

        // Clamp all colors to gba
        int minX = 255;
        int minY = 255;
        int maxX = 0;
        int maxY = 0;
        for (int y = 0; y < image->height(); y++)
        {
            for (int x = 0; x < image->width(); x++)
            {
                QColor color = image->pixelColor(x,y);

                // Get transparent color
                if (x == 0 && y == 0)
                {
                    if (ui->Transparency_Top->isChecked())
                    {
                        transColor = BNSprite::ClampRGB(color.rgb());
                    }
                }

                if (color.alpha() == 0)
                {
                    // Set transparency
                    image->setPixel(x,y,0);
                }
                else
                {
                    // Force all other color to alpha 255, and clamp them
                    QRgb clamped = BNSprite::ClampRGB(image->pixel(x,y));
                    if (clamped == transColor)
                    {
                        // Set transparency
                        image->setPixel(x,y,0);
                    }
                    else
                    {
                        minX = qMin(minX,x);
                        minY = qMin(minY,y);
                        maxX = qMax(maxX,x);
                        maxY = qMax(maxY,y);
                        image->setPixel(x,y,clamped);
                    }
                }
            }
        }

        int width = maxX - minX + 1;
        int height = maxY - minY + 1;

        // Check if there are duplicated names
        int dupNum = 1;
        QString nameNum = name;
        while(m_resourceNamesIDMap.contains(nameNum))
        {
            dupNum++;
            nameNum = name + "_" + QString::number(dupNum);
        }
        m_resourceNamesIDMap.insert(nameNum, m_resources.size());
        name = nameNum;
        resource.m_name = name;

        // Make thumbnail square image, if larger than icon size, scale it down
        int longerSide = qMax(width, height);
        int iconSize = ui->Resources_LW->iconSize().width() - 2; // Reserve two pixels for border
        int thumbnailX = 0;
        int thumbnailY = 0;
        if (longerSide <= iconSize)
        {
            thumbnailX = minX - (iconSize - width) / 2;
            thumbnailY = minY - (iconSize - height) / 2;
        }
        else
        {
            thumbnailX = minX - (longerSide - width) / 2;
            thumbnailY = minY - (longerSide - height) / 2;
        }
        int thumbnailSize = qMax(longerSide, iconSize);
        resource.m_selectable = false;
        resource.m_thumbnail = new QImage(image->copy(thumbnailX, thumbnailY, thumbnailSize, thumbnailSize));
        QListWidgetItem* item = new QListWidgetItem(QIcon(QPixmap::fromImage(*resource.m_thumbnail)), name);
        ui->Resources_LW->addItem(item);

        // Create an indexed image (added border to divisible minus 1 pixel)
        // E.g.1: 1x1, add 7x7 top left, 7x7 bottom right -> 15x15
        // E.g.2: 8x8, add nothing top left, 7x7 bottom right -> 15x15
        int const addBorderX = (width % 8 == 0) ? 0 : 8 - width % 8;
        int const addBorderY = (height % 8 == 0) ? 0 : 8 - height % 8;
        resource.m_croppedStartPos = QPoint(minX - addBorderX, minY - addBorderY);
        QSize const croppedSize(width + addBorderX + 7, height + addBorderY + 7);
        resource.m_croppedImage = new QImage(image->copy(resource.m_croppedStartPos.x(), resource.m_croppedStartPos.y(), croppedSize.width(), croppedSize.height()));

        int const tileXCount = resource.m_croppedImage->width() / 8;
        int const tileYCount = resource.m_croppedImage->height() / 8;
        resource.m_usedTiles = QVector<bool>(tileXCount * tileYCount, false);
        QVector<bool> testTiles(tileXCount * tileYCount, false);

        // Test 64 positions to see which uses the least amount of tiles
        // UPDATE: Only need to test added border + 1 (e.g. if image is 8x8 there's not need to test other than [0,0])
        int minUsedTileCount = INT32_MAX;
        for (int sampleY = 0; sampleY < addBorderY + 1; sampleY++)
        {
            for (int sampleX = 0; sampleX < addBorderX + 1; sampleX++)
            {
                if (FindTileUsedCount(resource.m_croppedImage, testTiles, minUsedTileCount, QPoint(sampleX, sampleY)))
                {
                    qDebug() << "Tile used:" << minUsedTileCount;
                    resource.m_usedTiles = testTiles;
                    resource.m_tileStartPos = QPoint(sampleX, sampleY);
                }
            }
        }

        SetResourceAllowance(resource, true);
        SampleOAM(resource);

        resource.m_image = image;
        m_resources.push_back(resource);

        ui->Resources_PB_GenAll->setEnabled(true);
        qApp->processEvents();
    }

    SetResourcesSelectable();

    UpdateStatus();
}

void CustomSpriteManager::on_Resources_PB_Delete_clicked()
{
    int resourceID = ui->Resources_LW->currentRow();
    if (resourceID < 0) return;

    DeleteResource(resourceID);
}

void CustomSpriteManager::on_Resources_PB_Sample_clicked()
{
    if (ui->Resource_Sample_Group->isHidden())
    {
        ui->Resources_PB_Sample->setText("<<");
        ui->Resource_Sample_Group->setHidden(false);
        this->setMinimumWidth(751 + 289);
        this->setMaximumWidth(751 + 289);
    }
    else
    {
        ui->Resources_PB_Sample->setText(">>");
        ui->Resource_Sample_Group->setHidden(true);
        this->setMinimumWidth(751);
        this->setMaximumWidth(751);
    }
}

void CustomSpriteManager::on_Resources_PB_GenAll_clicked()
{
    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString message = "Generate frame for each resource placing at ";
    message += "[" + QString::number(ui->Preview_X->value()) + "," + QString::number(ui->Preview_Y->value()) + "]?";
    resBtn = QMessageBox::information(this, "Auto Generate", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    // Reserve shadow
    message = "Do you want to reserve hidden shadow for each frame?";
    resBtn = QMessageBox::information(this, "Auto Generate", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        ui->Layer_RB_None->setChecked(true);
    }
    else
    {
        ui->Layer_RB_Reserved->setChecked(true);
    }

    for (int i = 0; i < m_resources.size(); i++)
    {
        Resource const& resource = m_resources[i];
        Ownership const& ownership = resource.m_paletteOwnerships[0];
        ui->Palette_SB_Group->setValue(ownership.first);
        ui->Palette_SB_Index->setValue(ownership.second);

        Layer layer;
        layer.m_pos = QPoint(ui->Preview_X->value(), ui->Preview_Y->value());
        layer.m_hFlip = false;
        layer.m_vFlip = false;
        layer.m_resourceName = resource.m_name;
        m_editingLayers.push_back(layer);

        SaveLayer();
        qApp->processEvents();
    }
}

void CustomSpriteManager::on_Resources_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    int resourceID = ui->Resources_LW->row(current);
    if (resourceID < 0) return;

    QListWidgetItem* item = ui->Resources_LW->item(resourceID);
    qDebug() << item->text() << "selected";

    ui->Resources_SB_64->blockSignals(true);
    ui->Resources_SB_32->blockSignals(true);
    ui->Resources_SB_16->blockSignals(true);
    ui->Resources_SB_8->blockSignals(true);
    ui->Resources_SB_4->blockSignals(true);
    ui->Resources_CB_SingleOAM->blockSignals(true);
    Resource const& resource = m_resources[resourceID];
    GetResourceAllowance(resource);
    ui->Resources_SB_64->blockSignals(false);
    ui->Resources_SB_32->blockSignals(false);
    ui->Resources_SB_16->blockSignals(false);
    ui->Resources_SB_8->blockSignals(false);
    ui->Resources_SB_4->blockSignals(false);
    ui->Resources_CB_SingleOAM->blockSignals(false);

    // Buttons
    ui->Resources_PB_Delete->setEnabled(true);
    ui->Resources_SB_64->setEnabled(!resource.m_forceSingleOAM);
    ui->Resources_SB_32->setEnabled(!resource.m_forceSingleOAM);
    ui->Resources_SB_16->setEnabled(!resource.m_forceSingleOAM);
    ui->Resources_SB_8->setEnabled(!resource.m_forceSingleOAM);
    ui->Resources_SB_4->setEnabled(!resource.m_forceSingleOAM);
    ui->Resources_CB_SingleOAM->setEnabled(true);

    UpdateDrawOAMSample();
}

void CustomSpriteManager::on_Resources_LW_pressed(const QModelIndex &index)
{
    // Update drop so only selectable resources are allowed
    int resourceID = ui->Resources_LW->currentRow();
    bool selectable = m_resources[resourceID].m_selectable;
    ui->Layer_LW->SetAcceptDropFromOthers(selectable);

    // Status
    if (!selectable)
    {
        QString message = "Resource incompatible with current selected palette, you cannot add this to the Frame Layer!";
        UpdateStatus(message, QColor(255,0,0));
    }
    else
    {
        UpdateStatus();
    }
}

void CustomSpriteManager::on_Resources_LW_itemDoubleClicked(QListWidgetItem *item)
{
    // Push resource to layer
    int resourceID = ui->Resources_LW->currentRow();
    if (m_resources[resourceID].m_selectable)
    {
        QListWidgetItem* copyItem = new QListWidgetItem(*item);
        ui->Layer_LW->addItem(copyItem);
        on_Layer_LW_layerUpdated(false);
    }
}

void CustomSpriteManager::on_Resources_SB_64_valueChanged(int arg1)
{
    UpdateDrawOAMSample();
}

void CustomSpriteManager::on_Resources_SB_32_valueChanged(int arg1)
{
    UpdateDrawOAMSample();
}

void CustomSpriteManager::on_Resources_SB_16_valueChanged(int arg1)
{
    UpdateDrawOAMSample();
}

void CustomSpriteManager::on_Resources_SB_8_valueChanged(int arg1)
{
    UpdateDrawOAMSample();
}

void CustomSpriteManager::on_Resources_SB_4_valueChanged(int arg1)
{
    UpdateDrawOAMSample();
}

void CustomSpriteManager::on_Resources_CB_SingleOAM_toggled(bool checked)
{
    UpdateDrawOAMSample();

    // Might be reverted, can just use the bool "checked"
    ui->Resources_SB_64->setEnabled(!ui->Resources_CB_SingleOAM->isChecked());
    ui->Resources_SB_32->setEnabled(!ui->Resources_CB_SingleOAM->isChecked());
    ui->Resources_SB_16->setEnabled(!ui->Resources_CB_SingleOAM->isChecked());
    ui->Resources_SB_8->setEnabled(!ui->Resources_CB_SingleOAM->isChecked());
    ui->Resources_SB_4->setEnabled(!ui->Resources_CB_SingleOAM->isChecked());
}

//---------------------------------------------------------------------------
// Set resource allowance
//---------------------------------------------------------------------------
void CustomSpriteManager::SetResourceAllowance(CustomSpriteManager::Resource &resource, bool loadDefault)
{
    if (loadDefault)
    {
        resource.setAllowance(ui->Resources_SB_Default64->value(),
                              ui->Resources_SB_Default32->value(),
                              ui->Resources_SB_Default16->value(),
                              ui->Resources_SB_Default8->value(),
                              ui->Resources_SB_Default4->value());
        resource.m_forceSingleOAM = false;
    }
    else
    {
        resource.setAllowance(ui->Resources_SB_64->value(),
                              ui->Resources_SB_32->value(),
                              ui->Resources_SB_16->value(),
                              ui->Resources_SB_8->value(),
                              ui->Resources_SB_4->value());
        resource.m_forceSingleOAM = ui->Resources_CB_SingleOAM->isChecked();
    }
}

//---------------------------------------------------------------------------
// Get resource allowance
//---------------------------------------------------------------------------
void CustomSpriteManager::GetResourceAllowance(const CustomSpriteManager::Resource &resource)
{
    QVector<int> const& allowances = resource.m_tileAllowance;
    ui->Resources_SB_64->setValue(allowances[0]);
    ui->Resources_SB_32->setValue(allowances[1]);
    ui->Resources_SB_16->setValue(allowances[2]);
    ui->Resources_SB_8->setValue(allowances[3]);
    ui->Resources_SB_4->setValue(allowances[4]);
    ui->Resources_CB_SingleOAM->setChecked(resource.m_forceSingleOAM);
}

//---------------------------------------------------------------------------
// Refresh the name to ID map
//---------------------------------------------------------------------------
void CustomSpriteManager::UpdateResourceNameIDMap()
{
    for (int i = 0; i < m_resources.size(); i++)
    {
        Resource const& resource = m_resources[i];
        m_resourceNamesIDMap[resource.m_name] = i;
    }
}

//---------------------------------------------------------------------------
// Update drawing the OAM sample preview
//---------------------------------------------------------------------------
void CustomSpriteManager::UpdateDrawOAMSample()
{
    int resourceID = ui->Resources_LW->currentRow();
    if (resourceID < 0 || resourceID >= m_resources.size()) return;

    Resource& resource = m_resources[resourceID];
    SetResourceAllowance(resource, false);
    SampleOAM(resource);

    // Show how many tiles and OAM used in total
    ui->Resources_Tile->setText("Tile Count: " + QString::number(resource.m_tileCount));
    ui->Resources_OAM->setText("OAM Count: " + QString::number(resource.m_oamInfoList.size()));

    //QImage testImage = DebugDrawUsedTiles(croppedImage, usedTiles, resource.m_tileStartPos, tileXCount, tileYCount);
    QImage testImage = DebugDrawOAM(resource);
    m_resourceGraphic->clear();
    QGraphicsPixmapItem* pixmapItem = m_resourceGraphic->addPixmap(QPixmap::fromImage(testImage));
    pixmapItem->setPos(128 - testImage.width() / 2, 128 - testImage.height() / 2);

    UpdateStatus();
}

//---------------------------------------------------------------------------
// Return all the resource IDs that has given palette, optional: use only this palette
//---------------------------------------------------------------------------
bool CustomSpriteManager::GetResourceIDsForPalette(QVector<int> &resourceIDs, int group, int index, bool thisPaletteOnly)
{
    for (int i = 0; i < m_resources.size(); i++)
    {
        Resource const& resource = m_resources[i];
        Ownership ownership(group, index);
        if (resource.m_paletteOwnerships.contains(ownership))
        {
            if (!thisPaletteOnly || (thisPaletteOnly && resource.m_paletteOwnerships.size() == 1))
            {
                resourceIDs.push_back(i);
            }
        }
    }

    return !resourceIDs.empty();
}

//---------------------------------------------------------------------------
// Test all 8x8 tiles, get the total number of used tiles
//---------------------------------------------------------------------------
bool CustomSpriteManager::FindTileUsedCount(const QImage *image, QVector<bool> &testTiles, int &minUsedTileCount, QPoint tileStartPos)
{
    int usedTileCount = 0;
    int const tileXCount = image->width() / 8;
    int const tileYCount = image->height() / 8;

    for (int tileY = 0; tileY < tileYCount; tileY++)
    {
        for (int tileX = 0; tileX < tileXCount; tileX++)
        {
            if (TestTileUsed(image, tileStartPos, tileX, tileY))
            {
                if (++usedTileCount >= minUsedTileCount)
                {
                    // This uses the same or more tiles than others, stop testing
                    return false;
                }

                testTiles[tileXCount * tileY + tileX] = true;
            }
            else
            {
                testTiles[tileXCount * tileY + tileX] = false;
            }
        }
    }

    // If we are here, we use fewer tiles!
    minUsedTileCount = usedTileCount;
    return true;
}

//---------------------------------------------------------------------------
// Test a 8x8 tile if it is used
//---------------------------------------------------------------------------
bool CustomSpriteManager::TestTileUsed(const QImage *image, QPoint tileStartPos, int tileX, int tileY)
{
    int const startY = tileStartPos.y() + tileY * 8;
    int const startX = tileStartPos.x() + tileX * 8;
    for (int y = startY; y < startY + 8; y++)
    {
        for (int x = startX; x < startX + 8; x++)
        {
            if (image->pixel(x,y) != 0)
            {
                return true;
            }
        }
    }

    return false;
}

//---------------------------------------------------------------------------
// Sample tiles to OAM from 64x64 to 8x8
//---------------------------------------------------------------------------
void CustomSpriteManager::SampleOAM(Resource &resource)
{
    QImage* image = resource.m_croppedImage;
    int const tileXCount = image->width() / 8;
    int const tileYCount = image->height() / 8;
    resource.m_oamInfoList.clear();

    if (resource.m_forceSingleOAM)
    {
        for (int oamSize = SIZE_8x8; oamSize >= SIZE_64x64; oamSize--)
        {
            QSize const size = m_oamSizesMap[(OAMSize)oamSize];
            if (size.width() >= tileXCount && size.height() >= tileYCount)
            {
                OAMInfo oamInfo;
                oamInfo.m_oamSize = (OAMSize)oamSize;
                oamInfo.m_topLeft = QPoint(resource.m_tileStartPos.x(), resource.m_tileStartPos.y());
                resource.m_oamInfoList.push_back(oamInfo);
                resource.m_tileCount = size.width() * size.height();

                // Success!
                return;
            }
        }

        // Failed if we are here
        QMessageBox::critical(this, "Error", "Unable to sameple \"" + resource.m_name + "\" with a single OAM, reverting to allowance values!", QMessageBox::Ok);
        ui->Resources_CB_SingleOAM->blockSignals(true);
        ui->Resources_CB_SingleOAM->setChecked(false);
        ui->Resources_CB_SingleOAM->blockSignals(false);
        resource.m_forceSingleOAM = false;
    }

    QSet<int> sampledTiles;
    for (int oamSize = SIZE_64x64; oamSize < SIZE_COUNT; oamSize++)
    {
        QSize const size = m_oamSizesMap[(OAMSize)oamSize];
        if (size.width() > tileXCount || size.height() > tileYCount) continue;

#if USE_FIRST_FIT_OAM
        //-----------------------------------------------------------------
        // Find the first position of tiles fit inside this OAM
        //-----------------------------------------------------------------
        for (int y = 0; y <= tileYCount - size.height(); y++)
        {
            for (int x = 0; x <= tileXCount - size.width(); x++)
            {
                QSet<int> sampledTilesForThisOAM;
                int allowance = resource.getAllowance((OAMSize)oamSize);
                if(TestTileInOAM(resource.m_usedTiles, sampledTiles, sampledTilesForThisOAM, allowance, size, x, y, tileXCount, tileYCount))
                {
                    // We can create this OAM! remember the sampled tiles
                    for(int tile : sampledTilesForThisOAM)
                    {
                        sampledTiles.insert(tile);
                    }

                    OAMInfo oamInfo;
                    oamInfo.m_oamSize = (OAMSize)oamSize;
                    oamInfo.m_topLeft = QPoint(resource.m_tileStartPos.x() + x * 8, resource.m_tileStartPos.y() + y * 8);
                    resource.m_oamInfoList.push_back(oamInfo);

                    // Skip the width so we can test slightly faster next
                    x += size.width() - 1;
                }
            }
        }
        //-----------------------------------------------------------------
#else
        //-----------------------------------------------------------------
        // Return OAM that fits the most used tiles
        //-----------------------------------------------------------------
        QSet<int> sampledTilesForThisOAM;
        int allowance = resource.getAllowance((OAMSize)oamSize);
        int tileXStart = 0;
        int tileYStart = 0;

        while(FindMostTileOAM(resource.m_usedTiles, sampledTiles, sampledTilesForThisOAM, allowance, tileXStart, tileYStart, size, tileXCount, tileYCount))
        {
            // We found a OAM to sample, remember the sampled tiles
            for(int tile : sampledTilesForThisOAM)
            {
                sampledTiles.insert(tile);
            }

            OAMInfo oamInfo;
            oamInfo.m_oamSize = (OAMSize)oamSize;
            oamInfo.m_topLeft = QPoint(resource.m_tileStartPos.x() + tileXStart * 8, resource.m_tileStartPos.y() + tileYStart * 8);
            resource.m_oamInfoList.push_back(oamInfo);

            // Restart
            sampledTilesForThisOAM.clear();
        }
        //-----------------------------------------------------------------
#endif
    }

    // Check how many tiles in total
    resource.m_tileCount = 0;
    for (OAMInfo const& info : resource.m_oamInfoList)
    {
        QSize size = m_oamSizesMap[info.m_oamSize];
        resource.m_tileCount += size.width() * size.height();
    }
}

//---------------------------------------------------------------------------
// Test until a OAM with the most used tile found
//---------------------------------------------------------------------------
bool CustomSpriteManager::FindMostTileOAM(const QVector<bool> &usedTiles, const QSet<int> &sampledTiles, QSet<int> &sampledTilesForThisOAM, int allowance, int& tileXStart, int& tileYStart, const QSize oamSize, int tileXCount, int tileYCount)
{
    int usedTileInOAM = 0;
    //qDebug() << "Testing OAM size: " + QString::number(oamSize.width()) + "," + QString::number(oamSize.height());

    bool success = false;
    for (int y = 0; y <= tileYCount - oamSize.height(); y++)
    {
        for (int x = 0; x <= tileXCount - oamSize.width(); x++)
        {
            QSet<int> sampledTilesForTestOAM;
            int testAllowance = allowance;
            if(TestTileInOAM(usedTiles, sampledTiles, sampledTilesForTestOAM, testAllowance, oamSize, x, y, tileXCount, tileYCount))
            {
                // Check how many used tiles in this OAM
                int usedTileInTestOAM = sampledTilesForTestOAM.size() - (allowance - testAllowance);
                if (usedTileInTestOAM > usedTileInOAM)
                {
                    // Found at least one OAM with more tiles fit in it
                    //qDebug() << "Used tiles in OAM:" << usedTileInTestOAM;

                    success = true;
                    usedTileInOAM = usedTileInTestOAM;
                    sampledTilesForThisOAM = sampledTilesForTestOAM;
                    tileXStart = x;
                    tileYStart = y;
                }
            }
        }
    }

    return success;
}

//---------------------------------------------------------------------------
// Test if tiles can fit inside this OAM with allowance
//---------------------------------------------------------------------------
bool CustomSpriteManager::TestTileInOAM(const QVector<bool> &usedTiles, const QSet<int> &sampledTiles, QSet<int> &sampledTilesForThisOAM, int& allowance, const QSize oamSize, int tileXStart, int tileYStart, int tileXCount, int tileYCount)
{
    for (int testY = 0; testY < oamSize.height(); testY++)
    {
        for (int testX = 0; testX < oamSize.width(); testX++)
        {
            int testTile = (tileYStart + testY) * tileXCount + (tileXStart + testX);
            if (sampledTiles.contains(testTile))
            {
                return false;
            }
            else if (!usedTiles[testTile])
            {
                // Not used, but we bypass if we still have allowance
                if (allowance > 0)
                {

                    allowance--;
                }
                else
                {
                    return false;
                }
            }

            sampledTilesForThisOAM.insert(testTile);
        }
    }

    // If allowance is too large it is possible we sampled empty tiles
    return !sampledTilesForThisOAM.empty();
}

//---------------------------------------------------------------------------
// Return an image with borders on used tiles
//---------------------------------------------------------------------------
QImage CustomSpriteManager::DebugDrawUsedTiles(const QImage *image, const QVector<bool> &usedTiles, QPoint tileStartPos, int tileXCount, int tileYCount)
{
    QImage testImage(*image);
    QPainter painter(&testImage);
    painter.setPen(QColor(255,0,0,128));

    for (int tileY = 0; tileY < tileYCount; tileY++)
    {
        for (int tileX = 0; tileX < tileXCount; tileX++)
        {
            if (usedTiles[tileXCount * tileY + tileX])
            {
                int const startY = tileStartPos.y() + tileY * 8;
                int const startX = tileStartPos.x() + tileX * 8;

                // Draw red square border
                painter.drawRect(startX, startY, 7, 7);
            }
        }
    }

    return testImage;
}

//---------------------------------------------------------------------------
// Return an image with borders on sampled OAMs
//---------------------------------------------------------------------------
QImage CustomSpriteManager::DebugDrawOAM(const Resource &resource)
{
    QImage testImage(*resource.m_croppedImage);
    if (resource.m_forceSingleOAM)
    {
        OAMInfo const& oamInfo = resource.m_oamInfoList[0];
        QSize const size = m_oamSizesMap[oamInfo.m_oamSize] * 8 + QSize(resource.m_tileStartPos.x(), resource.m_tileStartPos.y());

        if (size.width() > resource.m_croppedImage->width() || size.height() > resource.m_croppedImage->height())
        {
            testImage = QImage(size, QImage::Format_RGBA8888);
            for (int y = 0; y < testImage.height(); y++)
            {
                for (int x = 0; x < testImage.width(); x++)
                {
                    testImage.setPixel(x, y, 0);
                }
            }

            QPainter painter(&testImage);
            painter.drawImage(0,0,*resource.m_croppedImage);
        }
    }

    QPainter painter(&testImage);
    painter.setPen(QColor(255,0,0,128));

    for (OAMInfo const& oamInfo : resource.m_oamInfoList)
    {
        QSize const size = m_oamSizesMap[oamInfo.m_oamSize];
        painter.drawRect(oamInfo.m_topLeft.x(), oamInfo.m_topLeft.y(), size.width() * 8 - 1, size.height() * 8 - 1);
    }

    return testImage;
}

//---------------------------------------------------------------------------
// Highlight
//---------------------------------------------------------------------------
void CustomSpriteManager::SetResourcesSelectable()
{
    int group = ui->Palette_SB_Group->value();
    int index = ui->Palette_SB_Index->value();
    Ownership ownership(group, index);

    for (int i = 0; i < m_resources.size(); i++)
    {
        Resource& resource = m_resources[i];
        bool selectable = resource.m_paletteOwnerships.contains(ownership);
        if (resource.m_selectable ^ selectable)
        {
            resource.m_selectable = selectable;
            for (int y = 0; y < resource.m_thumbnail->height(); y++)
            {
                for (int x = 0; x < resource.m_thumbnail->width(); x++)
                {
                    if (x == 0 || x == resource.m_thumbnail->width() - 1 || y == 0 || y == resource.m_thumbnail->height() - 1)
                    {
                        resource.m_thumbnail->setPixel(x, y, qRgba(0, 200, 0, selectable ? 255 : 0));
                    }
                }
            }
            QListWidgetItem* item = ui->Resources_LW->item(i);
            item->setIcon(QIcon(QPixmap::fromImage(*resource.m_thumbnail)));
        }
    }
}

//---------------------------------------------------------------------------
// Delete a resource
//---------------------------------------------------------------------------
void CustomSpriteManager::DeleteResource(int resourceID)
{
    // Remove any frames that have this resource
    for (int i = m_frames.size() - 1; i >= 0; i--)
    {
        Frame const& frame = m_frames[i];
        bool remove = false;
        for (Layer const& layer : frame.m_layers)
        {
            if (m_resourceNamesIDMap[layer.m_resourceName] == resourceID)
            {
                remove = true;
                break;
            }
        }

        if (remove)
        {
            DeleteFrame(i);
        }
    }

    // Delete images of resource
    Resource& resource = m_resources[resourceID];
    resource.clear();
    m_resourceNamesIDMap.remove(m_resources[resourceID].m_name);
    m_resources.remove(resourceID);

    // We manually call item changed here, because takeItem() calls it but it's not deleted yet
    ui->Resources_LW->blockSignals(true);
    QListWidgetItem* item = ui->Resources_LW->takeItem(resourceID);
    delete item;
    on_Resources_LW_currentItemChanged(ui->Resources_LW->item(resourceID == m_resources.size() ? resourceID - 1 : resourceID), Q_NULLPTR);
    ui->Resources_LW->blockSignals(false);

    // Clear OAM preview
    if (m_resources.empty())
    {
        ui->Resources_PB_Delete->setEnabled(false);
        ui->Resources_PB_GenAll->setEnabled(false);
        ResetOAM();
    }

    UpdateResourceNameIDMap();

    UpdateStatus();
}

//---------------------------------------------------------------------------
// Preview slots
//---------------------------------------------------------------------------
void CustomSpriteManager::on_Preview_BG_currentTextChanged(const QString &arg1)
{
    m_previewBG->setPixmap(QPixmap(":/resources/" + arg1 + ".png"));
}

void CustomSpriteManager::on_Preview_Highlight_timeout()
{
    int layerIndex = ui->Layer_LW->currentRow();
    if (layerIndex == -1 || layerIndex >= m_layerItems.size())
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

    QSize size = m_previewPixmaps[layerIndex]->pixmap().size();
    QImage image = QImage(size, QImage::Format_RGBA8888);
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

void CustomSpriteManager::on_Preview_CB_Highlight_clicked(bool checked)
{
    int layerIndex = ui->Layer_LW->currentRow();
    if (!checked)
    {
        m_previewHighlight->setVisible(false);
        m_highlightTimer->stop();
    }
    else if (layerIndex != -1)
    {
        m_highlightUp = false;
        m_highlightAlpha = 255;
        m_highlightTimer->stop();
        m_highlightTimer->start(2);
        m_previewHighlight->setVisible(true);

        Layer const& layer = m_editingLayers[layerIndex];
        m_previewHighlight->setPos(layer.m_pos.x() + 128, layer.m_pos.y() + 128);
    }
}

void CustomSpriteManager::on_Preview_pressed(QPoint pos)
{
    // Preview screen is pressed, check if any layer is selected
    int layerID = ui->Layer_LW->currentRow();
    if (layerID < 0 || layerID >= m_editingLayers.size()) return;

    // Clamp the pos
    pos.rx() = qBound(0, pos.x(), 255);
    pos.ry() = qBound(0, pos.y(), 255);

    Layer& layer = m_editingLayers[layerID];
    layer.m_pos = pos - QPoint(128,128);

    ui->Layer_SB_X->setValue(layer.m_pos.x());
    ui->Layer_SB_Y->setValue(layer.m_pos.y());

    QGraphicsPixmapItem* graphicsItem = m_previewPixmaps[layerID];
    graphicsItem->setPos(layer.m_pos + QPoint(128,128));
    m_previewHighlight->setPos(layer.m_pos + QPoint(128,128));

    m_layerEdited = true;
    UpdateStatus();
}

//---------------------------------------------------------------------------
// Update Z-values of all pixmaps in the preview
//---------------------------------------------------------------------------
void CustomSpriteManager::UpdatePreviewZValues()
{
    for (int i = 0; i < m_previewPixmaps.size(); i++)
    {
        QGraphicsPixmapItem* graphicsItem = m_previewPixmaps[i];
        graphicsItem->setZValue(i);
    }
}

//---------------------------------------------------------------------------
// Layer slots
//---------------------------------------------------------------------------
void CustomSpriteManager::on_Layer_LW_layerUpdated(bool isSelf)
{
    if (isSelf)
    {
        // NOTE: At this point the item is copied, and old one is not removed
        // We handle the re-ordering in eventFilter();
    }
    else
    {
        // Find the new item added
        for (int i = 0; i < ui->Layer_LW->count(); i++)
        {
            QListWidgetItem* item = ui->Layer_LW->item(i);
            if (!m_layerItems.contains(item))
            {
                Layer layer;
                layer.m_pos = QPoint(ui->Preview_X->value(), ui->Preview_Y->value());
                layer.m_hFlip = false;
                layer.m_vFlip = false;
                layer.m_resourceName = item->text();

                m_editingLayers.insert(i, layer);
                m_layerItems.insert(i, item);

                // Find the resource of this
                QImage* image = Q_NULLPTR;
                int resourceID = m_resourceNamesIDMap[item->text()];
                image = m_resources[resourceID].m_image;
                Q_ASSERT(image != Q_NULLPTR);

                // Add to preview
                QGraphicsPixmapItem* graphicsItem = m_previewGraphic->addPixmap(QPixmap::fromImage(*image));
                m_previewPixmaps.insert(i, graphicsItem);
                graphicsItem->setPos(layer.m_pos.x() + 128, layer.m_pos.y() + 128);

                // Fix z-value to all graphics
                UpdatePreviewZValues();

                // We need to lock the selected palette now
                EnablePaletteWidgets();
                ui->Layer_PB_Save->setEnabled(true);

                m_layerEdited = true;
                UpdateStatus();
                return;
            }
        }

        // We shouldn't be here
        Q_ASSERT(false);
    }
}

void CustomSpriteManager::on_Layer_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    int index = ui->Layer_LW->row(current);
    if (index < 0 || index >= m_editingLayers.size()) return;

    qDebug() << "Layer" << index << "selected";

    // Highlight
    if (previous)
    {
        previous->setForeground(QColor(0,0,0));
    }
    ui->Layer_LW->setCurrentRow(index);
    current->setForeground(QColor(255,0,0));

    Layer const& layer = m_editingLayers[index];
    ui->Layer_SB_X->setValue(layer.m_pos.x());
    ui->Layer_SB_X->setEnabled(true);
    ui->Layer_SB_Y->setValue(layer.m_pos.y());
    ui->Layer_SB_Y->setEnabled(true);
    ui->Layer_CB_HFlip->setChecked(layer.m_hFlip);
    ui->Layer_CB_HFlip->setEnabled(true);
    ui->Layer_CB_VFlip->setChecked(layer.m_vFlip);
    ui->Layer_CB_VFlip->setEnabled(true);
    ui->Layer_PB_Delete->setEnabled(true);

    // Preview selection
    if (ui->Preview_CB_Highlight->isChecked())
    {
        m_highlightUp = false;
        m_highlightAlpha = 255;
        m_highlightTimer->stop();
        m_highlightTimer->start(2);
        m_previewHighlight->setVisible(true);
        m_previewHighlight->setPos(layer.m_pos.x() + 128, layer.m_pos.y() + 128);
    }
}

void CustomSpriteManager::on_Layer_LW_itemDoubleClicked(QListWidgetItem *item)
{
    if (!ui->Layer_PB_Delete->isHidden())
    {
        on_Layer_PB_Delete_clicked();
    }
}

void CustomSpriteManager::on_Layer_PB_Delete_clicked()
{
    if (m_editingLayers.size() == 1)
    {
        on_Layer_PB_Clear_clicked();
    }
    else
    {
        int layerID = ui->Layer_LW->currentRow();
        delete ui->Layer_LW->takeItem(layerID);

        QGraphicsItem* item = m_previewPixmaps[layerID];
        m_previewGraphic->removeItem(item);
        delete item;

        m_previewPixmaps.remove(layerID);
        m_editingLayers.remove(layerID);
        m_layerItems.remove(layerID);

        UpdatePreviewZValues();

        m_layerEdited = true;
        UpdateStatus();
    }
}

void CustomSpriteManager::on_Layer_PB_Clear_clicked()
{
    ResetLayer();
}

void CustomSpriteManager::on_Layer_SB_X_valueChanged(int arg1)
{
    int layerID = ui->Layer_LW->currentRow();
    Layer& layer = m_editingLayers[layerID];

    if (layer.m_pos.x() == arg1 || !ui->Layer_SB_X->isEnabled()) return;
    layer.m_pos.setX(arg1);

    QGraphicsPixmapItem* graphicsItem = m_previewPixmaps[layerID];
    graphicsItem->setPos(layer.m_pos + QPoint(128,128));
    m_previewHighlight->setPos(layer.m_pos + QPoint(128,128));

    m_layerEdited = true;
    UpdateStatus();
}

void CustomSpriteManager::on_Layer_SB_Y_valueChanged(int arg1)
{
    int layerID = ui->Layer_LW->currentRow();
    Layer& layer = m_editingLayers[layerID];

    if (layer.m_pos.y() == arg1 || !ui->Layer_SB_Y->isEnabled()) return;
    layer.m_pos.setY(arg1);

    QGraphicsPixmapItem* graphicsItem = m_previewPixmaps[layerID];
    graphicsItem->setPos(layer.m_pos + QPoint(128,128));
    m_previewHighlight->setPos(layer.m_pos + QPoint(128,128));

    m_layerEdited = true;
    UpdateStatus();
}

void CustomSpriteManager::on_Layer_CB_HFlip_toggled(bool checked)
{
    int layerID = ui->Layer_LW->currentRow();
    Layer& layer = m_editingLayers[layerID];

    if (layer.m_hFlip == checked || !ui->Layer_CB_HFlip->isEnabled()) return;
    layer.m_hFlip = checked;

    QGraphicsPixmapItem* graphicsItem = m_previewPixmaps[layerID];
    QPixmap source = graphicsItem->pixmap();
    QImage target(QSize(source.width(), source.height()), QImage::Format_RGBA8888);
    for (int y = 0; y < target.height(); y++)
    {
        for (int x = 0; x < target.width(); x++)
        {
            target.setPixel(x, y, 0);
        }
    }

    QPainter painter(&target);
    QTransform transf = painter.transform();
    transf.scale(-1, 1);
    painter.setTransform(transf);
    painter.drawPixmap(-source.width(), 0, source);
    graphicsItem->setPixmap(QPixmap::fromImage(target));

    m_layerEdited = true;
    UpdateStatus();
}

void CustomSpriteManager::on_Layer_CB_VFlip_toggled(bool checked)
{
    int layerID = ui->Layer_LW->currentRow();
    Layer& layer = m_editingLayers[layerID];

    if (layer.m_vFlip == checked || !ui->Layer_CB_VFlip->isEnabled()) return;
    layer.m_vFlip = checked;

    QGraphicsPixmapItem* graphicsItem = m_previewPixmaps[layerID];
    QPixmap source = graphicsItem->pixmap();
    QImage target(QSize(source.width(), source.height()), QImage::Format_RGBA8888);
    for (int y = 0; y < target.height(); y++)
    {
        for (int x = 0; x < target.width(); x++)
        {
            target.setPixel(x, y, 0);
        }
    }

    QPainter painter(&target);
    QTransform transf = painter.transform();
    transf.scale(1, -1);
    painter.setTransform(transf);
    painter.drawPixmap(0, -source.height(), source);
    graphicsItem->setPixmap(QPixmap::fromImage(target));

    m_layerEdited = true;
    UpdateStatus();
}

void CustomSpriteManager::on_Layer_RB_Reserved_toggled(bool checked)
{
    if (!m_editingLayers.empty())
    {
        m_layerEdited = true;
        UpdateStatus();
    }
}

void CustomSpriteManager::on_Layer_RB_First_toggled(bool checked)
{
    if (!m_editingLayers.empty())
    {
        m_layerEdited = true;
        UpdateStatus();
    }
}

void CustomSpriteManager::on_Layer_RB_None_toggled(bool checked)
{
    if (!m_editingLayers.empty())
    {
        m_layerEdited = true;
        UpdateStatus();
    }
}

void CustomSpriteManager::on_Layer_PB_Save_clicked()
{
    SaveLayer();
}

void CustomSpriteManager::on_Layer_PB_Update_clicked()
{
    // Remove frame cache since we need to update it
    m_cachedBNFrames.remove(m_currentFrame);

    Q_ASSERT(m_currentFrame != -1);
    SaveLayer(m_currentFrame);

    emit BuildCheckButton();
}

//---------------------------------------------------------------------------
// Create thumbnail save layer info, optionally replace existing one
//---------------------------------------------------------------------------
void CustomSpriteManager::SaveLayer(int replaceID)
{
    // Get thumanil image
    int minX = 127;
    int minY = 127;
    int maxX = -128;
    int maxY = -128;
    for (int i = 0; i < m_editingLayers.size(); i++)
    {
        Layer const& layer = m_editingLayers[i];
        int resourceID = m_resourceNamesIDMap[layer.m_resourceName];
        Resource const& resource = m_resources[resourceID];

        // use the cropped image to find the minimum size (pos is different if it's flipped)
        QImage const* image = resource.m_croppedImage;
        QPoint topLeft;
        if (layer.m_hFlip)
        {
            topLeft.setX(layer.m_pos.x() + resource.m_image->width() - resource.m_croppedStartPos.x() - image->width());
        }
        else
        {
            topLeft.setX(layer.m_pos.x() + resource.m_croppedStartPos.x());
        }
        if (layer.m_vFlip)
        {
            topLeft.setY(layer.m_pos.y() + resource.m_image->height() - resource.m_croppedStartPos.y() - image->height());
        }
        else
        {
            topLeft.setY(layer.m_pos.y() + resource.m_croppedStartPos.y());
        }
        QPoint bottomRight = topLeft + QPoint(image->width() - 1, image->height() - 1);

        minX = qMin(minX, topLeft.x());
        minY = qMin(minY, topLeft.y());
        maxX = qMax(maxX, bottomRight.x());
        maxY = qMax(maxY, bottomRight.y());
    }

    int width = maxX - minX + 1;
    int height = maxY - minY + 1;

    // Make thumbnail square image, if larger than icon size, scale it down
    int longerSide = qMax(width, height);
    int iconSize = ui->Resources_LW->iconSize().width() - 2; // Reserve two pixels for border
    int thumbnailX = 0;
    int thumbnailY = 0;
    if (longerSide <= iconSize)
    {
        thumbnailX = minX - (iconSize - width) / 2;
        thumbnailY = minY - (iconSize - height) / 2;
    }
    else
    {
        thumbnailX = minX - (longerSide - width) / 2;
        thumbnailY = minY - (longerSide - height) / 2;
    }
    int thumbnailSize = qMax(longerSide, iconSize);

    QImage thumbnail(thumbnailSize, thumbnailSize, QImage::Format_RGBA8888);
    for (int y = 0; y < thumbnail.height(); y++)
    {
        for (int x = 0; x < thumbnail.width(); x++)
        {
            thumbnail.setPixel(x, y, 0);
        }
    }

    QPainter painter(&thumbnail);
    for (int i = 0; i < m_editingLayers.size(); i++)
    {
        Layer const& layer = m_editingLayers[i];
        int resourceID = m_resourceNamesIDMap[layer.m_resourceName];
        Resource const& resource = m_resources[resourceID];

        // we can just draw the un-cropped image
        QImage const* image = resource.m_image;
        QPoint drawPos = layer.m_pos - QPoint(thumbnailX,thumbnailY);
        if (layer.m_hFlip)
        {
            drawPos.setX(-image->width() - drawPos.x());
        }
        if (layer.m_vFlip)
        {
            drawPos.setY(-image->height() - drawPos.y());
        }

        QTransform transf = QTransform();
        transf.scale(layer.m_hFlip ? -1 : 1, layer.m_vFlip ? -1 : 1);
        painter.setTransform(transf);

        painter.drawImage(drawPos, *image);
    }

    // Save the frame data
    Frame frame;
    frame.m_palGroup = ui->Palette_SB_Group->value();
    frame.m_palIndex = ui->Palette_SB_Index->value();
    frame.m_shadowType = ui->Layer_RB_Reserved->isChecked() ? ST_RESERVED : (ui->Layer_RB_First->isChecked() ? ST_FIRST : ST_NONE);
    frame.m_layers = m_editingLayers;

    if (replaceID < 0)
    {
        m_frames.push_back(frame);

        QListWidgetItem* item = new QListWidgetItem(QIcon(QPixmap::fromImage(thumbnail)), "Frame " + QString::number(ui->Frame_LW->count()));
        ui->Frame_LW->addItem(item);
    }
    else
    {
        m_frames[replaceID] = frame;
        ui->Frame_LW->item(replaceID)->setIcon(QIcon(QPixmap::fromImage(thumbnail)));
    }

    on_Layer_PB_Clear_clicked();

    // Unselect frame but keep set cuurent row on it
    if (m_currentFrame != -1)
    {
        // Un-hightlight
        ui->Frame_LW->item(m_currentFrame)->setForeground(QColor(0,0,0));
    }
    ui->Frame_LW->setCurrentRow(replaceID == -1 ? ui->Frame_LW->count() - 1 : m_currentFrame);
    m_currentFrame = -1;

    ui->Frame_PB_Delete->setEnabled(true);
    ui->Build_PB_Sprite->setEnabled(true);

    UpdateStatus();
}

//---------------------------------------------------------------------------
// Check if all OAM is within valid position
//---------------------------------------------------------------------------
bool CustomSpriteManager::CheckLayerValidOAM(const QVector<Layer> &layers, QString &errorLayer)
{
    for (Layer const& layer : layers)
    {
        int resourceID = m_resourceNamesIDMap[layer.m_resourceName];
        Resource const& resource = m_resources[resourceID];

        for (OAMInfo const& info : resource.m_oamInfoList)
        {
            // Get OAM pos base on flips
            QPoint oamPos = GetOAMPos(layer, resource, info);

            if (oamPos.x() < -128 || oamPos.y() < -128 || oamPos.x() > 127 || oamPos.y() > 127)
            {
                errorLayer = layer.m_resourceName;
                return false;
            }
        }
    }

    return true;
}

//---------------------------------------------------------------------------
// Get OAM position from layer -> resource -> OAM
//---------------------------------------------------------------------------
QPoint CustomSpriteManager::GetOAMPos(const Layer &layer, const CustomSpriteManager::Resource &resource, const OAMInfo &info)
{
    QSize size = m_oamSizesMap[info.m_oamSize];
    QPoint oamPos;
    if (layer.m_hFlip)
    {
        oamPos.setX(layer.m_pos.x() + resource.m_image->width() - resource.m_croppedStartPos.x() - info.m_topLeft.x() - size.width() * 8);
    }
    else
    {
        oamPos.setX(layer.m_pos.x() + resource.m_croppedStartPos.x() + info.m_topLeft.x());
    }
    if (layer.m_vFlip)
    {
        oamPos.setY(layer.m_pos.y() + resource.m_image->height() - resource.m_croppedStartPos.y() - info.m_topLeft.y() - size.height() * 8);
    }
    else
    {
        oamPos.setY(layer.m_pos.y() + resource.m_croppedStartPos.y() + info.m_topLeft.y());
    }

    return oamPos;
}

//---------------------------------------------------------------------------
// Frame slots
//---------------------------------------------------------------------------
void CustomSpriteManager::on_Frame_LW_itemPressed(QListWidgetItem *item)
{
    if (ui->Frame_LW->count() == 0) return;

    // Ask if discard changes
    if (m_layerEdited)
    {
        QMessageBox::StandardButton resBtn = QMessageBox::Yes;
        QString message = "Discard frame changes?";
        resBtn = QMessageBox::warning(this, "Warning", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resBtn == QMessageBox::No)
        {
            ui->Frame_LW->setCurrentRow(m_currentFrame);
            return;
        }
    }

    // Highlight
    if (m_currentFrame >= 0 && m_currentFrame < ui->Frame_LW->count())
    {
        QListWidgetItem* t = ui->Frame_LW->item(m_currentFrame);
        t->setForeground(QColor(0,0,0));
    }
    m_currentFrame = ui->Frame_LW->currentRow();
    item = ui->Frame_LW->currentItem();
    item->setForeground(QColor(255,0,0));

    qDebug() << "Frame" << m_currentFrame << "selected";

    ui->Frame_PB_Delete->setEnabled(true);

    // Load frame data in
    ResetLayer();
    Frame const& frame = m_frames[m_currentFrame];
    ui->Layer_RB_Reserved->setChecked(frame.m_shadowType == ST_RESERVED);
    ui->Layer_RB_First->setChecked(frame.m_shadowType == ST_FIRST);
    ui->Layer_RB_None->setChecked(frame.m_shadowType == ST_NONE);
    ui->Palette_SB_Group->setValue(frame.m_palGroup);
    ui->Palette_SB_Index->setValue(frame.m_palIndex);

    m_editingLayers = frame.m_layers;
    for (int i = 0; i < frame.m_layers.size(); i++)
    {
        Layer const& layer = frame.m_layers[i];
        int resourceID = m_resourceNamesIDMap[layer.m_resourceName];
        QListWidgetItem* itemCopy = new QListWidgetItem(*ui->Resources_LW->item(resourceID));
        ui->Layer_LW->addItem(itemCopy);
        m_layerItems.push_back(itemCopy);

        // Add to preview
        Resource const& resource = m_resources[resourceID];
        QImage flipImage(resource.m_image->size(), QImage::Format_RGBA8888);
        for (int y = 0; y < flipImage.height(); y++)
        {
            for (int x = 0; x < flipImage.width(); x++)
            {
                flipImage.setPixel(x, y, 0);
            }
        }

        QPainter painter(&flipImage);
        QTransform transf = QTransform();
        transf.scale(layer.m_hFlip ? -1 : 1, layer.m_vFlip ? -1 : 1);
        painter.setTransform(transf);
        painter.drawImage(layer.m_hFlip ? -resource.m_image->width() : 0, layer.m_vFlip ? -resource.m_image->height() : 0, *resource.m_image);

        QGraphicsPixmapItem* graphicsItem = m_previewGraphic->addPixmap(QPixmap::fromImage(flipImage));
        m_previewPixmaps.insert(i, graphicsItem);
        graphicsItem->setPos(layer.m_pos.x() + 128, layer.m_pos.y() + 128);
    }

    // Disable palette index change
    EnablePaletteWidgets();

    ui->Layer_PB_Save->setEnabled(true);
    emit BuildCheckButton();
}

void CustomSpriteManager::on_Frame_LW_itemDoubleClicked(QListWidgetItem *item)
{
    on_Build_PB_Frame_clicked();
}

void CustomSpriteManager::on_Frame_PB_Delete_clicked()
{
    int frameID = ui->Frame_LW->currentRow();
    if (frameID < 0) return;

    DeleteFrame(frameID);
}

//---------------------------------------------------------------------------
// Delete a frame
//---------------------------------------------------------------------------
void CustomSpriteManager::DeleteFrame(int frameID)
{
    m_frames.remove(frameID);
    QListWidgetItem* item = ui->Frame_LW->takeItem(frameID);
    delete item;

    // Fix the frame numbers
    for (int i = 0; i < ui->Frame_LW->count(); i++)
    {
        QListWidgetItem* item = ui->Frame_LW->item(i);
        item->setText("Frame " + QString::number(i));
    }

    if (m_frames.empty())
    {
        ResetLayer();
        m_currentFrame = -1;
        ui->Frame_PB_Delete->setEnabled(false);
        ui->Build_PB_Sprite->setEnabled(false);
    }
    else if (frameID == m_currentFrame)
    {
        // Fake click next item if current is deleted
        m_layerEdited = false; // this makes it no need to call ResetLayer() here
        on_Frame_LW_itemPressed(ui->Frame_LW->item(frameID == m_frames.size() ? frameID - 1 : frameID));
    }

    UpdateStatus();
}

//---------------------------------------------------------------------------
// Build slots
//---------------------------------------------------------------------------
void CustomSpriteManager::on_Build_PB_Sprite_clicked()
{
    // Set build options
    BuildOptionDialog dialog;
    dialog.setEmptyFrame(m_emptyFrame);
    dialog.setEvenOAM(m_evenOAM);
    dialog.exec();

    m_emptyFrame = dialog.getEmptyFrame();
    m_evenOAM = dialog.getEvenOAM();
    if (!dialog.getAccepted()) return;

    // Rebuild sprite if any changes is made
    ResetBuild();

    // Check if the first resource need to be re-sampled
    for (int i = 0; i < m_frames.size(); i++)
    {
        Frame const& frame = m_frames[i];
        Layer const& layer = frame.m_layers[0];

        int resourceID = m_resourceNamesIDMap[layer.m_resourceName];
        Resource& resource = m_resources[resourceID];
        if (frame.m_shadowType == ST_FIRST && !resource.m_forceSingleOAM)
        {
            resource.m_forceSingleOAM = true;
            SampleOAM(resource);
        }
    }

    // Create a list of tileset to resource array
    for (int i = 0; i < m_frames.size(); i++)
    {
        Frame const& frame = m_frames[i];

        QString errorLayer;
        if (!CheckLayerValidOAM(frame.m_layers, errorLayer))
        {
            ResetBuild();

            QString message = "Frame " + QString::number(i) + " layer \"" + errorLayer + "\" has OAM outside allowed area!";
            QMessageBox::critical(this, "Fix it!", message, QMessageBox::Ok);

            // Force to goto editing the frame
            ui->Frame_LW->setCurrentRow(i);
            on_Frame_LW_itemPressed(ui->Frame_LW->item(i));
            m_layerEdited = true;
            UpdateStatus(message, QColor(255,0,0));
            return;
        }

        Tileset tileset;
        tileset.m_palGroup = frame.m_palGroup;
        tileset.m_palIndex = frame.m_palIndex;
        tileset.m_reserveShadow = (frame.m_shadowType == ST_RESERVED);

        for (Layer const& layer : frame.m_layers)
        {
            int resourceID = m_resourceNamesIDMap[layer.m_resourceName];
            if (!tileset.m_resourceIDs.contains(resourceID))
            {
                tileset.m_resourceIDs.push_back(resourceID);
            }
        }

        m_tilesets.push_back(tileset);
        m_frameToTilesetMap.insert(i,i);
    }

    // Optimize tileset by finding reusable ones
    for (int i = 0; i < m_tilesets.size(); i++)
    {
        Tileset& t1 = m_tilesets[i];
        for (int j = 0; j < m_tilesets.size(); j++)
        {
            Tileset const& t2 = m_tilesets[j];
            if (t2.m_resourceIDs.empty())
            {
                continue;
            }

            if (i == j || t2.m_resourceIDs.size() < t1.m_resourceIDs.size())
            {
                continue;
            }

            // Different palette will index color differently
            if (t1.m_palGroup != t2.m_palGroup || t1.m_palIndex != t2.m_palIndex)
            {
                continue;
            }

            // t2 need to have empty tile if t1 has it
            if (t1.m_reserveShadow && !t2.m_reserveShadow)
            {
                continue;
            }

            // Check if t2 contains everything in t1
            bool contain = true;
            for (int const& resourceID : t1.m_resourceIDs)
            {
                if (!t2.m_resourceIDs.contains(resourceID))
                {
                    contain = false;
                    break;
                }
            }

            if (contain)
            {
                // Just make t1 empty right now, remove later
                t1.m_resourceIDs.clear();
                m_frameToTilesetMap[i] = j;

                // If we remove current tileset ID, we need to set those to the new ID too
                // B contains A, C contains B, remove B, C now contains B & A
                for (int& tilesetID : m_frameToTilesetMap)
                {
                    if (tilesetID == i)
                    {
                        tilesetID = j;
                    }
                }
                break;
            }
        }
    }

    // Remap all the frame to tilesets
    for (int i = m_tilesets.size() - 1; i >= 0; i--)
    {
        Tileset const& tileset = m_tilesets[i];
        if (tileset.m_resourceIDs.empty())
        {
            m_tilesets.remove(i);
            for (int& tilesetID : m_frameToTilesetMap)
            {
                if (tilesetID >= i)
                {
                    tilesetID--;
                }
            }
        }
    }

    for (int tilesetID = 0; tilesetID < m_tilesets.size(); tilesetID++)
    {
        Tileset& tileset = m_tilesets[tilesetID];

        // Check how many tile in the tileset
        int tileCount = tileset.m_reserveShadow ? (m_evenOAM ? 2 : 1) : 0;
        for (int const& resourceID : tileset.m_resourceIDs)
        {
            Resource const& r = m_resources[resourceID];
            tileCount += r.m_tileCount;

            // If need even OAM, add 1 tile for each 8x8
            if (m_evenOAM)
            {
                for (OAMInfo const& info : r.m_oamInfoList)
                {
                    QSize size = m_oamSizesMap[info.m_oamSize];
                    if (size == QSize(1,1))
                    {
                        tileCount++;
                    }
                }
            }
        }

        // 255 is the limit!
        if (tileCount > 255)
        {
            // Find the frame that has this tileset
            int i = 0;
            for (; i < m_frames.size(); i++)
            {
                if (m_frameToTilesetMap[i] == tilesetID)
                {
                    break;
                }
            }

            ResetBuild();

            QString message = "Frame " + QString::number(i) + " has more than 255 tiles!";
            QMessageBox::critical(this, "Fix it!", message, QMessageBox::Ok);

            // Force to goto editing the frame
            ui->Frame_LW->setCurrentRow(i);
            on_Frame_LW_itemPressed(ui->Frame_LW->item(i));
            m_layerEdited = true;
            UpdateStatus(message, QColor(255,0,0));
            return;
        }

        tileset.m_tileData.clear();
        tileset.m_tileData = QByteArray(tileCount * 64, 0);

        Palette const& palette = m_paletteGroups[tileset.m_palGroup][tileset.m_palIndex];

        // Sample OAM for each resource
        int tileDataPixel = tileset.m_reserveShadow ? (m_evenOAM ? 128 : 64) : 0;
        for (int const& resourceID : tileset.m_resourceIDs)
        {
            QMap<QRgb,int> colorToIndexMap;

            Resource const& resource = m_resources[resourceID];
            QImage const* image = resource.m_croppedImage;
            for (OAMInfo const& info : resource.m_oamInfoList)
            {
                // Collect pixel info
                QSize size = m_oamSizesMap[info.m_oamSize];
                for (int tileY = 0; tileY < size.height(); tileY++)
                {
                    for (int tileX = 0; tileX < size.width(); tileX++)
                    {
                        for (int y = tileY * 8; y < tileY * 8 + 8; y++)
                        {
                            for (int x = tileX * 8; x < tileX * 8 + 8; x++)
                            {
                                QPoint pixelPos = info.m_topLeft + QPoint(x,y);
                                if (pixelPos.x() < image->width() && pixelPos.y() < image->height())
                                {
                                    QRgb pixelColor = resource.m_croppedImage->pixel(pixelPos);
                                    if (pixelColor != 0)
                                    {
                                        if (!colorToIndexMap.contains(pixelColor))
                                        {
                                            int index = palette.indexOf(pixelColor);
                                            Q_ASSERT(index != -1);
                                            colorToIndexMap.insert(pixelColor, index);
                                        }
                                        tileset.m_tileData[tileDataPixel] = colorToIndexMap[pixelColor];
                                    }
                                }
                                tileDataPixel++;
                            }
                        }
                    }
                }

                // If need even OAM, skip 8x8 tile (64 pixels)
                if (m_evenOAM && size == QSize(1,1))
                {
                    tileDataPixel += 64;
                }
            }
        }
        Q_ASSERT(tileDataPixel == tileset.m_tileData.size());
    }

    // Add a 8x8 tileset empty frame
    if (m_emptyFrame)
    {
        Frame emptyFrame;
        emptyFrame.m_palGroup = 0;
        emptyFrame.m_palIndex = 0;
        emptyFrame.m_shadowType = ST_RESERVED; // use reserved to force an 8x8 in
        m_frames.push_back(emptyFrame);

        Tileset emptyTileset;
        emptyTileset.m_palGroup = 0;
        emptyTileset.m_palIndex = 0;
        emptyTileset.m_reserveShadow = true;
        emptyTileset.m_tileData = QByteArray(64, 0);
        m_tilesets.push_back(emptyTileset);
        m_frameToTilesetMap.insert(m_frames.size() - 1, m_tilesets.size() - 1);

        QImage emptyImage(ui->Frame_LW->iconSize(), QImage::Format_RGBA8888);
        emptyImage.fill(0);
        QListWidgetItem* item = new QListWidgetItem(QIcon(QPixmap::fromImage(emptyImage)), "Empty Frame");
        ui->Frame_LW->addItem(item);
    }

    emit BuildDataReady(m_tilesets.size());
    UpdateStatus();

    ui->Frame_LW->setCurrentRow(-1);
    ResetLayer();
    BuildLayoutChange(true);
    QMessageBox::information(this, "Custom Sprite Maker", "Sprite build completed!", QMessageBox::Ok);
}

void CustomSpriteManager::on_Build_PB_Frame_clicked()
{
    if (m_currentFrame == -1 || m_tilesets.empty()) return;
    emit BuildPushFrame(m_currentFrame, false);
}

void CustomSpriteManager::on_Build_PB_Anim_clicked()
{
    if (m_currentFrame == -1 || m_tilesets.empty()) return;
    emit BuildPushFrame(m_currentFrame, true);
}

void CustomSpriteManager::on_Build_PB_Resume_clicked()
{
    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString message = "Resume editing will require rebuilding sprite which will discard any unexported sprites in Sprite Editor! Do you wish to continue?";
    resBtn = QMessageBox::warning(this, "Resume Editing", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    // Remove the empty frame
    if (m_emptyFrame)
    {
        m_frames.pop_back();
        m_tilesets.pop_back();
        delete ui->Frame_LW->takeItem(ui->Frame_LW->count() - 1);
    }

    ResetBuild();
    BuildLayoutChange(false);
    UpdateStatus();
}

//---------------------------------------------------------------------------
// Passes tileset data converted to uint8_t
//---------------------------------------------------------------------------
void CustomSpriteManager::GetRawTilesetData(int tilesetID, std::vector<uint8_t> &data)
{
    Tileset const& tileset = m_tilesets[tilesetID];
    data.clear();
    data.reserve(tileset.m_tileData.size() / 2);
    for (int i = 0; i < tileset.m_tileData.size() / 2; i++)
    {
        char p1 = tileset.m_tileData[i*2];
        char p2 = tileset.m_tileData[i*2+1];
        data.push_back(static_cast<uint8_t>(p1 + (p2 << 4)));
    }
}

void CustomSpriteManager::SetPushAnimEnabled(bool enabled)
{
    bool frameSelected = m_currentFrame != -1;
    bool result = !m_tilesets.empty() && frameSelected && enabled;
    ui->Build_PB_Anim->setEnabled(result);
}

void CustomSpriteManager::SetPushFrameEnabled(bool enabled)
{
    bool frameSelected = m_currentFrame != -1;
    bool result = !m_tilesets.empty() && frameSelected && enabled;
    ui->Build_PB_Frame->setEnabled(result);
}

void CustomSpriteManager::GetFrameData(int frameID, BNSprite::Frame &bnFrame)
{
    // We already cached the frame before
    if (m_cachedBNFrames.contains(frameID))
    {
        BNSprite::Frame const& copyFrame = m_cachedBNFrames[frameID];
        bnFrame.m_paletteGroupID = copyFrame.m_paletteGroupID;
        bnFrame.m_tilesetID = copyFrame.m_tilesetID;

        BNSprite::SubAnimation subAnim;
        subAnim.m_subFrames.push_back(BNSprite::SubFrame());
        bnFrame.m_subAnimations.push_back(subAnim);
        bnFrame.m_objects.push_back(copyFrame.m_objects[0]);

        return;
    }

    // Generate frame data
    Frame const& frame = m_frames[frameID];
    bnFrame.m_paletteGroupID = frame.m_palGroup;
    bnFrame.m_tilesetID = m_frameToTilesetMap[frameID];
    Tileset const& tileset = m_tilesets[bnFrame.m_tilesetID];

    BNSprite::SubAnimation subAnim;
    subAnim.m_subFrames.push_back(BNSprite::SubFrame());
    bnFrame.m_subAnimations.push_back(subAnim);

    BNSprite::Object object;
    object.m_paletteIndex = frame.m_palIndex;

    // Add empty 8x8 for shadow
    if (frame.m_shadowType == ST_RESERVED)
    {
        object.m_subObjects.push_back(BNSprite::SubObject());
    }

    for (Layer const& layer : frame.m_layers)
    {
        int resourceID = m_resourceNamesIDMap[layer.m_resourceName];
        int resourcePosInTileset = tileset.m_resourceIDs.indexOf(resourceID);
        Q_ASSERT(resourcePosInTileset != -1);

        // Find the start tile of the OAM
        int tileStart = tileset.m_reserveShadow ? (m_evenOAM ? 2 : 1) : 0;
        for (int i = 0; i < resourcePosInTileset; i++)
        {
            Resource const& r = m_resources[tileset.m_resourceIDs.at(i)];
            tileStart += r.m_tileCount;

            // If need even OAM, add 1 tile for each 8x8
            if (m_evenOAM)
            {
                for (OAMInfo const& info : r.m_oamInfoList)
                {
                    QSize size = m_oamSizesMap[info.m_oamSize];
                    if (size == QSize(1,1))
                    {
                        tileStart++;
                    }
                }
            }
        }

        // Add sub object for each OAM
        Resource const& resource = m_resources[resourceID];
        for (OAMInfo const& info : resource.m_oamInfoList)
        {
            QSize size = m_oamSizesMap[info.m_oamSize];
            QPoint oamPos = GetOAMPos(layer, resource, info);

            BNSprite::SubObject subObject;
            subObject.m_startTile = tileStart;
            subObject.m_sizeX = size.width() * 8;
            subObject.m_sizeY = size.height() * 8;
            subObject.m_posX = oamPos.x();
            subObject.m_posY = oamPos.y();
            subObject.m_hFlip = layer.m_hFlip;
            subObject.m_vFlip = layer.m_vFlip;
            object.m_subObjects.push_back(subObject);

            tileStart += size.width() * size.height();

            // If need even OAM, skip 8x8 tile
            if (m_evenOAM && size == QSize(1,1))
            {
                tileStart++;
            }
        }
    }
    bnFrame.m_objects.push_back(object);

    // Cache the frame
    m_cachedBNFrames.insert(frameID, bnFrame);
}
