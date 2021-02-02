#ifndef CUSTOMSPRITEMANAGER_H
#define CUSTOMSPRITEMANAGER_H

#include <QDebug>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMessageBox>
#include <QtCore>
#include <QtGui>

#include "bnsprite.h"
#include "buildoptiondialog.h"

namespace Ui {
class CustomSpriteManager;
}

typedef QVector<QRgb> Palette;
typedef QVector<Palette> PaletteGroup;

class CustomSpriteManager : public QMainWindow
{
    Q_OBJECT

public:
    explicit CustomSpriteManager(QWidget *parent = nullptr);
    ~CustomSpriteManager();

    bool eventFilter(QObject *object, QEvent *event);
    void closeEvent (QCloseEvent *event);

    // Easy file reading
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);

    void SetDefaultPath(QString _path) {m_path = _path;}
    void LoadProject(QString const& file);

    // Build Sprite external functions
    void GetRawTilesetData(int tilesetID, std::vector<uint8_t>& data);
    QVector<PaletteGroup> const& GetPaletteData() {return m_paletteGroups;}
    void SetPushAnimEnabled(bool enabled);
    void SetPushFrameEnabled(bool enabled);
    void GetFrameData(int frameID, BNSprite::Frame& bnFrame);

signals:
    void BuildDataReady(int tilesetCount);
    void BuildCheckButton();
    void BuildPushFrame(int frame, bool newAnim);
    void LoadProjectSignal(QString file, uint32_t saveVersion, qint64 skipByte, int tilesetCount);
    void SaveProjectSignal(QString file);

private:
    typedef QPair<int,int> Ownership;
    typedef QVector<Ownership> PaletteOwnerships;

    enum OAMSize : int
    {
        SIZE_64x64 = 0,
        SIZE_64x32,
        SIZE_32x64,
        SIZE_32x32,
        SIZE_32x16,
        SIZE_16x32,
        SIZE_16x16,
        SIZE_32x8,
        SIZE_8x32,
        SIZE_16x8,
        SIZE_8x16,
        SIZE_8x8,
        SIZE_COUNT
    };

    struct OAMInfo
    {
        OAMSize m_oamSize;
        QPoint m_topLeft; // relative pos from m_croppedImage
    };

    struct Resource
    {
        QString m_name;
        QImage* m_image;
        Palette m_palette;
        PaletteOwnerships m_paletteOwnerships;

        QImage* m_thumbnail;
        bool m_selectable;

        QPoint m_croppedStartPos; // m_croppedImage relative pos from m_image
        QImage* m_croppedImage;
        QPoint m_tileStartPos;
        QVector<bool> m_usedTiles;
        QVector<int> m_tileAllowance;
        QVector<OAMInfo> m_oamInfoList;
        int m_tileCount;
        bool m_forceSingleOAM;

        Resource()
        {
            m_tileAllowance = QVector<int>(5, 0);
        }

        Resource(int _64, int _32, int _16, int _8, int _4)
        {
            setAllowance(_64,_32,_16,_8,_4);
        }

        void setAllowance(int _64, int _32, int _16, int _8, int _4)
        {
            m_tileAllowance.clear();
            m_tileAllowance.push_back(_64);
            m_tileAllowance.push_back(_32);
            m_tileAllowance.push_back(_16);
            m_tileAllowance.push_back(_8);
            m_tileAllowance.push_back(_4);
        }

        int getAllowance(OAMSize oamSize)
        {
            switch (oamSize)
            {
            case SIZE_64x64:
                return m_tileAllowance[0];
            case SIZE_64x32:
            case SIZE_32x64:
                return m_tileAllowance[1];
            case SIZE_32x32:
                return m_tileAllowance[2];
            case SIZE_32x16:
            case SIZE_16x32:
                return m_tileAllowance[3];
            case SIZE_16x16:
            case SIZE_32x8:
            case SIZE_8x32:
                return m_tileAllowance[4];
            default:
                return 0;
            }
        }

        void clear()
        {
            delete m_image;
            delete m_thumbnail;
            delete m_croppedImage;
        }
    };

    struct Layer
    {
        QString m_resourceName;
        QPoint m_pos;
        bool m_hFlip;
        bool m_vFlip;
    };

    enum ShadowType : int
    {
        ST_RESERVED = 0,
        ST_FIRST,
        ST_NONE
    };

    struct Frame
    {
        int m_palGroup;
        int m_palIndex;
        ShadowType m_shadowType;
        QVector<Layer> m_layers;
    };

    struct Tileset
    {
        int m_palGroup;
        int m_palIndex;
        bool m_reserveShadow;
        QVector<int> m_resourceIDs;
        QByteArray m_tileData;
    };

private slots:
    // Actions
    void on_actionLoad_Project_triggered();
    void on_actionSave_Project_triggered();

    // Palette
    void on_Palette_RB_16Mode_clicked();
    void on_Palette_RB_256Mode_clicked();
    void on_Palette_PB_NewPal_clicked();
    void on_Palette_PB_DelPal_clicked();
    void on_Palette_PB_NewGroup_clicked();
    void on_Palette_PB_DelGroup_clicked();
    void on_Palette_SB_Group_valueChanged(int arg1);
    void on_Palette_SB_Index_valueChanged(int arg1);
    void on_Palette_Color_changed(int paletteIndex, int colorIndex, QRgb color);

    // Resources
    void on_Resources_PB_Add_clicked();
    void on_Resources_PB_Delete_clicked();
    void on_Resources_PB_Sample_clicked();
    void on_Resources_PB_GenAll_clicked();
    void on_Resources_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_Resources_LW_itemDoubleClicked(QListWidgetItem *item);
    void on_Resources_LW_pressed(const QModelIndex &index);
    void on_Resources_SB_64_valueChanged(int arg1);
    void on_Resources_SB_32_valueChanged(int arg1);
    void on_Resources_SB_16_valueChanged(int arg1);
    void on_Resources_SB_8_valueChanged(int arg1);
    void on_Resources_SB_4_valueChanged(int arg1);
    void on_Resources_CB_SingleOAM_toggled(bool checked);

    // Preview
    void on_Preview_BG_currentTextChanged(const QString &arg1);
    void on_Preview_Highlight_timeout();
    void on_Preview_CB_Highlight_clicked(bool checked);
    void on_Preview_pressed(QPoint pos);

    // Layer
    void on_Layer_LW_layerUpdated(bool isSelf);
    void on_Layer_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_Layer_LW_itemDoubleClicked(QListWidgetItem *item);
    void on_Layer_PB_Delete_clicked();
    void on_Layer_PB_Clear_clicked();
    void on_Layer_SB_X_valueChanged(int arg1);
    void on_Layer_SB_Y_valueChanged(int arg1);
    void on_Layer_CB_HFlip_toggled(bool checked);
    void on_Layer_CB_VFlip_toggled(bool checked);
    void on_Layer_RB_Reserved_toggled(bool checked);
    void on_Layer_RB_First_toggled(bool checked);
    void on_Layer_RB_None_toggled(bool checked);
    void on_Layer_PB_Save_clicked();
    void on_Layer_PB_Update_clicked();

    // Frame
    void on_Frame_LW_itemPressed(QListWidgetItem *item);
    void on_Frame_LW_itemDoubleClicked(QListWidgetItem *item);
    void on_Frame_PB_Delete_clicked();

    // Build
    void on_Build_PB_Sprite_clicked();
    void on_Build_PB_Frame_clicked();
    void on_Build_PB_Anim_clicked();
    void on_Build_PB_Resume_clicked();

private:
    void ResetProgram();
    void ResetOAM();
    void ResetLayer();
    void ResetBuild();
    void BuildLayoutChange(bool buildComplete);
    void UpdateStatus(const QString &status = QString(), QColor color = QColor(0,0,0));

    // Palette
    bool GeneratePaletteFromFiles(QStringList const& files, int group);
    void GetImagePalette(Palette& palette, QImage const* image);
    bool ComparePalette(Palette const& source, Palette const& palette);
    bool FindExistingPalette(Resource& resource);
    void DeletePalette(int group, int index);
    void UpdatePalettePreview();
    void EnablePaletteWidgets();

    // Resources
    void SetResourceAllowance(Resource& resource, bool loadDefault);
    void GetResourceAllowance(Resource const& resource);
    void UpdateResourceNameIDMap();
    void UpdateDrawOAMSample();
    bool GetResourceIDsForPalette(QVector<int>& resourceIDs, int group, int index, bool thisPaletteOnly);
    bool FindTileUsedCount(QImage const* image, QVector<bool>& testTiles, int& minUsedTileCount, QPoint tileStartPos);
    bool TestTileUsed(QImage const* image, QPoint tileStartPos, int tileX, int tileY);
    void SampleOAM(Resource& resource);
    bool FindMostTileOAM(QVector<bool> const& usedTiles, QSet<int> const& sampledTiles, QSet<int>& sampledTilesForThisOAM, int allowance, int& tileXStart, int& tileYStart, QSize const oamSize, int tileXCount, int tileYCount);
    bool TestTileInOAM(QVector<bool> const& usedTiles, QSet<int> const& sampledTiles, QSet<int>& sampledTilesForThisOAM, int &allowance, QSize const oamSize, int tileXStart, int tileYStart, int tileXCount, int tileYCount);
    QImage DebugDrawUsedTiles(QImage const* image, QVector<bool> const& usedTiles, QPoint tileStartPos, int tileXCount, int tileYCount);
    QImage DebugDrawOAM(Resource const& resource);
    void SetResourcesSelectable();
    void DeleteResource(int resourceID);

    // Preview
    void UpdatePreviewZValues();

    // Layer
    void SaveLayer(int replaceID = -1);
    bool CheckLayerValidOAM(QVector<Layer> const& layers, QString& errorLayer);
    QPoint GetOAMPos(Layer const& layer, Resource const& resource, OAMInfo const& info);

    // Frame
    void DeleteFrame(int frameID);

private:
    Ui::CustomSpriteManager *ui;
    QString m_path;

    // Resource
    QVector<Resource> m_resources;
    QGraphicsScene* m_resourceGraphic;
    QMap<QString, int> m_resourceNamesIDMap;
    QMap<OAMSize, QSize> m_oamSizesMap;

    // Palette
    QVector<PaletteGroup> m_paletteGroups;

    // Highlight OAM square
    QTimer* m_highlightTimer;
    bool m_highlightUp;
    int m_highlightAlpha;
    QGraphicsPixmapItem* m_previewHighlight;

    // Preview
    QGraphicsScene* m_previewGraphic;
    QGraphicsPixmapItem* m_previewBG;
    QVector<QGraphicsPixmapItem*> m_previewPixmaps;

    // Layer
    QVector<QListWidgetItem*> m_layerItems;
    QVector<Layer> m_editingLayers;
    bool m_layerEdited;

    // Frame
    QVector<Frame> m_frames;
    int m_currentFrame;

    // Build
    bool m_emptyFrame;
    bool m_evenOAM;
    QVector<Tileset> m_tilesets;
    QMap<int,int> m_frameToTilesetMap;
    QMap<int,BNSprite::Frame> m_cachedBNFrames;
};

#endif // CUSTOMSPRITEMANAGER_H
